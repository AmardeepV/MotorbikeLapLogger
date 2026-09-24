#include"LeanAngle.h"

float LeanAngle::calculateRawLean(
    float qw,
    float qx,
    float qy,
    float qz)
{
    float rawAngle = 2.0f * asin(qx);
    rawAngle = rawAngle * 180.0f / PI;

    return rawAngle;
}

float LeanAngle::calculateLean(
    float qw,
    float qx,
    float qy,
    float qz)
{
    float rawAngle = calculateRawLean(qw, qx, qy, qz);

    return rawAngle - _offset;
}

void LeanAngle::startCalibration()
{
    _isCalibrating = true;
    _sampleCount = 0;
    _sampleSum = 0.0f;
}

void LeanAngle::updateCalibration(float rawAngle)
{
    if (!_isCalibrating)
    {
        return;
    }

    _sampleSum += rawAngle;
    _sampleCount++;

    if (_sampleCount % 50 == 0)
    {
        Serial.print("Calibration samples: ");
        Serial.print(_sampleCount);
        Serial.println("/200");
    }

    if (_sampleCount >= CALIBRATION_SAMPLES)
    {
        _offset = _sampleSum / CALIBRATION_SAMPLES;
        _isCalibrating = false;

        Serial.print("Calibration complete. Offset: ");
        Serial.println(_offset);
    }
}

bool LeanAngle::isCalibrating() const
{
    return _isCalibrating;
}

uint16_t LeanAngle::getCalibrationSampleCount() const
{
    return _sampleCount;
}

uint16_t LeanAngle::getCalibrationSampleTarget() const
{
    return CALIBRATION_SAMPLES;
}

float LeanAngle::getOffset() const
{
    return _offset;
}

void LeanAngle::resetExtremes()
{
    _maximumLeftLean = 0.0f;
    _maximumRightLean = 0.0f;
}

void LeanAngle::updateExtremes(float correctedAngle)
{
    if (correctedAngle < 0.0f)
    {
        _maximumLeftLean = max(_maximumLeftLean, -correctedAngle);
    }
    else
    {
        _maximumRightLean = max(_maximumRightLean, correctedAngle);
    }
}

float LeanAngle::getMaximumLeftLean() const
{
    return _maximumLeftLean;
}

float LeanAngle::getMaximumRightLean() const
{
    return _maximumRightLean;
}
