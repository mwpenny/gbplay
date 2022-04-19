import socket
import struct
import threading

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
        self._is_running = False
        self._connection_lock = threading.Lock()

        self.verbose = verbose
        self.host = host
        self.port = port

    def run(self, master_data_handler=None, slave_data_handler=None):
        self._master_data_handler = master_data_handler
        self._slave_data_handler = slave_data_handler

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            # Reduce latency
            server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            server.bind((self.host, self.port))
            server.listen(0)  # One Game Boy to rule them all
            print(f'Listening on {self.host}:{self.port}...')

            connection, client_addr = server.accept()
            print(f'Received connection from {client_addr[0]}:{client_addr[1]}')

            with self._connection_lock:
                self._connection = connection
                self._is_running = True

            with connection:
                try:
                    # Initial handshake - send protocol version number
                    self._send_packet(
                        1,  # Version packet
                        1,  # Major
                        4,  # Minor
                        0   # Patch
                    )

                    while self.is_running():
                        data = connection.recv(self.PACKET_SIZE_BYTES)
                        if not data:
                            print('Connection dropped')
                            break

                        b1, b2, b3, b4, timestamp = struct.unpack(self.PACKET_FORMAT, data)

                        # Cheat, and say we are exactly in sync with the client
                        if timestamp > self._last_received_timestamp:
                            self._last_received_timestamp = timestamp

                        handler = self._handlers[b1]
                        handler(b2, b3, b4)
                except Exception as e:
                    print('Socket error:', str(e))

    def is_running(self):
        with self._connection_lock:
            return self._is_running

    def stop(self):
        with self._connection_lock:
            self._is_running = False

    def send_master_byte(self, data):
        self._send_packet(
            104,   # Master data packet
            data,  # Data value
            0x81   # Control value
        )

    def _handle_version(self, major, minor, patch):
        if self.verbose:
            print(f'Received version packet: {major}.{minor}.{patch}')

        if (major, minor, patch) != (1, 4, 0):
            raise Exception(f'Unsupported protocol version {major}.{minor}.{patch}')

        self._send_status_packet()

    def _handle_joypad_update(self, _b2, _b3, _b4):
        # Do nothing. This is intended to control an emulator remotely.
        pass

    def _handle_sync1(self, data, _control, _b4):
        # Data received from master
        handler = self._master_data_handler

        if handler:
            response = handler(data)
            if response is not None:
                self._send_packet(
                    105,       # Slave data packet
                    response,  # Data value
                    0x80       # Control value
                )
        else:
            # Indicates no response from the GB
            self._send_packet(
                106,  # Sync3 packet
                1
            )

    def _handle_sync2(self, data, _control, _b4):
        # Data received from slave
        handler = self._slave_data_handler

        if handler:
            response = handler(data)
            if response:
                self.send_master_byte(response)

    def _handle_sync3(self, b2, b3, b4):
        if self.verbose:
            print('Received sync3 packet')

        # Ack/echo
        self._send_packet(
            106,  # Sync3 packet
            b2,
            b3,
            b4
        )

    def _handle_status(self, b2, _b3, _b4):
        # TODO: stop logic when client is paused
        if self.verbose:
            print('Received status packet:')
            print('\tRunning:', (b2 & 1) == 1)
            print('\tPaused:', (b2 & 2) == 2)
            print('\tSupports reconnect:', (b2 & 4) == 4)

        # The docs say not to respond to status with status, but not doing this
        # causes link instability. An alternative is to send sync3 packets
        # periodically, but this way is easier.
        self._send_status_packet()

    def _handle_want_disconnect(self, _b2, _b3, _b4):
        print('Client has initiated disconnect')

    def _send_status_packet(self):
        self._send_packet(
            108,  # Status packet
            1     # State=running
        )

    def _send_packet(self, type, b2=0, b3=0, b4=0, i1=None):
        if i1 is None:
            i1 = self._last_received_timestamp

        with self._connection_lock:
            try:
                if not self._is_running:
                    raise Exception('Server is not running')

                self._connection.send(struct.pack(
                    self.PACKET_FORMAT,
                    type, b2, b3, b4, i1
                ))
            except Exception as e:
                print('Socket error:', str(e))
                self._running = False
