#pragma once
#include <cmath>
#include <cstdint>

namespace pdz {
// Observe once per native movement update, including non-roll frames.
// Simulation time keeps pause from consuming the recovery interval.
struct DodgeRollFatigue {
  double time = 0., last_start = -100., previous_phase = 0., strength = 1.6;
  uint32_t clip = 0;
  unsigned tier = 0;
  bool active = false;
  void Reset() { *this = {}; }
  void Observe(double seconds, bool rolling, double phase, uint32_t descriptor) {
    if (!std::isfinite(seconds) || seconds < 0. || seconds > .25) return;
    time += seconds;
    if (!rolling || !std::isfinite(phase) || phase < 0. || phase > 1.) {
      active = false;
      return;
    }
    const bool start = !active || descriptor != clip || phase + .001 < previous_phase;
    if (start) {
      const double gap = time - last_start;
      if (gap >= 3.5) tier = 0;
      else if (gap <= 2. && tier < 2) ++tier;
      strength = tier == 0 ? 1.6 : tier == 1 ? 1.3 : 1.;
      last_start = time;
    }
    active = true;
    previous_phase = phase;
    clip = descriptor;
  }
};
inline bool IsDodgeRollAnimation(unsigned type) {
  return (type >= 140 && type <= 143) || (type >= 146 && type <= 157);
}
// Extra-distance envelope. Normalized integral is 1:
// a uniform native speed would average `strength`.
// For native root motion the actual distance depends on its velocity profile.
// Peak at 25% progress; ~91% of the extra impulse arrives before 60% progress.
inline double DodgeRollScale(bool enabled, double progress, double strength) {
  if (!enabled || !std::isfinite(progress) || progress < 0 || progress > 1 ||
      !std::isfinite(strength) || strength < 1 || strength > 2) return 1;
  const double remaining = 1 - progress;
  return 1 + (strength - 1) * 20 * progress * remaining * remaining * remaining;
}
}
