#include<Arduino.h>
#include "PacketParser.h"
#include "SDLogger.h"
#include "LapManager.h"
#include "Button.h"
#include "MetadataRecord.h"
#include "LeanAngle.h"
#include "BLEManager.h"

PacketParser parser;
SDLogger logger;
LapManager lapManager;
MetadataRecord metadata;
LeanAngle lean;    
BLEManager ble;
Button button(
    14,
    30,      // debounce
    3000);  

uint32_t sessionNumber = 1;
uint32_t latestTelemetryTimestamp{};

int temp_count = 1;

void setup()
{
    Serial.begin(115200);
    button.begin();
    ble.begin();
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

    // Button 
    Button::Event buttonEvent = button.getEvent();

    if (buttonEvent != Button::Event::None)
    {
        LapManager::Command command = LapManager::Command::None;

        if (buttonEvent == Button::Event::ShortPress)
        {
            command = LapManager::Command::Lap;
        }
        else if (buttonEvent == Button::Event::LongPress)
        {
            command = LapManager::Command::Stop;
        }

        if (command != LapManager::Command::None)
        {
            lapManager.update(command, millis());

            LapManager::Event lapEvent = lapManager.getEvent();

            handleLapEvent(lapEvent);
        }
    }

    // Bluetooth
    BLEManager::Command bleCommand = ble.getCommand();

    if (bleCommand != BLEManager::Command::None)
    {
        if (bleCommand == BLEManager::Command::Lap)
        {
            lapManager.update(
                LapManager::Command::Lap,
                millis()
            );
        }
        else if (bleCommand == BLEManager::Command::Stop)
        {
            lapManager.update(
                LapManager::Command::Stop,
                millis()
            );
        }
        else if (bleCommand == BLEManager::Command::Calibrate)
        {
            lean.startCalibration();
            temp_count = 2;
            Serial.println("Calibration started");
        }

        LapManager::Event lapEvent = lapManager.getEvent();

        handleLapEvent(lapEvent);
    }

    while (Serial2.available())
    {
        uint8_t byte = Serial2.read();

        if (parser.processByte(byte))
        {   
            latestTelemetryTimestamp = parser.getTelemetry().timestamp;

            float rawAngle = lean.calculateRawLean(
                parser.getTelemetry().qw,
                parser.getTelemetry().qx,
                parser.getTelemetry().qy,
                parser.getTelemetry().qz
            );
            if (temp_count == 1)
            {
                Serial.print("Raw angle: ");
                Serial.print(rawAngle);
                Serial.println();
                temp_count = 0;
            }

            lean.updateCalibration(rawAngle);

            float correctedAngle = lean.calculateLean(
                parser.getTelemetry().qw,
                parser.getTelemetry().qx,
                parser.getTelemetry().qy,
                parser.getTelemetry().qz
            );
            if (temp_count == 2 && !lean.isCalibrating())
            {
                Serial.print("Corrected angle: ");
                Serial.println(correctedAngle);
                temp_count = 0;
            }

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
