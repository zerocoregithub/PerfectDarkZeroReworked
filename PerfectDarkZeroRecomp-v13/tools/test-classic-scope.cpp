#include "../src/pdz_classic_scope.h"
#include <cassert>
#include <cstdio>
#include <string>
int main() {
  using namespace pdz::hud;
  for(bool fresh:{false,true})for(bool owner:{false,true})for(bool weapon:{false,true})
    for(bool aiming:{false,true})for(bool ads:{false,true})
      assert(ScopePresentationAllowed(fresh,owner,weapon,aiming,ads)==
             (fresh&&owner&&weapon&&(!aiming||ads)));
  assert(ShowAuxiliaryThirdPersonMarker(false,false));
  assert(ShowAuxiliaryThirdPersonMarker(false,true)); // stale aiming without scope
  assert(ShowAuxiliaryThirdPersonMarker(true,false)); // zoom-selection HUD
  assert(!ShowAuxiliaryThirdPersonMarker(true,true)); // actual scoped ADS
  // Captured inherited scope components40532/40533/40534 share ownerA07D0.
  assert(ValidScopeOwner(0x40532,0xA07D0,0xA07D0,4,true,0xA07D0));
  assert(!ValidScopeOwner(0x40532,0xA07D0,0xA07D0,3,true,0xA07D0));
  assert(!ValidScopeOwner(0x40532,0xA07D0,0xA0590,4,true,0xA07D0));
  assert(!ValidScopeOwner(0x40532,0xA07D0,0xA07D0,4,false,0xA07D0));
  assert(!ValidScopeOwner(0x40532,0xA07D0,0xA07D0,4,true,0xA0590));
  for(auto name:{"sniperrifle","magsec","superdragon","fac16","dwp5","shockwave","rcp90"})
    for(auto prefix:{"aid_darkxui_ingame_small_scopes_","aid_darkxui_ingame_large_scopes_"})
      { assert(ConventionalScopeResource(std::string(prefix)+name));
        assert(ConventionalScopeResource(std::string("DarkXUIAsset://")+prefix+name+"_xur")); }
  assert(ScopeResourceName("DarkXUIAsset://aid_darkxui_ingame_large_scopes_magsec_xur")
         =="aid_darkxui_ingame_large_scopes_magsec");
  for(auto name:{"threatdetector","camspyHUD","soundalyser","guidedrocket","basicscope"})
    assert(!ConventionalScopeResource(std::string("aid_darkxui_ingame_large_scopes_")+name));
  for(auto prefix:{"aid_darkxui_ingame_small_scopes_","aid_darkxui_ingame_large_scopes_"}) {
    const auto basic=std::string("DarkXUIAsset://")+prefix+"basicscope_xur";
    assert(VerifiedGenericScopeResource(basic,2));
    for(auto kind:{0u,20u,UINT32_MAX}) assert(!VerifiedGenericScopeResource(basic,kind));
    assert(!VerifiedGenericScopeResource(std::string(prefix)+"rcp90",2));
  }
  auto hip=ClassicScope(2048,1152,1,false,{1024,576});
  auto ads=ClassicScope(2048,1152,1,true,{1024,576});
  assert(hip.triangles.size()==16&&ads.triangles.size()==40);
  for(auto opacity:{0,64,128,255}) {
    const Color tint{213,61,147,uint8_t(opacity)};
    for(bool aiming:{false,true})
      for(const auto& triangle:ClassicScope(1920,1080,.5f,aiming,{960,540},tint).triangles)
        for(const auto& vertex:{triangle.a,triangle.b,triangle.c}) {
          assert(vertex.color.r==tint.r&&vertex.color.g==tint.g&&vertex.color.b==tint.b);
          assert(vertex.color.a==tint.a);
        }
  }
  assert(std::abs(hip.triangles[0].a.point.x-(307.2f-2.4f))<.001f);
  auto preview=ClassicScope(2048,1152,.25f,false,{1024,576});
  assert(preview.triangles[0].a.point.x>hip.triangles[0].a.point.x);
  auto moved=ClassicScope(2048,1152,1,true,{800,400});
  assert(moved.triangles[16].a.point.x==ads.triangles[16].a.point.x);
  assert(moved.triangles[16].b.point.x<ads.triangles[16].b.point.x);
  // Every stroke rectangle has disjoint interior: alpha must blend once only.
  for(float height:{240.f,720.f,1080.f,1152.f}) {
    const auto width=height*16.f/9.f;
    const auto box=ClassicScope(width,height,1,true,{width*.5f+.25f,height*.5f+.75f});
    assert(box.triangles[26].b.point.x==box.triangles[30].b.point.x);
    assert(box.triangles[26].a.point.y==box.triangles[30].c.point.y);
    for(size_t i=0;i<box.triangles.size();i+=2)
      for(size_t j=i+2;j<box.triangles.size();j+=2) {
        const auto& a=box.triangles[i];const auto& b=box.triangles[j];
        assert(std::min(a.c.point.x,b.c.point.x)<=std::max(a.a.point.x,b.a.point.x)||
               std::min(a.c.point.y,b.c.point.y)<=std::max(a.a.point.y,b.a.point.y));
      }
    const float x=width*.5f+.25f,y=height*.5f+.75f;
    assert(std::abs((x-box.triangles[32].a.point.x)-(box.triangles[34].b.point.x-x))<.001f);
    assert(std::abs((y-box.triangles[36].a.point.y)-(box.triangles[38].c.point.y-y))<.001f);
  }
  for(float width:{1280.f,1920.f,3440.f}) for(float height:{720.f,1080.f,1440.f})
    for(float ratio:{0.f,.01f,.5f,1.f})
      for(const auto& t:ClassicScope(width,height,ratio,true,{width*.4f,height*.3f}).triangles)
        for(const auto& p:{t.a.point,t.b.point,t.c.point}) assert(std::isfinite(p.x)&&std::isfinite(p.y));
  assert(ClassicScope(NAN,1080,1,true,{0,0}).triangles.empty());
  std::puts("PASS: scope resource isolation, widescreen geometry, preview sizing and moving tether endpoints");
}
