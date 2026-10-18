#pragma once
#include <cmath>
#include <cstdint>
namespace pdz::accuracy {
// Recorded identities: scoped pistol2, MagSec13, RCP90=20. Other IDs fail closed.
inline bool VerifiedWeapon(uint32_t kind) { return kind==2||kind==13||kind==20; }
inline double Spread(double native,bool enabled,uint32_t kind) {
  return enabled&&VerifiedWeapon(kind)&&std::isfinite(native)&&native>=0 ? native*.5 : native;
}
}
