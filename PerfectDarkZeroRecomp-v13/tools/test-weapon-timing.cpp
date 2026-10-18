#include "../src/pdz_weapon_timing.h"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
  unsigned checks = 0;
  for (uint32_t action = 0; action < 64; ++action) {
    const bool selected = action == 1 || action == 2 || action == 6 ||
        action == 13 || action == 14 || action == 15 || action == 22 ||
        action == 23 || action == 25;
    assert(pdz::IsFastWeaponAction(action) == selected);
    for (double rate : {0.25, 0.5, 1.0, 1.75, 2.0, 4.0}) {
      assert(pdz::WeaponActionRate(action, rate, 1) == rate);
      const double faster = pdz::WeaponActionRate(action, rate, 2);
      assert(faster == (selected ? rate * 2 : rate));
      // Clip duration / playback rate determines real playback duration.
      assert(3.0 / faster == (selected ? (3.0 / rate) / 2 : 3.0 / rate));
      checks += 3;
    }
  }
  for (double rate : {-1.0, 0.0, std::numeric_limits<double>::infinity()})
    assert(pdz::WeaponActionRate(6, rate, 2) == rate);
  for (double speed : {0.0, 0.5, 4.0, std::numeric_limits<double>::infinity()})
    assert(pdz::WeaponActionRate(6, 1, speed) == 1);
  assert(std::isnan(pdz::WeaponActionRate(6, std::numeric_limits<double>::quiet_NaN(), 2)));
  assert(pdz::WeaponActionRate(6, 1, std::numeric_limits<double>::quiet_NaN()) == 1);
  assert(pdz::WeaponActionRate(6, std::numeric_limits<float>::max(), 2) == std::numeric_limits<float>::max());
  std::cout << "PASS: " << checks << " weapon action selection/rate/duration checks and invalid-input guards\n";
}
