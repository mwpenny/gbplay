import struct

# Offset from 'A' (0x41). This conversion isn't perfect, but letters match
# See https://bulbapedia.bulbagarden.net/wiki/Character_encoding_(Generation_I)
POKE_TEXT_OFS = 0x3F
POKE_TEXT_TERMINATOR = 0x50
POKE_TEXT_MAX_LEN = 10
POKE_LIST_TERMINATOR = 0xFF

MAX_PARTY_POKEMON = 6
PREAMBLE_VALUE = 0xFD

def text_to_pokestr(s):
    if len(s) > POKE_TEXT_MAX_LEN:
        raise Exception(f'Pokestrs have a maximum length of {POKE_TEXT_MAX_LEN}')

    ps = [0] * (POKE_TEXT_MAX_LEN + 1)
    for i, c in enumerate(s):
        ps[i] = ord(c) + POKE_TEXT_OFS
    ps[len(s)] = POKE_TEXT_TERMINATOR

    return bytes(ps)

class Pokemon:
    SERIALIZED_LEN = 44

    def __init__(self, id, name):
        self.id = id
        self.hp = 0
        self.level_pc = 0
        self.status = 0
        self.type1 = 0
        self.type2 = 0
        self.catch_rate = 0
        self.move1 = 0
        self.move2 = 0
        self.move3 = 0
        self.move4 = 0
        self.trainer_id = 0
        self.xp = 0
        self.hp_ev = 0
        self.atk_ev = 0
        self.def_ev = 0
        self.spd_ev = 0
        self.spc_ev = 0
        self.iv = 0
        self.move1_pp = 0
        self.move2_pp = 0
        self.move3_pp = 0
        self.move4_pp = 0
        self.level = 0
        self.max_hp = 0
        self.atk = 0
        self.defence = 0
        self.spd = 0
        self.spc = 0

        self.name = name

    def serialize(self):
        # See https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9mon_data_structure_in_Generation_I
        return struct.pack(
            "<BH9BH3B6H5B5H",
            self.id, self.hp, self.level_pc, self.status, self.type1, self.type2,
            self.catch_rate, self.move1, self.move2, self.move3, self.move4,
            self.trainer_id, self.xp & 0xFF, (self.xp >> 8) & 0xFF,
            (self.xp >> 16) & 0xFF, self.hp_ev, self.atk_ev, self.def_ev,
            self.spd_ev, self.spc_ev, self.iv, self.move1_pp, self.move2_pp,
            self.move3_pp, self.move4_pp, self.level, self.max_hp, self.atk,
            self.defence, self.spd, self.spc
        )

class Trainer:
    def __init__(self, name):
        self.party_pokemon = []
        self.name = name

    def add_party_pokemon(self, pokemon):
        if len(self.party_pokemon) > MAX_PARTY_POKEMON:
            raise Exception('Too many party pokemon')
        self.party_pokemon.append(pokemon)

    def serialize(self):
        serialized = bytes()

        # See https://github.com/pret/pokered/blob/82f31b05c12c803d78f9b99b078198ed24cccdb1/wram.asm#L2197
        serialized += bytes([PREAMBLE_VALUE] * 7)
        serialized += text_to_pokestr(self.name)
        serialized += bytes([len(self.party_pokemon)])
        serialized += bytes(
            [mon.id for mon in self.party_pokemon] + \
            [POKE_LIST_TERMINATOR] * (MAX_PARTY_POKEMON - len(self.party_pokemon) + 1)
        )
        serialized += b''.join(mon.serialize() for mon in self.party_pokemon) + \
            (b'\0' * Pokemon.SERIALIZED_LEN) * (MAX_PARTY_POKEMON - len(self.party_pokemon))
        serialized += b''.join(text_to_pokestr(self.name) for _ in self.party_pokemon) + \
            (text_to_pokestr('') * (MAX_PARTY_POKEMON - len(self.party_pokemon)))
        serialized += b''.join(text_to_pokestr(mon.name) for mon in self.party_pokemon) + \
            (text_to_pokestr('') * (MAX_PARTY_POKEMON - len(self.party_pokemon)))

        return serialized
