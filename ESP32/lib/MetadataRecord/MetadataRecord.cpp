#include"MetadataRecord.h"
#include "CRC.h"


void MetadataRecord::build(Type type,
               uint32_t timestamp,
               uint16_t session,
               uint16_t lap)
    {
        _record.Magic      = MAGIC;
        _record.type       = static_cast<uint8_t>(type);
        _record.version    = VERSION;
        _record.timestamp  = timestamp;
        _record.session    = session;
        _record.lap        = lap;
        _record.reserved   = 0;
        _record.crc        = CRC::calculateCRC(reinterpret_cast<const uint8_t*>(&_record), sizeof(Record) - sizeof(_record.crc));
    }

const uint8_t* MetadataRecord::data() const
{
    return reinterpret_cast<const uint8_t*>(&_record);
}

size_t MetadataRecord::size() const
{
    return sizeof(_record);
}