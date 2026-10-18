#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
namespace pdz::input {
struct Point { double x=0,y=0; };
inline double StickExponent(std::string_view mode) {
  return mode=="extra_wide"?4.:mode=="wide"?3.:mode=="relaxed"?2.:1.;
}
inline Point Curve(Point p,double inner,double outer,double exponent) {
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(inner)||
     !std::isfinite(outer)||!std::isfinite(exponent)||exponent<1||inner<0||outer<=inner||outer>1) return {};
  const double length=std::hypot(p.x,p.y);
  if(length<=inner) return {};
  double magnitude=std::clamp((length-inner)/(outer-inner),0.,1.);
  if(exponent==2.)magnitude*=magnitude; // Preserve the existing Relaxed curve.
  else if(exponent!=1.)magnitude=std::pow(magnitude,exponent);
  return {p.x/length*magnitude,p.y/length*magnitude};
}
inline Point Curve(Point p,double inner,double outer,bool relaxed) {
  return Curve(p,inner,outer,relaxed?2.:1.);
}
inline constexpr std::array<uint16_t,14> masks={1,2,4,8,16,32,64,128,256,512,4096,8192,16384,32768};
struct Response {
  Point value;double held=0;
  void Reset(){value={};held=0;}
  Point Update(Point target,double dt,double easing,double acceleration) {
    if(!std::isfinite(dt)||dt<0||dt>.1){Reset();return {};}
    held=std::hypot(target.x,target.y)>.95?std::min(.2,held+dt):0.;
    const double weight=easing>0?-std::expm1(-dt/easing):1.;
    value.x=std::lerp(value.x,target.x,weight);value.y=std::lerp(value.y,target.y,weight);
    const double boost=1.+acceleration*held/.2;
    return {value.x*boost,value.y*boost};
  }
};
struct Buttons {uint16_t bits=0;uint8_t lt=0,rt=0;};
inline Buttons Remap(Buttons source,const std::array<int,16>& map) {
  Buttons result{uint16_t(source.bits&1024),0,0}; // Guide remains native.
  for(int i=0;i<16;++i) {
    const int target=map[i];if(target<0||target>15) continue;
    const uint8_t pressure=i<14?(source.bits&masks[i]?255:0):i==14?source.lt:source.rt;
    if(target<14) {if(pressure>=30) result.bits|=masks[target];}
    else {auto& trigger=target==14?result.lt:result.rt;trigger=std::max(trigger,pressure);}
  }
  return result;
}
}
