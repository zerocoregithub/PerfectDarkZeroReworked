#include "../src/pdz_door_autoclose.h"
#include <cassert>

int main() {
  for (const auto& door : pdz::doors::kWarehouseTimedDoors) {
    assert(pdz::doors::TimedDoorModel(door.position_bits) == door.model);
    auto other = door.position_bits;
    other[0] ^= 1;
    assert(pdz::doors::TimedDoorModel(other).empty());
  }
  assert(pdz::doors::TimedDoorModel({0, 0, 0}).empty());
}
