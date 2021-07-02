import random

class TetrisGameState:
    CONFIRMING_ROLES1 = 0x00
    SELECTING_MUSIC = 0x01
    SELECTING_DIFFICULTY = 0x02
    CONFIRMING_ROLES2 = 0x04
    SENDING_INITIAL_GARBAGE = 0x08
    CONFIRMING_ROLES3 = 0x10
    SENDING_PIECES = 0x20

# Limitations: music and difficulty level are hard-coded
class TetrisLinkInitializer:
    MASTER_MAGIC = 0x29
    SLAVE_MAGIC = 0x55
    MUSIC_A_MAGIC = 0x1C
    WAITING_FOR_MUSIC_MAGIC = 0x39
    CONFIRM_MUSIC_SELECTION_MAGIC = 0x50
    CONFIRM_DIFFICULTY_SELECTION_MAGIC = 0x60
    SOLID_TILE_FLAG = 0x80
    EMPTY_TILE_MAGIC = 0x2F
    GAME_START_MAGIC = [0x30, 0x00, 0x02, 0x02, 0x20]

    def __init__(self):
        self._state = TetrisGameState.CONFIRMING_ROLES1
        self._transfer_counter = 0
        self.last_byte_received = None

        # Same algorithm as the original game. 50/50 chance of an empty space
        # versus a filled one. Filled spaces use 1 of 8 different tiles.
        # See https://github.com/alexsteb/tetris_disassembly/blob/master/main.asm#L4604
        self._initial_garbage = [
            (random.randint(0, 7) | self.SOLID_TILE_FLAG) if random.random() >= 0.5 else self.EMPTY_TILE_MAGIC
            for _ in range(100)
        ]

        # Same algorithm as the original game.
        # See https://harddrop.com/wiki/Tetris_(Game_Boy) and
        # https://github.com/alexsteb/tetris_disassembly/blob/master/main.asm#L1780
        self._pieces = []
        prev_piece1, prev_piece2 = 0, 0
        for _ in range(256):
            next_piece = 0

            # Don't try forever
            for _ in range(3):
                # 7 choices of pieces, each is a multiple of 4 starting from 0
                next_piece = (random.randint(0, 6) * 4)

                # Try to avoid repeats
                if ((next_piece | prev_piece1 | prev_piece2) & 0xFC) != prev_piece2:
                    break

            self._pieces.append(next_piece)
            prev_piece1, prev_piece2 = next_piece, prev_piece1

    def get_send_delay_ms(self):
        if self._state < TetrisGameState.SENDING_INITIAL_GARBAGE:
            return get_default_send_delay_ms()
        return 0

    def data_handler(self, data):
        self.last_byte_received = data

        if self._state == TetrisGameState.CONFIRMING_ROLES1:
            if data == self.SLAVE_MAGIC:
                self._state = TetrisGameState.SELECTING_MUSIC
            else:
                return self.MASTER_MAGIC

        if self._state == TetrisGameState.SELECTING_MUSIC:
            if data == self.WAITING_FOR_MUSIC_MAGIC:
                self._state = TetrisGameState.SELECTING_DIFFICULTY
                return self.CONFIRM_MUSIC_SELECTION_MAGIC
            return self.MUSIC_A_MAGIC

        elif self._state == TetrisGameState.SELECTING_DIFFICULTY:
            # Wait for opponent to send difficulty
            if data == 0:
                self._state  = TetrisGameState.CONFIRMING_ROLES2
                return self.CONFIRM_DIFFICULTY_SELECTION_MAGIC

            # "Master" is not an actual player and so difficulty doesn't matter
            # TODO: use difficulty of other player so the UI is correct
            return 0

        elif self._state == TetrisGameState.CONFIRMING_ROLES2:
            if data == self.SLAVE_MAGIC:
                self._state = TetrisGameState.SENDING_INITIAL_GARBAGE
                self._transfer_counter = 1
                return self._initial_garbage[0]
            else:
                return self.MASTER_MAGIC

        elif self._state == TetrisGameState.SENDING_INITIAL_GARBAGE:
            to_send = self._initial_garbage[self._transfer_counter]
            self._transfer_counter += 1

            if self._transfer_counter == len(self._initial_garbage):
                self._state = TetrisGameState.CONFIRMING_ROLES3
            return to_send

        elif self._state == TetrisGameState.CONFIRMING_ROLES3:
            if data == self.SLAVE_MAGIC:
                self._state = TetrisGameState.SENDING_PIECES
                self._transfer_counter = 1
                return self._pieces[0]
            return self.MASTER_MAGIC

        elif self._state == TetrisGameState.SENDING_PIECES:
            if self._transfer_counter == len(self._pieces):
                # In game
                return None

            to_send = self._pieces[self._transfer_counter]
            self._transfer_counter += 1
            return to_send

def get_link_initializer():
    return TetrisLinkInitializer()

def get_start_sequence():
    return TetrisLinkInitializer.GAME_START_MAGIC

def get_default_send_delay_ms():
    return 30
