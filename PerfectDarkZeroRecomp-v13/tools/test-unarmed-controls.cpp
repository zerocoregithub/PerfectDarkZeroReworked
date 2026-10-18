#include "../src/pdz_unarmed_controls.h"
#include "../src/pdz_movement_math.h"
#include "../src/pdz_input_math.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <string>
int main() {
  using namespace pdz;
  for(int fps:{30,60,120,240}) {
    ClassicRunState state;
    double scale=0;
    for(int i=0;i<fps*3;++i)scale=ClassicRunScale(state,1./fps,1,0,true);
    assert(std::abs(scale-.8)<1e-10);
    for(int i=0;i<fps;++i)scale=ClassicRunScale(state,1./fps,1,0,true);
    assert(std::abs(scale-1.25)<1e-10);
    assert(std::abs(ClassicRunScale(state,1./fps,1,0,false)-1)<1e-10);
    for(int i=0;i<fps;++i)scale=ClassicRunScale(state,1./fps,0,1,true);
    assert(std::abs(scale-.8)<1e-10);
  }
  HolsterHold h;
  assert(h.Route(0,true,true,false,1,100)==0);
  assert(h.Route(0x8000,true,true,false,1,110)==0);
  assert(h.Route(0x8000,true,true,false,1,110)==0); // Repeated polls, same clock.
  assert(h.Route(0,true,true,false,1,200)==0x8000);
  assert(h.Route(0,true,true,false,1,220)==0x8000);
  assert(h.Route(0,true,true,false,1,250)==0);
  assert(h.Route(0x8000,true,true,false,1,300)==0);
  assert(h.Route(0x8000,true,true,false,1,500)==0);
  assert(h.Route(0x8000,true,true,false,1,700)==0);
  assert(h.Route(0x8000,true,true,false,1,800)==1);
  assert(h.Route(0x8000,true,true,false,1,800)==1);
  assert(h.Route(0x8000,true,true,false,1,860)==0);
  assert(h.Route(0,true,true,true,1,900)==0);
  // Already holstered: consume hold, never synthesize Up to re-equip.
  assert(h.Route(0x8000,true,true,true,1,1000)==0);
  for(int t=1100;t<=1600;t+=100)assert(h.Route(0x8000,true,true,true,1,t)==0);
  assert(h.Route(0,true,true,true,1,1610)==0);
  // Menus, disable, session change, stale polling, and release-at-threshold.
  assert(h.Route(0x8000,true,false,false,1,1700)==0x8000);
  assert(h.Route(0x8000,true,true,false,1,1800)==0);
  assert(h.Route(0,true,true,false,1,1900)==0);
  assert(h.Route(0x8000,false,true,false,1,2000)==0x8000);
  assert(h.Route(0x8000,true,true,false,2,2100)==0);
  assert(h.Route(0,true,true,false,2,2200)==0);
  h.Route(0x8000,true,true,false,2,2300);
  assert(h.Route(0x8000,true,true,false,2,2700)==0); // Stall cancels.
  h.Route(0,true,true,false,2,2710);
  h.Route(0x8000,true,true,false,2,2800);
  h.Route(0x8000,true,true,false,2,3000);
  h.Route(0x8000,true,true,false,2,3200);
  assert(h.Route(0,true,true,false,2,3300)==1);
  for(unsigned b=0;b<65536;++b) {
    HolsterHold fresh;
    assert(fresh.Route(uint16_t(b),false,true,false,1,1)==b);
    assert(fresh.Route(uint16_t(b),true,false,false,1,2)==b);
    assert((fresh.Route(uint16_t(b),true,true,false,1,3)&~0x8000u)==(b&~0x8000u));
  }
  assert(VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststandidle"));
  for(uint32_t mask:{0u,1u,2u,3u,0xffu,0x47010101u}) {
    assert(FistViewMask(mask,false)==mask);
    assert(FistViewMask(mask,true)==(mask&~1u));
    assert((FistViewMask(mask,true)&~1u)==(mask&~1u));
  }
  assert(VerifiedIdleFists("aid_anim_chars_overlay_fists_fistcrouchidle"));
  assert(!VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststandleftpunch"));
  assert(!VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststandrightpunch"));
  assert(!VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststanddisarm"));
  assert(!VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststandradius"));
  assert(!VerifiedIdleFists("aid_anim_chars_overlay_fists_fiststandidle_extra"));
  assert(!VerifiedIdleFists(""));
  for(auto clip:{"walk","jog","run2","unarmedsprint","backwardswalk",
                 "backwardsjog","backwardsrun","strafeleft","straferight",
                 "strafeleftfast","straferightfast"}) {
    const auto name=std::string("aid_anim_chars_movement_female_walks_")+clip;
    for(uint8_t channel=1;channel<=4;++channel)assert(VerifiedUnarmedLocomotion(name,channel));
    for(uint8_t channel:{0,5,30,31,32})assert(!VerifiedUnarmedLocomotion(name,channel));
    assert(!VerifiedUnarmedLocomotion(name+"_attack",1));
  }
  for(auto clip:{"aid_anim_chars_overlay_fists_fiststandleftpunch",
                 "aid_anim_chars_overlay_fists_fiststandrightpunch",
                 "aid_anim_chars_overlay_fists_fiststanddisarm",
                 "aid_anim_chars_overlay_fists_fiststandradius",""})
    for(uint8_t channel:{1,2,3,4,32})assert(!VerifiedUnarmedLocomotion(clip,channel));
  const std::string_view shared_walks[]={
    "aid_anim_chars_movement_male_walks_AimWalks_AimStandWalk",
    "aid_anim_chars_movement_male_walks_AimWalks_AimStandJog",
    "aid_anim_chars_movement_male_walks_AimWalks_AimStandRun"};
  for(uint8_t i=0;i<3;++i) {
    for(uint8_t channel:{0,1,2,3,4,30,31,32})
      assert(VerifiedUnarmedLocomotion(shared_walks[i],channel)==(channel==i+1));
    assert(!VerifiedUnarmedLocomotion(std::string(shared_walks[i])+"_attack",i+1));
  }
  assert(BobAmplitude(12,4,1)==12);
  assert(BobAmplitude(12,4,.5)==8);
  assert(BobAmplitude(12,4,0)==4);
  assert(BobAmplitude(12,4,-1)==12);
  assert(!ShowUnarmedHands(8,true,false,true,.2,false));
  assert(ShowUnarmedHands(8,true,true,true,.2,false));
  assert(ShowUnarmedHands(8,true,false,true,.2,true));
  for(unsigned action:{1u,2u,3u,4u,5u,12u,23u,25u}) {
    assert(ShowUnarmedHands(action,true,false,true,.99,false));
    assert(!ShowUnarmedHands(action,true,false,true,1,false));
    assert(ShowUnarmedHands(action,false,false,true,1,false));
    assert(ShowUnarmedHands(action,true,false,false,1,false));
    assert(ShowUnarmedHands(action,true,false,true,std::numeric_limits<double>::quiet_NaN(),false));
  }
  // Separate menu mapping is selected from raw buttons, not chained on top
  // of gameplay: A->B in gameplay, A->X in menus, keyboard untouched.
  std::array<int,16> gameplay,menu;
  for(int i=0;i<16;++i)gameplay[i]=menu[i]=i;
  gameplay[10]=11;menu[10]=12;
  assert(pdz::input::Remap({4096,0,0},gameplay).bits==8192);
  assert(pdz::input::Remap({4096,0,0},menu).bits==16384);
  std::puts("PASS: 125% unarmed ramp/armed identity, hold-Y clock/routing, bob amplitude, complete hand actions, independent menu remap");
}
