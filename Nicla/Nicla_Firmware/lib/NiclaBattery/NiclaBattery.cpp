#include "NiclaBattery.h"

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
     * No external NTC wire is connected.
     */
    nicla::setBatteryNTCEnabled(false);

    /*
     * Enable charging at 80 mA.
     *
     * This matches the standard charging current
     * specified for the 400 mAh battery.
     */
    if (!nicla::enableCharging(BATTERY_CHARGING_CURRENT_MA))
    {
        return false;
    }

    /*
     * Initialize the Nicla onboard LEDs.
     */
    nicla::leds.begin();

    /*
     * Start with the LEDs turned off.
     */
    nicla::leds.setColor(0, 0, 0);

    _lastUpdate = 0;

    /*
     * Force the first battery reading immediately.
     */
    _lastUpdate = millis() - BATTERY_UPDATE_INTERVAL_MS;

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
     * Read the battery voltage.
     */
    _batteryVoltage = nicla::getCurrentBatteryVoltage();

    /*
     * Determine whether a battery is connected.
     */
    _batteryConnected = (_batteryVoltage > 0.0f);

    if (!_batteryConnected)
    {
        _batteryLevel = 0;
        _operatingStatus = OperatingStatus::NotConnected;

        // LED off
        nicla::leds.setColor(0, 0, 0);

        return;
    }

    /*
     * Calculate an approximate battery percentage
     * from the measured voltage.
     *
     * This is not a coulomb-counter measurement.
     */
    float percentage =
        ((_batteryVoltage - BATTERY_MIN_VOLTAGE) /
         (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) *
        100.0f;

    percentage = constrain(percentage, 0.0f, 100.0f);

    _batteryLevel = static_cast<int>(percentage);

    /*
     * Read the actual operating status from the
     * Nicla power-management IC.
     */
    auto niclaOperatingStatus = nicla::getOperatingStatus();

    switch (niclaOperatingStatus)
    {
        case ::OperatingStatus::Charging:
        {
            _operatingStatus = NiclaBattery::OperatingStatus::Charging;

            // Yellow
            nicla::leds.setColor(255, 100, 0);

            break;
        }

        case ::OperatingStatus::ChargingComplete:
        {
            _operatingStatus = NiclaBattery::OperatingStatus::Full;

            // Green
            nicla::leds.setColor(0, 255, 0);

            /*
            * Stop charging after the power IC reports
            * that charging is complete.
            */
            nicla::disableCharging();

            break;
        }

        case ::OperatingStatus::Error:
        {
            _operatingStatus = NiclaBattery::OperatingStatus::Fault;

            // Red
            nicla::leds.setColor(255, 0, 0);

            break;
        }

        case ::OperatingStatus::Ready:
        {
            _operatingStatus = NiclaBattery::OperatingStatus::Discharging;

            // Blue
            nicla::leds.setColor(0, 0, 255);

            break;
        }

        default:
        {
            _operatingStatus = NiclaBattery::OperatingStatus::Unknown;

            // LED off
            nicla::leds.setColor(0, 0, 0);

            break;
        }
    }
}

bool NiclaBattery::isBatteryConnected() const
{
    return _batteryConnected;
}

float NiclaBattery::getCurrentBatteryVoltage() const
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

float NiclaBattery::getEstimatedChargingTimeHours() const
{
    /*
     * Charging time =
     * Battery capacity / (0.8 * charging current)
     */
    return static_cast<float>(BATTERY_CAPACITY_MAH) /
           (0.8f * BATTERY_CHARGING_CURRENT_MA);
}

float NiclaBattery::getEstimatedRemainingChargingTimeHours() const
{
    if (!_batteryConnected || _batteryLevel >= 100)
    {
        return 0.0f;
    }

    const float remainingCapacity =
        static_cast<float>(BATTERY_CAPACITY_MAH) *
        (100.0f - static_cast<float>(_batteryLevel)) /
        100.0f;

    return remainingCapacity /
           (0.8f * BATTERY_CHARGING_CURRENT_MA);
}

void NiclaBattery::printStatus(Stream& output)
{
    output.println("----- Battery Status -----");

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

    output.print("Estimated full charging time: ");
    output.print(getEstimatedChargingTimeHours(), 2);
    output.println(" hours");

    output.print("Estimated remaining charging time: ");
    output.print(getEstimatedRemainingChargingTimeHours(), 2);
    output.println(" hours");
}