#pragma once
#include <LeanEstimator.h>
#include <stdint.h>

// Independent 20-byte little-endian notifications. Never transmit NaN or
// silently clamp a measurement: 32767 is the unavailable sentinel.
namespace LiveProtocol {
constexpr int16_t UNAVAILABLE=32767;
inline void u16(uint8_t* b,uint16_t v) { b[0]=v; b[1]=v>>8; }
inline void u32(uint8_t* b,uint32_t v) { for(int i=0;i<4;++i) b[i]=v>>(8*i); }
inline int16_t fixed(float v,float scale) {
    const float n=roundf(v*scale);
    return isfinite(n) && n>=-32766 && n<=32766 ? int16_t(n):UNAVAILABLE;
}
inline void mainFrame(uint8_t* b,uint16_t sequence,uint32_t ms,
                      Lean::Reading r,Lean::Motion m,bool waiting) {
    b[0]=0xA1; b[1]=waiting?6:uint8_t(r.status); b[2]=waiting?6:uint8_t(m.status); b[3]=uint8_t(m.flags);
    u16(b+4,sequence);u32(b+6,ms);
    const bool angles=!waiting && r.status==Lean::Status::Valid;
    const bool motion=!waiting && m.status==Lean::Status::Valid;
    u16(b+10,angles?fixed(r.roll,100):UNAVAILABLE);
    u16(b+12,angles?fixed(r.pitch,100):UNAVAILABLE);
    u16(b+14,motion?fixed(m.linearMps2.x,100):UNAVAILABLE);
    u16(b+16,motion?fixed(m.linearMps2.y,100):UNAVAILABLE);
    u16(b+18,motion?fixed(m.linearMps2.z,100):UNAVAILABLE);
}
inline void ratesFrame(uint8_t* b,uint16_t sequence,uint32_t ms,
                       Lean::Motion m,bool usable,const int16_t* mag,bool magFresh) {
    b[0]=0xB1;b[1]=(usable?1:0)|(magFresh?2:0);u16(b+2,sequence);u32(b+4,ms);
    const float rates[]={m.angularDps.x,m.angularDps.y,m.angularDps.z};
    for(int i=0;i<3;++i) {u16(b+8+2*i,usable?fixed(rates[i],10):UNAVAILABLE);
                         u16(b+14+2*i,magFresh?mag[i]:UNAVAILABLE);}
}
}

namespace LiveProtocol {
struct Session {
    bool active=false;
    uint16_t number=0, lap=0;
    float left=0, right=0;
    void nextLap() {
        if(!active) { active=true; ++number; lap=0; left=right=0; }
        if(lap<65535) ++lap;
    }
    void stop() { active=false; lap=0; }
    void observe(Lean::Reading r) {
        if(active && r.status==Lean::Status::Valid) {
            left=fmaxf(left,-r.roll); right=fmaxf(right,r.roll);
        }
    }
};
inline void sessionFrame(uint8_t* b,uint16_t sequence,uint32_t ms,const Session& s) {
    b[0]=0xC1;b[1]=s.active?1:0;u16(b+2,sequence);u32(b+4,ms);
    u16(b+8,s.number);u16(b+10,s.lap);u16(b+12,fixed(s.left,100));u16(b+14,fixed(s.right,100));
    for(int i=16;i<20;++i) b[i]=0;
}
}

namespace LiveProtocol {
// Read-only diagnostics; no extra notification stream and no persistent storage.
struct LinkHealth {
    uint32_t sensorMaxUs=0,bleMaxUs=0,loopMaxUs=0,lastLoopUs=0;
    uint16_t disconnects=0;
    uint32_t pollMaxUs=0,notifyMaxUs=0,statusMaxUs=0,creditDeferrals=0;
    bool haveLoop=false;
    void loop(uint32_t now) {
        if(haveLoop && uint32_t(now-lastLoopUs)>loopMaxUs) loopMaxUs=now-lastLoopUs;
        lastLoopUs=now;haveLoop=true;
    }
    void sensor(uint32_t elapsed) { if(elapsed>sensorMaxUs) sensorMaxUs=elapsed; }
    void ble(uint32_t elapsed,int stage=0) {
        if(elapsed>bleMaxUs) bleMaxUs=elapsed;
        uint32_t& peak=stage==1?notifyMaxUs:stage==2?statusMaxUs:pollMaxUs;
        if(elapsed>peak) peak=elapsed;
    }
    void deferred() { if(creditDeferrals!=UINT32_MAX) ++creditDeferrals; }
    static uint16_t ms(uint32_t us) { return us/1000>65535?65535:uint16_t(us/1000); }
    void detailFrame(uint8_t* b,uint32_t uptime) const {
        b[0]=0xD2;b[1]=disconnects>255?255:uint8_t(disconnects);
        u16(b+2,ms(sensorMaxUs));u32(b+4,uptime);
        u16(b+8,ms(pollMaxUs));u16(b+10,ms(notifyMaxUs));u16(b+12,ms(statusMaxUs));
        u16(b+14,ms(loopMaxUs));u32(b+16,creditDeferrals);
    }
    void disconnected() { if(disconnects<65535) ++disconnects; }
    void frame(uint8_t* b,uint32_t ms) const {
        b[0]=0xD1;b[1]=1;u16(b+2,disconnects);u32(b+4,ms);
        u32(b+8,sensorMaxUs);u32(b+12,bleMaxUs);u32(b+16,loopMaxUs);
    }
};
}
