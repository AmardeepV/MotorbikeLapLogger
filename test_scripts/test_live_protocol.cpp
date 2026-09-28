#include <LiveProtocol.h>
#include <cassert>
#include <cstdio>
int main() {
 LiveProtocol::LinkHealth health;
 health.loop(0xfffffff0);health.loop(100);assert(health.loopMaxUs==116);
 health.sensor(1000);health.sensor(20);health.ble(2000);health.disconnected();
 uint8_t d[20];health.frame(d,5000);
 const uint8_t expected[]={0xd1,1,1,0,0x88,0x13,0,0,0xe8,3,0,0,0xd0,7,0,0,0x74,0,0,0};
 for(int i=0;i<20;++i) assert(d[i]==expected[i]);
 assert(health.sensorMaxUs==1000 && health.bleMaxUs==2000);
 health.ble(3000,1);health.ble(4000,2);health.deferred();health.detailFrame(d,5000);
 const uint8_t detail[]={0xd2,1,1,0,0x88,0x13,0,0,2,0,3,0,4,0,0,0,1,0,0,0};
 for(int i=0;i<20;++i) assert(d[i]==detail[i]);
 assert(LiveProtocol::LinkHealth::ms(UINT32_MAX)==65535);
 LiveProtocol::Session session;
 session.observe({Lean::Status::Valid,-40,0,1}); assert(session.left==0);
 session.nextLap();session.observe({Lean::Status::Valid,-25,0,1});
 session.nextLap();session.observe({Lean::Status::Valid,15,0,1});
 assert(session.number==1 && session.lap==2 && session.left==25 && session.right==15);
 session.observe({Lean::Status::Invalid,80,0,1});assert(session.right==15);
 session.stop();session.observe({Lean::Status::Valid,80,0,1});assert(session.right==15);
 session.nextLap();assert(session.number==2 && session.lap==1 && session.left==0 && session.right==0);
 uint8_t c[20];LiveProtocol::sessionFrame(c,1,100,session);
 for(auto v:c) printf("%02x",v);puts("");
 uint8_t a[20],b[20];int16_t mag[]={-10,20,-30};
 Lean::Reading r={Lean::Status::Valid,-25.12f,15.25f,1};
 Lean::Motion m={Lean::Status::Valid,{1.23f,-2.34f,0.5f},{-15.5f,2.3f,100},0,0,0,2,1};
 LiveProtocol::mainFrame(a,65535,0x12345678,r,m,false);
 assert(a[0]==0xA1 && a[1]==5 && a[2]==5 && a[3]==2 && a[4]==255 && a[5]==255);
 assert(a[6]==0x78 && a[9]==0x12 && a[10]==0x30 && a[11]==0xf6);
 // Golden bytes consumed by the independent Swift parser test.
 for(auto v:a) printf("%02x",v);puts("");
 LiveProtocol::ratesFrame(b,65535,0x12345678,m,true,mag,true);
 for(auto v:b) printf("%02x",v);puts("");
 LiveProtocol::mainFrame(a,0,0,r,m,true);
 assert(a[1]==6 && a[2]==6);
 for(int i=10;i<20;i+=2) assert(a[i]==255 && a[i+1]==127);
 assert(LiveProtocol::fixed(NAN,100)==32767 && LiveProtocol::fixed(1000,100)==32767);
 assert(LiveProtocol::fixed(-12.34f,100)==-1234);
}
