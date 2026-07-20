#pragma once

#include "Arduino.h"
#include "Arduino_BHY2.h"

class OrientationSensor
{
    public:
        struct OrientationData
        {
            uint32_t timestamp;

            float roll;
            float pitch;
            float yaw;

            float qw;
            float qx;
            float qy;
            float qz;
        };

        OrientationSensor();

        bool begin();
        bool update();
        const OrientationData& getOrientation() const;

    private:
        OrientationData _orientation;

};