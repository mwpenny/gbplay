import { EventEmitter } from "events";
import { Socket } from "net";
import { sleep } from "./util";

/**
 * Represents a Game Boy connected via a networked link cable.
 */
export class GameBoyClient {
    public readonly id: string;

    private static readonly dataTimeoutMs = 10000;

    private lastReceivedByte: number = 0;
    private lastSendTime: number = Date.now();
    private eventEmitter: EventEmitter = new EventEmitter({ captureRejections: true });

    constructor(private readonly socket: Socket, private sendDelayMs: number = 5) {
        this.id = `${socket.remoteAddress}:${socket.remotePort}`;
        this.onConnect();
    }

    private onConnect(): void {
        console.info(`Client '${this.id}' connected.`);

        this.socket.on("error", (err: Error) => {
            console.error(`Error on client '${this.id}' socket: ${err.message}`);
        });

        this.socket.on("close", async () => {
            console.info(`Client '${this.id}' socket closed.`);
            this.eventEmitter.emit("disconnect");
        });

        this.eventEmitter.on("error", (error: Error) => {
            console.error(`Unhandled error in client event handler: ${error.message}`);
        });
    }

    private waitSendDelay(): Promise<void> {
        // Account for connection latency in delay time
        const sendDelta = Date.now() - this.lastSendTime;
        const msToSleep = this.sendDelayMs - sendDelta;

        if (msToSleep > 0) {
            return sleep(msToSleep);
        }
        return Promise.resolve();
    }

    /**
     * Adds a listener for the specified event.
     * @param event Name of event
     * @param listener Event listener
     */
    on(event: "disconnect", listener: () => void): void {
        this.eventEmitter.on(event, listener);
    }

    /**
     * Sets the amount of time to wait before sending each byte.
     * @param sendDelayMs Amount of time in milliseconds to wait before sending
     */
    setSendDelayMs(sendDelayMs: number): void {
        this.sendDelayMs = sendDelayMs;
    }

    /**
     * Sends a byte to the Game Boy and returns the byte the Game Boy sent.
     * @param tx The value to send (only the least significant byte will be used)
     * @returns The byte received from the connected Game Boy
     */
    async exchangeByte(tx: number): Promise<number> {
        await this.waitSendDelay();

        return new Promise<number>((resolve, reject) => {
            let sentByte = false;

            // Don't wait forever
            const timeout = setTimeout(() => {
                console.warn(`Client '${this.id}' did not respond within ${GameBoyClient.dataTimeoutMs} ms. Disconnecting.`)
                this.disconnect();
            }, GameBoyClient.dataTimeoutMs);

            const cleanup = () => {
                clearTimeout(timeout);
                this.socket.removeListener("close", closeListener);
                this.socket.removeListener("data", dataListener);
            };

            const closeListener = () => {
                cleanup();
                reject(new Error(`Client '${this.id}' disconnected before responding.`));
            };

            const dataListener = (data: Buffer) => {
                if (sentByte) {
                    cleanup();
                    this.lastReceivedByte = data.readUInt8(0);
                    resolve(this.lastReceivedByte);
                } else {
                    console.warn(`Client '${this.id}' sent data before receiving any. Discarding.`);
                }
            };

            this.socket.once("close", closeListener);
            this.socket.on("data", dataListener);

            this.socket.write(new Uint8Array([ tx & 0xFF ]), (err?: Error) => {
                if (err) {
                    cleanup();
                    reject(err);
                } else {
                    sentByte = true;
                    this.lastSendTime = Date.now();
                }
            });
        });
    }

    /**
     * Exchanges the most recently received byte from this Game Boy with the
     * specified Game Boy, `other`, as if the two devices were physically
     * connected.
     * @param other The second Game Boy to communicate with
     * @param onTransfer Optional callback to intercept the transferred values
     */
    async forwardByte(other: GameBoyClient, onTransfer?: (b1: number, b2: number) => void): Promise<void> {
        await other.exchangeByte(this.lastReceivedByte);

        // TODO: easy byte injection (e.g., Pokemon random seed)
        if (onTransfer) {
            onTransfer(this.lastReceivedByte, other.lastReceivedByte);
        }

        await this.exchangeByte(other.lastReceivedByte);
    }

    /**
     * Repeatedly polls the Game Boy until it responds with the specified value.
     * @param pollValue The value to send in order to poll the Game Boy
     * @param waitValue The value to wait for from the Game Boy
     */
    async waitForByte(pollValue: number, waitValue: number): Promise<void> {
        while ((await this.exchangeByte(pollValue)) !== waitValue);
    }

    /**
     * Sends the contents of a buffer to the Game Boy.
     * @param buf The values to send to the Game Boy (only the least
     *            significant byte of each value will be sent)
     * @returns The last value received from the Game Boy
     */
    async sendBuffer(buf: number[]): Promise<number> {
        let rx = 0;
        for (const b of buf) {
            rx = await this.exchangeByte(b);
        }
        return rx;
    }

    /**
     * Closes the connection to the client.
     */
    disconnect(): void {
        this.socket.destroy();
    }
}
