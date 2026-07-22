#pragma once

#include "Arduino.h"
#include "Arduino_BHY2.h"

class OrientationSensor
{
    public:
        struct OrientationData
        {
            uint32_t timestamp;

            // Orientation
            float roll;
            float pitch;
            float yaw;

            float qw;
            float qx;
            float qy;
            float qz;

            // Raw IMU
            float accelX;
            float accelY;
            float accelZ;

            float gyroX;
            float gyroY;
            float gyroZ;
        };

        OrientationSensor();

        void begin();
        bool update();
        const OrientationData& getOrientation() const;
        void printConfiguration();

    private:
        OrientationData _orientationData{}; //Value-initialize this structure, Every numeric field becomes zero

        SensorXYZ _accel;
        SensorXYZ _gyro;
        SensorQuaternion _rotation;
        SensorOrientation _orientation;

        static constexpr float SAMPLE_RATE_HZ = 100.0f;
        static constexpr uint32_t LATENCY_MS = 0;

        static constexpr uint16_t ACCEL_RANGE_G = 4;
        static constexpr uint16_t GYRO_RANGE_DPS = 1000;


};