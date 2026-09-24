#include <Arduino.h>
#include "Orientation.h"
#include "TelemetryPacket.h"
#include "NiclaBattery.h"


OrientationSensor orientationSensor;
TelemetryPacket telemetryPacket;

NiclaBattery battery;
unsigned long lastBatteryStatusPrint = 0;

const unsigned long BATTERY_STATUS_INTERVAL_MS = 5000;

void updateBatteryStatus()
{
    const unsigned long currentTime = millis();

    if ((currentTime - lastBatteryStatusPrint) <
        BATTERY_STATUS_INTERVAL_MS)
    {
        return;
    }

    lastBatteryStatusPrint = currentTime;

    battery.update();

    Serial.println();
    Serial.println("----- Battery Status -----");

    battery.printStatus(Serial);

    Serial.println("--------------------------");
}

void setup()
{
    Serial.begin(115200);      // USB debug
    if (!battery.begin())
    {
        Serial.println("Battery initialization failed");
    }
    else
    {
        Serial.println("Battery initialized");
    }
    orientationSensor.begin();
    Serial.println("Nicla started");
}

void loop()
{
    updateBatteryStatus();
   if (orientationSensor.update())
{
    telemetryPacket.build(orientationSensor.getOrientation());
    
    const auto& orientation = orientationSensor.getOrientation();

    Serial.print("timestamp: ");
    Serial.print(orientation.timestamp, 2);

    Serial.print(" | Roll: ");
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
    Serial.println(orientation.gyroZ, 2);
    delay (500);
}
}