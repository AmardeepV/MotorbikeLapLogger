#include<Arduino.h>
#include "PacketParser.h"
#include "SDLogger.h"
#include "LapManager.h"
#include "Button.h"
#include "MetadataRecord.h"
#include "LeanAngle.h"
// #include "CRC.h"

PacketParser parser;
SDLogger logger;
LapManager lapManager;
MetadataRecord metadata;
LeanAngle lean;    
Button button(
    14,
    30,      // debounce
    3000);  

uint32_t sessionNumber = 1;
uint32_t latestTelemetryTimestamp{};

// Test data
// uint8_t testData[] = {
//     0xAB, 0xCD, 0x01, 0x01,
//     0xD0, 0x6C, 0x00, 0x00,
//     0x01, 0x00,
//     0x01, 0x00,
//     0x00, 0x00
// };

void setup()
{
    Serial.begin(115200);
    button.begin();
    /*
    float angle = lean.calculateLean(
        0.9858f,
        0.1666f,
        -0.0149f,
        -0.0079f
    );
    Serial.print("Lean angle is: ");
    Serial.println(angle);
    */
    Serial2.begin(
        115200,
        SERIAL_8N1,
        16,     // RX
        17      // TX
    );

    if (!logger.begin())
    {
        Serial.println("SD initialization failed");
        return;
    }

    Serial.println("System ready");


    // uint16_t calculated = CRC::calculateCRC(testData, 14);

    // Serial.print("Calculated CRC: 0x");
    // Serial.println(calculated, HEX);

}
void handleLapEvent(LapManager::Event event)
{
    switch (event)
    {
        case LapManager::Event::LapStarted:

            if (lapManager.getCurrentLap() == 1)
            {
                Serial.print("Session ");
                Serial.print(sessionNumber);
                Serial.println(" started");

                if (!logger.startLogging(sessionNumber))
                {
                    Serial.println("Failed to start logging");
                }
                else
                {
                    metadata.build(
                        MetadataRecord::Type::SessionStart,
                        latestTelemetryTimestamp,
                        sessionNumber,
                        1
                    );

                    if (!logger.writeMetadata(
                            metadata.data(),
                            metadata.size()))
                    {
                        Serial.println("ERROR: SessionStart metadata was not written");
                    }
                }   
            }

            Serial.print("Lap ");
            Serial.print(lapManager.getCurrentLap());
            Serial.println(" started");

            break;

        case LapManager::Event::LapFinished:

            Serial.print("Lap ");
            Serial.print(lapManager.getCurrentLap() - 1);
            Serial.println(" finished");

            Serial.print("Lap ");
            Serial.print(lapManager.getCurrentLap());
            Serial.println(" started");
            metadata.build(
                        MetadataRecord::Type::LapStart,
                        latestTelemetryTimestamp,
                        sessionNumber,
                        lapManager.getCurrentLap()
                    );

            if (!logger.writeMetadata(
                    metadata.data(),
                    metadata.size()))
            {
                Serial.println("ERROR: LapStart metadata was not written");
            }

            break;

        case LapManager::Event::LoggingStopped:

            metadata.build(
                MetadataRecord::Type::SessionEnd,
                latestTelemetryTimestamp,
                sessionNumber,
                lapManager.getCurrentLap()
            );

            if (!logger.writeMetadata(
                    metadata.data(),
                    metadata.size()))
            {
                Serial.println("ERROR: SessionEnd metadata was not written");
            }

            logger.stopLogging();

            Serial.print("Session ");
            Serial.print(sessionNumber);
            Serial.println(" stopped");

            sessionNumber++;

            break;

        case LapManager::Event::None:
            break;
    }
}
void loop()
{

    button.update();

    float angle = lean.calculateLean(parser.getTelemetry().qw,
                                     parser.getTelemetry().qx,
                                     parser.getTelemetry().qy,
                                     parser.getTelemetry().qz);
    Serial.print("Lean angle is: ");
    Serial.println(angle);

    Button::Event buttonEvent = button.getEvent();

    if (buttonEvent != Button::Event::None)
    {
        lapManager.update(buttonEvent, millis());

        LapManager::Event lapEvent = lapManager.getEvent();

        handleLapEvent(lapEvent);
    }
    while (Serial2.available())
    {
        uint8_t byte = Serial2.read();

        if (parser.processByte(byte))
        {   
            latestTelemetryTimestamp = parser.getTelemetry().timestamp;
            if (lapManager.isLogging())
            {
                if (!logger.writePacket(
                        parser.getPacket(),
                        parser.getPacketLength()))
                {
                    Serial.println("ERROR: telemetry packet was not written");
                }
            }
        }
    }
}
