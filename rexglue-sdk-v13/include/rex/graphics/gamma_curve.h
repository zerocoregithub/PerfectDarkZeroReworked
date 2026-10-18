#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rex::gpu {
inline double SanitizeDisplayGamma(double gamma) {
  return std::isfinite(gamma) && gamma >= .5 && gamma <= 2.0 ? gamma : 1.0;
}
// Compose with the original output ramp rather than adding brightness.
// Full black/white remain endpoints; 1.0 is bit-exact identity.
inline uint32_t AdjustGammaCode(uint32_t value, uint32_t maximum, double gamma) {
  value = std::min(value, maximum);
  gamma = SanitizeDisplayGamma(gamma);
  if (gamma == 1.0 || value == 0 || value == maximum || maximum == 0) return value;
  return uint32_t(std::clamp(std::lround(
      std::pow(double(value) / maximum, 1.0 / gamma) * maximum),
      0L, long(maximum)));
}
struct GammaSegment { uint32_t base, delta; };
inline GammaSegment AdjustGammaSegment(uint32_t base, uint32_t delta, double gamma) {
  const uint32_t adjusted_base = AdjustGammaCode(base, 65535, gamma);
  const uint32_t adjusted_end = AdjustGammaCode(base + delta, 65535, gamma);
  return {adjusted_base, adjusted_end - adjusted_base};
}
}
