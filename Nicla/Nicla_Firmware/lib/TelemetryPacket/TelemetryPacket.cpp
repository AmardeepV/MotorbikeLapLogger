#include"TelemetryPacket.h"

TelemetryPacket::TelemetryPacket()
{
}
void TelemetryPacket::build(const OrientationSensor::OrientationData& data)
{
    _writeIndex = 0;
    append(SOF1); // start of payload1
    append(SOF2); // start of payload2
    append(VERSION); // version
    append(uint8_t{0}); // length of payload

    //------------------------- Payload started -----------------------------------
    // Get the timestamp
    append(data.timestamp);

    append(data.qw);
    append(data.qx);
    append(data.qy);
    append(data.qz);

    append(data.accelX);
    append(data.accelY);
    append(data.accelZ);

    append(data.gyroX);
    append(data.gyroY);
    append(data.gyroZ);


    _length = _writeIndex + sizeof(uint16_t);
    _buffer[LENGTH_INDEX] = _length;

    uint16_t crc = calculateCRC();
    append(crc);

}

uint16_t TelemetryPacket::calculateCRC()
{
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < _writeIndex; i++)
    {
        uint8_t currentByte = _buffer[i];
        crc ^= (uint16_t)currentByte << 8;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000)
            {
                crc <<= 1;
                crc ^= CRC_POLYNOMIAL;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

uint8_t TelemetryPacket::getLength() const
{
    return _length;
}
const uint8_t* TelemetryPacket::getBuffer() const
{
    return _buffer;
}
