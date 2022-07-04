---
layout: protocol-doc

game: Street Fighter II
serial: DMG-ASFE-USA-1
year: 1995
max_players: 2
data_capture_name: street-fighter-2.csv

title: Street Fighter II Link Cable Protocol Documentation
image: /images/games/street-fighter-2/sf2_boxart.png
excerpt: >-
  A deep dive into Street Fighter II's Game Boy link cable protocol.
---

{%
   include image.html
   src="/images/games/street-fighter-2/sf2_boxart.png"
   caption="Not the best way to play Street Fighter, but look at that art!"
%}

## Description

It's not surprising that a Game Boy port of Street Fighter II exists given how
how many platforms the game has been released on over the years and how many
versions there are -- both licensed and
[unlicensed](https://bootleggames.fandom.com/wiki/Street_Fighter_II:_The_World_Warrior).
While not the best way to relive the arcade hit, it is an impressive-looking
port nonetheless.

The gameplay is simple: fight your way through each opponent in best-of-three
match-ups to become the champion. Each round lasts until one fighter loses all
of their HP or the timer runs out.

## Multiplayer gameplay

The core gameplay of Street Fighter II's multiplayer mode is the same as its
single-player mode. Players fight one-on-one in a best-of-three match. At the
end, the characters and stage can be changed. It is possible to play this mode
using either two Game Boys and a link cable, or a SNES with two controllers and
a Super Game Boy.

## Link cable protocol

### Role negotiation

{%
   include image.html
   src="/images/games/street-fighter-2/sf2_link_role_negotiation.png"
   caption="Initiating a connection"
%}

The first game to select "versus" from the main menu becomes the master and
takes on the responsibility of initiating all subsequent data transfers. It
indicates this by sending the byte `0x75`. The connected game responds with
`0x54`. The required delay between transfers is ~10 ms.

### Fighter selection

{%
   include image.html
   src="/images/games/street-fighter-2/sf2_link_fighter_selection.png"
   caption="Selecting characters. Player 2 (right) has just confirmed their selection by pressing `A` (bit 0)."
%}

On the fighter selection screen, each player chooses their character. When
entering this screen, two synchronization transfers take place before input is
allowed: `0xE9`/`0xEA` and `0xF0`/`0xFA`. After that, the joypad state is
constantly exchanged between both games to keep the UIs updated -- it is as if
each joypad is also connected to the linked Game Boy, similar to a multiplayer
console game. Each bit represents one of the 8 buttons, and they are in the same
order as they would be if read directly from the
[hardware](https://gbdev.io/pandocs/Joypad_Input.html). The required delay
between transfers is ~10 ms.

|Bit|Button|
|---|------|
|0  |A     |
|1  |B     |
|2  |Select|
|3  |Start |
|4  |Right |
|5  |Left  |
|6  |Up    |
|7  |Down  |

Once each player selects a fighter using either the `A` or `Start` button, both
games move to the stage selection screen. There is no data sent that indicates
this directly. Each of the two games is able to determine it is time because
they are both synchronized.

### Stage selection

{%
   include image.html
   src="/images/games/street-fighter-2/sf2_link_stage_selection.png"
   caption="Selecting characters. Player 1 (left) has just moved the cursor to the left (bit 5)."
%}

After fighter selection, the location to fight is chosen. Joypad input bytes are
again repeatedly exchanged between both games allowing either player to move the
cursor and confirm. As soon as one has made a selection using either `A` or
`Start`, both games begin the first round of the fight. The required delay
between transfers is ~10 ms.

### Fighting

{%
   include image.html
   src="/images/games/street-fighter-2/sf2_link_fighting.png"
   caption="Two synchronization transfers take place before a round can start"
%}

Each round has a short introduction, after which both games will pause until
two synchronization transfers take place: `0xE9`/`0xEA` and `0xF0`/`0xFA`. After
this, joypad input is constantly exchanged as in the menu-related states. The
required delay between transfers is ~5 ms.

The link cable protocol has no mechanism to report where fighters are or which
moves are used. Instead, both games feed the received joypad data to the
opponent character and update the game state in the same way as when the local
player presses a button. This works and keeps the two in sync because the game
engine is deterministic.

Both games detect the end of the round on their own and play a short animation.
After this happens, the next round starts using the same logic (beginning with
the two synchronization transfers). After a player has won two rounds, both
return to fighter selection.

## Summary and notes

Street Fighter II's protocol is not complicated at all. The game is real-time
and the protocol takes advantage of that by treating the second player as a
second controller. Avoiding special cases for multiplayer allows much of the
single-player code to be re-used and keeps data transfers as simple as they can
be.
