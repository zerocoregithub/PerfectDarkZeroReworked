#include <rex/graphics/gamma_curve.h>
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
  size_t checks = 0;
  for (uint32_t maximum : {1023u, 65535u}) {
    for (double gamma : {.5, .75, 1., 1.25, 1.5, 2.}) {
      uint32_t previous = 0;
      for (uint32_t value = 0; value <= maximum; ++value) {
        const auto adjusted = rex::gpu::AdjustGammaCode(value, maximum, gamma);
        assert(adjusted >= previous && adjusted <= maximum);
        if (gamma == 1.) assert(adjusted == value);
        if (gamma > 1.) assert(adjusted >= value);
        if (gamma < 1.) assert(adjusted <= value);
        previous = adjusted;
        ++checks;
      }
      assert(rex::gpu::AdjustGammaCode(0, maximum, gamma) == 0);
      assert(rex::gpu::AdjustGammaCode(maximum, maximum, gamma) == maximum);
    }
  }
  assert(rex::gpu::AdjustGammaCode(256, 1023, 2.) > 500);
  assert(rex::gpu::AdjustGammaCode(256, 1023, .5) < 70);
  for (double gamma : {0., 3., std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()})
    assert(rex::gpu::AdjustGammaCode(333, 1023, gamma) == 333);
  for (double gamma : {.5, 1., 1.5, 2.}) {
    for (uint32_t i=0; i<128; ++i) {
      const auto adjusted = rex::gpu::AdjustGammaSegment(i*512, 512, gamma);
      assert(adjusted.base <= 65535 && adjusted.base+adjusted.delta <= 65535);
      assert(adjusted.base == rex::gpu::AdjustGammaCode(i*512,65535,gamma));
      assert(adjusted.base+adjusted.delta == rex::gpu::AdjustGammaCode(i*512+512,65535,gamma));
    }
  }
  std::printf("PASS: %zu code checks; identity, monotonicity, endpoints, midtones, invalid values, PWL bounds\n", checks);
}
