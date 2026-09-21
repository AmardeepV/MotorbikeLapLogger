#pragma once
#include<Arduino.h>
#include <FS.h>
#include <SD.h>


class SDLogger
{
    public:
        bool begin();
        bool startLogging(uint32_t sessionNumber);
        bool writePacket(const uint8_t* packet,
                     uint8_t length);
        bool writeMetadata(const uint8_t* data, size_t length);

        void stopLogging();

        bool isLogging() const;
        bool reinitialize();

    private:
        File _file;
        bool _isLogging{false};
};