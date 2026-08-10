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
}
}