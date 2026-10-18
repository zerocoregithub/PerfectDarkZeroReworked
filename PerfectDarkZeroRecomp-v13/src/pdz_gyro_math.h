#pragma once
#include "pdz_input_math.h"
#include <string_view>
namespace pdz::input {
inline double GyroAccelerationGain(double speed,std::string_view curve) {
  if(curve=="none"||!std::isfinite(speed))return 1.;
  double t=std::clamp((speed-30.)/150.,0.,1.);
  if(curve=="relaxed")t*=t;
  else if(curve=="aggressive")t=std::sqrt(t);
  else if(curve!="linear")return 1.;
  return 1.+t;
}
// Estimated up in controller coordinates. Gyro propagation avoids interpreting
// rapid controller translation as tilt; acceleration corrects slow drift.
struct GyroSpace {
  std::array<double,3> up{0,1,0};
  bool ready=false;
  void Reset(){up={0,1,0};ready=false;}
  Point Project(double p,double y,double r,double ax,double ay,double az,double dt,int space) {
    if(!std::isfinite(p)||!std::isfinite(y)||!std::isfinite(r)||!std::isfinite(dt)||dt<=0||dt>.1){Reset();return {};}
    const double length=std::hypot(ax,std::hypot(ay,az));
    const bool usable=std::isfinite(length)&&length>7.8&&length<11.8;
    if(!ready){if(!usable)return {};up={ax/length,ay/length,az/length};ready=true;}
    const auto old=up;
    up[0]+=(r*old[1]-y*old[2])*dt;
    up[1]+=(p*old[2]-r*old[0])*dt;
    up[2]+=(y*old[0]-p*old[1])*dt;
    if(usable){const double blend=-std::expm1(-dt/1.5);for(int i=0;i<3;++i)up[i]=std::lerp(up[i],(i==0?ax:i==1?ay:az)/length,blend);}
    const double norm=std::hypot(up[0],std::hypot(up[1],up[2]));for(auto& component:up)component/=norm;
    if(space==1){const double projected=y*up[1]+r*up[2];return {std::copysign(std::min(std::abs(projected)*1.4142135623730951,std::hypot(y,r)),projected),p};}
    const double horizontal=std::hypot(up[1],up[2]);
    const double reduction=std::clamp((std::max(std::abs(up[1]),std::abs(up[2]))-.125)/.125,0.,1.);
    return {p*up[0]+y*up[1]+r*up[2],horizontal>1e-6?reduction*(p*horizontal-(y*up[1]+r*up[2])*up[0]/horizontal):0};
  }
};
struct GyroFilter {
  Point filtered;
  void Reset(){filtered={};}
  Point Update(double pitch,double yaw,double roll,double dt,double sensitivity,
      double pitch_scale,double yaw_scale,double threshold,double smoothing,int axis,
      bool invert_x,bool invert_y,std::string_view acceleration="none",
      bool tiered=false,double smooth_threshold=10.) {
    if(!std::isfinite(pitch)||!std::isfinite(yaw)||!std::isfinite(roll)||
       !std::isfinite(dt)||dt<=0||dt>.1){Reset();return {};}
    constexpr double degrees=57.29577951308232;
    Point rate{-(axis==1?roll:axis==2?(yaw-roll)*.7071067811865475:yaw)*degrees,
               -pitch*degrees};
    if(invert_x)rate.x=-rate.x;if(invert_y)rate.y=-rate.y;
    // Radial soft threshold suppresses resting noise without a jump at its edge.
    const double length=std::hypot(rate.x,rate.y);
    const double weight=length>threshold?(length-threshold)/length:0;
    rate.x*=weight;rate.y*=weight;
    // Select the bypass using physical speed, independent of acceleration gain.
    const double speed=std::hypot(rate.x,rate.y);
    const double gain=GyroAccelerationGain(speed,acceleration);
    rate.x*=gain;rate.y*=gain;
    if(tiered&&smoothing>0&&smooth_threshold>0) {
      const double direct=std::clamp((speed-smooth_threshold*.5)/(smooth_threshold*.5),0.,1.);
      const Point slow{rate.x*(1-direct),rate.y*(1-direct)};
      const Point before=filtered;
      const double blend=-std::expm1(-dt/smoothing);
      filtered.x=std::lerp(filtered.x,slow.x,blend);
      filtered.y=std::lerp(filtered.y,slow.y,blend);
      // Exact integral of the low-pass state over this sample interval. This
      // preserves slow movement area after the tail drains, at varying rates.
      return {(rate.x*direct*dt+slow.x*dt+(before.x-filtered.x)*smoothing)*sensitivity*yaw_scale,
              (rate.y*direct*dt+slow.y*dt+(before.y-filtered.y)*smoothing)*sensitivity*pitch_scale};
    }
    const double blend=smoothing>0?-std::expm1(-dt/smoothing):1;
    filtered.x=std::lerp(filtered.x,rate.x,blend);
    filtered.y=std::lerp(filtered.y,rate.y,blend);
    return {filtered.x*dt*sensitivity*yaw_scale,filtered.y*dt*sensitivity*pitch_scale};
  }
};
}
