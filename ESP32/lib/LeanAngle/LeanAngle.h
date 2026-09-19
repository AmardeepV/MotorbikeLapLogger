#pragma once
#include<Arduino.h>

class LeanAngle
{
    public:
        float calculateLean(float qw,float qx,float qy, float qz);
};
