#pragma once
#include <math.h>
#include <stdint.h>
#include <ImuFrames.h>

// Bench tilt estimator. Host delivery times cannot establish hardware freshness
// or compensate for sustained vehicle acceleration. No heading estimate used.
namespace Lean
{
using Vec = ImuFrames::Vector3;
inline float dot(Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec cross(Vec a, Vec b)
{ return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
inline Vec add(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec scale(Vec a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float norm(Vec a) { return sqrtf(dot(a,a)); }
inline bool finite(Vec a) { return isfinite(a.x) && isfinite(a.y) && isfinite(a.z); }
constexpr uint32_t FRESH_US = 100000, STARTUP_US = 5000000, STABLE_US = 3000000;
constexpr float DEGREES_PER_RADIAN = 57.2957795131f;
enum class Status { Startup, Calibrating, Stale, Invalid, Fault, Valid };
inline const char* name(Status s)
{
    switch (s) {
    case Status::Startup: return "STARTUP";
    case Status::Calibrating: return "CALIBRATING";
    case Status::Stale: return "STALE";
    case Status::Invalid: return "INVALID";
    case Status::Fault: return "CAPTURE_FAULT";
    case Status::Valid: return "VALID";
    }
    return "INVALID";
}
struct Reading { Status status; float roll, pitch; uint32_t calibration; };
// Linear acceleration is an estimate based on Game RV gravity; not a velocity
// or distance measurement. Body rates are not Euler-angle derivatives.
struct Motion {
    Status status;
    Vec linearMps2, angularDps;
    uint32_t accelAgeUs, gyroAgeUs, quaternionAgeUs, flags, calibration;
};
class Estimator
{
public:
    void acceleration(Vec g, uint32_t us)
    { acc=g; accUs=us; haveAcc=true; accOK=finite(g) && norm(g)>0.05f; }
    void angularVelocity(Vec dps, uint32_t us)
    { gyro=dps; gyroUs=us; haveGyro=true; gyroOK=finite(dps); }
    void fault() { failed=true; stableCount=0; }
    void recalibrate() { calibrated=false; stableCount=0; }
    bool isCalibrated() const { return calibrated && !failed; }
    void quaternion(float x, float y, float z, float w, uint32_t us)
    {
        if(us>=STARTUP_US) startupComplete=true;
        const uint32_t previous=qUs;
        const bool continuous=haveQ && uint32_t(us-previous)<=FRESH_US;
        haveQ=true; qUs=us;
        const float n2=x*x+y*y+z*z+w*w;
        qOK=isfinite(n2) && n2>=0.99f*0.99f && n2<=1.01f*1.01f;
        if (!qOK) { stableCount=0; return; }
        // R(q)^T * reference-up, then confirmed reported->motorcycle frame.
        up={-2*(x*z-w*y)/n2, -2*(y*z+w*x)/n2, 1-2*(x*x+y*y)/n2};
        if (calibrated) return;
        if (!continuous) stableCount=0;
        const bool stable=startupComplete && healthy(us) && !failed &&
            fabsf(norm(acc)-1.0f)<=0.05f && norm(gyro)<1.0f && up.z>0.9659258f;
        if (!stable) { stableCount=0; return; }
        // All accepted directions must stay within 0.5 degrees of the first.
        if (stableCount && dot(anchor,up)<0.9999619f) stableCount=0;
        if (!stableCount) { stableStart=us; anchor=up; sum={0,0,0}; }
        sum=add(sum,up); ++stableCount;
        if (uint32_t(us-stableStart)>=STABLE_US && stableCount>=120)
        {
            reference=scale(sum,1.0f/norm(sum));
            calibrated=true; ++calibrationId; stableCount=0;
        }
    }
    const char* calibrationHint(uint32_t us) const {
        if (!fresh(us)) return "CAL_WAIT_DATA";
        if (!qOK || !accOK || !gyroOK || failed) return "CAL_INVALID_SENSOR";
        if (up.z<=0.9659258f) return "CAL_WAIT_UPRIGHT";
        if (norm(gyro)>=1.0f) return "CAL_WAIT_STILL";
        if (fabsf(norm(acc)-1.0f)>0.05f) return "CAL_WAIT_GRAVITY";
        return "CAL_HOLD_STILL";
    }
    Reading reading(uint32_t us) const
    {
        Status status=Status::Valid;
        if (failed) status=Status::Fault;
        else if (!startupComplete && us<STARTUP_US) status=Status::Startup;
        else if (calibrated ? (!haveQ || uint32_t(us-qUs)>FRESH_US) : !fresh(us)) status=Status::Stale;
        else if (!qOK || (!calibrated && (!accOK || !gyroOK))) status=Status::Invalid;
        else if (!calibrated) status=Status::Calibrating;
        if (status!=Status::Valid) return {status,NAN,NAN,calibrationId};
        // Shortest rotation taking calibrated up to +Z; preserve mounting yaw.
        // This rotates the full vector, not separate Euler-angle offsets.
        const Vec corrected=correctMount(up);
        // Roll is undefined when the forward axis is almost vertical.
        if (hypotf(corrected.y,corrected.z)<0.0871557f)
            return {Status::Invalid,NAN,NAN,calibrationId};
        return {Status::Valid,atan2f(corrected.y,corrected.z)*DEGREES_PER_RADIAN,
                atan2f(corrected.x,hypotf(corrected.y,corrected.z))*DEGREES_PER_RADIAN,
                calibrationId};
    }
    Motion motion(uint32_t us) const
    {
        const Reading orientation=reading(us);
        Motion m={orientation.status,{NAN,NAN,NAN},{NAN,NAN,NAN},
                  uint32_t(us-accUs),uint32_t(us-gyroUs),uint32_t(us-qUs),0,calibrationId};
        // bit 0: near configured range limit. bit 1: specific-force magnitude
        // differs from 1 g; informative only, NOT a reliable motion detector.
        if (fabsf(acc.x)>=3.96f || fabsf(acc.y)>=3.96f || fabsf(acc.z)>=3.96f ||
            fabsf(gyro.x)>=990 || fabsf(gyro.y)>=990 || fabsf(gyro.z)>=990) m.flags|=1;
        if (fabsf(norm(acc)-1)>0.05f) m.flags|=2;
        if (m.status!=Status::Valid) return m;
        // Tilt uses the fused quaternion. Linear acceleration and body rates
        // additionally require fresh, valid accelerometer and gyro streams.
        if (!fresh(us)) { m.status=Status::Stale; return m; }
        if (!accOK || !gyroOK) { m.status=Status::Invalid; return m; }
        if (m.flags&1) { m.status=Status::Invalid; return m; }
        const uint32_t youngest=m.accelAgeUs<m.gyroAgeUs ?
            (m.accelAgeUs<m.quaternionAgeUs ? m.accelAgeUs:m.quaternionAgeUs) :
            (m.gyroAgeUs<m.quaternionAgeUs ? m.gyroAgeUs:m.quaternionAgeUs);
        const uint32_t oldest=m.accelAgeUs>m.gyroAgeUs ?
            (m.accelAgeUs>m.quaternionAgeUs ? m.accelAgeUs:m.quaternionAgeUs) :
            (m.gyroAgeUs>m.quaternionAgeUs ? m.gyroAgeUs:m.quaternionAgeUs);
        if (oldest-youngest>40000) { m.flags|=4; m.status=Status::Stale; return m; }
        const Vec specificForce=correctMount(ImuFrames::reportedToMotorcycle(acc));
        m.linearMps2=scale(add(specificForce,scale(correctMount(up),-1)),9.80665f);
        m.angularDps=correctMount(ImuFrames::reportedToMotorcycle(gyro));
        return m;
    }
private:
    Vec correctMount(Vec v) const
    {
        const Vec k={reference.y,-reference.x,0};
        return add(add(v,cross(k,v)),scale(cross(k,cross(k,v)),1/(1+reference.z)));
    }
    bool fresh(uint32_t us) const
    { return haveAcc && haveGyro && haveQ && uint32_t(us-accUs)<=FRESH_US &&
        uint32_t(us-gyroUs)<=FRESH_US && uint32_t(us-qUs)<=FRESH_US; }
    bool healthy(uint32_t us) const { return fresh(us) && accOK && gyroOK && qOK; }
    Vec acc{},gyro{},up{},anchor{},sum{},reference{0,0,1};
    uint32_t accUs=0,gyroUs=0,qUs=0,stableStart=0,stableCount=0,calibrationId=0;
    bool haveAcc=false,haveGyro=false,haveQ=false,accOK=false,gyroOK=false,qOK=false;
    bool calibrated=false,failed=false,startupComplete=false;
};
}
