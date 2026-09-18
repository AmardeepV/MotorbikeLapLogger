#pragma once
#include<Arduino.h>

class MetadataRecord
{   
    public:
        enum class Type : uint8_t
        {
            SessionStart = 0x01,
            LapStart     = 0x02,
            SessionEnd   = 0x03
         };

        static constexpr uint16_t MAGIC = 0xCDAB;
        static constexpr uint8_t VERSION = 0x01;

        struct Record
        {
            uint16_t Magic;
            uint8_t type;
            uint8_t version;
            uint32_t timestamp;
            uint16_t session;
            uint16_t lap;
            uint16_t reserved;
            uint16_t crc;
        };
        static_assert(sizeof(Record)== 16,"MetadataRecord::Record must be exactly 16 bytes");

        void build(Type type,
               uint32_t timestamp,
               uint16_t session,
               uint16_t lap);    
        const uint8_t* data() const;
        size_t size() const;       
            
    private:
        Record _record{};

};