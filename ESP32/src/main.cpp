#include <Arduino.h>
#include "Button.h"
#include "LapManager.h"

Button button(
    14,
    30,      // debounce
    3000);   // long press

LapManager lapManager;
uint32_t lastPrint = 0;
void setup()
{
    Serial.begin(115200);
    button.begin();
    Serial.println();
    Serial.println("Motorcycle Lap Logger");
}

void loop()
{
    uint32_t now = millis();

    button.update();
    Button::Event buttonEvent = button.getEvent();

    lapManager.update(buttonEvent, now);
    LapManager::Event lapManagerEvent = lapManager.getEvent();

    switch(lapManagerEvent)
    {
        case LapManager::Event::LapStarted:
            Serial.println("Lap Started");
            break;

        case LapManager::Event::LapFinished:
            Serial.println("Lap Finished");
            Serial.print("lastlap time: ");
            Serial.println(lapManager.getLastLapTime());

            Serial.print("Starting Lap: ");
            Serial.println(lapManager.getCurrentLap()); 
            break;

        case LapManager::Event::LoggingStopped:
            Serial.println("Lap Finished");
            Serial.print("lastlap time: ");
            Serial.println(lapManager.getLastLapTime());

            break;

        default:
            if(now - lastPrint >= 1000)
            {
                if(lapManager.isLogging())
                {
                    Serial.print("Current Lap: ");
                    Serial.println(lapManager.getCurrentLap()); 
                    Serial.print("Running time : ");
                    Serial.println(lapManager.getCurrentLapRunningTime(now));
                }
            lastPrint = now;
            }
            
            break;
    }
}   