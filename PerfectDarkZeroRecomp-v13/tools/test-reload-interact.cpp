#include "../src/pdz_reload_interact.h"
#include <cassert>
#include <iostream>
int main() {
  pdz::input::ReloadInteract route;
  auto poll=[&](uint16_t buttons,int64_t time,bool reload=false) {
    return route.Route(buttons,1,true,true,true,reload,time);
  };
  assert(poll(0x4000,1000,true)==0x4000);
  assert(poll(0x4000,1010,false)==0x4000);
  poll(0,1020);
  assert(poll(0x4000,1100)==0x1000);
  for(int i=0;i<100;++i)assert(poll(0x4000,1100+i)==0x1000);
  assert(poll(0x4000,1200)==0);
  assert(poll(0x5000,1210)==0x1000);
  poll(0,1220);
  assert(poll(0x4000,1300)==0x1000);
  assert(poll(0,1301)==0x1000);
  assert(poll(0,1400)==0);
  assert(route.Route(0x4000,1,true,true,false,false,1500)==0x4000);
  assert(route.Route(0x4000,1,false,true,true,false,1600)==0x4000);
  assert(route.Route(0x4000,1,true,false,true,false,1700)==0x4000);
  assert(route.Route(0x4000,2,true,true,true,false,1800)==0x1000);
  std::cout<<"PASS: timed tap survives repeated polls and early release; reload latch, physical A, held-X cover prevention, unknown/menu/reset fallback\n";
}
