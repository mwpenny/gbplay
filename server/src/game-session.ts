import { EventEmitter } from "events";
import { GameBoyClient } from "./client";

/**
 * Returns a decorator which registers a `GameSession` member function as the
 * handler for the specified state. Once registered, the handler will
 * automatically be called on each tick while the session is in the state.
 * @param state State to associate with the decorated function
 * @returns Decorator to register a `GameSession` member function as the
 *          handler for `state`
 */
export function stateHandler(state: number) {
    return function (target: GameSession, _propertyKey: string, descriptor: PropertyDescriptor) {
        // Each GameSession subclass gets its own static state handler map
        if (!target.constructor.stateHandlers) {
            target.constructor.stateHandlers = new Map<number, Function>();
        }
        target.constructor.stateHandlers.set(state, descriptor.value);
    };
}

/**
 * Base class for server-side game logic.
 */
export abstract class GameSession {
    // Allows accessing static properties from a class instance. This is needed
    // so each subclass can have its own static state handler lookup map.
    declare ["constructor"]: typeof GameSession;

    // Initialized when the first handler is added by the decorator
    declare static stateHandlers: Map<number, Function>;

    /** Clients connected to the session */
    protected clients: GameBoyClient[] = [];

    /** The current game state */
    protected state: number = 0;

    private eventEmitter: EventEmitter = new EventEmitter({ captureRejections: true });
    private ended: boolean = false;

    constructor(public readonly id: string) {
        this.eventEmitter.on("error", (error: Error) => {
            console.error(`Unhandled error in ${this.constructor.name} event handler: ${error.message}`);
        });

        console.info(`Created new ${this.constructor.name} with ID '${this.id}'.`);
        this.reset();
    }

    private async handleState(state: number): Promise<any> {
        const handler = this.constructor.stateHandlers.get(state);
        if (!handler) {
            // TODO: actual enum value name in error message
            throw new Error(`${this.constructor.name} has no handler for state '${state}'.`);
        }
        return Promise.resolve(handler.apply(this));
    }

    private end(): void {
        if (!this.ended) {
            console.info(`Ending session '${this.id}'.`);

            this.ended = true;
            this.clients.forEach(c => c.disconnect());
            this.clients = [];

            this.eventEmitter.emit("end");
        }
    }

    /**
     * Resets the game back state back to its initial values.
     */
    protected abstract reset(): void;

    /**
     * Adds a listener for the specified event.
     * @param event Name of event
     * @param listener Event listener
     */
    on(event: "end", listener: (client: GameBoyClient) => void): void {
        this.eventEmitter.on(event, listener);
    }

    /**
     * Adds a client to the game.
     * @param client The client to add
     */
    async addClient(client: GameBoyClient): Promise<void> {
        console.info(`Client '${client.id}' joined session '${this.id}'.`);

        client.on("disconnect", async () => {
            console.info(`Client '${client.id}' left session '${this.id}'.`);

            // Game Boy games can't handle players re-joining. Bail.
            this.end();
        });

        this.clients.push(client);
    }

    /**
     * Returns whether or not clients are allowed to join the session.
     */
    isJoinable(): boolean {
        return !this.ended && this.clients.length < 2;
    }

    /**
     * Performs the same action for each client and waits until it has
     * completed for all of them.
     * @param func Callback to run for each client. The `GameBoyClient`
     *             instance is passed as an argument.
     * @returns Array of results from each invocation of `func`
     */
    forAllClients<T>(func: (client: GameBoyClient) => Promise<T>): Promise<T[]> {
        return Promise.all(this.clients.map(c => func(c)));
    }

    /**
     * Runs the state machine for the game session.
     */
    run(): void {
        this.handleState(this.state).then(() => {
            setImmediate(() => this.run());
        }).catch((error: Error) => {
            // Socket errors will occur naturally when the game ends
            if (!this.ended) {
                // TODO: actual enum value name in error message
                console.error(`Error handling state '${this.state}' in session '${this.id}': ${error.message}`);
                this.end();
            }
        });
    }
}
