#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pdz::aim {
// Camera-local convention only: +X right, +Y up, +Z forward. The guest
// adapter must prove its basis/signs before using this for live shots.
struct Point { double x = 0, y = 0; };
struct Ray { double x = 0, y = 0, z = 1; };
struct Projection { double tan_half_vertical = 1, aspect = 1; };
inline bool ValidAimFov(double radians) {
  // Strong scope magnification is valid, not a corrupt camera projection.
  // The projection itself still requires finite positive tangent and aspect.
  return std::isfinite(radians) && radians > 0 && radians < 3.1;
}
inline bool FreeAimActive(bool enabled,bool ads_only,bool aiming) {
  return enabled && (!ads_only || aiming);
}
inline bool ManualSwayActive(bool during_ads,bool aiming) {
  return during_ads || !aiming;
}
inline bool FreeAimCamera(uint8_t mode,bool aiming) {
  return mode==0 || (mode==3 && aiming);
}
// Render-only easing. Never feed this state back into input or firing math.
struct CursorVisual {
  Point point;
  bool initialized=false;
  void Reset() { point={}; initialized=false; }
  Point Update(Point raw,double seconds,double easing_seconds) {
    if(!std::isfinite(raw.x)||!std::isfinite(raw.y)) { Reset(); return {}; }
    if(!initialized||!std::isfinite(seconds)||seconds<0||seconds>.25||
       !std::isfinite(easing_seconds)||easing_seconds<=0) {
      point=raw; initialized=true; return point;
    }
    const double weight=-std::expm1(-seconds/easing_seconds);
    point.x=std::lerp(point.x,raw.x,weight);
    point.y=std::lerp(point.y,raw.y,weight);
    return point;
  }
};
// Designer-selected range curve, not a reconstruction of original PD balance.
// Boost only the native attraction acceptance envelope; keep selector/LOS vetoes.
inline double CloseRangeAttraction(double distance, double boost) {
  if(!std::isfinite(distance)||distance<0||!std::isfinite(boost)) return 1.;
  const double t=std::clamp((distance-500.)/1500.,0.,1.);
  const double smooth=t*t*(3.-2.*t);
  return 1.+(std::clamp(boost,1.,2.)-1.)*(1.-smooth);
}
inline Point SwayDeadzone(Point angles,double radians) {
  if(!std::isfinite(radians)||radians<0||radians>.06) return {};
  const auto axis=[radians](double a) {
    return std::isfinite(a)?std::copysign(std::max(0.,std::abs(a)-radians),a):0.;
  };
  return {axis(angles.x),axis(angles.y)};
}
// Dab's edge threshold: no camera turn inside the central rectangle;
// signed linear turn demand from the boundary to the screen edge.
inline double EdgeTurn(double position, double boundary) {
  if (!std::isfinite(position) || !std::isfinite(boundary) ||
      boundary < 0 || boundary >= 1) return 0;
  return std::copysign(std::clamp((std::abs(position)-boundary)/(1-boundary),0.,1.),position);
}
inline double EdgeZoomScale(double base_fov,double live_fov,double ads) {
  if(!std::isfinite(ads)||ads<0) return 0;
  if(!std::isfinite(base_fov)||!std::isfinite(live_fov)||base_fov<=0||live_fov<=0)
    return ads; // No verified zoom: keep the existing unzoomed turn rate.
  return std::min(1.,live_fov/base_fov)*ads;
}
struct FreeAim {
  Point point;
  void Reset() { point={}; }
  bool Update(Point motion, double boundary, double seconds, Point& camera) {
    camera={};
    if (!std::isfinite(motion.x)||!std::isfinite(motion.y)||
        !std::isfinite(boundary)||boundary<0||boundary>1||
        !std::isfinite(seconds)||seconds<0||seconds>.25) { Reset(); return false; }
    point.x=std::clamp(point.x+motion.x,-1.,1.);
    point.y=std::clamp(point.y+motion.y,-1.,1.);
    camera={EdgeTurn(point.x,boundary)*seconds,EdgeTurn(point.y,boundary)*seconds};
    return true;
  }
};
inline bool Valid(Projection p) {
  return std::isfinite(p.tan_half_vertical) && p.tan_half_vertical > 0 &&
         std::isfinite(p.aspect) && p.aspect > 0;
}
inline bool Finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline bool SwayOffset(Point angles, Projection projection, Point& out) {
  if(!Finite(angles)||!Valid(projection)||std::abs(angles.x)>.5||std::abs(angles.y)>.5) return false;
  Point p{std::tan(angles.x)/(projection.tan_half_vertical*projection.aspect),
          std::tan(angles.y)/projection.tan_half_vertical};
  if(!Finite(p)) return false;
  out=p; return true;
}
inline bool ToRay(Point p, Projection projection, Ray& out) {
  if (!Finite(p) || !Valid(projection)) return false;
  const double x = p.x * projection.tan_half_vertical * projection.aspect;
  const double y = p.y * projection.tan_half_vertical;
  const double length = std::hypot(x, y, 1.);
  if (!std::isfinite(length)) return false;
  out = {x / length, y / length, 1. / length};
  return true;
}
// Viewmodel Euler yaw/pitch are opposite camera-local screen angles. Keep this
// conversion shared by the reticle and shot point; do not invert visual sway.
inline bool WeaponEulerOffset(Point angles, Projection projection, Point& out) {
  return SwayOffset({-angles.x, -angles.y}, projection, out);
}
inline bool ToPoint(Ray r, Projection projection, Point& out) {
  if (!Valid(projection) || !std::isfinite(r.x) || !std::isfinite(r.y) ||
      !std::isfinite(r.z) || r.z <= 0) return false;
  Point p{r.x / r.z / projection.tan_half_vertical / projection.aspect,
          r.y / r.z / projection.tan_half_vertical};
  if (!Finite(p)) return false;
  out = p;
  return true;
}
// Explicit tick rate: reference damping must not be silently treated as 60Hz.
inline bool Smooth(Point& state, Point target, double retention_per_tick,
                   double reference_hz, double seconds) {
  if (!Finite(state) || !Finite(target) || !std::isfinite(seconds) ||
      seconds < 0 || !std::isfinite(reference_hz) || reference_hz <= 0 ||
      !std::isfinite(retention_per_tick) || retention_per_tick < 0 ||
      retention_per_tick > 1) return false;
  if (seconds == 0) return true;
  const double weight = retention_per_tick == 0 ? 1. :
      -std::expm1(std::log(retention_per_tick) * seconds * reference_hz);
  state.x = std::lerp(state.x, target.x, weight);
  state.y = std::lerp(state.y, target.y, weight);
  return true;
}
struct State {
  Point firing;
  Point weapon;
  void Reset() { firing = {}; weapon = {}; }
};
// One authoritative update; rendering only reads State. No target acquisition
// or difficulty values are guessed here, and native spread remains separate.
inline bool Update(State& state, Point target, double fire_retention,
                   double weapon_retention, double reference_hz,
                   double seconds, double limit) {
  if (!Finite(target) || !std::isfinite(limit) || limit <= 0 || limit > 1)
    return false;
  target.x = std::clamp(target.x, -limit, limit);
  target.y = std::clamp(target.y, -limit, limit);
  State next = state;
  if (!Smooth(next.firing, target, fire_retention, reference_hz, seconds) ||
      !Smooth(next.weapon, target, weapon_retention, reference_hz, seconds))
    return false;
  state = next;
  return true;
}
}
