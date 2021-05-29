import serial

# Enables link cable communication with a Game Boy over serial. Requires a
# serial <-> Game Boy adapter which will wait for a byte to be written by
# the host (PC), send it to the GB, and send byte receievd from the GB back.
#
# An Arduino-based implementation of such an adapter can be found at
# gbplay/arduino/gb_to_serial.
class SerialLinkCableServer:
    def __init__(self, serial_port, baudrate=28800):
        self._serial_config = {
            'port': serial_port,
            'baudrate': baudrate,
            'stopbits': serial.STOPBITS_ONE,
            'parity': serial.PARITY_NONE,
        }

    def run(self, data_handler):
        self._client_data_handler = data_handler

        with serial.Serial(**self._serial_config) as link:
            # Wait for boot
            link.read()
            print('Connected')

            response = None
            while True:
                to_send = self._client_data_handler(response)
                link.write(bytearray([to_send]))
                response = link.read()[0]
