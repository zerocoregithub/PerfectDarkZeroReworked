#pragma once
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <cmath>
namespace pdz::hud {
inline bool NativeHalfSizeElement(std::string_view id,std::string_view parent) {
  // Scale top-level HUD groups once; children inherit their parent's transform.
  return parent=="aid_darkxui_ingame_hud_HD_1player" &&
      !id.empty() && id!="textSubtitle" && id!="labelCutsceneSubtitle" &&
      id!="XuiSceneBlank";
}
inline float HalfSizePosition(float position,float scale,float extent,float pivot) {
  return position+scale*.25f*(extent*.5f-pivot);
}
inline std::string_view StackAnchor(std::string_view id) {
  if(id=="AmmoMetersL") return "AmmoBarL";
  if(id=="AmmoMetersR") return "AmmoBarR";
  if(id=="textScrollerNotification1"||id=="textScrollerNotification2"||
     id=="textScrollerNotification3"||id=="textScrollerNotification4"||id=="textScrollerNotification5")
    return "textScrollerNotification1";
  if(id=="textScrollerInfo1"||id=="textScrollerInfo2"||id=="textScrollerInfo3"||
     id=="textScrollerInfo4"||id=="textScrollerInfo5") return "textScrollerInfo1";
  return {};
}
inline float HealthOpacity(int64_t now,int64_t last,float alpha) {
  if(!last||now<last||now-last>120||!std::isfinite(alpha)) return 0.f;
  return std::clamp(alpha,0.f,1.f);
}
struct LiveState {
  float health = 0, shield = 0;
  float health_alpha = 0;
  float recoverable_health = 0;
  uint32_t loaded = 0, capacity = 0, reserve = 0;
  uint32_t secondary_loaded = 0, secondary_capacity = 0, secondary_reserve = 0;
  uint32_t reserve_capacity = 0, secondary_reserve_capacity = 0;
  bool function_known = false, secondary_function = false;
  uint32_t weapon = 0;
  bool ammo_activity = false;
};
inline bool HealthElement(std::string_view id) {
  return id == "barHealth" || id == "barMaxHealth" || id == "barArmour";
}
inline bool ObjectiveElement(std::string_view id) {
  return id == "textObjectiveTitle" || id == "textObjectiveInfo" ||
         id == "textObjectiveMessages" || id == "imageObjectiveMessages" ||
         id == "HUD_objectiveinfo" || id == "HUD_objectivetitle";
}
inline bool InteractionPromptElement(std::string_view id) {
  // Live textContext label contains COVER plus the A-button image. textAction
  // is a separate HUD channel. Do not hide gadget context, subtitles,
  // pickup/swap panels, or action progress bars along with the prompt.
  return id == "textAction" || id == "textContext";
}
inline bool ReplacedElement(std::string_view id) {
  return HealthElement(id) ||
         id == "AmmoBarL" || id == "AmmoBarR" ||
         id == "AmmoMetersL" || id == "AmmoMetersR";
}
}
// Temporarily change native transform inputs, not layout/animation state.
// The generated transform computes matrices (including descendants) before restore.
struct PDZ_NativeHudScaleScope {
  uint8_t* base=nullptr;
  uint32_t data=0, original[4]{};
  PDZ_NativeHudScaleScope(uint8_t* memory,uint32_t element);
  ~PDZ_NativeHudScaleScope();
};
bool PDZ_ReadClassicHud(pdz::hud::LiveState& state);
bool PDZ_SuppressClassicHudRender(uint8_t* base, uint32_t handle, uint32_t message);
void PDZ_ObserveHudDraw(uint8_t* base, uint32_t batch, uint32_t position,
                       uint32_t color, uint32_t tile, uint32_t flags,
                       uint32_t caller, double width, double height);
