#include <Arduino.h>
#include "Orientation.h"
#include "TelemetryPacket.h"


OrientationSensor orientationSensor;
TelemetryPacket telemetryPacket;

uint32_t lastPrint = 0;
int sampleCount = 0;

void setup()
{
    Serial.begin(115200);
    while(!Serial)
    {
    }
    orientationSensor.begin();
    delay(2000);
    //orientationSensor.printConfiguration();
    Serial.println("Nicla Started");
}

void loop()
{
   uint32_t now = millis();

    if(orientationSensor.update())
    {
        // sampleCount += 1;
        
        if(now - lastPrint >= 1000)
        {
            telemetryPacket.build(orientationSensor.getOrientation());
            const uint8_t* packet = telemetryPacket.getBuffer();

            for (uint8_t i = 0; i < telemetryPacket.getLength(); i++)
            {
                Serial.print(packet[i], HEX);
                Serial.print(' ');
            }
            Serial.println(" ");
        // Serial.println("------------------------------------");
        // Serial.print("Sample count is: ");
        // Serial.println(sampleCount);

        // const auto& data = orientationSensor.getOrientation();

        // Serial.print("Time: ");
        // Serial.println(data.timestamp);
        // Serial.println();
        // Serial.println("Orientation data: ");
        // Serial.println();
        // Serial.print("roll: ");
        // Serial.println(data.roll);
        // Serial.print("pitch: ");
        // Serial.println(data.pitch);
        // Serial.print("yaw: ");
        // Serial.println(data.yaw);
        // Serial.println();
        // Serial.print("Qx: ");
        // Serial.println(data.qx);
        // Serial.print("Qy: ");
        // Serial.println(data.qy);
        // Serial.print("Qz: ");
        // Serial.println(data.qz);
        // Serial.print("Qw: ");
        // Serial.println(data.qw);

        // Serial.println();
        // Serial.println("Accelerometer: ");
        // Serial.println();

        // Serial.print("acclX: ");
        // Serial.println(data.accelX);
        // Serial.print("acclY: ");
        // Serial.println(data.accelY);
        // Serial.print("acclZ: ");
        // Serial.println(data.accelZ);
        
        // sampleCount = 0;
        lastPrint = now;
        }
    }
}