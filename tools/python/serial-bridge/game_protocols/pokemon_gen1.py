# Games supported: Pokemon R/G/B/Y and G/S/C when using the time capsule
class PokemonGen1LinkInitializer:
    MASTER_MAGIC = 0x01
    SLAVE_MAGIC = 0x02  

    def __init__(self):
        self.last_byte_received = None

    def data_handler(self, data):
        self.last_byte_received = data

        if data == self.SLAVE_MAGIC:
            return None
        return self.MASTER_MAGIC

def get_link_initializer():
    return PokemonGen1LinkInitializer()
