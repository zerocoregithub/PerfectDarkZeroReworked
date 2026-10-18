#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace pdz::doors {
struct TimedDoor {
  std::array<uint32_t, 3> position_bits;
  std::string_view model;
};

// Shipped warehouse package 1959974102.pkg, definition offsets +4/+8/+12.
// Match immutable definition coordinates, NOT the moving door's world transform.
// Exact coordinates plus model restrict this to the seven identified instances;
// unknown definitions, security gates and shutters retain native behavior.
inline constexpr std::string_view kWooden =
    "aid_darkcharacter_xenon_props_door_wooden_door";
inline constexpr std::string_view kRusty =
    "aid_darkcharacter_xenon_props_door_Rusty_door";
inline constexpr std::array<TimedDoor, 7> kWarehouseTimedDoors{{
    {{{0x44A3C989, 0x442B8000, 0xC5F404AC}}, kWooden},  // Door5
    {{{0x441D0000, 0x442B8000, 0xC5ED4279}}, kWooden},  // Door3
    {{{0x44A33D06, 0x4312B1AA, 0xC5F54C00}}, kWooden},  // Door6
    {{{0x45401000, 0x43160000, 0xC6345DF2}}, kWooden},  // Door16
    {{{0x45A2BC39, 0x442B8873, 0xC5F2EC00}}, kRusty},   // Door7
    {{{0x44938000, 0x43120000, 0xC6018F7F}}, kWooden},  // Door13
    {{{0x46222B82, 0xC4A14021, 0xC6735D2D}}, kWooden},  // Door31
}};

inline std::string_view TimedDoorModel(
    const std::array<uint32_t, 3>& position_bits) {
  for (const auto& door : kWarehouseTimedDoors) {
    if (door.position_bits == position_bits) return door.model;
  }
  return {};
}
}  // namespace pdz::doors
