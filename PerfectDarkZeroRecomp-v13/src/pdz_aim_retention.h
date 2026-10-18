#pragma once
#include <cmath>
#include <cstdint>
namespace pdz::aim {
struct RetentionRect { double x=0,y=0,width=0,height=0; };
struct Retention {
  uint32_t target=0;
  int64_t last_applied=0,outside_since=-1;
  void Reset() { *this={}; }
  bool Active(uint32_t candidate,int64_t now) const {
    return candidate && candidate==target && now>=last_applied && now-last_applied<=250;
  }
  void Observe(uint32_t selected,bool applied,int64_t now) {
    if(!selected) { Reset();return; }
    // Selection has its own fresh eligibility/visibility checks. A failed
    // application is not necessarily a visibility veto. Keep only the same
    // previously accepted target, without extending the last-success timeout.
    if(!applied) {
      if(!Active(selected,now))Reset();
      return;
    }
    if(selected!=target||now<last_applied||now-last_applied>250) { Reset();target=selected; }
    last_applied=now;
  }
  bool Expand(uint32_t candidate,int64_t now,double cursor_x,double cursor_y,
              double offset_x,double offset_y,RetentionRect& rect) {
    if(!Active(candidate,now))return false;
    if(!std::isfinite(cursor_x)||!std::isfinite(cursor_y)||
       !std::isfinite(offset_x)||!std::isfinite(offset_y)||
       !std::isfinite(rect.x)||!std::isfinite(rect.y)||
       !std::isfinite(rect.width)||!std::isfinite(rect.height)||rect.width<=0||rect.height<=0) {
      Reset();return false;
    }
    const double cx=rect.x+rect.width*.5,cy=rect.y+rect.height*.5;
    const double dx=std::abs(cursor_x+offset_x-cx)/(rect.width*.5);
    const double dy=std::abs(cursor_y+offset_y-cy)/(rect.height*.5);
    double scale=1.5;
    if(dx<=1.5&&dy<=1.5)outside_since=-1;
    else {
      if(dx>2||dy>2) { Reset();return false; }
      if(outside_since<0)outside_since=now;
      if(now<outside_since||now-outside_since>=150) { Reset();return false; }
      scale=2;
    }
    rect.width*=scale;rect.height*=scale;
    // Shift the query rectangle, not the native output/shot coordinates.
    rect.x=cx-rect.width*.5-offset_x;rect.y=cy-rect.height*.5-offset_y;
    return true;
  }
};
// Assistance owns the final aim while a fresh correction is accepted. Never
// add independent lag/free-aim displacement to that already corrected point.
inline bool AssistanceOwnsAim(bool enabled,uint32_t player,uint32_t observed_player,
                              bool applied,int64_t observed_time,int64_t now) {
  return enabled&&player&&player==observed_player&&applied&&
         now>=observed_time&&now-observed_time<=100;
}
}
