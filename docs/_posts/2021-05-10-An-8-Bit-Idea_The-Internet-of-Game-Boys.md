---
layout: post
title: "An 8-bit Idea: The Internet of Game Boys"
author:
   name: Matt Penny
   username: mwpenny

date: 2021-05-10
---

{%
   include image.html
   src="/images/dmg_link_cable_port.jpg"
   caption="The side of an original DMG-01 Game Boy"
%}

## The spark

In early May, my friend [@aidancrowther](https://github.com/aidancrowther)
and I came across an incredibly cool video by hardware hacking YouTuber
[stacksmashing](https://www.youtube.com/c/stacksmashing). For his latest project
he'd created an adapter to connect an original Game Boy to a PC via the link cable
peripheral, along with a web server which could host multiplayer Tetris games by
bridging that connection. This combination allows players to face off in
competitive block stacking match-ups over the internet with original hardware!

As someone who loves 80s and 90s-era Nintendo and has
[dabbled](https://github.com/mwpenny/GameDroid) in
[emulation](https://github.com/mwpenny/pureNES), this is awesome!
However Aidan and I both saw room for improvement and a chance to learn a lot
along the way. So we decided to start our own online link cable project.
Before diving into more detail, check out stacksmashing's original video below.

{% include youtube.html id="KtHu693wE9o" caption="Stacksmashing's original video" %}

## How it works

To summarize the video, there are three main components involved in making this work:

1. A Raspberry Pi Pico, which implements the Game Boy's link cable protocol and mediates
   data transfer between the PC and handheld

2. A website, which utilizes [WebUSB](https://developer.mozilla.org/en-US/docs/Web/API/WebUSB_API)
   to communicate with the Pi and send data to and from the backend server

3. A backend server, which manages players and host-side game logic such as game over,
   sending garbage lines to other players when lines are cleared, etc.

Under normal conditions (before we started living in the future) one Tetris
instance acts as the game host and controls certain aspects of play such as the
music and when the game ends. Stacksmashing moved that logic to the server side
which allows each instance of the game to act as a client, thereby supporting a
theoretically infinite number of players! The server broadcasts received messages
to all connected clients so everyone is effectively part of the same game.

In Tetris, the only substantial interaction one player has with another is
attacking them by sending garbage lines to add some challenge. This makes the
game well-suited to a "battle royale" style like this (in fact, that's exactly
what the officially-licensed
[Tetris 99](https://en.wikipedia.org/wiki/Tetris_99) is).

## Improvements and goals

This is all very impressive but there are a few limitations: it only supports
Tetris and in order to play you need to be sitting at your computer using a
hard-wired connection. This is a handheld console! Furthermore, stacksmashing
didn't flesh out the hardware too much beyond the proof-of-concept stage, which
isn't the easiest to share with non-technical friends.

Being on the lookout for new side project ideas, we couldn't resist.

Our goal is to create a solution that supports as much of the Game Boy library
as possible (ideally generic/game-agnostic) and is plug and play, such that
everything can be done entirely from the Game Boy. This will take the form of
a small dongle with a link cable connector and Wi-Fi connectivity. Configuration
will be done via a custom Game Boy cartridge, with a mobile-friendly web page as
a backup option -- much more portable.

Overly-ambitious? Probably. But a fantastic opportunity for both of us to learn
more about hardware projects and stretch those low-level programming muscles. We
both love seeing how far old technology can be pushed beyond what it was ever
intended to do, and creating custom hardware to go with it will add a whole other
layer of depth and fun. We're extremely excited to start.

## Hardware details and feasibility

With our goals and ideas in mind, how realistic is this project? Clearly,
something along these lines is possible -- stacksmashing's project exists. But
before any work can begin we need to understand as many of the hardware details
as possible. Tetris works, but can other games cope with the latency of the
internet? We don't want to come to a disappointing conclusion after putting in
hours of time and effort.

### Understanding the link cable protocol

First, let's look at the link cable protocol. It's essentially
[SPI](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface), which is still
in wide use today in embedded devices ranging from SD cards to LCD screens. That
means there are many ready-to-use libraries for interfacing with it. Doing so
manually isn't particularly difficult either (by design). It's a very simple
serial protocol in which one master device communicates with one or more slave
devices. A minimum of three signals are needed:

1. A clock signal, to indicate when a bit is being transferred (SCLK; serial clock)

2. Output to slave device(s) (MOSI; master out, slave in)

3. Input from slave device(s) (MISO; master in, slave out)

The Game Boy exposes these signals on its link cable port.

{%
   include image.html
   src="/images/dmg_link_port_pinout.png"
   caption="Game Boy link port pinout. The link cable swaps the wires for &quot;out&quot; and &quot;in&quot; on each end."
%}

In SPI configurations with multiple devices, there are also slave select (SS)
signals to indicate which device the master is communicating with at a given
time. However that's not the case with the Game Boy -- only 2 devices can ever
be directly involved in a transfer. This is true even when using the
[4-player adapter](https://shonumi.github.io/articles/art9.html), which
communicates separately with each connected Game Boy to get around the hardware
limitation.

With SPI, when the master device generates a clock pulse, both the master and
slave send a bit on their output lines and receive a bit on their input lines.
In other words, the protocol is bidirectional and synchronous. On the Game Boy,
games store the next byte to send in an on-board shift register. When a transfer
is initiated or a clock pulse is received, the next bit to send is shifted out
of the register and the received bit is simultaneously shifted in. After 8 of
these 1-bit transfers have occurred, an interrupt is generated on each Game Boy
to signal completion and that it's safe for the game to read the value.

GB SPI holds the clock line high when idle and indicates a transfer by pulsing
it low. Data is shifted out (most significant bit first) on each falling clock
edge and sampled on each rising edge. This configuration is known as SPI mode 3.

{%
   include image.html
   src="/images/gb_spi.png"
   caption="An example GB SPI transfer. Here, the master sends 0xD9 (217) and the slave sends 0x45 (69)."
%}

### GB SPI and latency

Since the link protocol is synchronous and bits are sent and received
simultaneously, that means the master device requires the slave to send its
response at a rate equal to the clock speed. In non-Game Boy Color mode, the
master Game Boy supplies an 8KHz clock (data transfer speed of 1KB/s). This
means that there is only a ~120&mu;s window to respond! The Game Boy Color can
operate at even higher speeds. No internet connection could possibly satisfy
this latency requirement. However, the slave device has no such constraints.
It just responds when it receives data!

According to the excellent
[Pan Docs](https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#external-clock),
the Game Boy can operate with link cable clock speeds of up to 500KHz (62.5KB/s
transfer speed) and importantly there is no lower bound. By default, the slave
will wait forever until it receives data from the master. In fact, the clock
pulses don't even need to be sent at a consistent speed and there can be large
gaps in between.

Of course, some games may still implement timeouts but this sounds perfect for
our use-case. If we can somehow force both Game Boys to operate in slave mode
then the latency can be theoretically infinite, supporting even the roughest of
connections. As it turns out, this is exactly what stacksmashing did for Tetris
and what another project called [TCPoke](http://pepijndevos.nl/TCPoke/)
did for Pokemon Generation I and II. Bingo! This is a viable approach and has
already been proven to work with another game.

## Next steps

Understanding the link cable protocol and seeing similar projects gave us
confidence that what we want to do is possible. So, we did what anybody else
in this situation would: ordered an obscene amount of hardware! Link cables,
breakout boards, Game Boy games, microcontrollers, flash cartridges, and more.
With all of this in hand, the plan is to get a basic prototype PC to Game Boy
interface working and then build on top of that. Once proven, we'll be able
to design the hardware and software to run it all.

There are still some unknowns that will need to be delved into through
experimentation -- namely, testing compatibility with different games and
finding a game-agnostic way to force both Game Boys to operate in slave mode.
In the worst case, we'll need to write game-specific code. But since we don't
plan on supporting massive game lobbies like stacksmashing (as of now), once
the Game Boys are initialized properly the rest should just be a matter of
forwarding bytes back and forth between them as normal. We could be wrong, but
that's the fun part!

Stay tuned for further updates and more deep dives.
