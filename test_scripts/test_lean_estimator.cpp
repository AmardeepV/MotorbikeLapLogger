#include <LeanEstimator.h>
#include <assert.h>
#include <cmath>
#include <iostream>
struct Q { float x,y,z,w; };
Q mul(Q a,Q b) { return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
 a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
 a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}; }
Q axis(float x,float y,float z,float deg) {
 float n=std::sqrt(x*x+y*y+z*z),h=deg/Lean::DEGREES_PER_RADIAN/2;
 return {x/n*std::sin(h),y/n*std::sin(h),z/n*std::sin(h),std::cos(h)};
}
Q pose(float roll,float pitch,float yaw=0,Q mount={0,0,0,1}) {
 return mul(mul(mul(mul(axis(0,0,1,yaw),axis(0,1,0,-pitch)),
                axis(1,0,0,roll)),mount),axis(0,0,1,180));
}
void feed(Lean::Estimator& e,uint32_t t,Q q, float speed=0,float g=1) {
 e.acceleration({0,0,g},t); e.angularVelocity({speed,0,0},t);
 e.quaternion(q.x,q.y,q.z,q.w,t);
}
void calibrate(Lean::Estimator& e,Q q=pose(0,0)) {
 for(uint32_t t=0;t<=8100000;t+=20000) feed(e,t,q);
 assert(e.reading(8100000).status==Lean::Status::Valid);
}
void close(float a,float b) { assert(std::fabs(a-b)<0.002f); }
int main() {
 Lean::Estimator hints;
 assert(std::string(hints.calibrationHint(6000000))=="CAL_WAIT_DATA");
 feed(hints,6000000,pose(20,0));assert(std::string(hints.calibrationHint(6000000))=="CAL_WAIT_UPRIGHT");
 feed(hints,6020000,pose(0,0),2);assert(std::string(hints.calibrationHint(6020000))=="CAL_WAIT_STILL");
 feed(hints,6040000,pose(0,0),0,1.2f);assert(std::string(hints.calibrationHint(6040000))=="CAL_WAIT_GRAVITY");
 feed(hints,6060000,pose(0,0));assert(std::string(hints.calibrationHint(6060000))=="CAL_HOLD_STILL");
 Lean::Estimator e; assert(e.reading(0).status==Lean::Status::Startup);
 assert(e.reading(6000000).status==Lean::Status::Stale); calibrate(e);
 for(float yaw: {0.f,45.f,135.f,-90.f}) for(float roll: {-45.f,0.f,35.f})
 for(float pitch: {-30.f,0.f,25.f}) {
  feed(e,8200000,pose(roll,pitch,yaw)); auto r=e.reading(8200000);
  assert(r.status==Lean::Status::Valid); close(r.roll,roll);close(r.pitch,pitch);
 }
 // q and -q are identical; Q14 quantization still yields a sensible angle.
 Q q=pose(30,-20,80); feed(e,8300000,{-q.x,-q.y,-q.z,-q.w}); close(e.reading(8300000).roll,30);
 feed(e,8400000,{0,0,0,0}); assert(e.reading(8400000).status==Lean::Status::Invalid);
 feed(e,8500000,{NAN,0,0,1}); assert(e.reading(8500000).status==Lean::Status::Invalid);
 feed(e,8600000,pose(0,0)); assert(e.reading(8700001).status==Lean::Status::Stale);
 e.quaternion(q.x,q.y,q.z,q.w,8800000);
 assert(e.reading(8800000).status==Lean::Status::Valid);
 assert(e.motion(8800000).status==Lean::Status::Stale);
 assert(e.isCalibrated()); // delivery gaps do not revoke calibration or LAP readiness
 assert(e.reading(8900001).status==Lean::Status::Stale && e.isCalibrated());
 feed(e,8900000,pose(0,90)); assert(e.reading(8900000).status==Lean::Status::Invalid);
 // Known mounting tilt must be corrected as a 3D rotation under combined motion.
 Lean::Estimator mounted; Q m=axis(1,2,0,8); calibrate(mounted,pose(0,0,0,m));
 for(float roll: {-40.f,0.f,30.f}) for(float pitch: {-25.f,0.f,25.f}) {
  feed(mounted,8200000,pose(roll,pitch,120,m)); auto r=mounted.reading(8200000);
  assert(r.status==Lean::Status::Valid); close(r.roll,roll);close(r.pitch,pitch);
 }
 // Movement prevents calibration, then a full fresh stable window is required.
 Lean::Estimator moving;
 for(uint32_t t=0;t<10000000;t+=20000) feed(moving,t,pose(0,0),2);
 assert(moving.reading(9980000).status==Lean::Status::Calibrating);
 for(uint32_t t=10000000;t<13000000;t+=20000) feed(moving,t,pose(0,0));
 assert(moving.reading(12980000).status==Lean::Status::Calibrating);
 feed(moving,13000000,pose(0,0)); assert(moving.reading(13000000).status==Lean::Status::Valid);
 moving.recalibrate(); assert(!moving.isCalibrated()); auto pending=moving.reading(13000000);
 assert(pending.status==Lean::Status::Calibrating && std::isnan(pending.roll));
 for(uint32_t t=13020000;t<=16020000;t+=20000) feed(moving,t,pose(0,0));
 assert(moving.reading(16020000).calibration==2);
 moving.fault(); assert(!moving.isCalibrated()); assert(moving.reading(16020000).status==Lean::Status::Fault);
 // Acceleration / pose changes / large upright error cannot silently zero lean.
 for(int scenario=0;scenario<3;++scenario) {
  Lean::Estimator bad;
  for(uint32_t t=0;t<=10000000;t+=20000)
   feed(bad,t,pose(scenario==1 ? (t/20000%2 ? 1.f:0.f) : scenario==2 ? 20.f:0.f,0),0,scenario==0 ? 1.2f:1.f);
  assert(bad.reading(10000000).status==Lean::Status::Calibrating);
 }
 // Stationary gravity must cancel at combined lean/pitch, including mounting tilt.
 for(float roll: {-40.f,0.f,35.f}) for(float pitch: {-25.f,0.f,20.f}) {
  Q orientation=pose(roll,pitch,60,m);
  float x=orientation.x,y=orientation.y,z=orientation.z,w=orientation.w;
  Lean::Vec gravity={2*(x*z-w*y),2*(y*z+w*x),1-2*(x*x+y*y)};
  mounted.acceleration(gravity,9000000);mounted.angularVelocity({0,0,0},9000000);
  mounted.quaternion(x,y,z,w,9000000);
  auto motion=mounted.motion(9000000);assert(motion.status==Lean::Status::Valid);
  close(motion.linearMps2.x,0);close(motion.linearMps2.y,0);close(motion.linearMps2.z,0);
 }
 Lean::Estimator motion;calibrate(motion);
 for(float forward: {-0.3f,0.2f}) {
  feed(motion,9000000,pose(0,0));
  motion.acceleration({-forward,-0.1f,1.05f},9000000);
  motion.angularVelocity({-5,7,3},9000000);
  const auto output=motion.motion(9000000);assert(output.status==Lean::Status::Valid);
  close(output.linearMps2.x,forward*9.80665f);close(output.linearMps2.y,0.980665f);
  close(output.linearMps2.z,0.4903325f);
  close(output.angularDps.x,5);close(output.angularDps.y,-7);close(output.angularDps.z,3);
 }
 motion.acceleration({4,0,1},9000000);
 auto clipped=motion.motion(9000000);assert(clipped.status==Lean::Status::Invalid && (clipped.flags&1));
 assert(std::isnan(clipped.linearMps2.x));
 feed(motion,9100000,pose(0,0));motion.acceleration({0,0,1},9150000);
 auto skew=motion.motion(9150000);assert(skew.status==Lean::Status::Stale && (skew.flags&4));
 assert(motion.motion(9300000).status==Lean::Status::Stale);
 // Continuous BLE operation must survive micros() wrapping after ~71 minutes.
 Lean::Estimator wrapped;calibrate(wrapped);
 feed(wrapped,0xffff0000u,pose(0,0));feed(wrapped,10000,pose(0,0));
 assert(wrapped.reading(10000).status==Lean::Status::Valid);
 wrapped.recalibrate();
 for(uint32_t elapsed=0;elapsed<=3020000;elapsed+=20000)
  feed(wrapped,uint32_t(0xffff0000u+elapsed),pose(0,0));
 assert(wrapped.reading(uint32_t(0xffff0000u+3020000)).status==Lean::Status::Valid);
 std::cout << "Lean estimator checks passed\n";
}
