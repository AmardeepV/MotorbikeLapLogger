#pragma once

#include<Arduino.h>
#include"Orientation.h"
#include <cstring>

class TelemetryPacket
{
    public:

        TelemetryPacket();
        void build(const OrientationSensor::OrientationData& data);
        const uint8_t* getBuffer() const;
        uint8_t getLength() const;

    private:
        static constexpr uint8_t MAX_PACKET_SIZE = 52;

        static constexpr uint8_t SOF1 = 0xAA;
        static constexpr uint8_t SOF2 = 0x55;
        static constexpr uint8_t VERSION = 1;
        static constexpr uint16_t CRC_POLYNOMIAL = 0x1021;
        static constexpr uint8_t LENGTH_INDEX = 3;
        static constexpr uint8_t HEADER_SIZE = 4;

        uint8_t _buffer[MAX_PACKET_SIZE]{};
        uint8_t _length{};
        uint8_t _writeIndex{};
        uint16_t calculateCRC();

        template <typename T>
        void append(const T& value);
        
};

template <typename T>
void TelemetryPacket::append(const T& value)
{
    // static_assert(std::is_trivially_copyable_v<T>,
    //           "append() only supports trivially copyable types");
              
    memcpy(&_buffer[_writeIndex], &value, sizeof(value));
    _writeIndex += sizeof(value);
}
