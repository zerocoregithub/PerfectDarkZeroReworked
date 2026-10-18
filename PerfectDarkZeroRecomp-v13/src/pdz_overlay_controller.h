#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <imgui.h>

namespace pdz::ui {
constexpr uint16_t kMenu=0x10, kView=0x20, kMenuChord=kMenu|kView;
constexpr uint64_t kQuickAccess=uint64_t(1)<<28; // SDK private native QAM bit.
struct OverlayPad {
  uint16_t buttons=0;
  float lx=0,ly=0,lt=0,rt=0;
  bool connected=false,quick_access=false;
};
// Delay lone Menu/View briefly; released taps survive the combo window and
// are replayed for 120ms so multiple polls cannot consume a one-frame pulse. A chord
// consumes both buttons until both are released, with exactly one toggle.
struct OverlayShortcuts {
  uint16_t previous=0,pending=0,passed=0,replay=0;
  double since=0,replay_until=0;
  bool chord_latched=false,quick_latched=false;
  struct Result { uint16_t buttons; bool overlay=false,render=false; };
  Result Update(uint16_t buttons,bool quick,double now,bool captured) {
    Result result{buttons};
    const uint16_t held=buttons&kMenuChord;
    if(held==kMenuChord&&!chord_latched){result.overlay=true;chord_latched=true;pending=passed=replay=0;}
    result.render=quick&&!quick_latched;
    quick_latched=quick;
    uint16_t output=0;
    if(chord_latched){if(!held)chord_latched=false;}
    else if(captured){pending=passed=replay=0;}
    else {
      if(held&&!previous){pending=held;since=now;}
      if(pending&&now-since>=.25){passed=held&pending;replay=pending;replay_until=now+.12;pending=0;}
      if(now>=replay_until)replay=0;
      output=(held&passed)|replay;
      if(!held)passed=0;
    }
    previous=held;
    result.buttons=(buttons&~kMenuChord)|output;
    return result;
  }
};
inline float MenuAxis(float value){return std::clamp((value-.25f)/.65f,0.f,1.f);}
inline void FeedOverlayPad(ImGuiIO& io,const OverlayPad& pad,bool enabled) {
  const uint16_t buttons=enabled?pad.buttons:0;
  const auto key=[&](ImGuiKey key,uint16_t mask){io.AddKeyEvent(key,(buttons&mask)!=0);};
  key(ImGuiKey_GamepadFaceDown,0x1000);key(ImGuiKey_GamepadFaceRight,0x2000);
  key(ImGuiKey_GamepadFaceLeft,0x4000);key(ImGuiKey_GamepadFaceUp,0x8000);
  key(ImGuiKey_GamepadDpadUp,1);key(ImGuiKey_GamepadDpadDown,2);
  key(ImGuiKey_GamepadDpadLeft,4);key(ImGuiKey_GamepadDpadRight,8);
  const auto axis=[&](ImGuiKey key,float value){const float amount=enabled?MenuAxis(value):0;io.AddKeyAnalogEvent(key,amount>0,amount);};
  axis(ImGuiKey_GamepadLStickLeft,-pad.lx);axis(ImGuiKey_GamepadLStickRight,pad.lx);
  axis(ImGuiKey_GamepadLStickUp,pad.ly);axis(ImGuiKey_GamepadLStickDown,-pad.ly);
  axis(ImGuiKey_GamepadL2,pad.lt);axis(ImGuiKey_GamepadR2,pad.rt);
  if(pad.connected)io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
  else io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
}
inline float OverlayScale(float setting,float height) {
  const float desired=setting>0?std::clamp(setting,1.f,2.5f):std::clamp(height/720.f,1.f,2.25f);
  return std::min(desired,std::max(1.f,height/600.f));
}
}
