// Replay recorded callback order through the same estimator used by firmware.
// Does not simulate serial throughput or prove hardware acquisition timing.
#include <LeanEstimator.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 std::ifstream f(argv[1]); if(!f) return 2;
 Lean::Estimator estimator; std::string line;
 bool started=false, ended=false; unsigned count=0,valid=0; int previous=-1;
 std::cout << "host_us,status,roll,pitch,calibration_id\n";
 while(std::getline(f,line)) {
  if(line.find("# NICLA_QUATERNION_V1")!=std::string::npos) { started=true;continue; }
  if(line.find("# END,")==0) { ended=true;break; }
  if(!started) continue;
  unsigned id,seq,t,accuracy; int x,y,z,w;
  if(std::sscanf(line.c_str(),"%u,%u,%u,%d,%d,%d,%d,%u",&id,&seq,&t,&x,&y,&z,&w,&accuracy)!=8) continue;
  if(id==4) estimator.acceleration({x*4.f/32768,y*4.f/32768,z*4.f/32768},t);
  if(id==13) estimator.angularVelocity({x*1000.f/32768,y*1000.f/32768,z*1000.f/32768},t);
  if(id==37) {
   estimator.quaternion(x/16384.f,y/16384.f,z/16384.f,w/16384.f,t);
   const auto r=estimator.reading(t); ++count;
   if(r.status==Lean::Status::Valid) ++valid;
   if(previous!=static_cast<int>(r.status)) {
    std::cerr << "Status at " << t << ": " << Lean::name(r.status) << '\n'; previous=static_cast<int>(r.status);
   }
   std::cout << t << ',' << Lean::name(r.status) << ',' << r.roll << ',' << r.pitch << ',' << r.calibration << '\n';
  }
 }
 std::cerr << "Quaternion events=" << count << ", valid readings=" << valid << '\n';
 return started && ended && valid ? 0:2;
}
