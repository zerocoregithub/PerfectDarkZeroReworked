#pragma once
namespace pdz::ui {
constexpr int DisplayMode(bool fullscreen,bool exclusive) {
  return !fullscreen?0:exclusive?2:1;
}
constexpr const char* DisplayModeLabel(int mode) {
  return mode==2?"Fullscreen":mode==1?"Borderless":"Windowed";
}
constexpr int NextDisplayMode(int mode) {return (mode+1)%3;}
}
