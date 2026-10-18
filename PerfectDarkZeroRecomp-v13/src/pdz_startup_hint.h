#pragma once
#include <cmath>
namespace pdz::ui {
struct StartupHint {
  double start=-1;
  bool Visible(double now,bool valid_display) {
    if(!valid_display||!std::isfinite(now)||now<0)return false;
    if(start<0)start=now;
    return now>=start&&now-start<5.0;
  }
};
}
