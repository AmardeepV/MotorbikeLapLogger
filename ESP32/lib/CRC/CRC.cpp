#include"CRC.h"

static constexpr uint16_t CRC_POLYNOMIAL = 0x1021;

uint16_t CRC::calculateCRC(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for(size_t i=0; i < length; i++)
    {
        uint8_t currentByte = data[i];
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
