#!/usr/bin/python3
from bgb_link_cable_server import BGBLinkCableServer
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

    def __init__(self, trainer_data):
        self._server = BGBLinkCableServer(self.on_client_data, verbose=False)
        self._trade_state = TradeState.NOT_CONNECTED
        self._transfer_counter = 0
        self._serialized_trainer_data = trainer_data.serialize()

    def run(self):
        self._server.run()

    # See http://www.adanscotney.com/2014/01/spoofing-pokemon-trades-with-stellaris.html
    def on_client_data(self, data):
        to_send = data

        if self._trade_state == TradeState.NOT_CONNECTED:
            # We will always be slave
            if data == self.MASTER_MAGIC:
                to_send = self.SLAVE_MAGIC
            elif data == 0:
                to_send = 0
            elif data == self.CONNECTED_MAGIC:
                self._trade_state = TradeState.WAITING_FOR_LINK_TYPE
                to_send = self.CONNECTED_MAGIC
                print('Pokemon link initiated')

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

        elif self._trade_state == TradeState.TRADE_CONFIRMATION:
            if data == self.TRADE_CANCELLED_MAGIC:
                self._trade_state = TradeState.TRADE_CANCELLED
                print('Trade cancelled')
            elif data != 0:
                self._trade_state = TradeState.NOT_CONNECTED
                print('Trade confirmed')

        elif self._trade_state == TradeState.TRADE_CANCELLED:
            if data == 0:
                self._trade_state = TradeState.WAITING_FOR_TRADE

        return to_send

trainer_data = Trainer('Matt')
trainer_data.add_party_pokemon(Pokemon(0x15, 'MEW'))

PokeTrader(trainer_data).run()
