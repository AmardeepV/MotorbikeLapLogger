#include "Orientation.h"

OrientationSensor::OrientationSensor()
    : _accel(SENSOR_ID_ACC),
      _gyro(SENSOR_ID_GYRO),
      _rotation(SENSOR_ID_RV),
      _orientation(SENSOR_ID_ORI)
{
}

void OrientationSensor::begin()
{
    BHY2.begin();

    _accel.begin(SAMPLE_RATE_HZ, LATENCY_MS);
    _gyro.begin(SAMPLE_RATE_HZ, LATENCY_MS);
    _rotation.begin(SAMPLE_RATE_HZ, LATENCY_MS);
    _orientation.begin(SAMPLE_RATE_HZ, LATENCY_MS);
     BHY2.update();
    _accel.setRange(ACCEL_RANGE_G);
    _gyro.setRange(GYRO_RANGE_DPS);

}

void OrientationSensor::printConfiguration()
{
    
    SensorConfig cfg;

    cfg = _accel.getConfiguration();
    Serial.print("Accelerometer configuration: ");
    Serial.println(String("range of accel: +/-") + cfg.range + String("g"));

    cfg = _gyro.getConfiguration();
    Serial.print("Gyro configuration: ");
    Serial.println(String("range of gyro: +/-") + cfg.range + String("dps"));

    
    cfg = _rotation.getConfiguration();
    Serial.print("Rotation configuration: ");
    Serial.println(String("rotation configuration - rate: ") + cfg.sample_rate + String(" - latency: ") + cfg.latency + String("ms - range: ") + cfg.range);
    

    cfg = _orientation.getConfiguration();
    Serial.print("Orientation configuration: ");
    Serial.println(String("orientation configuration - rate: ") + cfg.sample_rate + String(" - latency: ") + cfg.latency + String("ms - range: ") + cfg.range);
}

bool OrientationSensor::update()
{
    bool newData = false;
    BHY2.update();
    uint32_t now = millis();

    if(_rotation.dataAvailable())
    {
        _orientationData.qx = _rotation.x();
        _orientationData.qy = _rotation.y();
        _orientationData.qz = _rotation.z();
        _orientationData.qw = _rotation.w();

        _rotation.clearDataAvailFlag();

        newData = true;
        _orientationData.timestamp = now;
    }

    if(_orientation.dataAvailable())
    {
        _orientationData.roll = _orientation.roll();
        _orientationData.pitch = _orientation.pitch();
        _orientationData.yaw = _orientation.heading();

        _orientation.clearDataAvailFlag();
        newData = true;
        _orientationData.timestamp = now;
    }

    if(_gyro.dataAvailable())
    {
        _orientationData.gyroX = _gyro.x();
        _orientationData.gyroY = _gyro.y();
        _orientationData.gyroZ = _gyro.z();

        _gyro.clearDataAvailFlag();
        newData = true;
        _orientationData.timestamp = now;
    }

    if(_accel.dataAvailable())
    {
        _orientationData.accelX = _accel.x();
        _orientationData.accelY = _accel.y();
        _orientationData.accelZ = _accel.z();

        _accel.clearDataAvailFlag();
        newData = true;
        _orientationData.timestamp = now;
    }
    return newData;
}

const OrientationSensor::OrientationData& OrientationSensor::getOrientation() const
{
    return _orientationData;
}