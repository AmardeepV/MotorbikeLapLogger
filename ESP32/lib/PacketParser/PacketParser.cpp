#include"PacketParser.h"
#include <cstring>

PacketParser::PacketParser()
{
}

const PacketParser::TelemetryData& PacketParser::getTelemetry() const
{
    return _telemetry;
}

bool PacketParser::processByte(uint8_t byte)
{
    switch (_state)
    {
    case State::WaitingForSOF1:
        if(byte == SOF1)
        {
            _buffer[0] = byte;
            _writeIndex ++;
            _state = State::WaitingForSOF2;
        }
        break;

    case State::WaitingForSOF2:
        if(byte == SOF2)
        {
            _buffer[1] = byte;
            _writeIndex ++;
            _state = State::WaitingForVersion;
        }
        else if (byte == SOF1)
        {
            _buffer[0] = SOF1;
            _writeIndex = 1;
            _state = State::WaitingForSOF2;
        }
        else{
            reset();
        }
        break;

    case State::WaitingForVersion:
        if(byte == VERSION)
        {
            _buffer[2] = byte;
            _writeIndex ++;
            _state = State::WaitingForLength;
        }
        else{
            reset();
        }
        break;

    case State::WaitingForLength:
        _expectedLength = byte;
        if(byte < MIN_PACKET_LENGTH || byte > sizeof(_buffer))
        {
            reset();
            break;
        }
        else{
            _buffer[_writeIndex] = byte;
            _writeIndex ++;
            _state = State::ReceivingPacket;
        }
        break;

    case State::ReceivingPacket:
        _buffer[_writeIndex] = byte;
        _writeIndex ++;
        if(_writeIndex == _expectedLength)
        {
            uint16_t calculatedCRC = calculateCRC();
            uint16_t receivedCRC    ;
            memcpy(&receivedCRC,&_buffer[_expectedLength - 2],sizeof(receivedCRC));

            if(calculatedCRC == receivedCRC)
            {
                _readIndex = 4;
                
                read(_telemetry.timestamp);
                read(_telemetry.qw);
                read(_telemetry.qx);
                read(_telemetry.qy);
                read(_telemetry.qz);
                read(_telemetry.accelX);
                read(_telemetry.accelY);
                read(_telemetry.accelZ);
                read(_telemetry.gyroX);
                read(_telemetry.gyroY);
                read(_telemetry.gyroZ);

                _packetLength = _expectedLength;

                reset();
                return true;

            }
            else{
                reset();
            }
        }
        break;
    default:
        break;
    }
    return false;  
}

uint16_t PacketParser::calculateCRC()
{
    uint16_t crc = 0xFFFF;

    for(uint8_t i=0; i < (_expectedLength -2); i++)
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

void PacketParser::reset()
{
    _state = State::WaitingForSOF1;
    _writeIndex = 0;
    _readIndex = 0;
    _expectedLength = 0;
}

const uint8_t* PacketParser::getPacket() const
{
    return _buffer;
}

uint8_t PacketParser::getPacketLength() const
{
    return _packetLength;
}