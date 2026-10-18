#pragma once
#include <cmath>
#include <cstdint>

namespace pdz {
// Original GunAnim IDs, before weapon-specific virtual clip remapping.
inline bool IsFastWeaponAction(uint32_t action) {
  return action == 1 || action == 2 || action == 6 || action == 13 ||
         action == 14 || action == 15 || action == 22 || action == 23 ||
         action == 25;
}
inline double WeaponActionRate(uint32_t action, double original, double speed) {
  if (!IsFastWeaponAction(action) || !std::isfinite(original) || original <= 0 ||
      !std::isfinite(speed) || speed < 1 || speed > 3 || speed == 1)
    return original;
  const float adjusted = float(original * speed);
  return std::isfinite(adjusted) && adjusted > 0 ? double(adjusted) : original;
}
}
