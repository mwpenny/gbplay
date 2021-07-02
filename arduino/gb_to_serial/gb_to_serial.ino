const int BAUD = 28800;  // Fastest that I found to be stable
const int PIN_CLK = 2;
const int PIN_SO = 3;
const int PIN_SI = 4;

void setup()
{
    Serial.begin(BAUD, SERIAL_8N1);
    pinMode(PIN_CLK, OUTPUT);
    pinMode(PIN_SO, OUTPUT);
    pinMode(PIN_SI, INPUT_PULLUP);

    // Signal to the PC that we are ready
    Serial.write(0);
}

byte transfer_byte(byte tx)
{
    byte rx = 0;

    for (int i = 0; i < 8; ++i)
    {
        digitalWrite(PIN_SO, (tx & 0x80) ? HIGH : LOW);
        tx <<= 1;

        // http://www.devrs.com/gb/files/gblpof.gif
        // 120 us/bit
        digitalWrite(PIN_CLK, LOW);
        delayMicroseconds(60);

        byte rx_bit = (digitalRead(PIN_SI) == HIGH) ? 1 : 0;
        rx = (rx << 1) | rx_bit;

        digitalWrite(PIN_CLK, HIGH);
        delayMicroseconds(60);
    }

    return rx;
}

void loop()
{
    if (!Serial.available())
    {
        return;
    }

    byte tx = Serial.read();
    byte rx = transfer_byte(tx);

    Serial.write(rx);

    // Give the Game Boy "enough" time to prepare the next byte
    // This value is purely anecdotal may need to be adjusted
    //
    // For the final hardware, it should be implemented on the
    // server side (latency will likely be high enough anyway)
    delay(5);
}
