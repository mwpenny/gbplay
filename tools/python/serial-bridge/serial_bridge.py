#!/usr/bin/python3
import argparse
import os
import sys

# Ugliness to do relative imports without a headache
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from common.serial_link_cable import SerialLinkCableClient
from game_protocols import pokemon_gen1

def enter_slave_mode(gb_link):
    # TODO: other games
    link_initializer = pokemon_gen1.get_link_initializer()

    # Initiate link cable connection such that game will use external clock
    response = None
    while True:
        to_send = link_initializer.data_handler(response)
        if to_send is None:
            # Initialized
            return link_initializer.last_byte_received

        response = gb_link.send(to_send)
        if args.trace:
            print(f'{to_send:02X},{response:02X}')


arg_parser = argparse.ArgumentParser(description='Links 2 Game Boys via serial.')
arg_parser.add_argument('--trace', default=False, action='store_true', help='enable communication logging')
arg_parser.add_argument('gb1_port', type=str, help='serial port of the first Game Boy')
arg_parser.add_argument('gb2_port', type=str, help='serial port of the second Game Boy')

args = arg_parser.parse_args()

with SerialLinkCableClient(args.gb1_port) as gb1_link:
    with SerialLinkCableClient(args.gb2_port) as gb2_link:
        gb1_byte = enter_slave_mode(gb1_link)
        print('Game Boy 1 entered slave mode')

        enter_slave_mode(gb2_link)
        print('Game Boy 2 entered slave mode')

        # Start the ping-ponging a la Newton's cradle
        while True:
            gb2_byte = gb2_link.send(gb1_byte)

            if args.trace:
                print(f'{gb1_byte:02X},{gb2_byte:02X}')

            gb1_byte = gb1_link.send(gb2_byte)
