#pragma once
#include <cmath>
#include <cstdint>
namespace pdz::input {
// Give a pair of stick clicks a short chord window. Preserve a released lone
// native tap for 120ms so guest polling cannot miss it. Third-person RS alone
// becomes shoulder swap; first-person RS and lone LS remain native.
struct StickCameraShortcuts {
  static constexpr uint16_t left=64,right=128,both=left|right;
  uint16_t previous=0,pending=0,passed=0,replay=0;
  double since=0,replay_until=0,last=0;
  bool chord=false,shoulder_owned=false,blocked=false;
  struct Result {uint16_t buttons;bool perspective=false,shoulder=false;};
  Result Update(uint16_t buttons,double now,bool eligible,bool third_person) {
    Result result{buttons};const uint16_t held=buttons&both;
    if(!std::isfinite(now)||now<last){*this={};blocked=held!=0;return result;}
    last=now;
    if(!eligible){pending=passed=replay=0;blocked=held!=0;previous=held;return result;}
    if(blocked){previous=held;if(!held)blocked=false;return result;}
    if(chord){result.buttons=buttons&~both;if(!held)chord=false;previous=held;return result;}
    if(!held)shoulder_owned=false;
    const uint16_t pressed=held&~previous;
    if(!pending&&pressed){pending=pressed;since=now;}
    if(held==both&&pending&&!passed&&!shoulder_owned&&now-since<=.15) {
      result.perspective=true;chord=true;pending=replay=passed=0;
      result.buttons=buttons&~both;previous=held;return result;
    }
    if(pending&&(now-since>=.15||!(held&pending))) {
      uint16_t native=pending;
      if(third_person&&(pending&right)) {
        result.shoulder=true;native&=~right;shoulder_owned=(held&right)!=0;
      }
      passed|=native&held;replay=native;replay_until=now+.12;pending=0;
    }
    if(now>=replay_until)replay=0;
    passed&=held;
    result.buttons=(buttons&~both)|(held&passed)|replay;
    if(shoulder_owned)result.buttons&=~right;
    previous=held;return result;
  }
};
// Camera-only lateral easing, independent of frame rate. Large gaps/invalid
// state snap safely rather than retaining a stale camera across sessions.
struct ShoulderTransition {
  double value=0;bool initialized=false;
  void Reset(){*this={};}
  double Update(double target,double dt) {
    if(!std::isfinite(target)){Reset();return 0;}
    if(!initialized||!std::isfinite(dt)||dt<0||dt>.25){value=target;initialized=true;return value;}
    value+=(target-value)*(-std::expm1(-dt/.055));
    if(std::abs(target-value)<.01)value=target;
    return value;
  }
};
struct ShoulderSwapPress {
  bool previous=false,owned=false;
  struct Result {bool toggle,consume;};
  Result Update(bool down,bool eligible) {
    const bool toggle=down&&!previous&&eligible;
    if(toggle)owned=true;
    const bool consume=down&&owned;
    if(!down)owned=false;
    previous=down;
    return {toggle,consume};
  }
};
inline double OppositeShoulder(double offset) {
  return std::isfinite(offset)?(offset==0?-80.:-offset):offset;
}
}
