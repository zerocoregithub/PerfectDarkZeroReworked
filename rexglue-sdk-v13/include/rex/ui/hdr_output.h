#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace rex::ui::hdr {
// Optional UI pipelines must be ready before the presenter enables HDR.
inline std::atomic<bool> ui_pipelines_available{false};
inline float WhiteNits(double value) {
  return std::isfinite(value) ? float(std::clamp(value, 80.0, 400.0)) : 200.0f;
}
inline float DecodeSrgb(float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  return value <= 0.04045f ? value / 12.92f
                         : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
inline float ToScRgb(float value, double white_nits) {
  return DecodeSrgb(value) * (WhiteNits(white_nits) / 80.0f);
}
inline bool ShouldEnable(bool requested, bool display_hdr, bool pipelines_ready,
                         bool failed) {
  return requested && display_hdr && pipelines_ready && !failed;
}
}  // namespace rex::ui::hdr
