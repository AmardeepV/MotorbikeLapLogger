#ifndef NICLA_BATTERY_H
#define NICLA_BATTERY_H

#include <Arduino.h>
#include <Nicla_System.h>

class NiclaBattery
{
public:
    enum class OperatingStatus
    {
        Unknown,
        Charging,
        Discharging,
        Full,
        NotConnected,
        Fault
    };

    NiclaBattery();

    bool begin();

    void update();

    bool isBatteryConnected() const;

    float getCurrentBatteryVoltage() const;
    int getBatteryLevel() const;

    OperatingStatus getOperatingStatus() const;

    void printStatus(Stream& output);

private:
    float _batteryVoltage;
    int _batteryLevel;

    bool _batteryConnected;

    OperatingStatus _operatingStatus;

    unsigned long _lastUpdate;
};

#endif