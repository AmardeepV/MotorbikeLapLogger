#pragma once
#include<Arduino.h>


class CRC
{   
    public:
        static uint16_t calculateCRC(const uint8_t* data, size_t length);
};
