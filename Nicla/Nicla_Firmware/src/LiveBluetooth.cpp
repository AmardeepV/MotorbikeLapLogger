#ifdef NICLA_LIVE_BLE
#include "LiveBluetooth.h"
#include <Arduino.h>
#include <Arduino_BHY2.h>
#include <ArduinoBLE.h>
#include <LiveProtocol.h>
#include <string.h>

namespace {
BLEService service("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
BLECharacteristic command("6e400002-b5a3-f393-e0a9-e50e24dcca9e",BLEWrite,20);
BLECharacteristic status("6e400003-b5a3-f393-e0a9-e50e24dcca9e",BLERead|BLENotify,20);
BLECharacteristic motionData("6e400005-b5a3-f393-e0a9-e50e24dcca9e",BLERead|BLENotify,20,true);
BLECharacteristic ratesData("6e400006-b5a3-f393-e0a9-e50e24dcca9e",BLERead|BLENotify,20,true);
BLECharacteristic sessionData("6e400007-b5a3-f393-e0a9-e50e24dcca9e",BLERead|BLENotify,20,true);
Lean::Estimator estimator;
LiveProtocol::Session session;
bool radioReady=false, sensorFault=false, armed=false, completionSent=false;
bool wasConnected=false, haveMag=false;
uint32_t lastSendMs=0, lastMagUs=0, calibrationStartedMs=0;
uint16_t sequence=0;
uint8_t frames[3][20];
uint8_t pendingFrames=0;
uint32_t lastFrameMs=0,lastHintMs=0;
const char* lastHint=nullptr;
int16_t magnetic[3]={0,0,0};

void writeStatus(const char* value) { status.writeValue(value); }

class VectorSensor:public SensorXYZ {
public:
    explicit VectorSensor(uint8_t id):SensorXYZ(id) {}
    using SensorXYZ::setData;
    void setData(SensorDataPacket& p) override {
        if(p.size!=7) {sensorFault=true;estimator.fault();return;}
        const uint32_t now=micros();
        SensorXYZ::setData(p);
        if(id()==SENSOR_ID_ACC) estimator.acceleration({x()*4.f/32768,y()*4.f/32768,z()*4.f/32768},now);
        else if(id()==SENSOR_ID_GYRO) estimator.angularVelocity({x()*1000.f/32768,y()*1000.f/32768,z()*1000.f/32768},now);
        else {magnetic[0]=x();magnetic[1]=y();magnetic[2]=z();lastMagUs=now;haveMag=true;}
    }
};
VectorSensor accel(SENSOR_ID_ACC),gyro(SENSOR_ID_GYRO),mag(SENSOR_ID_MAG);
class RotationSensor:public SensorQuaternion {
public:
    RotationSensor():SensorQuaternion(SENSOR_ID_GAMERV) {}
    using SensorQuaternion::setData;
    void setData(SensorDataPacket& p) override {
        if(p.size!=11) {sensorFault=true;estimator.fault();return;}
        if(armed) estimator.quaternion(p.getInt16(0)/16384.f,p.getInt16(2)/16384.f,
                                      p.getInt16(4)/16384.f,p.getInt16(6)/16384.f,micros());
        if(armed && !sensorFault) session.observe(estimator.reading(micros()));
    }
};
RotationSensor rotation;

bool configuration(uint8_t id,float expectedRate,uint16_t expectedRange) {
    uint8_t bytes[12];uint32_t length=0;int8_t result;
    for(int attempt=0;attempt<3;++attempt) {
        result=sensortec.bhy2_getParameter(BHY2_PARAM_SENSOR_CONF_0+id,bytes,sizeof(bytes),&length);
        if(result==BHY2_OK && length==sizeof(bytes)) {
            uint32_t bits=BHY2_LE2U32(bytes);float rate;memcpy(&rate,&bits,4);
            return isfinite(rate) && fabsf(rate-expectedRate)<0.1f && BHY2_LE2U32(bytes+4)==0 &&
                (!expectedRange || BHY2_LE2U16(bytes+10)==expectedRange);
        }
        BHY2.update();delay(10);length=0;
    }
    return false;
}
void publish() {
    const uint32_t now=micros(),ms=millis();
    lastSendMs=ms; // Forced state updates also restart the regular notification interval.
    Lean::Reading r=estimator.reading(now);Lean::Motion m=estimator.motion(now);
    if(sensorFault) {r.status=Lean::Status::Fault;m.status=Lean::Status::Fault;}
    const bool waiting=!armed && !sensorFault;
    uint8_t* a=frames[0];uint8_t* b=frames[2];uint8_t* c=frames[1];++sequence;
    LiveProtocol::mainFrame(a,sequence,ms,r,m,waiting);
    LiveProtocol::ratesFrame(b,sequence,ms,m,armed && !sensorFault && m.status==Lean::Status::Valid,
        magnetic,!sensorFault && haveMag && uint32_t(now-lastMagUs)<=200000);
    // Independent sequence/time-tagged characteristics; receiver never joins
    // values from different samples as one snapshot. No SD or session storage.
    LiveProtocol::sessionFrame(c,sequence,ms,session);
    // Queue an immutable snapshot. Transmit only one characteristic per loop,
    // allowing BHY2 servicing between potentially blocking BLE writes.
    pendingFrames=(armed && !completionSent)?3:7;
    if(armed && !sensorFault && !completionSent && r.status==Lean::Status::Valid) {
        completionSent=true;writeStatus("CALIBRATION_COMPLETE");
    }
}
}

void LiveBluetooth::begin() {
    // Standalone prevents Arduino_BHY2 from creating its own BLE service.
    bool ok=BHY2.begin(NICLA_STANDALONE);
    ok=ok && BHY2.hasSensor(SENSOR_ID_ACC) && BHY2.hasSensor(SENSOR_ID_GYRO) &&
        BHY2.hasSensor(SENSOR_ID_MAG) && BHY2.hasSensor(SENSOR_ID_GAMERV);
    if(ok) ok=accel.setRange(4)==1 && gyro.setRange(1000)==1;
    if(ok) ok=accel.begin(50,0) && gyro.begin(50,0) && rotation.begin(50,0) && mag.begin(12.5f,0);
    if(ok) {
        const bool a=configuration(SENSOR_ID_ACC,50,4),g=configuration(SENSOR_ID_GYRO,50,1000);
        const bool q=configuration(SENSOR_ID_GAMERV,50,0),m=configuration(SENSOR_ID_MAG,12.5f,0);
        ok=a && g && q && m;
    }
    sensorFault=sensorFault || !ok;if(sensorFault) estimator.fault();
    if(!BLE.begin()) return;
    radioReady=true;
    BLE.setConnectionInterval(12,24); // 15–30 ms, subject to the central's negotiation.
    BLE.setLocalName("Nicla Motion");BLE.setDeviceName("Nicla Motion");
    BLE.setAdvertisedService(service);
    service.addCharacteristic(command);service.addCharacteristic(status);
    service.addCharacteristic(sessionData);
    service.addCharacteristic(motionData);service.addCharacteristic(ratesData);
    BLE.addService(service);
    writeStatus(sensorFault?"SENSOR_FAULT":"READY_TO_CALIBRATE");
    publish();BLE.advertise();
}

void LiveBluetooth::update() {
    if(!radioReady) return;
    if(!sensorFault) BHY2.update();
    const bool connected=BLE.connected();
    if(connected!=wasConnected) {
        // A radio interruption does not invalidate an upright reference.
        // Stop the session on link loss, but preserve completed calibration.
        session.stop();pendingFrames=0;
        if(!estimator.isCalibrated()) {
            armed=false;completionSent=false;estimator.recalibrate();
        }
        writeStatus(sensorFault?"SENSOR_FAULT":estimator.isCalibrated()?"CALIBRATION_RETAINED":"READY_TO_CALIBRATE");
        publish();
        if(!connected) BLE.advertise();
        wasConnected=connected;
    }
    if(command.written()) {
        char value[21]={};const int size=command.valueLength();
        command.readValue(reinterpret_cast<uint8_t*>(value),20);
        if(connected && size==4 && memcmp(value,"STOP",4)==0) {
            session.stop();writeStatus("SESSION_STOPPED");publish();
        } else if(sensorFault) writeStatus("SENSOR_FAULT");
        else if(connected && size==3 && memcmp(value,"LAP",3)==0) {
            if(armed && estimator.isCalibrated()) {
                session.nextLap();writeStatus("LAP_STARTED");publish();
            } else writeStatus("LAP_NOT_READY");
        } else if(connected && size==9 && memcmp(value,"CALIBRATE",9)==0) {
            if(session.active) {writeStatus("CAL_REJECTED_ACTIVE");return;}
            if(armed && !completionSent) {writeStatus("CALIBRATION_STARTED");return;}
            session.left=session.right=0;
            estimator.recalibrate();armed=true;completionSent=false;
            calibrationStartedMs=millis();lastHint=nullptr;lastHintMs=millis();writeStatus("CALIBRATION_STARTED");publish();
        } else writeStatus("COMMAND_UNSUPPORTED");
    }
    if(armed && !completionSent && uint32_t(millis()-lastHintMs)>=1000) {
        lastHintMs=millis();
        const char* hint=estimator.calibrationHint(micros());
        if(!lastHint || strcmp(hint,lastHint)!=0) {writeStatus(hint);lastHint=hint;}
    }
    if(armed && !completionSent && uint32_t(millis()-calibrationStartedMs)>30000) {
        armed=false;estimator.recalibrate();writeStatus("CALIBRATION_TIMEOUT");publish();
    }
    const uint32_t interval=(armed && !completionSent)?200:100;
    if(!pendingFrames && uint32_t(millis()-lastSendMs)>=interval) publish();
    if(pendingFrames && uint32_t(millis()-lastFrameMs)>=20) {
        const int index=(pendingFrames&1)?0:(pendingFrames&2)?1:2;
        BLECharacteristic& target=index==0?motionData:index==1?sessionData:ratesData;
        target.writeValue(frames[index],20);
        pendingFrames &= ~(1<<index);
        lastFrameMs=millis();
    }
}
#endif
