#pragma once
#include<Arduino.h>

class LeanAngle
{
    public:
        float calculateLean(float qw,float qx,float qy, float qz);
        void startCalibration();
        void updateCalibration(float rawAngle);
        bool isCalibrating() const;

    private:
        bool _isCalibrating{false};
        uint16_t _sampleCount{0};
        float _sampleSum{0.0f};

        float _offset{0.0f};

        static constexpr uint16_t CALIBRATION_SAMPLES = 200;
};
