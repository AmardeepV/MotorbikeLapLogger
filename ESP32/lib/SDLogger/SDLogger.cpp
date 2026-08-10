#include"SDLogger.h"
static constexpr uint8_t SD_CS_PIN = 33;
bool SDLogger::begin()
{
    if (!SD.begin(SD_CS_PIN))
    {
        Serial.println("SD initialization failed");
        return false;
    }

    Serial.println("SD initialized");
    return true;
}

bool SDLogger::writePacket(const uint8_t* packet,
                           uint8_t length)
{
    if (!_isLogging)
    {
        return false;
    }

    size_t written = _file.write(packet, length);

    if (written != length)
    {
        Serial.print("SD write failed: expected ");
        Serial.print(length);
        Serial.print(", wrote ");
        Serial.println(written);

        return false;
    }

    return true;
}
bool SDLogger::isLogging() const
{
    return _isLogging;
}
bool SDLogger::startLogging(uint32_t sessionNumber)
{
    char filename[32];

    snprintf(
        filename,
        sizeof(filename),
        "/SESSION%03lu.BIN",
        static_cast<unsigned long>(sessionNumber)
    );

    Serial.print("Opening: ");
    Serial.println(filename);

    _file = SD.open(filename, FILE_WRITE);

    if (!_file)
    {
        Serial.println("Failed to open log file");
        return false;
    }

    _isLogging = true;

    Serial.print("Logging to: ");
    Serial.println(filename);

    Serial.print("Initial file size: ");
    Serial.println(_file.size());

    return true;
}   

void SDLogger::stopLogging()
{
    if (!_isLogging)
    {
        Serial.println("SDLogger: already stopped");
        return;
    }

    Serial.print("File size before close: ");
    Serial.println(_file.size());

    _file.flush();

    Serial.println("File flushed");

    _file.close();

    Serial.println("File closed");

    _isLogging = false;

    Serial.println("Logging stopped");
}   