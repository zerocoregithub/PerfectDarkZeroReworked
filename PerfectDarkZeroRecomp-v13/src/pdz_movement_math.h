#pragma once
#include <algorithm>
#include <cmath>

namespace pdz {
// Dab's head translation has its own .982-per-240Hz low-pass (~687ms to 95%).
// Approximate its momentum with a shorter start ramp and deliberately quicker
// braking/reversals. All durations below are time to 95%, not time constants.
inline double ClassicMovementRetention(double original, double seconds,
                                       unsigned axis, double old = 0,
                                       double target = 1) {
  if (!std::isfinite(original) || original < 0 || original > 1 ||
      !std::isfinite(seconds) || seconds <= 0 || axis > 1 ||
      !std::isfinite(old) || !std::isfinite(target))
    return original;
  const bool reversing = old * target < 0;
  const bool braking = std::abs(target) < std::abs(old);
  const double duration = reversing ? .100 : braking ? .180 : .550;
  return double(float(std::pow(.05, seconds / duration)));
}
struct ClassicRunState {
  double forward_seconds = 0;
  double boost = 1;
};
// Dab: 180 60Hz ticks before boost, then .01/tick up to 1.25.
// User-requested forward cone replaces Dab's forward-input-only eligibility.
inline double ClassicRunScale(ClassicRunState& state, double seconds,
                              double forward, double strafe, bool fast_unarmed=false) {
  if (!std::isfinite(seconds) || seconds <= 0 || seconds > .25 ||
      !std::isfinite(forward) || !std::isfinite(strafe)) {
    state = {};
    return .8;
  }
  const bool eligible = forward >= .5 && forward + 1e-6 >= std::abs(strafe);
  const double previous = state.forward_seconds;
  state.forward_seconds = eligible ? std::min(3.25, previous + seconds) : 0;
  if (eligible && state.forward_seconds > 3.) {
    const double boost_seconds = std::min(seconds, state.forward_seconds - 3.);
    state.boost = std::min(1.25, state.boost + .6 * boost_seconds);
  } else {
    state.boost = std::max(1., state.boost - .6 * seconds);
  }
  // Same timer/ramp; re-equipping immediately restores the armed curve.
  return fast_unarmed ? .8+(state.boost-1.)*1.8 : .8*state.boost;
}
inline double ClassicEquipmentFactor(double original, bool enabled) {
  return enabled && std::isfinite(original) && original > 0 ? 1. : original;
}
// Native walking retains R of the old velocity and blends (1-R) of the
// requested velocity. R is already time-scaled by the original game.
// R^speed shortens the response time without changing its steady-state target.
inline double MovementRetention(double original, double speed) {
  if (!std::isfinite(original) || original < 0.0 || original > 1.0 ||
      !std::isfinite(speed) || speed <= 1.0) return original;
  return double(float(std::pow(original, std::clamp(speed, 1.0, 8.0))));
}
}
