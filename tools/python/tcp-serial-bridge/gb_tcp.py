import os
import socket
import sys

# Ugliness to do relative imports without a headache
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from common.serial_link_cable import SerialLinkCableClient
from game_protocols import pokemon_gen1

DEFAULT_SERVER_HOST = 'localhost'
DEFAULT_SERVER_PORT = 1989

# Accepts 2 client connections - one for each Game Boy. Once both have connected,
# the server will put both into slave mode (by sending game-specific data) to
# enable high-latency communication and then act as a bridge between them.
class GBSerialTCPServer:
    def __init__(self, host=DEFAULT_SERVER_HOST, port=DEFAULT_SERVER_PORT, trace=False):
        self._host = host
        self._port = port
        self._trace = trace

    def run(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            # Reduce latency
            server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            server.bind((self._host, self._port))
            server.listen(1)  # Two Game Boys
            print(f'Listening on {self._host}:{self._port}...')

            client1, client1_addr = server.accept()
            print(f'Received connection 1 from {client1_addr[0]}:{client1_addr[1]}')

            try:
                with client1:
                    client2, client2_addr = server.accept()
                    print(f'Received connection 2 from {client2_addr[0]}:{client2_addr[1]}')

                    with client2:
                        gb1_byte = self._enter_slave_mode(client1)
                        print('Game Boy 1 entered slave mode')

                        self._enter_slave_mode(client2)
                        print('Game Boy 2 entered slave mode')

                        # Start the ping-ponging a la Newton's cradle
                        while True:
                            gb2_byte = self._exchange_byte(client2, gb1_byte)

                            if self._trace:
                                print(f'{gb1_byte:02X},{gb2_byte:02X}')

                            gb1_byte = self._exchange_byte(client1, gb2_byte)
            except Exception as e:
                print('Socket error:', str(e))

    def _exchange_byte(self, client, byte):
        client.sendall(bytearray([byte]))
        return client.recv(1)[0]

    def _enter_slave_mode(self, client):
        # TODO: other games
        link_initializer = pokemon_gen1.get_link_initializer()

        # Initiate link cable connection such that game will use external clock
        response = None
        while True:
            to_send = link_initializer.data_handler(response)
            if to_send is None:
                # Initialized
                return link_initializer.last_byte_received

            response = self._exchange_byte(client, to_send)

            if self._trace:
                print(f'{to_send:02X},{response:02X}')


# Connects to a running GBSerialTCPServer and forwards received data to the
# serial-connected Game Boy. The Game Boy's response is sent back over TCP.
class GBSerialTCPClient:
    def __init__(self, serial_port, server_host=DEFAULT_SERVER_HOST, server_port=DEFAULT_SERVER_PORT):
        self._serial_port = serial_port
        self._server_host = server_host
        self._server_port = server_port

    def connect(self):
        with SerialLinkCableClient(self._serial_port) as gb_link:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as tcp_link:
                # Reduce latency
                tcp_link.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

                tcp_link.connect((self._server_host, self._server_port))
                print(f'Connected to {self._server_host}:{self._server_port}...')

                while True:
                    rx = tcp_link.recv(1)
                    if not rx:
                        print('Connection closed')
                        return

                    gb_byte = gb_link.send(rx[0])
                    tcp_link.sendall(bytearray([gb_byte]))