#include"LeanAngle.h"

float LeanAngle::calculateLean(float qw, float qx, float qy, float qz)
{
    float angle = 2.0f * asin(qx);

    angle = angle * 180.0f / PI;

    return angle;
}