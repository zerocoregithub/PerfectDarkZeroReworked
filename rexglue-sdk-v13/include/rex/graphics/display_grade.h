#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace rex::gpu {
struct DisplayGrade {
  float contrast = 1.0f;
  float saturation = 1.0f;
  float tint = 0.0f;
  bool IsNeutral() const {
    return contrast == 1.0f && saturation == 1.0f && tint == 0.0f;
  }
};
inline float SanitizeGradeValue(double value, double low, double high, float fallback) {
  return std::isfinite(value) && value >= low && value <= high ? float(value) : fallback;
}
// CPU reference for the matching presentation shader. Display-encoded RGB,
// not scene-linear exposure. Luma-preserving green/magenta tint gains.
inline std::array<float, 3> ApplyDisplayGrade(std::array<float, 3> rgb, DisplayGrade grade) {
  if (grade.IsNeutral()) return rgb;
  for (auto& channel : rgb) channel = (channel - .5f) * grade.contrast + .5f;
  float luma = rgb[0] * .299f + rgb[1] * .587f + rgb[2] * .114f;
  for (auto& channel : rgb) channel = luma + (channel - luma) * grade.saturation;
  rgb[0] *= 1.0f + grade.tint;
  rgb[1] *= 1.0f - grade.tint * (.413f / .587f);
  rgb[2] *= 1.0f + grade.tint;
  for (auto& channel : rgb) channel = std::clamp(channel, 0.0f, 1.0f);
  return rgb;
}
}  // namespace rex::gpu
