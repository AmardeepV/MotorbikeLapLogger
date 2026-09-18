#pragma once
#include<Arduino.h>
#include <cstring>

class PacketParser
{
    public:
        PacketParser();

        struct TelemetryData
        {
            uint32_t timestamp;

            float qw;
            float qx;
            float qy;
            float qz;

            // Raw IMU
            float accelX;
            float accelY;
            float accelZ;

            float gyroX;
            float gyroY;
            float gyroZ;
        };

        bool processByte(uint8_t byte);
        const TelemetryData& getTelemetry() const;
        const uint8_t* getPacket() const;
        uint8_t getPacketLength() const;
    
    private:

        enum class State
        {
            WaitingForSOF1,
            WaitingForSOF2,
            WaitingForVersion,
            WaitingForLength,
            ReceivingPacket
        };

        State _state{State::WaitingForSOF1};

        static constexpr uint8_t SOF1 = 0xAA;
        static constexpr uint8_t SOF2 = 0x55;
        static constexpr uint8_t VERSION = 0x01;
        static constexpr uint8_t MIN_PACKET_LENGTH = 50;

        uint8_t _buffer[64]{};
        uint8_t _writeIndex{};
        uint8_t _readIndex{};
        uint8_t _expectedLength{};
        uint8_t _packetLength{};

        TelemetryData _telemetry{};

        void reset();

        template<typename T>
        void read(T& value);

};
template<typename T>
void PacketParser::read(T& value)
{
    memcpy(&value, &_buffer[_readIndex], sizeof(value));
    _readIndex += sizeof(value);
};