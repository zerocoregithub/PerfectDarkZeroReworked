#include "../src/pdz_dodge_roll_math.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>

int main() {
  pdz::DodgeRollFatigue fatigue;
  const auto idle = [&](int frames) {
    for (int i = 0; i < frames; ++i) fatigue.Observe(.1, false, 0., 0);
  };
  fatigue.Observe(0., true, 0., 100);
  assert(fatigue.strength == 1.6);
  for (int i = 1; i <= 9; ++i) {
    fatigue.Observe(.1, true, i / 10., 100);
    assert(fatigue.strength == 1.6); // Never drops mid-roll.
  }
  idle(1);
  fatigue.Observe(0., true, 0., 100);
  assert(fatigue.strength == 1.3); // Same clip, new roll.
  fatigue.Observe(.1, true, .9, 100);
  fatigue.Observe(.1, true, .01, 100);
  assert(fatigue.strength == 1.); // Phase wrap, no intervening non-roll frame.
  idle(10);
  fatigue.Observe(0., true, 0., 101);
  assert(fatigue.strength == 1.); // Saturates rather than falling below native.
  idle(25);
  fatigue.Observe(0., true, 0., 102);
  assert(fatigue.strength == 1.); // 2..3.5s retains the current tier.
  idle(36);
  fatigue.Observe(0., true, 0., 100);
  assert(fatigue.strength == 1.6);
  const double clock = fatigue.time;
  fatigue.Observe(std::numeric_limits<double>::quiet_NaN(), false, 0., 0);
  fatigue.Observe(1., false, 0., 0);
  assert(fatigue.time == clock); // Bad dt is ignored, not mistaken for recovery.
  fatigue.Reset();
  assert(fatigue.strength == 1.6 && !fatigue.active);
  for (unsigned type = 0; type < 232; ++type)
    assert(pdz::IsDodgeRollAnimation(type) ==
           ((type >= 140 && type <= 143) || (type >= 146 && type <= 157)));
  for (double strength : {1., 1.5, 2.}) {
    double integral = 0, early = 0;
    constexpr int samples = 10000;
    for (int i = 0; i < samples; ++i) {
      const double phase = (i + .5) / samples;
      const double scale = pdz::DodgeRollScale(true, phase, strength);
      assert(scale >= 1 && scale <= 3.109376);
      assert(pdz::DodgeRollScale(false, phase, strength) == 1);
      integral += scale / samples;
      if (phase < .6) early += (scale - 1) / samples;
    }
    assert(std::abs(integral - strength) < 1e-7);
    if (strength > 1) assert(early / (strength - 1) > .91);
    assert(pdz::DodgeRollScale(true, 0, strength) == 1);
    assert(pdz::DodgeRollScale(true, 1, strength) == 1);
    assert(pdz::DodgeRollScale(true, .25, strength) >=
           pdz::DodgeRollScale(true, .5, strength));
  }
  for (double invalid : {-1., 2., std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()})
    assert(pdz::DodgeRollScale(true, invalid, 1.5) == 1);
  assert(pdz::DodgeRollScale(true, .25, 0) == 1);
  assert(pdz::DodgeRollScale(true, .25, 2.1) == 1);
  // The alternate integrator boosts a temporary world velocity, then removes
  // the multiplier from collision-resolved feedback. It must not compound.
  for (double hz : {30., 60., 120., 240.}) {
    for (double direction : {-1., 1.}) {
      double feedback = direction * 80., distance = 0., expected = 0.;
      const double dt = 1. / hz;
      for (int frame = 0; frame < int(hz); ++frame) {
        const double scale = pdz::DodgeRollScale(true, frame / hz, 2.);
        const double requested = feedback * scale;
        // Open-ground collision result equals the request; wall contact is 0.
        const double resolved = frame < int(hz / 2.) ? requested : 0.;
        distance += resolved * dt;
        if (frame < int(hz / 2.)) expected += direction * 80. * scale * dt;
        feedback = resolved / scale;
        assert(std::abs(feedback) <= 80. + 1e-10);
      }
      assert(std::abs(distance - expected) < 1e-9);
      assert(feedback == 0.); // A blocked roll never resurrects velocity.
    }
  }
  std::puts("PASS: disabled identity, smooth endpoints, front-loaded envelope, bounds, integral and invalid-input guards");
}
