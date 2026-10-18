#include "../src/pdz_movement_math.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <utility>

int main() {
  int checks = 0;
  for (int i = 0; i <= 1000; ++i) {
    const double r = i / 1000.0;
    assert(pdz::MovementRetention(r, 1) == r);
    for (double speed : {2., 4., 8.}) {
      const double adjusted = pdz::MovementRetention(r, speed);
      assert(adjusted >= 0 && adjusted <= r + 1e-7);
      for (double old : {-500., 0., 500.})
        for (double target : {-500., 0., 500.}) {
          const double next = adjusted * old + (1 - adjusted) * target;
          assert(next >= std::min(old, target) - 1e-6);
          assert(next <= std::max(old, target) + 1e-6);
          if (old == target) assert(std::abs(next - target) < 1e-6);
          ++checks;
        }
    }
  }
  // The same elapsed time gives the same exponential response at 30/60/120 Hz.
  for (int fps : {30, 60, 120}) {
    const double perFrame = std::pow(.5, 1.0 / fps);
    const double oneSecond = std::pow(pdz::MovementRetention(perFrame, 4), fps);
    assert(std::abs(oneSecond - std::pow(.5, 4)) < 2e-6);
  }
  assert(pdz::MovementRetention(-.1, 4) == -.1);
  assert(pdz::MovementRetention(1.1, 4) == 1.1);
  assert(std::isnan(pdz::MovementRetention(std::numeric_limits<double>::quiet_NaN(), 4)));
  assert(pdz::MovementRetention(.5, std::numeric_limits<double>::infinity()) == .5);
  for (unsigned axis : {0u, 1u}) {
    const double duration = .550;
    assert(std::abs(pdz::ClassicMovementRetention(.8, duration, axis) - .05) < 1e-7);
    for (int fps : {30, 60, 120, 240}) {
      const double perFrame = pdz::ClassicMovementRetention(.8, 1. / fps, axis);
      assert(perFrame >= 0 && perFrame <= 1);
      assert(std::abs(std::pow(perFrame, fps / 12.) -
                      std::pow(.05, (1. / 12.) / duration)) < 1e-6);
    }
    const double firstStopFrame = pdz::ClassicMovementRetention(.8, 1. / 60., axis, 1, 0);
    assert(firstStopFrame > .7 && firstStopFrame < .8);
    assert(pdz::ClassicMovementRetention(.8, duration * 2, axis) < .003);
    assert(std::abs(pdz::ClassicMovementRetention(.8, .180, axis, 1, 0) - .05) < 1e-7);
    assert(std::abs(pdz::ClassicMovementRetention(.8, .100, axis, 1, -1) - .05) < 1e-7);
    assert(pdz::ClassicMovementRetention(.8, 1./60., axis, 0, 1) > .9);
  }
  for (int fps : {30, 60, 120, 240}) {
    pdz::ClassicRunState state;
    double scale = 0;
    for (int frame = 0; frame < 3 * fps; ++frame)
      scale = pdz::ClassicRunScale(state, 1./fps, 1, 0);
    assert(std::abs(scale - .8) < 1e-10);
    for (int frame = 0; frame < fps / 2; ++frame)
      scale = pdz::ClassicRunScale(state, 1./fps, .70710678, .70710678);
    assert(std::abs(scale - 1) < 1e-10);
    for (int frame = 0; frame < fps / 2; ++frame)
      scale = pdz::ClassicRunScale(state, 1./fps, 0, 1);
    assert(std::abs(scale - .8) < 1e-10);
    assert(state.forward_seconds == 0);
    for (auto input : {std::pair{-1., 0.}, std::pair{0., 1.}, std::pair{.4, 0.}, std::pair{.6, .7}}) {
      for (int frame = 0; frame < 4 * fps; ++frame)
        scale = pdz::ClassicRunScale(state, 1./fps, input.first, input.second);
      assert(scale == .8 && state.forward_seconds == 0);
    }
    pdz::ClassicRunScale(state, .5, 1, 0);
    assert(state.forward_seconds == 0 && state.boost == 1);
  }
  assert(pdz::ClassicMovementRetention(.8, 0, 0) == .8);
  assert(pdz::ClassicMovementRetention(.8, -1, 0) == .8);
  assert(pdz::ClassicMovementRetention(.8, .03, 2) == .8);
  assert(pdz::ClassicMovementRetention(.8, std::numeric_limits<double>::infinity(), 0) == .8);
  for (double equipment : {.6, .73, .85, 1., 1.15}) {
    assert(pdz::ClassicEquipmentFactor(equipment, true) == 1.);
    assert(pdz::ClassicEquipmentFactor(equipment, false) == equipment);
  }
  assert(pdz::ClassicEquipmentFactor(0, true) == 0);
  // Model the two paths through the native getter: equipment selection first,
  // then the optional hit-timer substitution. Modern is identity at both sites.
  for (bool classic : {false, true})
    for (bool recentHit : {false, true})
      for (double equipment : {.6, .73, .85, 1., 1.15}) {
        double result = pdz::ClassicEquipmentFactor(equipment, classic);
        if (recentHit) result = pdz::ClassicEquipmentFactor(.73, classic);
        assert(result == (classic ? 1. : recentHit ? .73 : equipment));
      }
  assert(pdz::ClassicEquipmentFactor(-1, true) == -1);
  assert(std::isnan(pdz::ClassicEquipmentFactor(std::numeric_limits<double>::quiet_NaN(), true)));
  std::printf("Movement blend checks passed: %d; vanilla identity, bounds, steady speed, timestep consistency.\n", checks);
}
