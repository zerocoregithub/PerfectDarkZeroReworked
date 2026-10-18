#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace pdz {
// Match the standard humanoid NPC damage/health implementations, not merely
// "not the local player" (which would include props and other players).
inline bool IsClassicNpcHealthPath(uint32_t damageHandler, uint32_t healthGetter) {
  return damageHandler == 0x8251EBB0u && healthGetter == 0x8251EDE8u;
}
// Effective health is reduced by increasing only final health loss. This is
// reversible immediately, preserves already-lethal hits and never edits saves.
inline double ClassicNpcHealthLoss(double original, bool enabled, double scale) {
  if (!enabled || !std::isfinite(original) || original <= 0 ||
      original > std::numeric_limits<float>::max() ||
      !std::isfinite(scale) || scale < .25 || scale > 1.) return original;
  if (scale == 1.) return original;
  return double(float(std::min(original / scale,
                              double(std::numeric_limits<float>::max()))));
}
}
