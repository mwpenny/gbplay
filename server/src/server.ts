import { Socket, Server } from "net";
import { GameBoyClient } from "./client";
import { TetrisGameSession } from "./games/tetris";

function generateSessionId(): string {
    // 26^4 = 456976 possibilities
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    let id = "";
    for (let i = 0; i < 4; ++i) {
        id += alphabet[Math.floor(Math.random() * alphabet.length)];
    }

    return id;
}

const SERVER_PORT = 1989;

// Just Tetris, for now
const sessions = new Map<string, TetrisGameSession>();

const server = new Server(async (socket: Socket) => {
    // Reduce latency
    socket.setNoDelay(true);

    const client = new GameBoyClient(socket);

    // Try to join an existing session first, then fall back to a new one
    let session = [...sessions.values()].find(s => s.isJoinable());
    if (!session) {
        let id: string;
        do {
            id = generateSessionId();
        } while (sessions.has(id));

        const newSession = new TetrisGameSession(id);
        newSession.on("end", () => {
            console.info(`Session '${newSession.id}' ended.`);
            sessions.delete(newSession.id);
        });

        sessions.set(id, newSession);
        newSession.run();

        session = newSession;
    }

    await session.addClient(client);
});

server.listen(SERVER_PORT, "0.0.0.0");
console.info(`Listening on port ${SERVER_PORT}...`);
