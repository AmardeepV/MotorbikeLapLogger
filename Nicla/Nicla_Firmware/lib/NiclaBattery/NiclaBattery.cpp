#include "NiclaBattery.h"

// Battery specifications
static constexpr int BATTERY_CHARGING_CURRENT_MA = 40;

// LiPo voltage limits
static constexpr float BATTERY_MIN_VOLTAGE = 3.20f;
static constexpr float BATTERY_MAX_VOLTAGE = 4.20f;

// Battery status update interval
static constexpr unsigned long BATTERY_UPDATE_INTERVAL_MS = 1000;

NiclaBattery::NiclaBattery()
    : _batteryVoltage(0.0f),
      _batteryLevel(0),
      _batteryConnected(false),
      _operatingStatus(OperatingStatus::Unknown),
      _lastUpdate(0)
{
}

bool NiclaBattery::begin()
{
    /*
     * Initialize the Nicla power-management system.
     */
    if (!nicla::begin())
    {
        return false;
    }

    /*
     * The battery is connected only to VBAT and GND.
     *
     * No separate NTC wire is connected, so disable
     * the external battery NTC input.
     */
    nicla::setBatteryNTCEnabled(false);

    /*
     * The Nicla Sense ME charger is configured for 40 mA.
     *
     * This is appropriate for the specified 400 mAh battery
     * and is below its stated continuous charging limit.
     */
    if (!nicla::enableCharging(BATTERY_CHARGING_CURRENT_MA))
    {
        return false;
    }

    _lastUpdate = 0;

    update();

    return true;
}

void NiclaBattery::update()
{
    const unsigned long currentTime = millis();

    if ((currentTime - _lastUpdate) < BATTERY_UPDATE_INTERVAL_MS)
    {
        return;
    }

    _lastUpdate = currentTime;

    /*
     * Read battery voltage.
     */
    _batteryVoltage = nicla::getBatteryVoltage();


    /*
     * Determine whether a battery is connected.
     */
    _batteryConnected = (_batteryVoltage > 0.0f);

    /*
     * Calculate battery percentage based on voltage.
     *
     * This is an approximate voltage-based estimate,
     * not a coulomb-counter measurement.
     */
    if (!_batteryConnected)
    {
        _batteryLevel = 0;
        _operatingStatus = OperatingStatus::NotConnected;
        return;
    }

    float percentage =
        ((_batteryVoltage - BATTERY_MIN_VOLTAGE) /
         (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) *
        100.0f;

    percentage = constrain(percentage, 0.0f, 100.0f);

    _batteryLevel = static_cast<int>(percentage);

    /*
     * Determine operating status.
     */

    else if (_batteryLevel >= 95)
    {
        _operatingStatus = OperatingStatus::Full;
    }

}


bool NiclaBattery::isBatteryConnected() const
{
    return _batteryConnected;
}

float NiclaBattery::getBatteryVoltage() const
{
    return _batteryVoltage;
}

int NiclaBattery::getBatteryLevel() const
{
    return _batteryLevel;
}

NiclaBattery::OperatingStatus NiclaBattery::getOperatingStatus() const
{
    return _operatingStatus;
}

void NiclaBattery::printStatus(Stream& output)
{
    output.print("Battery voltage: ");
    output.print(_batteryVoltage, 3);
    output.println(" V");

    output.print("Battery level: ");
    output.print(_batteryLevel);
    output.println(" %");


    output.print("Battery connected: ");
    output.println(_batteryConnected ? "YES" : "NO");

    output.print("Operating status: ");

    switch (_operatingStatus)
    {
        case OperatingStatus::Charging:
            output.println("Charging");
            break;

        case OperatingStatus::Discharging:
            output.println("Discharging");
            break;

        case OperatingStatus::Full:
            output.println("Full");
            break;

        case OperatingStatus::NotConnected:
            output.println("Not connected");
            break;

        case OperatingStatus::Fault:
            output.println("Fault");
            break;

        default:
            output.println("Unknown");
            break;
    }
}