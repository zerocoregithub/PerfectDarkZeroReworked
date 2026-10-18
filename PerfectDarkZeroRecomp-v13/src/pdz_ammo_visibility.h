#pragma once
#include "pdz_hud_bridge.h"
#include <array>
namespace pdz::hud {
// Overlay-only state; no guest memory queries or native UI callback work.
struct AmmoVisibility {
  std::array<uint32_t, 9> previous{};
  double last_activity = 0;
  bool initialized = false;
  void Reset() { initialized = false; }
  float Update(const LiveState& state, double now, bool enabled, double delay) {
    const std::array<uint32_t, 9> current{state.weapon, state.loaded, state.capacity,
        state.reserve, state.secondary_loaded, state.secondary_capacity,
        state.secondary_reserve, uint32_t(state.function_known), uint32_t(state.secondary_function)};
    if(!initialized || current != previous || state.ammo_activity || !enabled || now < last_activity)
      last_activity = now;
    previous = current; initialized = true;
    if(!enabled) return 1.f;
    // A short fade after the selected inactivity period, not an abrupt pop.
    return float(std::clamp(1.0 - (now-last_activity-std::clamp(delay,1.0,30.0))/.25,0.0,1.0));
  }
};
}
