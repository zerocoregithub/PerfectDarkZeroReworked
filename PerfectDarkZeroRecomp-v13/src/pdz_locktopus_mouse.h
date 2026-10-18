#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pdz {
// A persistent analog position, not a mouse velocity. No puzzle solutions,
// target coordinates, tolerance or native hold timers are consulted here.
struct LocktopusMouse {
  uint32_t owner=0;
  uint64_t epoch=0;
  int64_t time=0;
  double last_x=0,last_y=0,x=0,y=0;
  bool initialized=false,claimed=false;
  void Reset() { *this={}; }
  bool Update(uint32_t id,uint64_t generation,int64_t now,bool eligible,
              double total_x,double total_y,double& horizontal,double& vertical) {
    if(!eligible||!id||!std::isfinite(total_x)||!std::isfinite(total_y)||
       !std::isfinite(horizontal)||!std::isfinite(vertical)) { Reset(); return false; }
    if(!initialized||owner!=id||epoch!=generation||now<time||now-time>150) {
      Reset(); initialized=true; owner=id; epoch=generation;
      last_x=total_x; last_y=total_y;
      time=now; return false; // Never import movement from outside this puzzle.
    }
    const double dx=total_x-last_x,dy=total_y-last_y;
    last_x=total_x; last_y=total_y; time=now;
    // Any held native stick/keyboard deflection wins, not only its first edge.
    if(std::abs(horizontal)>0.01||std::abs(vertical)>0.01) {
      claimed=false; return false;
    }
    if(dx!=0||dy!=0) {
      if(!claimed) { x=std::clamp(horizontal,-1.,1.); y=std::clamp(vertical,-1.,1.); }
      claimed=true;
      x+=dx/250.; y-=dy/250.;
      const double radius=std::hypot(x,y);
      if(radius>1.) { x/=radius; y/=radius; }
    }
    if(!claimed) return false;
    horizontal=x; vertical=y; return true;
  }
};
}
