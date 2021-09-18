#!/usr/bin/python3
import argparse
import os
import sys

# Ugliness to do relative imports without a headache
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from common.bgb_link_cable_server import BGBLinkCableServer
from common.serial_link_cable import SerialLinkCableServer
from pokemon_data_structures import Trainer, Pokemon

class TradeState:
    NOT_CONNECTED = 0x00
    WAITING_FOR_LINK_TYPE = 0x01
    SELECTED_TRADE = 0x02
    WAITING_FOR_RANDOM_SEED = 0x04
    SENDING_RANDOM_SEED = 0x08
    SENDING_TRAINER_DATA = 0x10
    WAITING_FOR_TRADE = 0x20
    TRADE_INITIATED = 0x40
    TRADE_CONFIRMATION = 0x80
    TRADE_CANCELLED = 0x100

class PokeTrader:
    # Control bytes
    MASTER_MAGIC = 0x01
    SLAVE_MAGIC = 0x02
    CONNECTED_MAGIC = 0x60
    SELECT_TRADE_MAGIC = 0xD4
    SELECT_BATTLE_MAGIC = 0xD5
    SELECT_CANCEL_MAGIC = 0xD6
    TERMINATOR_MAGIC = 0xFD
    TRADE_MENU_CLOSED_MAGIC = 0x6F
    FIRST_POKEMON_MAGIC = 0x60
    LAST_POKEMON_MAGIC = 0x65
    TRADE_CANCELLED_MAGIC = 0x61
    TRADE_CONFIRMED_MAGIC = 0x62

    def __init__(self, server, trainer_data, is_master=False):
        self._trade_state = TradeState.NOT_CONNECTED
        self._transfer_counter = 0
        self._server = server
        self._serialized_trainer_data = trainer_data.serialize()
        self._is_master = is_master

    def run(self):
        self._server.run(self.on_client_data)

    # See http://www.adanscotney.com/2014/01/spoofing-pokemon-trades-with-stellaris.html
    def on_client_data(self, data):
        to_send = data or 0

        if self._trade_state == TradeState.NOT_CONNECTED:
            if data == self.CONNECTED_MAGIC:
                self._trade_state = TradeState.WAITING_FOR_LINK_TYPE
                to_send = self.CONNECTED_MAGIC
                print('Pokemon link initiated')
            elif self._is_master:
                to_send = self.MASTER_MAGIC
            elif data == self.MASTER_MAGIC:
                to_send = self.SLAVE_MAGIC

        elif self._trade_state == TradeState.WAITING_FOR_LINK_TYPE:
            if data == self.CONNECTED_MAGIC:
                to_send = self.CONNECTED_MAGIC
            elif data == self.SELECT_TRADE_MAGIC:
                self._trade_state = TradeState.SELECTED_TRADE
                print('Selected trade center')
            elif data == self.SELECT_BATTLE_MAGIC:
                raise Exception('Battles are not supported')
            elif data == self.SELECT_CANCEL_MAGIC or data == self.MASTER_MAGIC:
                raise Exception('Link cancelled by client')

        elif self._trade_state == TradeState.SELECTED_TRADE:
            if data == self.TERMINATOR_MAGIC:
                self._trade_state = TradeState.WAITING_FOR_RANDOM_SEED
                print('Waiting for random seed')

        elif self._trade_state == TradeState.WAITING_FOR_RANDOM_SEED:
            if data != self.TERMINATOR_MAGIC:
                self._trade_state = TradeState.SENDING_RANDOM_SEED
                print('Sending random seed')

        elif self._trade_state == TradeState.SENDING_RANDOM_SEED:
            if data == self.TERMINATOR_MAGIC:
                self._trade_state = TradeState.SENDING_TRAINER_DATA
                self._transfer_counter = 0
                print('Sending trainer data')

        elif self._trade_state == TradeState.SENDING_TRAINER_DATA:
            if self._transfer_counter < len(self._serialized_trainer_data):
                to_send = self._serialized_trainer_data[self._transfer_counter]
                self._transfer_counter += 1
            else:
                self._trade_state = TradeState.WAITING_FOR_TRADE
                self._transfer_counter = 0
                print('Waiting for trade')

        elif self._trade_state == TradeState.WAITING_FOR_TRADE:
            if data == self.TRADE_MENU_CLOSED_MAGIC:
                self._trade_state = TradeState.SELECTED_TRADE
                print('Trade menu closed')
            elif self.FIRST_POKEMON_MAGIC <= data <= self.LAST_POKEMON_MAGIC:
                self._trade_state = TradeState.TRADE_INITIATED
                print('Trade initiated')

        elif self._trade_state == TradeState.TRADE_INITIATED:
            if data != 0:
                # Always trade the first
                to_send = self.FIRST_POKEMON_MAGIC
            else:
                self._trade_state = TradeState.TRADE_CONFIRMATION
                print('Waiting for trade confirmation')

        elif self._trade_state == TradeState.TRADE_CONFIRMATION:
            if data == self.TRADE_CANCELLED_MAGIC:
                self._trade_state = TradeState.TRADE_CANCELLED
                print('Trade cancelled')
            elif data == self.TRADE_CONFIRMED_MAGIC:
                self._trade_state = TradeState.SELECTED_TRADE
                print('Trade confirmed')

        elif self._trade_state == TradeState.TRADE_CANCELLED:
            if data == 0:
                self._trade_state = TradeState.WAITING_FOR_TRADE

        return to_send


arg_parser = argparse.ArgumentParser(description='Mocks a Pokemon trade.')
arg_subparsers = arg_parser.add_subparsers(dest='connection_type', required=True, help='Types of connections')

bgb_parser = arg_subparsers.add_parser('bgb', help='Connect to BGB emulator')
bgb_parser.add_argument('--port', type=int, help='port to listen on for BGB data')

serial_parser = arg_subparsers.add_parser('serial', help='Connect to Game Boy over serial')
serial_parser.add_argument('port', type=str, help='serial port to connect to')

args = arg_parser.parse_args()
is_master = False

if args.connection_type == 'bgb':
    kwargs = { 'port': args.port } if args.port is not None else {}
    server = BGBLinkCableServer(**kwargs)
elif args.connection_type == 'serial':
    server = SerialLinkCableServer(args.port)
    is_master = True
else:
    raise Exception(f'Unknown connection type:', args.connection_type)

trainer_data = Trainer('Matt')
trainer_data.add_party_pokemon(Pokemon(0x15, 'MEW'))
PokeTrader(server, trainer_data, is_master).run()
