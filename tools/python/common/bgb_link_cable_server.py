import socket
import struct

# Implements the BGB link cable protocol
# See https://bgb.bircd.org/bgblink.html
class BGBLinkCableServer:
    PACKET_FORMAT = '<4BI'
    PACKET_SIZE_BYTES = 8

    def __init__(self, verbose=False, host='', port=8765):
        self._handlers = {
            1: self._handle_version,
            101: self._handle_joypad_update,
            104: self._handle_sync1,
            105: self._handle_sync2,
            106: self._handle_sync3,
            108: self._handle_status,
            109: self._handle_want_disconnect
        }
        self._last_received_timestamp = 0

        self.verbose = verbose
        self.host = host
        self.port = port

    def run(self, data_handler):
        self._client_data_handler = data_handler

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            # Reduce latency
            server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            server.bind((self.host, self.port))
            server.listen(0)  # One Game Boy to rule them all
            print(f'Listening on {self.host}:{self.port}...')

            connection, client_addr = server.accept()
            print(f'Received connection from {client_addr[0]}:{client_addr[1]}')

            with connection:
                try:
                    # Initial handshake - send protocol version number
                    connection.send(struct.pack(
                        self.PACKET_FORMAT,
                        1,  # Version packet
                        1,  # Major
                        4,  # Minor
                        0,  # Patch
                        0   # Timestamp
                    ))

                    while True:
                        data = connection.recv(self.PACKET_SIZE_BYTES)
                        if not data:
                            print('Connection dropped')
                            break

                        b1, b2, b3, b4, timestamp = struct.unpack(self.PACKET_FORMAT, data)

                        # Cheat, and say we are exactly in sync with the client
                        self._last_received_timestamp = timestamp

                        handler = self._handlers[b1]
                        response = handler(b2, b3, b4)

                        if response:
                            connection.send(response)
                except Exception as e:
                    print('Socket error:', str(e))

    def _handle_version(self, major, minor, patch):
        if self.verbose:
            print(f'Received version packet: {major}.{minor}.{patch}')

        if (major, minor, patch) != (1, 4, 0):
            raise Exception(f'Unsupported protocol version {major}.{minor}.{patch}')

        return self._get_status_packet()

    def _handle_joypad_update(self, _b2, _b3, _b4):
        # Do nothing. This is intended to control an emulator remotely.
        pass

    def _handle_sync1(self, data, _control, _b4):
        # Data received from master
        response = self._client_data_handler(data)
        if response is not None:
            return struct.pack(
                self.PACKET_FORMAT,
                105,        # Slave data packet
                response,   # Data value
                0x81,       # Control value
                0,          # Unused
                self._last_received_timestamp
            )

    def _handle_sync2(self, _data, _control, _b4):
        # Data received from slave. We will only act as slave.
        pass

    def _handle_sync3(self, b2, b3, b4):
        if self.verbose:
            print('Received sync3 packet')

        # Ack/echo
        return struct.pack(
            self.PACKET_FORMAT,
            106,    # Sync3 packet
            b2,
            b3,
            b4,
            self._last_received_timestamp
        )

    def _handle_status(self, b2, _b3, _b4):
        # TODO: stop logic when client is paused
        if self.verbose:
            print('Received status packet:')
            print('\tRunning:', (b2 & 1) == 1)
            print('\tPaused:', (b2 & 2) == 2)
            print('\tSupports reconnect:', (b2 & 4) == 4)

        # The docs say not to respond to status with status,
        # but not doing this causes link instability
        return self._get_status_packet()

    def _handle_want_disconnect(self, _b2, _b3, _b4):
        print('Client has initiated disconnect')

    def _get_status_packet(self):
        return struct.pack(
            self.PACKET_FORMAT,
            108,    # Status packet
            1,      # State=running
            0,      # Unused
            0,      # Unused
            self._last_received_timestamp
        )
