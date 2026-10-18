#pragma once
#include <array>
#include <cmath>

namespace pdz {
// This is the target-acquisition ray, not the bullet/muzzle origin. Translate
// both endpoints onto the displayed camera's ray before native LOS testing.
// Actual firing must retain its own origin and muzzle obstruction checks.
inline bool AlignCoverTargetRay(const std::array<float,3>& eye,
                                const std::array<float,3>& native_origin,
                                std::array<float,3>& start,
                                std::array<float,3>& end) {
  std::array<double,3> delta{};
  double length2=0;
  for(int i=0;i<3;++i) {
    if(!std::isfinite(eye[i])||!std::isfinite(native_origin[i])||
       !std::isfinite(start[i])||!std::isfinite(end[i])) return false;
    delta[i]=double(eye[i])-native_origin[i]; length2+=delta[i]*delta[i];
  }
  if(length2>500.*500. || length2<.000001) return false;
  auto corrected_start=start,corrected_end=end;
  for(int i=0;i<3;++i) {
    corrected_start[i]=float(double(start[i])+delta[i]);
    corrected_end[i]=float(double(end[i])+delta[i]);
    if(!std::isfinite(corrected_start[i])||!std::isfinite(corrected_end[i])) return false;
  }
  start=corrected_start; end=corrected_end; return true;
}
}
