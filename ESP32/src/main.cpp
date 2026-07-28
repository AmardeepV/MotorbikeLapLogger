#include<Arduino.h>
#include "PacketParser.h"

PacketParser parser;
uint8_t packet[] =
{
    0xAA, 0x55, 0x01, 0x32,
    0x04, 0x17, 0x00, 0x00,
    0xD9, 0x5F, 0x69, 0x3E,
    0xD8, 0x1F, 0x6C, 0x3F,
    0xE5, 0xC7, 0x9E, 0x3E,
    0xE9, 0xBF, 0x07, 0x3D,
    0x00, 0x00, 0xF4, 0xC3,
    0x00, 0x50, 0x65, 0x45,
    0x00, 0xC8, 0xE2, 0xC5,
    0x00, 0x00, 0xD8, 0xC1,
    0x00, 0x00, 0x00, 0xC0,
    0x00, 0x00, 0x00, 0x00,
    0xF3, 0xC5
};

void setup()
{
    Serial.begin(115200);
    Serial.println("Unit test starting.....");
    Serial.print("Packet size = ");
    Serial.println(sizeof(packet));
    for (uint8_t byte : packet)
    {
        if (parser.processByte(byte))
        {
            Serial.println("Packet parsed!");

            const auto& data = parser.getTelemetry();
            Serial.print("Timestamp: ");
            Serial.println(data.timestamp);
            Serial.print("AccelX: ");
            Serial.println(data.accelX);
            Serial.print("AccelY: ");
            Serial.println(data.accelY);
            Serial.print("AccelZ: ");
            Serial.println(data.accelZ);
        }
    }
}

void loop()
{
}

// #include <Arduino.h>
// #include "Button.h"
// #include "LapManager.h"
// #include "PacketParser.h"

// Button button(
//     14,
//     30,      // debounce
//     3000);   // long press

// LapManager lapManager;
// PacketParser packetParser;
// uint32_t lastPrint = 0;
// void setup()
// {
//     Serial.begin(115200);
//     button.begin();
//     Serial.println();
//     Serial.println("Motorcycle Lap Logger");
// }

// void loop()
// {
//     uint32_t now = millis();

//     button.update();
//     Button::Event buttonEvent = button.getEvent();

//     lapManager.update(buttonEvent, now);
//     LapManager::Event lapManagerEvent = lapManager.getEvent();

//     switch(lapManagerEvent)
//     {
//         case LapManager::Event::LapStarted:
//             Serial.println("Lap Started");
//             break;

//         case LapManager::Event::LapFinished:
//             Serial.println("Lap Finished");
//             Serial.print("lastlap time: ");
//             Serial.println(lapManager.getLastLapTime());

//             Serial.print("Starting Lap: ");
//             Serial.println(lapManager.getCurrentLap()); 
//             break;

//         case LapManager::Event::LoggingStopped:
//             Serial.println("Lap Finished");
//             Serial.print("lastlap time: ");
//             Serial.println(lapManager.getLastLapTime());

//             break;

//         default:
//             if(now - lastPrint >= 1000)
//             {
//                 if(lapManager.isLogging())
//                 {
//                     Serial.print("Current Lap: ");
//                     Serial.println(lapManager.getCurrentLap()); 
//                     Serial.print("Running time : ");
//                     Serial.println(lapManager.getCurrentLapRunningTime(now));
//                 }
//             lastPrint = now;
//             }
            
//             break;
//     }
// }   
