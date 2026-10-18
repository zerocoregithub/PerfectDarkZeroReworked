#pragma once
#include <algorithm>
#include <cmath>
namespace pdz {
inline void AccumulateSwayInput(double x_degrees,double y_degrees,double& yaw,double& pitch) {
  constexpr double radians=3.14159265358979323846/180.;
  // Match the negated forward-vector angles used by the original sway path,
  // not the opposite-sign angles stored in the camera controller.
  yaw+=x_degrees*radians;
  pitch-=y_degrees*radians;
}
struct WeaponSway {
  double yaw = 0, pitch = 0, horizontal = 0, vertical = 0;
  bool initialized = false;
  void Reset() { *this = {}; }
  bool Update(double next_yaw, double next_pitch, double seconds, double amount) {
    if (!std::isfinite(next_yaw) || !std::isfinite(next_pitch) ||
        !std::isfinite(seconds) || seconds <= 0 || seconds > .25 ||
        !std::isfinite(amount) || amount < 0 || amount > 8) {
      Reset(); return false;
    }
    if (!initialized) { yaw=next_yaw; pitch=next_pitch; initialized=true; return true; }
    constexpr double pi=3.14159265358979323846;
    const double dy=std::remainder(next_yaw-yaw,2*pi), dp=next_pitch-pitch;
    yaw=next_yaw; pitch=next_pitch;
    // Camera cuts and teleport-like rotations must not kick the viewmodel.
    if (std::abs(dy)>.5 || std::abs(dp)>.5) { horizontal=vertical=0; return true; }
    const double cap=.06*amount;
    const double hx=std::clamp(-dy/seconds*.035*amount,-cap,cap);
    const double vy=std::clamp(-dp/seconds*.025*amount,-cap,cap);
    const double blend=-std::expm1(-seconds/.065);
    horizontal=std::lerp(horizontal,hx,blend);
    vertical=std::lerp(vertical,vy,blend);
    return true;
  }
};
}
