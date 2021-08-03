import { GameSession, stateHandler } from "../game-session";
import { sleep } from "../util";

enum TetrisGameState {
    WaitingForPlayers,
    PlayersConnected,
    DifficultySelection,
    SendingInitializationData,
    Playing,
    RoundOver
};

enum TetrisCtrlByte {
    Master = 0x29,
    Slave = 0x55,
    SolidTile = 0x80,
    EmptyTile = 0x2F,
    ReadyForMusic = 0x39,
    MusicTypeA = 0x1C,
    MusicTypeB = 0x1D,
    MusicTypeC = 0x1E,
    MusicOff = 0x1F,
    ConfirmMusic = 0x50,
    ConfirmMenu = 0x60,
    Win = 0x77,
    Lose = 0xAA,
    Poll = 0x02,
    ReadyForRoundEnd = 0x34,
    ReadyForRestart = 0x27,
    BeginRoundOverScreen = 0x43,
    EndRoundOverScreen = 0x79
};

export class TetrisGameSession extends GameSession {
    // TODO: configurable
    private readonly musicType = TetrisCtrlByte.MusicTypeA;

    private client1WinCount: number = 0;
    private client2WinCount: number = 0;

    constructor(id: string) {
        super(id);
        this.state = TetrisGameState.WaitingForPlayers;
    }

    private generateGarbageLines(): number[] {
        // Same algorithm as the original game. 50/50 chance of an empty space
        // versus a filled one. Filled spaces use 1 of 8 different tiles.
        // See https://github.com/alexsteb/tetris_disassembly/blob/master/main.asm#L4604
        const garbage: number[] = [];

        for (let i = 0; i < 100; ++i) {
            if (Math.random() >= 0.5) {
                const tile = Math.floor(Math.random() * 8);
                garbage.push(tile | TetrisCtrlByte.SolidTile);
            } else {
                garbage.push(TetrisCtrlByte.EmptyTile);
            }
        }

        return garbage;
    }

    private generatePieces(): number[] {
        // Same algorithm as the original game.
        // See https://harddrop.com/wiki/Tetris_(Game_Boy)#Randomizer and
        // https://github.com/alexsteb/tetris_disassembly/blob/master/main.asm#L1780
        const pieces: number[] = [];

        for (let i = 0; i < 256; ++i) {
            let nextPiece = 0;

            // Don't try forever
            for (let attempt = 0; attempt < 3; ++attempt) {
                // 7 choices of pieces, each is a multiple of 4 starting from 0
                nextPiece = Math.floor((Math.random() * 7)) * 4;

                // Try to avoid repeats
                const prevPiece1 = pieces[i - 1] || 0;
                const prevPiece2 = pieces[i - 2] || 0;
                if (((nextPiece | prevPiece1 | prevPiece2) & 0xFC) != prevPiece2) {
                    break;
                }
            }

            pieces.push(nextPiece);
        }

        return pieces;
    }

    protected reset(): void {
        this.client1WinCount = 0;
        this.client2WinCount = 0;
    }

    @stateHandler(TetrisGameState.WaitingForPlayers)
    handleWaitingForPlayers() {
        if (this.clients.length === 2) {
            this.state = TetrisGameState.PlayersConnected;
        }
    }

    @stateHandler(TetrisGameState.PlayersConnected)
    async handlePlayersConnected() {
        await this.forAllClients(async c => {
            c.setSendDelayMs(30);

            await c.waitForByte(TetrisCtrlByte.Master, TetrisCtrlByte.Slave);

            // Music selection
            await c.waitForByte(this.musicType, TetrisCtrlByte.ReadyForMusic);
            return c.exchangeByte(TetrisCtrlByte.ConfirmMusic);
        });

        this.state = TetrisGameState.DifficultySelection;
    }

    @stateHandler(TetrisGameState.DifficultySelection)
    async handleDifficultySelection() {
        this.reset();

        let lastDifficultyChangeTime = Date.now();
        let client1Difficulty = 0;
        let client2Difficulty = 0;

        // Neither player has the ability to confirm difficulty, so
        // do it automatically after changes have stopped occurring
        while ((Date.now() - lastDifficultyChangeTime) < 5000) {
            await this.clients[0].forwardByte(this.clients[1], (client1Byte: number, client2Byte: number) => {
                if (client1Byte !== client1Difficulty || client2Byte !== client2Difficulty) {
                    client1Difficulty = client1Byte;
                    client2Difficulty = client2Byte;
                    lastDifficultyChangeTime = Date.now();
                }
            });
        }

        await this.forAllClients(c => {
            return c.waitForByte(TetrisCtrlByte.ConfirmMenu, TetrisCtrlByte.Slave);
        });

        this.state = TetrisGameState.SendingInitializationData;
    }

    @stateHandler(TetrisGameState.SendingInitializationData)
    async handleSendingInitializationData() {
        const garbageLineData = this.generateGarbageLines();
        const pieceData = this.generatePieces();

        // Send global data
        await this.forAllClients(async c => {
            // This is a lot of data, and timing requirements aren't as strict
            c.setSendDelayMs(0);

            await c.waitForByte(TetrisCtrlByte.Master, TetrisCtrlByte.Slave);
            await c.sendBuffer(garbageLineData);

            await c.waitForByte(TetrisCtrlByte.Master, TetrisCtrlByte.Slave);
            return c.sendBuffer(pieceData);
        });

        // Start the game
        await this.forAllClients(c => {
            // The main game loop needs some time for each transfer
            c.setSendDelayMs(30);

            const startSequence = [0x30, 0x00, 0x02, 0x02, 0x20];
            return c.sendBuffer(startSequence);
        });

        this.state = TetrisGameState.Playing;
    }

    @stateHandler(TetrisGameState.Playing)
    async handlePlaying() {
        // We can't send the game anything right away or it will freeze
        await sleep(500);

        let roundOver = false;

        let client1StatusByte = 0;
        let client2StatusByte = 0;

        const isGameEndingByte = (b: number) => {
            return b === TetrisCtrlByte.Win || b === TetrisCtrlByte.Lose;
        };

        while (!roundOver) {
            await this.clients[0].forwardByte(this.clients[1], (client1Byte: number, client2Byte: number) => {
                if (isGameEndingByte(client1Byte)) {
                    client1StatusByte = client1Byte;
                }
                if (isGameEndingByte(client2Byte)) {
                    client2StatusByte = client2Byte;
                }

                if (client1Byte === TetrisCtrlByte.ReadyForRoundEnd &&
                    client2Byte === TetrisCtrlByte.ReadyForRoundEnd) {
                    roundOver = true;
                }
            });
        }

        let roundEndPollByte = TetrisCtrlByte.Poll;

        if (client1StatusByte === client2StatusByte) {
            // Draw. Notify clients in round end polling phase.
            roundEndPollByte = client1StatusByte;
        } else if (client1StatusByte === TetrisCtrlByte.Win ||
                   client2StatusByte === TetrisCtrlByte.Lose) {
            ++this.client1WinCount;
        } else {
            ++this.client2WinCount;
        }

        await this.forAllClients(async c => {
            await c.waitForByte(roundEndPollByte, TetrisCtrlByte.ReadyForRoundEnd);
            return c.exchangeByte(TetrisCtrlByte.BeginRoundOverScreen);
        });

        this.state = TetrisGameState.RoundOver;
    }

    @stateHandler(TetrisGameState.RoundOver)
    async handleRoundOver() {
        // Give time to look at results
        await sleep(10000);

        let expectedByte: number;
        let nextState: TetrisGameState;

        if (this.client1WinCount < 4 && this.client2WinCount < 4) {
            // New round
            expectedByte = TetrisCtrlByte.Slave;
            nextState = TetrisGameState.SendingInitializationData;
        } else {
            // New game
            expectedByte = 0;
            nextState = TetrisGameState.DifficultySelection;
        }

        // Prepare for a restart
        await this.forAllClients(async c => {
            await c.exchangeByte(TetrisCtrlByte.ConfirmMenu);
            await c.waitForByte(TetrisCtrlByte.Poll, TetrisCtrlByte.ReadyForRestart);
            return c.waitForByte(TetrisCtrlByte.EndRoundOverScreen, expectedByte);
        });

        this.state = nextState;
    }
};
