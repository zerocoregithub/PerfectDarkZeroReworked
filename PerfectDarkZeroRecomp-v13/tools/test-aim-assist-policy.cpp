#include "../src/pdz_aim_assist_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
  using namespace pdz::aim;
  for(bool overlay:{false,true})for(bool native:{false,true})for(bool scoped:{false,true})
    assert(UseNativePolicy(overlay,native,scoped)==(!overlay&&native&&!scoped));
  for(auto native:{0u,1u,2u,0xffffffffu}) {
    assert(OverlayAimMode(native,true,true,false)==1);
    assert(OverlayAimMode(native,true,false,false)==0);
    assert(OverlayAimMode(native,true,true,true)==0);
    assert(OverlayAimMode(native,true,false,true)==0);
    for(bool enabled:{false,true})for(bool scoped:{false,true}) {
      assert(OverlayAimMode(native,false,enabled,scoped)==native);
      for(double motion:{0.,.001,.25,1.,2.}) {
        assert(OverlayAimStrength(motion,false,enabled,scoped)==motion);
        if(enabled&&!scoped)assert(OverlayAimStrength(motion,true,enabled,scoped)==1.);
        else assert(OverlayAimStrength(motion,true,enabled,scoped)==motion);
      }
    }
  }
  puts("PASS: overlay policy overrides native on/off; scoped attraction disabled; unrelated contexts unchanged; stationary/moving strength consistent.");
}
