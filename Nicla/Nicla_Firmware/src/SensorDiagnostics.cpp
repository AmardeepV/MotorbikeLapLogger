#include "SensorDiagnostics.h"
#include <Arduino.h>
#include <Arduino_BHY2.h>
#include <CaptureBuffer.h>
#include <LeanEstimator.h>
#include <math.h>
#include <string.h>

namespace
{
constexpr float RATE_HZ = 100.0f;
constexpr uint16_t ACCEL_RANGE_G = 4, GYRO_RANGE_DPS = 1000;
constexpr uint32_t STARTUP_US = 5000000;
constexpr uint32_t DURATION_US = STARTUP_US + 30000000;
bool quaternionMode = false, leanMode = false, motionMode = false;
uint16_t magRange = 0;
uint32_t motionSequence = 0;
Lean::Estimator lean;
uint32_t previousLeanUs = 0;
float requestedRate(uint8_t id=0) { return id==SENSOR_ID_MAG ? 12.5f : quaternionMode ? 50.0f : RATE_HZ; }
uint32_t captureDuration() { return leanMode ? 120000000UL : quaternionMode ? 60000000UL : DURATION_US; }
DiagnosticCapture::Buffer<256> samples;
enum class State { Waiting, Capturing, Draining, Done, Failed };
State state = State::Waiting;
uint32_t startedUs = 0, malformed = 0;
uint32_t maxPollGapUs = 0, previousPollUs = 0;

// Save each delivered event, including multiple events per BHY2.update().
// Plain SensorXYZ retains only the latest value, losing intermediate events.
class CaptureSensor : public SensorXYZ
{
public:
    explicit CaptureSensor(uint8_t sensorId) : SensorXYZ(sensorId) {}
    using SensorXYZ::setData;
    void setData(SensorDataPacket& packet) override
    {
        if (state != State::Capturing) return;
        const uint32_t elapsed = micros() - startedUs;
        if (elapsed >= captureDuration()) return;
        ++received;
        // Arduino_BHY2 1.0.8 includes the sensor-ID byte in packet.size.
        if (packet.size != 7) { ++malformed; if (leanMode) lean.fault(); return; }
        SensorXYZ::setData(packet);
        if (leanMode && id()!=SENSOR_ID_MAG)
        {
            const float factor = id() == SENSOR_ID_ACC ? 4.0f/32768 : 1000.0f/32768;
            const ImuFrames::Vector3 v = {x()*factor,y()*factor,z()*factor};
            if (id() == SENSOR_ID_ACC) lean.acceleration(v,elapsed);
            else lean.angularVelocity(v,elapsed);
        }
        samples.push({received, elapsed, x(), y(), z(), id(), 0, 0});
    }
    uint32_t received = 0;
};

// Bosch-corrected virtual sensors, not passthrough or uncalibrated IDs.
CaptureSensor accel(SENSOR_ID_ACC), gyro(SENSOR_ID_GYRO), magnetometer(SENSOR_ID_MAG);

class CaptureQuaternion : public SensorQuaternion
{
public:
    CaptureQuaternion() : SensorQuaternion(SENSOR_ID_GAMERV) {}
    using SensorQuaternion::setData;
    void setData(SensorDataPacket& packet) override
    {
        if (state != State::Capturing) return;
        const uint32_t elapsed = micros() - startedUs;
        if (elapsed >= captureDuration()) return;
        ++received;
        if (packet.size != 11) { ++malformed; if (leanMode) lean.fault(); return; }
        if (leanMode) lean.quaternion(packet.getInt16(0)/16384.0f,
            packet.getInt16(2)/16384.0f,packet.getInt16(4)/16384.0f,
            packet.getInt16(6)/16384.0f,elapsed);
        // Keep all four components in original Q14 form (divide by 16384).
        // No renormalization, coordinate remapping or assumption about direction.
        samples.push({received, elapsed, packet.getInt16(0), packet.getInt16(2),
                      packet.getInt16(4), id(), packet.getInt16(6), packet.getUint16(8)});
    }
    uint32_t received = 0;
};
CaptureQuaternion rotation;

bool readConfiguration(SensorClass& sensor, const char* phase, uint16_t expectedRange)
{
    // getConfiguration() in 1.0.8 discards the transport error. Read the same
    // parameter with an explicit status and length check instead.
    uint8_t bytes[12]{};
    uint32_t length = 0;
    int8_t result = BHY2_OK;
    // A failed read is not evidence of a bad configuration. Retry only failed
    // transactions, retaining the error/length for each attempt in the log.
    // A valid response with unexpected settings still fails without retries.
    constexpr uint8_t MAX_READ_ATTEMPTS = 3;
    for (uint8_t attempt = 1; attempt <= MAX_READ_ATTEMPTS; ++attempt)
    {
        memset(bytes, 0, sizeof(bytes));
        length = 0;
        result = sensortec.bhy2_getParameter(
            BHY2_PARAM_SENSOR_CONF_0 + sensor.id(), bytes, sizeof(bytes), &length);
        if (result == BHY2_OK && length == sizeof(bytes)) break;

        Serial.print("# CONFIG_READ_FAILURE,phase="); Serial.print(phase);
        Serial.print(",sensor_id="); Serial.print(sensor.id());
        Serial.print(",attempt="); Serial.print(attempt);
        Serial.print(",status="); Serial.print(static_cast<int>(result));
        Serial.print(",bytes="); Serial.print(length);
        Serial.println(",expected_bytes=12");
        if (attempt < MAX_READ_ATTEMPTS)
        {
            // Service arriving events rather than losing the startup evidence.
            BHY2.update();
            delay(10);
        }
    }
    if (result != BHY2_OK || length != sizeof(bytes))
    {
        Serial.print("# ERROR,configuration_read,"); Serial.println(sensor.id());
        return false;
    }
    const uint32_t rateBits = BHY2_LE2U32(bytes);
    float rate;
    memcpy(&rate, &rateBits, sizeof(rate));
    const uint32_t latency = BHY2_LE2U32(bytes + 4);
    const uint16_t sensitivity = BHY2_LE2U16(bytes + 8);
    const uint16_t range = BHY2_LE2U16(bytes + 10);
    Serial.print("# CONFIG,"); Serial.print(phase);
    Serial.print(','); Serial.print(sensor.id());
    Serial.print(','); Serial.print(rate, 3);
    Serial.print(','); Serial.print(latency);
    Serial.print(','); Serial.print(range);
    Serial.print(','); Serial.println(sensitivity);
    if (sensor.id()==SENSOR_ID_MAG && strcmp(phase,"start")==0) magRange=range;
    return isfinite(rate) && fabsf(rate - requestedRate(sensor.id())) < 0.1f &&
           latency == 0 && (sensor.id() == SENSOR_ID_GAMERV ||
           (sensor.id()==SENSOR_ID_MAG ? range==magRange : range==expectedRange));
}

void fail()
{
    accel.end(); gyro.end();
    if (motionMode) magnetometer.end();
    if (quaternionMode) rotation.end();
    state = State::Failed;
    Serial.println("# ERROR,configuration_not_verified; reset before retrying");
}

void start()
{
    Serial.println(motionMode ? "# NICLA_MOTION_V1" : leanMode ? "# NICLA_LEAN_V1" : quaternionMode ? "# NICLA_QUATERNION_V1" : "# NICLA_DIAGNOSTIC_V1");
    Serial.println(quaternionMode
        ? "# Arduino_BHY2=1.0.8; Game RV=37; no magnetometer fusion or BLE output"
        : "# Arduino_BHY2=1.0.8; corrected counts; no orientation or BLE output");
    Serial.println("# configuration_read_attempts=3");
    Serial.println("# CONFIG,phase,sensor_id,rate_hz,latency_ms,range,sensitivity");
    if (motionMode) {
        Serial.println("# MOTION,sequence,host_us,status,forward_mps2,left_mps2,up_mps2,wx_dps,wy_dps,wz_dps,acc_age_us,gyro_age_us,q_age_us,flags,calibration_id");
        Serial.println("# motion_flags: 1=range_limit,2=specific_force_not_1g,4=delivery_skew; linear acceleration is experimental");
        Serial.println("# magnetometer=22,rate_hz=12.5; Bosch-corrected raw counts; units/frame not validated; not used in lean fusion");
    }
    if (leanMode) Serial.println("# LEAN,host_us,status,roll_deg,pitch_nose_up_deg,calibration_id; blank angles when invalid");
    Serial.println(leanMode ? "# startup_us=5000000,duration_us=120000000,rate_hz=50"
                              : quaternionMode ? "# startup_us=5000000,duration_us=60000000,rate_hz=50"
                                  : "# startup_us=5000000,duration_us=35000000");
    Serial.println("# host_us is delivery time; hardware timestamps and upstream loss are not exposed");
    if (!BHY2.begin(NICLA_STANDALONE) ||
        !BHY2.hasSensor(SENSOR_ID_ACC) || !BHY2.hasSensor(SENSOR_ID_GYRO))
    {
        state = State::Failed;
        Serial.println("# ERROR,sensor_initialization; reset before retrying");
        return;
    }
    if (quaternionMode && !BHY2.hasSensor(SENSOR_ID_GAMERV))
    {
        state = State::Failed;
        Serial.println("# ERROR,game_rotation_vector_unavailable");
        return;
    }
    if (motionMode && !BHY2.hasSensor(SENSOR_ID_MAG))
    {
        state=State::Failed;
        Serial.println("# ERROR,magnetometer_unavailable");
        return;
    }
    // Configure ranges while disabled, before consuming measurements.
    if (accel.setRange(ACCEL_RANGE_G) != 1 || gyro.setRange(GYRO_RANGE_DPS) != 1)
    {
        fail(); return;
    }
    Serial.println(quaternionMode
        ? "sensor_id,sequence,host_us,x_raw,y_raw,z_raw,w_raw,accuracy_raw"
        : "sensor_id,sequence,host_us,x_counts,y_counts,z_counts");
    startedUs = micros();
    previousPollUs = startedUs;
    state = State::Capturing;
    if (!accel.begin(requestedRate(), 0) || !gyro.begin(requestedRate(), 0) ||
        (quaternionMode && !rotation.begin(requestedRate(), 0)) ||
        (motionMode && !magnetometer.begin(requestedRate(SENSOR_ID_MAG),0)))
    {
        fail(); return;
    }
    const bool accelOK = readConfiguration(accel, "start", ACCEL_RANGE_G);
    const bool gyroOK = readConfiguration(gyro, "start", GYRO_RANGE_DPS);
    const bool rotationOK = !quaternionMode || readConfiguration(rotation, "start", 0);
    const bool magOK=!motionMode || readConfiguration(magnetometer,"start",0);
    if (!accelOK || !gyroOK || !rotationOK || !magOK) fail();
}

void writeLean()
{
    if (!leanMode) return;
    const uint32_t now = micros()-startedUs;
    if (now-previousLeanUs < 100000) return;
    previousLeanUs=now;
    if (samples.dropped()) lean.fault();
    const Lean::Reading r=lean.reading(now);
    Serial.print("L,"); Serial.print(now); Serial.print(','); Serial.print(Lean::name(r.status));
    Serial.print(',');
    if (r.status==Lean::Status::Valid) Serial.print(r.roll,3);
    Serial.print(',');
    if (r.status==Lean::Status::Valid) Serial.print(r.pitch,3);
    Serial.print(','); Serial.println(r.calibration);
    if (motionMode)
    {
        const Lean::Motion m=lean.motion(now);
        Serial.print("M,"); Serial.print(++motionSequence); Serial.print(','); Serial.print(now);
        Serial.print(','); Serial.print(Lean::name(m.status));
        const float values[]={m.linearMps2.x,m.linearMps2.y,m.linearMps2.z,
                              m.angularDps.x,m.angularDps.y,m.angularDps.z};
        for (float value:values) { Serial.print(','); if (m.status==Lean::Status::Valid) Serial.print(value,3); }
        Serial.print(','); Serial.print(m.accelAgeUs);
        Serial.print(','); Serial.print(m.gyroAgeUs);
        Serial.print(','); Serial.print(m.quaternionAgeUs);
        Serial.print(','); Serial.print(m.flags);
        Serial.print(','); Serial.println(m.calibration);
    }
}

void writeOneSample()
{
    DiagnosticCapture::Sample sample;
    if (!samples.pop(sample)) return;
    // One compact row per loop keeps polling between serial writes.
    // max_poll_gap_us exposes host stalls; no printing inside sensor callbacks.
    char line[80];
    const int length = quaternionMode
        ? snprintf(line, sizeof(line), "%u,%lu,%lu,%d,%d,%d,%d,%u\n",
            sample.sensorId, static_cast<unsigned long>(sample.sequence),
            static_cast<unsigned long>(sample.hostUs), sample.x, sample.y, sample.z,
            sample.w, sample.accuracy)
        : snprintf(line, sizeof(line), "%u,%lu,%lu,%d,%d,%d\n",
        sample.sensorId, static_cast<unsigned long>(sample.sequence),
        static_cast<unsigned long>(sample.hostUs), sample.x, sample.y, sample.z);
    Serial.write(reinterpret_cast<const uint8_t*>(line), length);
}
}

void SensorDiagnostics::begin()
{
    Serial.println("# Place the box still. Send s to capture 5 s startup + 30 s stationary data.");
    Serial.println("# Send q for a 60 s six-axis quaternion bench capture at 50 Hz.");
    Serial.println("# Send m for a 120 s motion logger bench test including magnetometer raw counts.");
    Serial.println("# Send l for a 120 s live lean bench test. Start upright and still; c recalibrates.");
}

void SensorDiagnostics::update()
{
    if (state == State::Waiting)
    {
        if (Serial.available())
        {
            const int command = Serial.read();
            if (command == 's' || command == 'q' || command == 'l' || command == 'm')
            {
                motionMode = command == 'm';
                leanMode = command == 'l' || motionMode;
                quaternionMode = command == 'q' || leanMode;
                start();
            }
        }
        return;
    }
    if (state == State::Failed || state == State::Done) return;
    if (state == State::Capturing)
    {
        if (leanMode && Serial.available() && Serial.read() == 'c') lean.recalibrate();
        const uint32_t now = micros();
        const uint32_t gap = now - previousPollUs;
        if (gap > maxPollGapUs) maxPollGapUs = gap;
        previousPollUs = now;
        BHY2.update();
        writeLean();
        if (static_cast<uint32_t>(micros() - startedUs) >= captureDuration())
        {
            state = State::Draining;
            const bool accelOK = readConfiguration(accel, "end", ACCEL_RANGE_G);
            const bool gyroOK = readConfiguration(gyro, "end", GYRO_RANGE_DPS);
            const bool rotationOK = !quaternionMode || readConfiguration(rotation, "end", 0);
            const bool magOK=!motionMode || readConfiguration(magnetometer,"end",0);
            accel.end(); gyro.end();
            if (motionMode) magnetometer.end();
            if (quaternionMode) rotation.end();
            if (!accelOK || !gyroOK || !rotationOK || !magOK)
                Serial.println("# ERROR,configuration_changed_or_unverified");
        }
    }
    writeOneSample();
    if (state == State::Draining && samples.size() == 0)
    {
        Serial.print("# END,accel_events="); Serial.print(accel.received);
        Serial.print(",gyro_events="); Serial.print(gyro.received);
        if (quaternionMode)
        {
            Serial.print(",quaternion_events="); Serial.print(rotation.received);
        }
        if (motionMode) { Serial.print(",magnetometer_events="); Serial.print(magnetometer.received); }
        Serial.print(",queue_dropped="); Serial.print(samples.dropped());
        Serial.print(",malformed="); Serial.print(malformed);
        Serial.print(",max_poll_gap_us="); Serial.println(maxPollGapUs);
        Serial.println("# Reset the board before another capture.");
        state = State::Done;
    }
}
