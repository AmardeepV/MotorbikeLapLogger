#include<Arduino.h>
#include "PacketParser.h"
#include "SDLogger.h"
#include "LapManager.h"
#include "Button.h"

PacketParser parser;
SDLogger logger;
LapManager lapManager;
Button button(
    14,
    30,      // debounce
    3000);  

uint32_t sessionNumber = 1;

void setup()
{
    Serial.begin(115200);
    button.begin();

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

            break;

        case LapManager::Event::LoggingStopped:

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
