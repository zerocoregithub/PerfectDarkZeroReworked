#include "../src/pdz_locktopus_mouse.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main() {
  pdz::LocktopusMouse s;
  double x=0,y=0;
  auto step=[&](int t,double mx,double my,bool active=true,uint32_t id=1,uint64_t epoch=1) {
    return s.Update(id,epoch,t,active,mx,my,x,y);
  };
  assert(!step(0,900,900)); // Discard pre-entry motion.
  assert(step(16,1025,837.5)); assert(x==.5&&y==.25);
  x=y=0; assert(step(32,1025,837.5)); assert(x==.5&&y==.25); // Hold.
  x=y=0; assert(step(48,5000,-5000)); assert(std::abs(std::hypot(x,y)-1)<1e-12);
  x=0.8;y=0; assert(!step(64,5000,-5000)); assert(x==.8&&y==0); // Stick wins.
  x=y=0; assert(!step(80,5000,-5000)); // No surprise reclaim on release.
  assert(step(96,5025,-5000)); assert(std::abs(x-.1)<1e-12&&y==0);
  assert(!step(112,5025,-5000,false));
  x=y=0;assert(!step(128,9000,9000)); // Reentry reset.
  assert(!step(144,9000,9000,true,2)); // New puzzle.
  assert(!step(160,9000,9000,true,2,2)); // Focus/capture epoch.
  assert(!step(400,9000,9000,true,2,2)); // Stale lease.
  assert(!step(399,9000,9000,true,2,2)); // Clock rollback.
  assert(!step(415,std::numeric_limits<double>::infinity(),0,true,2,2));
  assert(!s.initialized);
  s.Reset(); x=.8;y=0; assert(!step(500,0,0));
  assert(!step(516,50,0)); assert(x==.8); // Held stick cannot be overridden.
  assert(!step(532,100,0)); assert(x==.8);
  std::puts("PASS: Locktopus position/hold, radial bound, native takeover, entry/identity/focus/stale/invalid resets");
}
