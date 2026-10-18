#include "../src/pdz_ammo_visibility.h"
#include <cassert>
#include <cstdio>
int main() {
  pdz::hud::AmmoVisibility timer;
  pdz::hud::LiveState s; s.weapon=1;s.loaded=8;s.capacity=8;s.reserve=20;
  assert(timer.Update(s,0,true,5)==1);
  assert(timer.Update(s,4.99,true,5)==1);
  assert(timer.Update(s,5.125,true,5)==.5f);
  assert(timer.Update(s,5.25,true,5)==0);
  --s.loaded; assert(timer.Update(s,6,true,5)==1); // firing
  assert(timer.Update(s,12,true,5)==0);
  ++s.reserve; assert(timer.Update(s,13,true,5)==1); // pickup
  s.ammo_activity=true; assert(timer.Update(s,30,true,5)==1); // empty fire/reload input
  s.ammo_activity=false; assert(timer.Update(s,36,true,5)==0);
  ++s.loaded; assert(timer.Update(s,37,true,5)==1); // reload completion
  assert(timer.Update(s,43,true,5)==0);
  ++s.secondary_loaded; assert(timer.Update(s,44,true,5)==1);
  assert(timer.Update(s,50,true,5)==0);
  ++s.weapon; assert(timer.Update(s,51,true,5)==1);
  assert(timer.Update(s,57,false,5)==1); // opt out
  assert(timer.Update(s,58,true,5)==1); // enabling gets grace period
  timer.Reset(); assert(timer.Update(s,100,true,5)==1); // resume/new mission
  assert(timer.Update(s,99,true,5)==1); // clock regression
  std::puts("PASS: ammo idle fade, firing, pickups, reload, secondary, equip, opt-out and reset");
}
