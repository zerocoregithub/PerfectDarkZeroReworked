#include "../src/pdz_classic_hud.h"
#include "../src/pdz_hud_bridge.h"
#include <cassert>
#include <limits>
#include <cstdio>
static void Check(const pdz::hud::Mesh& mesh) {
  assert(mesh.triangles.size()<=64);
  for(const auto& triangle:mesh.triangles)
    for(const auto& v:{triangle.a,triangle.b,triangle.c}) {
      assert(std::isfinite(v.point.x)&&std::isfinite(v.point.y));
      assert(v.color.a<=160);
    }
}
int main() {
  const auto root="aid_darkxui_ingame_hud_HD_1player";
  for(auto id:{"Radar","textHint","weapon swap","weapon inventory","AmmoBarL","barAction"})
    assert(pdz::hud::NativeHalfSizeElement(id,root));
  for(auto id:{"textSubtitle","labelCutsceneSubtitle","XuiSceneBlank",""})
    assert(!pdz::hud::NativeHalfSizeElement(id,root));
  assert(!pdz::hud::NativeHalfSizeElement("ItemToPickupR","weapon swap"));
  assert(!pdz::hud::NativeHalfSizeElement("textHint","PauseMenu"));
  assert(pdz::hud::HalfSizePosition(100,1,80,0)==110);
  assert(pdz::hud::HalfSizePosition(100,1,80,40)==100);
  assert(pdz::hud::HalfSizePosition(100,2,80,0)==120);
  assert(pdz::hud::StackAnchor("textScrollerNotification5")=="textScrollerNotification1");
  assert(pdz::hud::StackAnchor("AmmoMetersR")=="AmmoBarR");
  assert(pdz::hud::StackAnchor("textSubtitle").empty());
  bool solid=false,recovery=false,empty_health=false;
  for(const auto& triangle:pdz::hud::Health(.2f,0,.65f).triangles) {
    const auto color=triangle.a.color;
    if(color.r||color.g) {solid|=color.a==96;recovery|=color.a==32;}
    else empty_health|=color.a==128;
  }
  assert(solid&&recovery&&empty_health);
  for(const auto& triangle:pdz::hud::Health(.65f,0,.65f).triangles)
    assert(triangle.a.color.a!=32);
  for(int i=0;i<=100;++i) Check(pdz::hud::Health(float(i)/100,.5f,.8f));
  assert(pdz::hud::HealthOpacity(1000,0,1)==0);
  assert(pdz::hud::HealthOpacity(1000,950,.5f)==.5f);
  assert(pdz::hud::HealthOpacity(1000,879,1)==0);
  assert(pdz::hud::HealthOpacity(1000,1001,1)==0);
  assert(pdz::hud::HealthOpacity(1000,950,NAN)==0);
  assert(pdz::hud::HealthOpacity(1000,950,2)==1);
  for(auto id:{"textObjectiveTitle","textObjectiveInfo","textObjectiveMessages","imageObjectiveMessages","HUD_objectiveinfo","HUD_objectivetitle"})
    assert(pdz::hud::ObjectiveElement(id));
  for(auto id:{"barAction","textSubtitle","ScopeZoomLabel","ItemToPickupR","textObjectiveTitleOther"})
    assert(!pdz::hud::ObjectiveElement(id));
  assert(pdz::hud::InteractionPromptElement("textAction"));
  assert(pdz::hud::InteractionPromptElement("textContext"));
  for(auto id:{"barAction","textContextOther","textGadgetContext","textSubtitle",
               "ItemToPickupR","textActionOther",""})
    assert(!pdz::hud::InteractionPromptElement(id));
  const pdz::hud::Color green{0,192,0,160}, amber{255,176,48,160};
  auto square=pdz::hud::Crosshair(0,4,2,green);
  assert(square.triangles.size()==2);
  assert(square.triangles[0].a.point.x==-1&&square.triangles[0].a.point.y==-1);
  assert(square.triangles[0].c.point.x==1&&square.triangles[0].c.point.y==1);
  assert(pdz::hud::Crosshair(3,4,2,green).triangles.size()==8);
  assert(pdz::hud::Crosshair(0,4,2,green,1).triangles.size()==4);
  assert(pdz::hud::Crosshair(NAN,4,2,green).triangles.empty());
  for(int i=0;i<=32;++i) Check(pdz::hud::Crosshair(float(i),4,2,green,1));
  for(auto& t:pdz::hud::Ammo(3,10,6,70,amber).triangles)
    if(t.a.color.r) assert(t.a.color.r==255&&t.a.color.g==176&&t.a.color.b==48);
  for(auto id:{"barHealth","barMaxHealth","barArmour","AmmoBarL","AmmoBarR","AmmoMetersL","AmmoMetersR"})
    assert(pdz::hud::ReplacedElement(id));
  for(auto id:{"barAction","HudRadar","textSubtitle","ItemToPickupR","LevelRegionLabel","","barHealthOther","AmmoBarRight"})
    assert(!pdz::hud::ReplacedElement(id));
  for(int i=-20;i<=120;++i) Check(pdz::hud::Health(float(i)/100,float(100-i)/100));
  Check(pdz::hud::Health(std::numeric_limits<float>::quiet_NaN(),INFINITY));
  for(int capacity=1;capacity<=100;++capacity)
    for(int ammo=-1;ammo<=capacity+1;++ammo) Check(pdz::hud::Ammo(ammo,capacity));
  assert(pdz::hud::Ammo(1,0).triangles.empty());
  assert(pdz::hud::Ammo(1,10,-1).triangles.empty());
  assert(pdz::hud::Ammo(3,10).triangles.size()==20);
  assert(pdz::hud::Ammo(30,60).triangles.size()==4);
  auto pistol=pdz::hud::Ammo(8,8);
  assert(pistol.triangles.size()==16);
  assert(pistol.triangles[0].b.point.x==9);
  assert(pistol.triangles[0].c.point.y-pistol.triangles[0].a.point.y>6);
  for(uint32_t limit:{8u,32u,180u,800u})for(uint32_t n=0;n<=limit+1;++n) {
    auto reserve=pdz::hud::Reserve(n,limit);Check(reserve);
    assert(reserve.triangles.size()<=4); // Never turns into separate reserve blocks.
  }
  assert(pdz::hud::Reserve(2,0).triangles.empty());
  auto reserve=pdz::hud::Reserve(2,180);
  assert(reserve.triangles[0].a.point.y==0);
  assert(std::abs(reserve.triangles[0].c.point.y-.4f)<.001f);
  assert(pdz::hud::FunctionMarker(false).triangles[0].a.color.r==224);
  assert(pdz::hud::FunctionMarker(false).triangles[0].a.color.g==0);
  assert(pdz::hud::FunctionMarker(true).triangles[0].a.color.g==184);
  auto empty=pdz::hud::Ammo(0,10), full=pdz::hud::Ammo(10,10);
  for(auto& t:empty.triangles) assert(t.a.color.g==0);
  for(auto& t:full.triangles) assert(t.a.color.g==192);
  std::puts("PASS: Classic HUD geometry, visibility expiry/fade, exact HUD/objective ID filters, amber ammo, cross/square-dot and invalid input guards");
}
