---
layout: post

title: Building a Link Cable Server
image: /images/tetris_link_nodejs.jpg
excerpt: >-
  With a better understanding of how different games use the link cable and what
  is required for them to work properly online, we have enough information to
  create a stable, future-proof backend server.

author: matt
date: 2022-09-04
---

{% capture intro_research_post %}
{% post_url 2021-05-10-An-8-Bit-Idea_The-Internet-of-Game-Boys %}
{% endcapture %}

{% capture indirect_link_post %}
{% post_url 2021-06-13-Beyond-Serial_Linking-Game-Boys-the-Hard-Way %}
{% endcapture %}

{% capture analysis_post %}
{% post_url 2022-07-24-Multi-Game-Link-Cable-Protocol-Analysis %}
{% endcapture %}

{%
   include image.html
   src="/images/tetris_link_nodejs.jpg"
   caption="Our prior analysis now allows us to build a stable, future-proof server"
%}

## Learning from experience

Until this point, our networking code has relied on several assumptions such as
the idea that naive byte forwarding would be sufficient to enable games to
operate properly after slave mode initialization. After
[research and experimentation]({{ analysis_post }}), these assumptions have
proven to be incorrect. We [knew]({{ intro_research_post }}#next-steps) we
[could be]({{ indirect_link_post }}#connecting-game-boys-through-a-pc) wrong,
and we were. This is why we didn't invest too much time in creating the existing
test scripts. However, we now have a much better idea of the different kinds of
usage patterns we must support and therefore more confidence going forward. It's
time to replace our experimental TCP serial bridge script with a fully-featured
backend server.

Due to the multitude of ways games can use the link cable, the new server cannot
be game-agnostic. To compensate, it must provide high-level abstractions and be
as flexible as possible. Even though we have a relatively thorough understanding
of link cable usage, the server should still be easily extensible to enable new
features and refactoring as the project evolves or new edge cases are
discovered. It's better to err on the side of caution than try to get away with
shortcuts like we've been doing until now -- they won't apply to all games and
will make the code harder to maintain.

After building the necessary server infrastructure
[identified]({{ analysis_post }}#conclusion) in our prior analysis, we will add
support for the simplest and most latency-tolerant game we have studied so far:
[Tetris]({{ analysis_post }}#tetris). Its protocol is very forgiving and does
not require every feature we have seen. Instead, it will serve as a solid
foundation to build an end-to-end vertical slice around (server, client,
front-end, etc.). More complexity (and games) can then be added once everything
is working at all layers of the stack.

## Server technologies and architecture

We chose to write the server in TypeScript and run on Node.js. Node's
asynchronous IO is perfect for a high-latency application like this. We will be
sending many small packets with minimal logic in between. The primary bottleneck
is going to be network latency. Lower-level technologies would offer little to
no performance benefit in practice at the cost of having to re-implement useful
abstractions and features that we get here for free (e.g., event loop, async IO,
easy networking, etc.). On the language side TypeScript is concise and flexible
yet strict, reducing boilerplate and facilitating rapid iteration with
confidence. In summary, our chosen backend stack is robust and scalable. It
allows us to focus on project-related logic rather than unnecessary low-level
details. Using web-friendly technologies from the start will also make it easier
to eventually add a front-end when the time comes.

With the stack decided upon, the next step is implementation. Due to the fact
that server-side logic will be required for most (if not all) games, it is
important to abstract away as much as possible so that per-game code can be
simple and only need to read and write bytes. We started by creating 2 classes
to handle network connections and game sessions:

| Class           | Description |
| --------------- | ----------- |
| `GameBoyClient` | This [class]({{ site.github.repository_url }}/blob/1dd98eac7a0c297391e6b9775c5308194eb90866/server/src/client.ts) manages the network connection for a single player (i.e., Game Boy). It is able to send and receive bytes, detect dropped connections, control the transfer delay, and forward data between itself and another client (naive byte forwarding) with the option to monitor the transmission. |
| `GameSession`   | This is an abstract [class]({{ site.github.repository_url }}/blob/1dd98eac7a0c297391e6b9775c5308194eb90866/server/src/game-session.ts) which corresponds to a single game that multiple `GameBoyClient`s are connected to. Each supported game requires a subclass which implements any game-specific logic. The base class logic keeps track of whether the game is joinable and is responsible for ending itself when there is an error or a client disconnects. It also runs a state machine which handles calling the appropriate subclass function depending on game state. |

When the
[server]({{ site.github.repository_url }}/blob/1dd98eac7a0c297391e6b9775c5308194eb90866/server/src/server.ts)
is started, it listens for incoming TCP connections. Since this is still just
using TCP, our existing tools -- namely the
[TCP serial client script]({{ indirect_link_post }}#networking-game-boys)
used to connect to real hardware -- will work here with no modifications needed.
Upon receiving a connection, a new `GameBoyClient` instance is created to manage
it and the server looks for an open `GameSession` for the client to join. A new
session is created if none exist. Sessions know when they're full and will wait
until enough players have joined before starting their main loop. For now,
players cannot configure the game being played or the specific session to join.
The game is hard-coded to Tetris and clients will join the first session
available. Choice of game and session will come later after more of the stack
has been built up.

Although the server is currently small, we are already able to take advantage of
some of the benefits of Node.js and TypeScript. Detection of dropped client
connections and ended games is accomplished using Node's
[EventEmitter](https://nodejs.org/api/events.html#class-eventemitter) class. A
`GameBoyClient` emits a `disconnect` event when relevant, which its
`GameSession` listens for and reacts to by ending the game. When this happens,
the session emits its own `end` event which the top-level session management
code uses to update the list of active sessions. Node's event loop takes care of
dispatching these events, allowing us to easily react asynchronously with only a
small amount of code.

When it comes to the `GameSession` subclasses, we want to be able to create each
new game-specific implementation quickly and easily with minimal duplication.
There will be many of these classes and so it is important to limit them to pure
game protocol logic, removing infrastructure-related code whenever possible.
TypeScript's
[decorator](https://www.typescriptlang.org/docs/handbook/decorators.html)
feature allows us to do just that. Specifically, we use
[decorator factories](https://www.typescriptlang.org/docs/handbook/decorators.html#decorator-factories)
to add each state handler function to a static lookup map so the base
`GameSession` logic knows when and how to call them. With this, all that is
needed to implement a new game is a class like the following:

```ts
enum MyGameState {
    Menus,
    InGame
};

class MyGame extends GameSession
{
    @stateHandler(MyGameState.Menus)
    handleMenus() {
        // Menu logic...

        this.state = MyGameState.InGame;
    }

    @stateHandler(MyGameState.InGame)
    handleInGame() {
        // Gameplay logic...

        this.state = MyGameState.Menus;
    }
};
```

The `stateHandler` decorator automatically sets up the function dispatch
mechanism and nothing else needs to be done. The `GameSession`'s main loop will
call `handleMenus()` when the game is in the `Menus` state and `handleInGame()`
when in the `InGame` state. This keeps all management-related code (except state
changes) out of the game logic classes which makes it simple to create new ones.
It is also very readable and easy to see what each function is for.

## Game-specific logic

With overall server structure and state handling taken care of, what remains is
the game state logic itself and how it interacts with clients. For example,
we've seen that many games require synchronization transfers: polling with a
particular value until a specific byte is received to indicate the connected
game is in a known state. Code and effort to implement common operations like
this should not be duplicated. To achieve this we abstracted as many of them as
possible. There are helper functions for:

* Waiting until a client has responded with a specified value
* Transferring a buffer to a client
* Adjusting the delay between each data transfer
* Performing the same operations for all clients
* Byte forwarding between clients, with the ability to monitor the data

These helpers allow the communication code for each state handler to be reduced
to a combination of primitive operations. For example, here is how Tetris'
pre-round initialization is implemented:

```ts
@stateHandler(TetrisGameState.SendingInitializationData)
async handleSendingInitializationData() {
    const garbageLineData = generateGarbageLines();
    const pieceData = generatePieces();

    // Send global data
    await this.forAllClients(async c => {
        // This is a lot of data, and timing requirements aren't as strict
        c.setSendDelayMs(5);

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
```

First, the initialization data (garbage line and piece buffers) is generated and
sent using `sendBuffer()`. This still takes place one byte at a time, but the
individual transfers are handled by the server to simplify the calling code.
Synchronization occurs before each buffer is sent -- the `waitForByte()` helper
function ensures the main data is only transmitted to a client after it has
indicated it is ready. `forAllClients()` repeats this logic for each connected
game and prevents moving on until they have reached the same point in lockstep.
Once all clients have been initialized, the round is started by sending the
magic bytes required by the game. Note that the two phases of this process have
different timing requirements. `setSendDelayMs()` ensures we are taking full
advantage and sending data as fast as possible at each step. It also takes
latency into account and will shorten or eliminate the delay based on the
connection.

These abstractions greatly reduce the complexity of server-side game logic,
which is important considering the number of potential games to support. If
needed, there is also room for further improvement through the creation of
higher-level helper functions for common _combinations_ of the existing
functions. For instance, forwarding bytes until the values do not change as
required by the menus of Tetris, F-1 Race, and Wave Race. Performance-wise,
`sendBuffer()` could be improved with packetization. If games do not send any
meaningful data while receiving a buffer, the server can send it all at once in
its entirety to avoid the latency overhead of transferring one byte at a time.
The client can then quickly feed each byte to the game individually. These
improvements are not needed right now but are straightforward to add if/when
required in the future. This also applies to unimplemented features like
keepalive packets and configurable master-only options.

## Tetris implementation

With the server's flexibility and all of the abstractions, the challenges of
implementing Tetris had more to do with game logic than networking or any other
GBPlay-specific functionality. For example, organizing end of game checks to
properly handle edge cases like draws. The goal of the server's architecture is
to provide everything necessary except game logic and so we view this as a
success. Our time was primarily spent on problems the original developers of the
game would have had to face as well.

What's interesting about the way the protocol works and the control we have here
is that we can generate garbage lines and random pieces however we want with
completely different algorithms (e.g., using a
[bag randomizer](https://harddrop.com/wiki/Random_Generator) like modern
Tetris). This is fun to think about, but to keep the experience as original as
possible we re-implemented the original algorithms.

Below is a video of Tetris running with the new backend. For the most part, it
behaves exactly as it would when running with two directly connected Game Boys.
Notably, it supports difficulty selection and multiple rounds due to the
server-side state machine. All of the work under the hood gives the
[appearance of none](https://www.youtube.com/watch?v=edCqF_NtpOQ) -- a success.

{%
   include youtube.html
   id="HI1r6OjBdxs"
   caption="Multiplayer Tetris running with the new backend server"
%}

One noticeable quirk is that both players appear as Luigi on the menus and round
end screen. This is because both games are running in slave mode (effectively
"player 2"), which is [required]({{ intro_research_post }}#gb-spi-and-latency)
to work over the internet. However, it is purely cosmetic. The important part is
that each game knows the difference between its own state and the state of the
opposing player, which they do. This is worth revisiting once keepalive
transfers are implemented. For lenient protocols like Tetris', we could allow
one game to operate in master mode and supply it with fake data until the slave
responds -- handling latency without the graphical oddity. The complexity of
this approach needs to be investigated further.

The full Tetris code is available
[here]({{ site.github.repository_url }}/blob/fa368a6ea0672940ef443a37bd3e1cd8e4e3000b/server/src/games/tetris.ts).
The game's link cable protocol details are documented
[here](../_game-protocols/Tetris.md).

## Looking forward

The knowledge gained through analyzing a wide variety of link cable protocols
has allowed us to design and implement a server architecture that makes it easy
to support different kinds of games. This new backend is lightweight and easily
extensible as we learn more, while also stable and good for testing. This is in
contrast to the existing TCP serial bridge script which is based on invalid
assumptions and is now obsolete. The fact that our server requires game-specific
logic is not ideal, but the logic itself is not very complex and abstractions
simplify its creation.

Implementing the Tetris protocol has helped validate the server design as well
as provide an internet-tolerant testbed to use while getting the full GBPlay
stack up and running end-to-end. With a working core we can now turn our focus
to building other areas that rely on it as well as adding missing features to
the server itself. With that in mind, our next step will be to use what we have
learned to improve upon our
[USB link cable adapter]({% post_url 2021-05-29-Connecting-to-a-Game-Boy-Link-Cable-From-a-PC %})
and create a Wi-Fi enabled adapter that can connect to this new server!
