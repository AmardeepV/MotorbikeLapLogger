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

    float getEstimatedChargingTimeHours() const;
    float getEstimatedRemainingChargingTimeHours() const;

    void printStatus(Stream& output);

private:
    float _batteryVoltage;
    int _batteryLevel;

    bool _batteryConnected;

    OperatingStatus _operatingStatus;

    unsigned long _lastUpdate;

    uint8_t _zeroVoltageReadings;

    // Battery configuration
    static constexpr int BATTERY_CAPACITY_MAH = 400;
    static constexpr int BATTERY_CHARGING_CURRENT_MA = 80;

    // LiPo voltage limits
    static constexpr float BATTERY_MIN_VOLTAGE = 3.20f;
    static constexpr float BATTERY_MAX_VOLTAGE = 4.20f;

    // Battery status update interval
    static constexpr unsigned long BATTERY_UPDATE_INTERVAL_MS = 1000;
};

#endif

