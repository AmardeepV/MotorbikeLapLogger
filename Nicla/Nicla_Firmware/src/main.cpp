#include <Arduino.h>
#include "Orientation.h"
#include "TelemetryPacket.h"


OrientationSensor orientationSensor;
TelemetryPacket telemetryPacket;

void setup()
{
    Serial.begin(115200);      // USB debug
    Serial1.begin(115200);     // UART to ESP32

    orientationSensor.begin();

    Serial.println("Nicla started");
}

void loop()
{
   if (orientationSensor.update())
{
    telemetryPacket.build(orientationSensor.getOrientation());

    Serial1.write(
        telemetryPacket.getBuffer(),
        telemetryPacket.getLength()
    );
    const auto& orientation = orientationSensor.getOrientation();

    Serial.print("Roll: ");
    Serial.print(orientation.roll, 2);

    Serial.print(" | Pitch: ");
    Serial.print(orientation.pitch, 2);

    Serial.print(" | Yaw: ");
    Serial.println(orientation.yaw, 2);
}
}