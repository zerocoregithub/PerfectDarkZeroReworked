#include "../src/pdz_weapon_controls.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
  // Exhaustive button preservation and LT boundary: only left/right consumed.
  for (unsigned buttons = 0; buttons <= 0xffff; ++buttons) {
    const auto b = uint16_t(buttons);
    assert(pdz::RouteZoomButtons(b, 29, true, true) == b);
    assert(pdz::RouteZoomButtons(b, 255, false, true) == b);
    assert(pdz::RouteZoomButtons(b, 255, true, false) == b);
    assert(pdz::RouteZoomButtons(b, 30, true, true) == uint16_t(b & ~12u));
    assert(pdz::RouteZoomButtons(b, 255, true, true) == uint16_t(b & ~12u));
  }
  pdz::WorldSwapHold hold;
  using Action = pdz::WeaponButtonAction;
  for (int i = 0; i < 20; ++i) assert(hold.Update(true, true, true, 7, .01) == Action::None);
  assert(hold.Update(true, true, false, 7, .01) == Action::CycleCarried);
  assert(hold.Update(true, true, false, 7, .01) == Action::None);
  for (int i = 0; i < 34; ++i) assert(hold.Update(true, true, true, 7, .01) == Action::None);
  assert(hold.Update(true, true, true, 7, .01) == Action::SwapWorld);
  assert(hold.Update(true, true, true, 7, .1) == Action::None);
  assert(hold.Update(true, true, false, 7, .01) == Action::None);
  assert(hold.Update(true, true, true, 7, .2) == Action::None);
  assert(hold.Update(true, true, true, 8, .2) == Action::None);
  assert(hold.Update(true, true, true, 8, .2) == Action::None);
  assert(hold.Update(true, true, false, 8, .01) == Action::CycleCarried);
  hold.Update(true, true, false, 0, .01);
  assert(hold.Update(true, true, true, 0, .2) == Action::CycleCarried);
  assert(hold.Update(true, true, true, 7, .2) == Action::None);
  assert(hold.Update(true, true, false, 7, .01) == Action::None);
  assert(hold.Update(true, true, true, 7, .1) == Action::None);
  hold.Update(true, false, true, 7, .01);
  assert(hold.Update(true, true, true, 7, .2) == Action::None);
  assert(hold.Update(true, true, false, 7, .01) == Action::None);
  assert(hold.Update(false, true, true, 7, .01) == Action::Original);
  assert(hold.Update(true, true, true, 7, .1) == Action::None);
  assert(hold.Update(true, true, true, 7, std::numeric_limits<double>::quiet_NaN()) == Action::None);

  pdz::DpadZoom zoom;
  // Vanilla must disable BOTH producers. Every native analog pressure passes
  // through unchanged, including after an earlier custom-zoom gesture.
  for(double pressure:{0.,.1,.33,.5,.75,1.})
    assert(zoom.Update(false,true,true,false,false,pressure,.016)==pressure);
  assert(zoom.Update(false, true, true, false, true, .4, .01) == .4);
  assert(zoom.Update(true, false, true, false, true, .4, .01) == .4);
  const auto tick=[&](bool left,bool right) {return zoom.Update(true,true,true,left,right,0,.016,0);};
  assert(tick(false,false)==2./3.);
  for(double pressure:{0.,.1,.33,.5,.75,1.})
    assert(zoom.Update(false,true,true,false,false,pressure,.016)==pressure);
  assert(tick(false,false)==2./3.);
  assert(tick(false,true)==1.);
  for(int i=0;i<100;++i) assert(tick(false,true)==1.); // No hold repeat.
  tick(false,false); assert(tick(false,true)==1.);
  tick(false,false); assert(tick(false,true)==1.);
  assert(tick(true,false)==2./3.);
  tick(false,false); assert(tick(true,false)==1./3.);
  tick(false,false); assert(tick(true,false)==1./3.);
  assert(tick(true,true)==1./3.);
  tick(false,false); tick(false,true); tick(false,false); tick(false,true);
  for (int i = 0; i < 5; ++i) {
    assert(zoom.Update(true, false, true, false, false, 0, .01) == 0);
    assert(tick(false,false)==1.); // Stage survives ADS release.
  }
  zoom.EndGesture(); assert(tick(false,false)==1.);
  assert(zoom.Update(true, true, false, false, false, .7, .01) == .7);
  assert(tick(false,false)==1.);
  zoom.Update(true, true, true, false, false, 0, .5);
  assert(tick(false,false)==1.);
  assert(zoom.Update(false, true, true, false, false, .2, .01) == .2);
  assert(tick(false,false)==1.);
  for(int hz:{30,60,120,240}) {
    pdz::DpadZoom smooth;
    const auto first=smooth.Update(true,true,true,false,false,0,1./hz,.08);
    assert(first>0&&first<2./3.);
    for(int i=1;i<hz;++i) smooth.Update(true,true,true,false,false,0,1./hz,.08);
    assert(std::abs(smooth.position-(2./3.)*(1-std::exp(-12.5)))<1e-10);
    const auto before=smooth.position;
    const auto next=smooth.Update(true,true,true,false,true,0,1./hz,.08);
    assert(next>before&&next<1.);
    smooth.EndGesture(); assert(smooth.stage==3);
  }
  assert(!pdz::DualWeapons(0,0)); assert(!pdz::DualWeapons(1,0));
  assert(!pdz::DualWeapons(0,1)); assert(!pdz::DualWeapons(1,1));
  assert(pdz::DualWeapons(1,2));
  pdz::WeaponZoomMemory memory;
  auto& first=memory.Select(100,10);
  first.Update(true,true,true,false,false,0,.01,0,2);
  assert(first.stage==3);
  auto& second=memory.Select(100,20);
  assert(second.Update(true,true,true,false,false,0,.01,0)==2./3.);
  second.Update(true,true,true,true,false,0,.01,0);
  assert(second.stage==1); // Manual one-third remains available.
  second.Update(true,true,true,false,true,0,.01,0);
  assert(second.stage==2);
  auto& restored=memory.Select(100,10);
  assert(restored.stage==3&&!restored.active&&restored.position==0);
  const double eased=restored.Update(true,true,true,false,false,0,.01,.08);
  assert(eased>0&&eased<1); // Smooth return toward remembered maximum.
  assert(memory.Select(100,20).stage==2);
  memory.Select(100,0); // Temporary unequip/gadget interval must not erase guns.
  assert(memory.Select(100,10).stage==3);
  assert(memory.Select(200,10).stage==2); // New player/root lifetime.
  assert(memory.stages.empty());
  memory.Select(200,10).stage=3;
  memory.Reset();
  assert(memory.Select(200,10).stage==2); // Mission exit invalidates session.
  std::puts("PASS: weapon swap regression, three zoom stages, one press per stage, ADS memory, easing and dual-hand detection");
}
