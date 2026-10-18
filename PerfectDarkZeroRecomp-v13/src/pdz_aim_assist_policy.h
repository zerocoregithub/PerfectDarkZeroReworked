#pragma once
#include <cstdint>

namespace pdz::aim {
constexpr bool UseNativePolicy(bool overlay_enabled,bool native_setting,bool scoped) {
  return !overlay_enabled && native_setting && !scoped;
}
// Only the verified local gameplay application gate may override native policy.
// Candidate selection, line of sight and unrelated camera modes remain native.
constexpr uint32_t OverlayAimMode(uint32_t native,bool eligible,bool enabled,bool scoped) {
  return eligible ? (enabled && !scoped ? 1u : 0u) : native;
}
constexpr double OverlayAimStrength(double native,bool eligible,bool enabled,bool scoped) {
  // Match the already accepted adapted mode0 behavior, independently of the
  // native setting and its squared-stick-motion input.
  return eligible && enabled && !scoped ? 1.0 : native;
}
}
