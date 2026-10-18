#include "../src/pdz_weapon_sway_math.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
  double input_yaw=0,input_pitch=0;
  pdz::WeaponSway input_sway;
  assert(input_sway.Update(0,0,.016,1));
  pdz::AccumulateSwayInput(1,1,input_yaw,input_pitch);
  assert(input_yaw>0&&input_pitch<0);
  assert(input_sway.Update(input_yaw,input_pitch,.016,1));
  assert(input_sway.horizontal<0&&input_sway.vertical>0);
  pdz::AccumulateSwayInput(-1,-1,input_yaw,input_pitch);
  assert(std::abs(input_yaw)<1e-12&&std::abs(input_pitch)<1e-12);
  double reference=0;
  for (int hz : {30,60,120,240}) {
    pdz::WeaponSway s;
    assert(s.Update(0,0,1./hz,1));
    for (int i=1;i<=hz;++i) assert(s.Update(i*.2/hz,0,1./hz,1));
    assert(s.horizontal<0 && std::abs(s.horizontal)<=.06);
    if (hz==30) reference=s.horizontal;
    assert(std::abs(s.horizontal-reference)<1e-10);
    for (int i=0;i<hz;++i) assert(s.Update(.2,0,1./hz,1));
    assert(std::abs(s.horizontal)<1e-8);
  }
  pdz::WeaponSway s;
  assert(s.Update(3.14,0,.01,1));
  assert(s.Update(-3.14,0,.01,1));
  assert(std::abs(s.horizontal)<.01);
  assert(s.Update(-1,0,.01,1));
  assert(s.horizontal==0);
  assert(!s.Update(0,0,std::numeric_limits<double>::quiet_NaN(),1));
  assert(!s.initialized);
  assert(s.Update(0,0,.01,0));
  assert(s.Update(.1,.1,.01,0));
  assert(s.horizontal==0 && s.vertical==0);
  s.Reset();
  assert(s.Update(0,0,.01,4));
  for(int i=1;i<=100;++i) assert(s.Update(i*.1,i*.02,.01,4));
  assert(std::abs(s.horizontal)>.12 && std::abs(s.horizontal)<=.24);
  assert(std::abs(s.vertical)<=.24);
  s.Reset();
  assert(s.Update(0,0,.01,8));
  for(int i=1;i<=100;++i) assert(s.Update(i*.1,i*.02,.01,8));
  assert(std::abs(s.horizontal)>.24 && std::abs(s.horizontal)<=.48);
  assert(std::abs(s.vertical)<=.48);
  assert(!s.Update(0,0,.01,8.01));
  std::cout<<"Weapon sway: all tests passed\n";
}
