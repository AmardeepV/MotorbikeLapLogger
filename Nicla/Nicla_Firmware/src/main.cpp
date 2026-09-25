
#include <Arduino.h>
#include <math.h>
#include <float.h>

#include "Orientation.h"
#include "TelemetryPacket.h"
#include "NiclaBattery.h"

OrientationSensor orientationSensor;
TelemetryPacket telemetryPacket;

NiclaBattery battery;
unsigned long lastBatteryStatusPrint = 0;

const unsigned long BATTERY_STATUS_INTERVAL_MS = 5000;

int count = 0;
int total_readings = 100;

// ===============================
// Acceleration Statistics
// ===============================

// Sum of readings (for average)
double sumX = 0.0;
double sumY = 0.0;
double sumZ = 0.0;

// Minimum and maximum values
float minX = FLT_MAX;
float minY = FLT_MAX;
float minZ = FLT_MAX;

float maxX = -FLT_MAX;
float maxY = -FLT_MAX;
float maxZ = -FLT_MAX;

// Welford algorithm variables
double meanX = 0.0;
double meanY = 0.0;
double meanZ = 0.0;

double M2X = 0.0;
double M2Y = 0.0;
double M2Z = 0.0;

// ===============================
// Battery Status
// ===============================

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

    battery.printStatus(Serial);

    Serial.println("--------------------------");
}

// ===============================
// Print Statistics
// ===============================

void printStatistics()
{
    if (count == 0)
    {
        Serial.println("No readings collected.");
        return;
    }

    // Calculate averages
    double averageX = sumX / count;
    double averageY = sumY / count;
    double averageZ = sumZ / count;

    // Calculate population variance and standard deviation
    double varianceX = M2X / count;
    double varianceY = M2Y / count;
    double varianceZ = M2Z / count;

    double stdDevX = sqrt(varianceX);
    double stdDevY = sqrt(varianceY);
    double stdDevZ = sqrt(varianceZ);

    Serial.println();
    Serial.println("================================");
    Serial.println("   ACCELERATION STATISTICS");
    Serial.println("================================");

    Serial.println();
    Serial.println("---------- ACCEL X ----------");

    Serial.print("Average: ");
    Serial.println(averageX, 4);

    Serial.print("Minimum: ");
    Serial.println(minX, 4);

    Serial.print("Maximum: ");
    Serial.println(maxX, 4);

    Serial.print("Std Dev: ");
    Serial.println(stdDevX, 4);

    Serial.println();
    Serial.println("---------- ACCEL Y ----------");

    Serial.print("Average: ");
    Serial.println(averageY, 4);

    Serial.print("Minimum: ");
    Serial.println(minY, 4);

    Serial.print("Maximum: ");
    Serial.println(maxY, 4);

    Serial.print("Std Dev: ");
    Serial.println(stdDevY, 4);

    Serial.println();
    Serial.println("---------- ACCEL Z ----------");

    Serial.print("Average: ");
    Serial.println(averageZ, 4);

    Serial.print("Minimum: ");
    Serial.println(minZ, 4);

    Serial.print("Maximum: ");
    Serial.println(maxZ, 4);

    Serial.print("Std Dev: ");
    Serial.println(stdDevZ, 4);

    Serial.println();
    Serial.print("Total readings: ");
    Serial.println(count);

    Serial.println("================================");
}

// ===============================
// Setup
// ===============================

void setup()
{
    Serial.begin(115200);

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

    while (count < total_readings)
    {
        if (orientationSensor.update())
        {
            telemetryPacket.build(
                orientationSensor.getOrientation()
            );

            const auto& orientation =
                orientationSensor.getOrientation();

            float accelX = orientation.accelX;
            float accelY = orientation.accelY;
            float accelZ = orientation.accelZ;

            // ===============================
            // Print Current Reading
            // ===============================

            Serial.print("timestamp: ");
            Serial.print(orientation.timestamp, 2);

            Serial.print(" | accelX: ");
            Serial.print(accelX, 2);

            Serial.print(" | accelY: ");
            Serial.print(accelY, 2);

            Serial.print(" | accelZ: ");
            Serial.println(accelZ, 2);

            // ===============================
            // Update Sums
            // ===============================

            sumX += accelX;
            sumY += accelY;
            sumZ += accelZ;

            // ===============================
            // Update Min / Max
            // ===============================

            if (accelX < minX)
                minX = accelX;

            if (accelX > maxX)
                maxX = accelX;

            if (accelY < minY)
                minY = accelY;

            if (accelY > maxY)
                maxY = accelY;

            if (accelZ < minZ)
                minZ = accelZ;

            if (accelZ > maxZ)
                maxZ = accelZ;

            // ===============================
            // Welford Standard Deviation
            // ===============================

            int n = count + 1;

            // X-axis
            double deltaX = accelX - meanX;
            meanX += deltaX / n;
            double delta2X = accelX - meanX;
            M2X += deltaX * delta2X;

            // Y-axis
            double deltaY = accelY - meanY;
            meanY += deltaY / n;
            double delta2Y = accelY - meanY;
            M2Y += deltaY * delta2Y;

            // Z-axis
            double deltaZ = accelZ - meanZ;
            meanZ += deltaZ / n;
            double delta2Z = accelZ - meanZ;
            M2Z += deltaZ * delta2Z;

            // Increment reading counter
            count += 1;
        }
    }

    // ===============================
    // Print Final Statistics
    // ===============================

    printStatistics();
}

// ===============================
// Loop
// ===============================

void loop()
{
    // Statistics are collected once in setup().
    // No continuous processing needed here.
}