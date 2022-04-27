# GBPlay tools

This directory contains scripts for development and debugging.

## Requirements

* [Python 3](https://www.python.org/downloads/)
* [BGB](https://bgb.bircd.org/) Game Boy emulator (for relevant tools/modes)
* Run `pip install -r requirements.txt`

Tools which communicate with a real Game Boy over serial require a serial to
Game Boy adapter which will wait for a byte to be written by the host (PC), send
it to the GB, and send the byte receieved from the GB back.

An Arduino-based implementation of such an adapter can be found
[in this repository](../arduino/gb_to_serial).

## Directory index

| Directory             | Description                                       |
| -----------           | ------------------------------------------------- |
| `bgb-serial-link/`    | Provides BGB <-> serial link cable communication  |
| `common/`             | Code shared by multiple tools                     |
| `pokered-mock-trade/` | Sends fake Pokemon trade data to a GB or emulator |
| `tcp-serial-bridge/`  | Links GBs and/or emulators in slave mode via TCP  |

All tools support the `--help` argument.
