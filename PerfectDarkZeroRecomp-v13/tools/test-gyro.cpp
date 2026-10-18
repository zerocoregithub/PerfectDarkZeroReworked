#include "../src/pdz_gyro_math.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
 using namespace pdz::input;
 GyroSpace space;
 auto projected=space.Project(1,2,0,0,9.81,0,.01,1);
 assert(std::abs(projected.x-2)<1e-9&&projected.y==1);
 space.Reset();projected=space.Project(1,2,0,0,9.81,0,.01,2);
 assert(std::isfinite(projected.x)&&std::isfinite(projected.y));
 space.Reset();projected=space.Project(0,0,-2,0,0,-9.81,.01,1);
 assert(std::abs(projected.x-2)<1e-9);
 space.Reset();projected=space.Project(1,2,3,0,0,0,.01,2);assert(projected.x==0&&projected.y==0&&!space.ready);
 space.Reset();projected=space.Project(0,2,0,0,9.81,0,.01,2);assert(std::abs(projected.x-2)<1e-9&&projected.y==0);
 space.Reset();projected=space.Project(0,-2,0,0,9.81,0,.01,1);assert(std::abs(projected.x+2)<1e-9);
 space.Reset();projected=space.Project(0,0,2,0,-9.81,0,.01,1);assert(projected.x==0);
 space.Reset();projected=space.Project(2,0,0,9.81,0,0,.01,2);assert(std::abs(projected.x-2)<1e-9&&projected.y==0);
 projected=space.Project(0,2,0,0,9.81,0,.2,2);assert(projected.x==0&&projected.y==0&&!space.ready);
 projected=space.Project(0,2,0,std::numeric_limits<double>::quiet_NaN(),0,0,.01,2);assert(!space.ready);
 space.Reset();for(int i=0;i<1000;++i){projected=space.Project(.1,.2,.3,0,9.81,0,.004,2);assert(std::isfinite(projected.x)&&std::isfinite(projected.y));}
 assert(std::abs(std::hypot(space.up[0],std::hypot(space.up[1],space.up[2]))-1)<1e-12);
 GyroFilter f;
 for(auto curve:{"none","linear","relaxed","aggressive"}) {
   assert(GyroAccelerationGain(0,curve)==1.);
   assert(GyroAccelerationGain(30,curve)==1.);
   assert(GyroAccelerationGain(180,curve)==(std::string_view(curve)=="none"?1.:2.));
   double last=1.;for(int speed=0;speed<400;speed++) {
     const double gain=GyroAccelerationGain(speed,curve);assert(gain>=last&&gain<=2.);last=gain;
   }
 }
 assert(GyroAccelerationGain(90,"relaxed")<GyroAccelerationGain(90,"linear"));
 assert(GyroAccelerationGain(90,"linear")<GyroAccelerationGain(90,"aggressive"));
 assert(GyroAccelerationGain(90,"unknown")==1.);
 const auto fast=f.Update(0,3,0,.01,1,1,1,0,0,0,false,false,"linear");
 const auto fast_half=f.Update(0,3,0,.005,1,1,1,0,0,0,false,false,"linear");
 assert(std::abs(fast.x-fast_half.x*2)<1e-9);
 const auto turn=[&](double p,double y,double r,double dt,int axis=0,bool ix=false,bool iy=false){return f.Update(p,y,r,dt,1,1,1,0,0,axis,ix,iy);};
 auto v=turn(1,1,0,.01);assert(std::abs(v.x+.5729577951308232)<1e-10&&std::abs(v.y-v.x)<1e-10);
 // Rate integration is frame-rate independent without optional smoothing.
 double sum=0;for(int i=0;i<100;++i)sum+=turn(0,1,0,.01).x;
 double sum2=0;for(int i=0;i<50;++i)sum2+=turn(0,1,0,.02).x;
 assert(std::abs(sum-sum2)<1e-9);
 v=turn(0,0,1,.01,1);assert(v.x<0&&v.y==0);
 v=turn(0,1,1,.01,2);assert(v.x==0);
 v=turn(1,1,0,.01,0,true,true);assert(v.x>0&&v.y>0);
 v=turn(1,1,0,.2);assert(v.x==0&&v.y==0);
 v=turn(std::numeric_limits<double>::quiet_NaN(),0,0,.01);assert(v.x==0&&v.y==0);
 v=f.Update(0,.0001,0,.01,1,1,1,.1,0,0,false,false);assert(v.x==0);
 v=f.Update(0,1,0,.01,2,1,.5,0,0,0,false,false);assert(std::abs(v.x+.5729577951308232)<1e-10);
 // Prepared regression cases: do not confuse filter movement with frame polling.
 constexpr double radians=1./57.29577951308232;
 for(int hz:{100,250,1000}) {
   GyroFilter adaptive;
   double area=0.;
   for(int i=0;i<hz;++i)
     area+=adaptive.Update(0,3*radians,0,1./hz,1,1,1,0,.015,0,false,false,"none",true,10).x;
   // Draining the smoothing tail must preserve deliberate slow movement.
   for(int i=0;i<hz;++i)
     area+=adaptive.Update(0,0,0,1./hz,1,1,1,0,.015,0,false,false,"none",true,10).x;
   assert(std::abs(area+3.)<1e-9);
 }
 {
   GyroFilter adaptive;
   const auto direct=adaptive.Update(0,100*radians,0,.004,1,1,1,0,.015,0,false,false,"none",true,10);
   assert(std::abs(direct.x+.4)<1e-12); // Fast turns bypass smoothing immediately.
   adaptive.Reset();
   GyroFilter stronger;
   double filtered_energy=0.,stronger_energy=0.,raw_energy=0.;
   for(int i=0;i<1000;++i) {
     const double speed=i%2?12.:-12.;
     const auto vibration=adaptive.Update(0,speed*radians,0,.001,1,1,1,0,.015,0,false,false,"none",true,40);
     filtered_energy+=vibration.x*vibration.x;
     const auto enhanced=stronger.Update(0,speed*radians,0,.001,1,1,1,0,.0225,0,false,false,"none",true,50);
     stronger_energy+=enhanced.x*enhanced.x;
     raw_energy+=.012*.012;
   }
   assert(filtered_energy<raw_energy*.05);
   assert(stronger_energy<filtered_energy);
   adaptive.Reset();
   const auto disabled=adaptive.Update(0,12*radians,0,.001,1,1,1,0,0,0,false,false,"none",true,40);
   assert(std::abs(disabled.x+.012)<1e-12);
   adaptive.Reset();
   const auto invalid=adaptive.Update(0,12*radians,0,.2,1,1,1,0,.015,0,false,false,"none",true,40);
   assert(invalid.x==0&&adaptive.filtered.x==0);
 }
 std::cout<<"PASS: gyro units, integration, axes, inversions, threshold, sensitivity, stale/invalid reset, adaptive area, fast bypass, synthetic vibration attenuation\n";
}
