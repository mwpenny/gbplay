---
layout: protocol-doc

game: Tetris
serial: DMG-TR-USA
year: 1989
max_players: 2
data_capture_name: tetris.csv

title: Tetris Link Cable Protocol Documentation
image: /images/games/tetris/tetris_boxart.png
excerpt: >-
  Let's analyze the link cable protocol for Tetris on the Game Boy.
---

{%
   include image.html
   src="/images/games/tetris/tetris_boxart.png"
   caption="The real puzzle was figuring out who the [rights holders](https://en.wikipedia.org/wiki/Tetris#History) were"
%}

## Description

Tetris needs no introduction. The classic puzzle game has been re-released on
countless platforms over the years. During gameplay, tetrominoes (pieces
composed of 4 tiles in different arrangements) fall one at a time from the top
of the playfield. The player can move and rotate each piece until it reaches a
previously-placed piece or the bottom of the playfield. When a horizontal line
of tiles is created, the line is cleared and all pieces above it move down one
row. The objective of the game is to clear as many lines as possible for a high
score before the stack of tetrominoes reaches the top of the playfield.

## Multiplayer gameplay

In Tetris' multiplayer mode both players play the game in parallel. The
completion of horizontal lines causes unclearable "garbage" lines to appear at
the bottom of the opposing player's playfield to add difficulty. More cleared
lines result in more garbage lines. A player wins a round when they have cleared
30 lines or the opponent has lost. After one player has won 4 rounds, the
multiplayer game ends and can be restarted if desired.

While a round is in progress, the height of the opposing player's stack is
shown via a thermometer-style indicator on the left side of the screen. Both
players receive the same set of random tetrominoes to ensure fairness, and both
have the option of selecting a difficulty before the game begins. The difficulty
determines how many garbage lines will be present at the bottom of the playfield
at the start of each round. The possible lines are shared by both players (i.e.,
each selects the relevant number of garbage lines from the top of the shared
data).

## Link cable protocol

### Role negotiation

{%
   include image.html
   src="/images/games/tetris/tetris_link_role_negotiation.png"
   caption="Initiating a connection"
%}

The connection begins with the two games choosing which device will act as the
master and initiate all data transfers. This role goes to the first player to
select "2 player" on the main menu, which causes the byte `0x29` to be sent.
Upon receiving this byte, the linked game sends `0x55` as an acknowledgement and
both move to the music selection screen.

### Music selection

{%
   include image.html
   src="/images/games/tetris/tetris_link_music_selection.png"
   caption="Choosing the music track. Both UIs are synchronized."
%}

On the music selection screen, the master (and **only** the master) chooses which
track will play during the game's rounds. The current menu position (`0x1C` -
`0x1F`) is repeatedly sent to the slave so that its graphics and sound can be
updated accordingly (its responses are ignored). When the selection is
confirmed by pressing start, `0x50` is sent and both games move to the
difficulty selection screen.

### Difficulty selection

{%
   include image.html
   src="/images/games/tetris/tetris_link_difficulty_selection.png"
   caption="Choosing the difficulty. This time communication is bidirectional."
%}

After music selection, each player chooses their difficulty. This controls the
number of garbage lines present at the bottom of the playfield when a round
begins. Unlike music selection, both players have control over their own
difficulty setting. During this time, the two games repeatedly exchange their
current menu position (`0x00` - `0x05`) so their graphics can be updated to
reflect the other player's choice. Again, only the master can press start and
trigger the next state (pre-game initialization). This is indicated with the
byte `0x60`, to which the slave responds with `0x55`.

### Pre-game initialization

{%
   include image.html
   src="/images/games/tetris/tetris_link_pre_game_initialization.png"
   caption="Sending initialization data. This screenshot captures some tetromino data -- `0x0C`, `0x04`, `0x14`, `0x18`, `0x08` represents O, J, S, T, and I pieces."
%}

To avoid giving any player an unfair advantage, the master sends 256 random
tetrominoes and then 10 random garbage lines before each round begins so that
both players receive the same pieces and have the same initial garbage (if
difficulty settings are different between players, they just truncate the shared
garbage tiles at different heights). The start of each block of data is signaled
with the same exchange as in role negotiation (`0x29`/`0x55`).

After these transfers, the game can begin. This is indicated with the magic bytes
`0x30`, `0x00`, `0x02`, `0x02`, `0x20`.

#### Tetromino generation

Each random tetromino is generated by selecting a number between 0 and 6
(inclusive) as the candidate piece. Next, the bitwise OR of the candidate piece
and previous two pieces is computed. If the resulting number is not equal to the
that of the tetromino from two selections prior, the candidate is accepted and
the value multiplied by 4 is sent. Otherwise, the process repeats up to two more
times, after which the candidate is accepted even if the check fails. This logic
is intended to lower the likelihood of duplicate consecutive pieces. More
information is available
[here](https://harddrop.com/wiki/Tetris_(Game_Boy)#Randomizer). Tetromino IDs
are listed below.

|ID    |Tetromino shape|
|------|---------------|
|`0x00`|L              |
|`0x04`|J              |
|`0x08`|I              |
|`0x0C`|O              |
|`0x10`|Z              |
|`0x14`|S              |
|`0x18`|T              |

If a round lasts long enough to exhaust the 256-tetromino buffer, the sequence
repeats.

#### Garbage tile generation

Garbage lines are generated much more simply than tetrominoes. For each
individual tile that can contain garbage at the start of a round (10 rows x 10
tiles = 100 bytes) there is a ~50% chance of the tile being solid. If yes, bit 7
is set and the bottom 3 bits are randomly populated to select the visual style
of the tile. Otherwise, the value `0x2F` is used to denote that the tile is
clear.

### Main gameplay

{%
   include image.html
   src="/images/games/tetris/tetris_link_main_gameplay.png"
   caption="A game in progress. Note how the sent data matches the sending player's stack height and received data matches the thermometer-style indicator."
%}

Finally, the round can begin. During gameplay, both games constantly send the
height of their stack to each other. This information is used to draw the
thermometer-style indicator on the left of the screen. When the opponent's stack
height decreases (meaning that lines were cleared), the receiving game adds
garbage to the bottom of the playfield (i.e., the opponent "attacks" and makes
the game more difficult). At any time, the master game can also trigger a pause
by sending `0x94` (acknowledged with a `0x00` from the slave). The pause will
end when `0x94` stops being sent (acknowledged with `0xFF`).

The game indicates a win (clearing 30 lines) with `0x77` and a loss (exceeding
the height of the playfield) with `0xAA`. Once both games acknowledge the end of
the round by sending `0x34`, the master delays for a short time and then sends
`0x43` to move to the round end screen.

### Round end

{%
   include image.html
   src="/images/games/tetris/tetris_link_round_end.png"
   caption="The score summary at the end of a round. In this case the master player won."
%}

The round end screen displays who won or lost, or whether the round ended in a
draw. The master player (who always appears as Mario) decides when to move on
by pressing start, at which point `0x60` is sent (acknowledged with `0x27`)
followed by `0x79`.

What happens next depends on whether a player has won 4 rounds or not. If
yes, both games return to difficulty selection. If no, another round begins
(pre-game initialization state). There is no opportunity to change the music.

## Summary and notes

Tetris' link cable protocol is simple and straightforward. So simple, in fact,
that it makes no attempt to detect a dropped connection. The link cable can be
unplugged at any time. If done during the setup menus, this will freeze the
game for the slave. However, during the main gameplay state both players can
continue to play -- albeit with out-of-date opponent stack information and the
inability to send or receive garbage lines. After reconnecting the cable, both
games will resume as expected.

This makes a lot of sense for a Game Boy launch title. Later games and those
with more real-time elements have more complex protocols. Tetris' primitive
protocol is as good as it needs to be.
