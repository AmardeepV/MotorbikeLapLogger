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
uint32_t lastTelemetryReceivedTime{};
constexpr uint32_t TELEMETRY_TIMEOUT_MS = 2000;
constexpr uint32_t BLE_TELEMETRY_INTERVAL_MS = 100;

int temp_count = 1;
bool previousCalibrationState = false;
bool telemetryConnectionLost = false;
uint32_t lastBleTelemetrySentTime{};



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
                if (latestTelemetryTimestamp == 0)
                {
                    Serial.println("Cannot start logging: no telemetry received");
                    lapManager.reset();
                    return;
                }

                Serial.print("Session ");
                Serial.print(sessionNumber);
                Serial.println(" started");

                if (!logger.startLogging(latestTelemetryTimestamp))
                {
                    Serial.println("Failed to start logging");
                    lapManager.reset();
                    return;
                }
                else
                {
                    lean.resetExtremes();
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
                        logger.stopLogging();
                        lapManager.reset();
                        return;
                    }

                    ble.sendStatus(
                        "SESSION_STARTED," + String(sessionNumber)
                    );
                }   
            }

            Serial.print("Lap ");
            Serial.print(lapManager.getCurrentLap());
            Serial.println(" started");

            ble.sendStatus(
                "LAP_STARTED," + String(lapManager.getCurrentLap())
            );

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

            ble.sendStatus(
                "LAP_STARTED," + String(lapManager.getCurrentLap())
            );

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

            ble.sendStatus(
                "SESSION_STOPPED," + String(sessionNumber)
            );

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
            if (lapManager.isLogging())
            {
                Serial.println(
                    "Calibration rejected: logging is active"
                );
                ble.sendStatus("CALIBRATION_REJECTED_LOGGING");
            }
            else if (lean.isCalibrating())
            {
                Serial.println(
                    "Calibration already in progress"
                );
                ble.sendStatus("CALIBRATION_REJECTED_ALREADY_ACTIVE");
            }
            else
            {
                lean.startCalibration();

                temp_count = 2;

                Serial.println("Calibration started");
                ble.sendStatus("CALIBRATION_STARTED");
            }
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
            lastTelemetryReceivedTime = millis();
            telemetryConnectionLost = false;

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
            bool currentCalibrationState = lean.isCalibrating();

            if (previousCalibrationState && !currentCalibrationState)
            {
                Serial.println("Calibration finished");

                ble.sendStatus("CALIBRATION_COMPLETE");
                lean.resetExtremes();
            }

            previousCalibrationState = currentCalibrationState;

            uint16_t sampleCount =
                lean.getCalibrationSampleCount();

            if (lean.isCalibrating() &&
                sampleCount > 0 &&
                sampleCount % 25 == 0)
            {
                Serial.print("Calibration: ");
                Serial.print(sampleCount);
                Serial.print("/");
                Serial.println(lean.getCalibrationSampleTarget());
            }

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
                lean.updateExtremes(correctedAngle);
            }

            const uint32_t now = millis();
            if (now - lastBleTelemetrySentTime >= BLE_TELEMETRY_INTERVAL_MS)
            {
                ble.sendTelemetry(
                    now,
                    correctedAngle,
                    lean.getMaximumLeftLean(),
                    lean.getMaximumRightLean(),
                    lapManager.isLogging()
                );
                lastBleTelemetrySentTime = now;
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
    if (lapManager.isLogging() &&
    lastTelemetryReceivedTime != 0 &&
    millis() - lastTelemetryReceivedTime > TELEMETRY_TIMEOUT_MS &&
    !telemetryConnectionLost)
            {
                Serial.println("WARNING: Telemetry connection lost");
                telemetryConnectionLost = true;
            }
}
