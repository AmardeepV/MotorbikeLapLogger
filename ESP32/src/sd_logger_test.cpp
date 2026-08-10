#include <Arduino.h>
#include "SDLogger.h"
#include "PacketParser.h"

SDLogger logger;
PacketParser parser;

uint32_t loggingStartTime = 0;
bool testFinished = false;

constexpr uint32_t TEST_DURATION_MS = 10000;
constexpr uint32_t TEST_SESSION = 1;

void setup()
{
    Serial.begin(115200);

    Serial2.begin(
        115200,
        SERIAL_8N1,
        16,     // RX
        17      // TX
    );

    if (!logger.begin())
    {
        Serial.println("Logger initialization failed");
        return;
    }

    if (!logger.startLogging(TEST_SESSION))
    {
        Serial.println("Could not start logging");
        return;
    }

    loggingStartTime = millis();

    Serial.println("Logger started");
}

void loop()
{
    while (Serial2.available())
    {
        uint8_t byte = Serial2.read();

        if (parser.processByte(byte))
        {
            Serial.println("Packet received");

            if (!logger.writePacket(
                    parser.getPacket(),
                    parser.getPacketLength()))
            {
                Serial.println("Failed to write packet");
            }
        }
    }

    if (!testFinished &&
        millis() - loggingStartTime >= TEST_DURATION_MS)
    {
        logger.stopLogging();

        testFinished = true;

        Serial.println("Test finished");
    }
}