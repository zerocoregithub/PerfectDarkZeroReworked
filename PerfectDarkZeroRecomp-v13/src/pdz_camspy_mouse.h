#pragma once
#include <cmath>
#include <cstdint>

namespace pdz {
inline bool CamSpyControlled(uint32_t player_vtable, uint32_t drone_vtable,
                              uint32_t player, uint32_t owner, uint8_t active) {
  return player_vtable == 0x820C93A0u && drone_vtable == 0x820C1090u &&
         player != 0 && player == owner && active == 1;
}
inline float CamSpyYaw(float original_degrees, float delta_degrees) {
  return std::remainder(original_degrees - delta_degrees, 360.0f);
}
}
