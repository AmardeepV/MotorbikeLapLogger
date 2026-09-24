#include <Arduino.h>
#include "Orientation.h"
#include "TelemetryPacket.h"


OrientationSensor orientationSensor;
TelemetryPacket telemetryPacket;

void setup()
{
    Serial.begin(115200);      // USB debug
    orientationSensor.begin();
    Serial.println("Nicla started");
}

void loop()
{
   if (orientationSensor.update())
{
    telemetryPacket.build(orientationSensor.getOrientation());
    
    const auto& orientation = orientationSensor.getOrientation();

    Serial.print("Roll: ");
    Serial.print(orientation.roll, 2);

    Serial.print(" | Pitch: ");
    Serial.print(orientation.pitch, 2);

    Serial.print(" | Yaw: ");
    Serial.print(orientation.yaw, 2);

    Serial.print(" | qw: ");
    Serial.print(orientation.qw, 2);

    Serial.print(" | qx: ");
    Serial.print(orientation.qx, 2);

    Serial.print(" | qy: ");
    Serial.print(orientation.qy, 2);

    Serial.print(" | qz: ");
    Serial.print(orientation.qz, 2);

    Serial.print(" | accelX: ");
    Serial.print(orientation.accelX, 2);

    Serial.print(" | accelY: ");
    Serial.print(orientation.accelY, 2);

    Serial.print(" | accelZ: ");
    Serial.print(orientation.accelZ, 2);

    Serial.print(" | gyroX: ");
    Serial.print(orientation.gyroX, 2);

    Serial.print(" | gyroY: ");
    Serial.print(orientation.gyroY, 2);

    Serial.print(" | gyroZ: ");
    Serial.print(orientation.gyroZ, 2);


}
}