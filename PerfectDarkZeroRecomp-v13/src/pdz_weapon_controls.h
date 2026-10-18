#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace pdz {
inline bool DualWeapons(uint32_t first,uint32_t second) {
  return first!=0 && second!=0 && first!=second;
}
// Record raw buttons for zoom BEFORE passing this filtered copy to the game.
inline uint16_t RouteZoomButtons(uint16_t buttons, uint8_t lt,
                                 bool enabled, bool gameplay) {
  return enabled && gameplay && lt >= 30
             ? uint16_t(buttons & ~uint16_t(4u | 8u)) : buttons;
}
enum class WeaponButtonAction { None, Original, CycleCarried, SwapWorld };
// Gameplay routing only: menus retain their original Y handling. Delay the
// press only when a world-swap target exists; tap cycles on release in that
// context. Target changes cancel pickup, never redirect a hold to another gun.
struct WorldSwapHold {
  uint32_t target = 0;
  double elapsed = 0;
  bool held = false;
  bool armed = false;
  bool consumed = false;
  bool pending_tap = false;

  WeaponButtonAction Update(bool enabled, bool active, bool down, uint32_t candidate,
              double dt, double duration = .35) {
    using Action = WeaponButtonAction;
    if (!enabled) { *this = {}; held = down; return Action::Original; }
    if (!active || !std::isfinite(dt) || dt < 0 || dt > .25 ||
        !std::isfinite(duration) || duration < .1 || duration > 2) {
      *this = {};
      held = down;  // Require a fresh press after loss of gameplay ownership.
      return active ? Action::None : Action::Original;
    }
    if (!down) {
      const bool cycle = held && pending_tap && !consumed;
      *this = {};
      return cycle ? Action::CycleCarried : Action::None;
    }
    const bool fresh = !held;
    held = true;
    if (fresh) {
      target = candidate; elapsed = 0; armed = candidate != 0;
      pending_tap = armed;
      if (!armed) { consumed = true; return Action::CycleCarried; }
    }
    if (!armed || consumed) return Action::None;
    if (!candidate || candidate != target) { armed = false; elapsed = 0; return Action::None; }
    elapsed += dt;
    if (elapsed + 1e-9 < duration) return Action::None;
    consumed = true;
    return Action::SwapWorld;
  }
};

// Normalized position within the weapon's own magnification range. Do not
// synthesize LT or alter aim/cover intent; replace only zoom-pressure input.
struct DpadZoom {
  static constexpr double minimum = 1./3.;
  int stage = 2;
  double position = 0;
  int previous = 0;
  bool active = false;
  bool initialized = false;

  void EndGesture() { active = false; previous = 0; }

  double Update(bool enabled, bool aiming, bool gameplay, bool left, bool right,
                double native_position, double dt, double easing = .08, int wheel = 0) {
    if (!enabled || !aiming || !gameplay || !std::isfinite(dt) || dt < 0 || dt > .25) {
      EndGesture();
      return native_position;
    }
    if (!initialized) {
      position = 0.; stage = 2;
      initialized = true;
    }
    if(!active) position=0.;
    active = true;
    stage=std::clamp(stage+std::clamp(wheel,-3,3),1,3);
    const int direction = left == right ? 0 : right ? 1 : -1;
    // One stage per press; holding a direction never skips an intermediate step.
    if(direction && direction!=previous) stage=std::clamp(stage+direction,1,3);
    previous = direction;
    const double target=stage/3.;
    const double weight=std::isfinite(easing)&&easing>0 ? -std::expm1(-dt/easing) : 1.;
    position=std::lerp(position,target,weight);
    return position;
  }
};
// Remember requested stages, not intermediate easing positions. A session is
// the current local player/root lifetime; never serialize this mission state.
struct WeaponZoomMemory {
  DpadZoom zoom;
  uint64_t session = 0;
  uint32_t weapon = 0;
  std::unordered_map<uint32_t, int> stages;
  void Reset() { zoom={}; session=0; weapon=0; stages.clear(); }
  DpadZoom& Select(uint64_t next_session, uint32_t next_weapon) {
    if(session != next_session) { Reset(); session=next_session; }
    if(weapon != next_weapon) {
      if(weapon) stages[weapon]=zoom.stage;
      weapon=next_weapon;
      zoom={};
      if(const auto found=stages.find(weapon); found!=stages.end()) {
        zoom.stage=std::clamp(found->second,1,3);
        zoom.initialized=true;
      }
    }
    return zoom;
  }
};
}  // namespace pdz
