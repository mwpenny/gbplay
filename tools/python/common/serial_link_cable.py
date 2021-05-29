from io import DEFAULT_BUFFER_SIZE
import serial

# Enables link cable communication with a Game Boy over serial. Requires a
# serial <-> Game Boy adapter which will wait for a byte to be written by
# the host (PC), send it to the GB, and send byte receievd from the GB back.
#
# An Arduino-based implementation of such an adapter can be found at
# gbplay/arduino/gb_to_serial.

DEFAULT_BAUD_RATE = 28800
BASE_SERIAL_CONFIG = {
    'stopbits': serial.STOPBITS_ONE,
    'parity': serial.PARITY_NONE,
}

class SerialLinkCableServer:
    def __init__(self, serial_port, baudrate=DEFAULT_BAUD_RATE):
        self._serial_config = BASE_SERIAL_CONFIG | {
            'port': serial_port,
            'baudrate': baudrate
        }

    def run(self, data_handler):
        self._client_data_handler = data_handler

        with serial.Serial(**self._serial_config) as link:
            # Wait for boot
            link.read()
            print('Serial link connected')

            response = None
            while True:
                to_send = self._client_data_handler(response)
                link.write(bytearray([to_send]))
                response = link.read()[0]


class SerialLinkCableClient:
    def __init__(self, serial_port, baudrate=DEFAULT_BAUD_RATE):
        serial_config = BASE_SERIAL_CONFIG | {
            'port': serial_port,
            'baudrate': baudrate
        }
        self._link = serial.Serial(**serial_config)

        # Wait for boot
        self._link.read()
        print('Serial link connected')

    def __enter__(self):
        return self

    def __exit__(self, _type, _value, _traceback):
        if self._link.isOpen():
            self._link.close()

    def send(self, data):
        self._link.write(bytearray([data]))
        return self._link.read()[0]
