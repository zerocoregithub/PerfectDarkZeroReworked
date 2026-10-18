#include "../src/pdz_input_math.h"
#include "../src/pdz_weapon_controls.h"
#include <rex/input/mnk/binding_math.h>
#include <cassert>
#include <iostream>
#include <limits>
int main(){
 using namespace pdz::input;
 for(uint8_t want=0;want<8;++want)for(uint8_t live=0;live<8;++live) {
   assert(rex::input::mnk::BindingModifiersMatch(want,live,'W')==(want==live));
   for(int i=0;i<3;++i)assert(rex::input::mnk::BindingModifiersMatch(want,live,uint16_t(0x10+i))==(uint8_t(want|(1<<i))==live));
 }
 assert(Curve({.05,0},.08,.98,false).x==0);
 for(int i=0;i<360;++i){double a=i*3.141592653589793/180.;auto p=Curve({std::cos(a),std::sin(a)},.08,.98,false);assert(std::abs(std::hypot(p.x,p.y)-1)<1e-10);}
 assert(std::abs(Curve({.53,0},.08,.98,false).x-.5)<1e-10);
 assert(std::abs(Curve({.53,0},.08,.98,true).x-.25)<1e-10);
 assert(Curve({1,0},.5,.4,false).x==0);
 assert(Curve({std::numeric_limits<double>::quiet_NaN(),0},0,1,false).x==0);
 std::array<int,16> map;for(int i=0;i<16;++i)map[i]=i;
 for(int b=0;b<65536;++b){auto result=Remap({uint16_t(b),127,255},map);assert(result.bits==uint16_t(b&~2048));assert(result.lt==127&&result.rt==255);}
 map[10]=14;map[14]=10;auto swapped=Remap({4096,0,0},map);assert(swapped.lt==255&&swapped.bits==0);
 swapped=Remap({0,29,0},map);assert(swapped.bits==0);swapped=Remap({0,30,0},map);assert(swapped.bits==4096);
 map.fill(-1);assert(Remap({65535,255,255},map).bits==1024);
 for(int fps:{30,60,120,240}){Response r;for(int i=0;i<fps;++i)r.Update({1,0},1./fps,.04,0);assert(std::abs(r.value.x-(1-std::exp(-25)))<1e-10);}
 Response r;assert(r.Update({1,0},.016,0,0).x==1);for(int i=0;i<20;++i)r.Update({1,0},.016,0,1);assert(r.Update({1,0},.016,0,1).x==2);assert(r.Update({},.016,0,1).x==0);
 pdz::DpadZoom z;assert(z.Update(true,true,true,false,false,0,.016,0)==2./3.);
 assert(z.Update(true,true,true,false,false,0,.016,0,1)==1.);
 assert(z.Update(true,true,true,false,false,0,.016,0,1)==1.);
 assert(z.Update(true,true,true,false,false,0,.016,0,-1)==2./3.);
 z.Update(true,false,true,false,false,0,.016,0,-1);assert(z.stage==2);
 std::cout<<"PASS: radial/relaxed curves, all button masks, trigger remaps, frame-rate-independent easing, acceleration, wheel zoom\n";
}
