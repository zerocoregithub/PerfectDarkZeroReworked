#pragma once

#include <array>
#include <cmath>

namespace pdz {
inline bool ValidCoverView(const std::array<float, 16>& m) {
  for (float v : m) if (!std::isfinite(v)) return false;
  if (std::abs(m[3]) > .001f || std::abs(m[7]) > .001f ||
      std::abs(m[11]) > .001f || std::abs(m[15] - 1.f) > .001f) return false;
  for (int a = 0; a < 3; ++a)
    for (int b = 0; b < 3; ++b) {
      float dot = 0;
      for (int r = 0; r < 3; ++r) dot += m[r*4+a] * m[r*4+b];
      if (std::abs(dot - (a == b ? 1.f : 0.f)) > .01f) return false;
    }
  return true;
}
inline std::array<float, 3> CoverViewEye(const std::array<float, 16>& m) {
  std::array<float, 3> eye{};
  for (int r = 0; r < 3; ++r)
    for (int a = 0; a < 3; ++a) eye[r] -= m[r*4+a] * m[12+a];
  return eye;
}
inline void SetCoverViewEye(std::array<float, 16>& m,
                            const std::array<float, 3>& eye) {
  for (int a = 0; a < 3; ++a)
    m[12+a] = -(eye[0]*m[a] + eye[1]*m[4+a] + eye[2]*m[8+a]);
}
inline bool CanCorrectCoverView(unsigned cover_flag, unsigned native_branch) {
  return cover_flag == 1 && native_branch == 0;
}
// PDZ's row-major view matrix stores world-space camera axes in columns.
// Rotate those axes, preserving the eye position rather than the old view
// translation (which would otherwise orbit the camera around the world).
inline bool RotateCoverView(std::array<float, 16>& m, float yaw_degrees,
                            float pitch_degrees) {
  for (float v : m) if (!std::isfinite(v)) return false;
  if (!std::isfinite(yaw_degrees) || !std::isfinite(pitch_degrees)) return false;
  if (std::abs(m[3]) > .001f || std::abs(m[7]) > .001f ||
      std::abs(m[11]) > .001f || std::abs(m[15] - 1.f) > .001f) return false;
  for (int a = 0; a < 3; ++a) {
    for (int b = 0; b < 3; ++b) {
      float dot = 0;
      for (int r = 0; r < 3; ++r) dot += m[r*4+a] * m[r*4+b];
      if (std::abs(dot - (a == b ? 1.f : 0.f)) > .01f) return false;
    }
  }
  const auto original = m;
  std::array<float, 3> eye{};
  for (int r = 0; r < 3; ++r)
    for (int a = 0; a < 3; ++a) eye[r] -= m[r*4+a] * m[12+a];
  constexpr float radians = 3.14159265358979323846f / 180.f;
  const float cy = std::cos(yaw_degrees * radians);
  const float sy = std::sin(yaw_degrees * radians);
  for (int a = 0; a < 3; ++a) {
    m[a] = cy*original[a] + sy*original[8+a];
    m[8+a] = -sy*original[a] + cy*original[8+a];
  }
  // Positive PDZ pitch looks down; rotation is about the yaw-adjusted right.
  const std::array<float, 3> axis{m[0], m[4], m[8]};
  const float cp = std::cos(-pitch_degrees * radians);
  const float sp = std::sin(-pitch_degrees * radians);
  const auto yawed = m;
  for (int a = 0; a < 3; ++a) {
    const std::array<float, 3> v{yawed[a], yawed[4+a], yawed[8+a]};
    const std::array<float, 3> cross{
      axis[1]*v[2]-axis[2]*v[1], axis[2]*v[0]-axis[0]*v[2],
      axis[0]*v[1]-axis[1]*v[0]};
    const float dot = axis[0]*v[0]+axis[1]*v[1]+axis[2]*v[2];
    for (int r = 0; r < 3; ++r)
      m[r*4+a] = cp*v[r] + sp*cross[r] + (1-cp)*dot*axis[r];
    m[12+a] = -(eye[0]*m[a] + eye[1]*m[4+a] + eye[2]*m[8+a]);
  }
  return true;
}
}
