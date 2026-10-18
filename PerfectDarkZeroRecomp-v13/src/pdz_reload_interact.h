#pragma once
#include <cstdint>
namespace pdz::input {
// One decision per X gesture: changing ammo during reload cannot trigger action.
struct ReloadInteract {
  uint32_t player=0;
  bool held=false,interact=false;
  int64_t pulse_until=0;
  uint16_t Route(uint16_t buttons,uint32_t owner,bool enabled,bool gameplay,
                 bool known,bool can_reload,int64_t now) {
    constexpr uint16_t x=0x4000,a=0x1000;
    if(!enabled||!gameplay||!owner) { *this={}; return buttons; }
    if(player!=owner) { *this={}; player=owner; }
    const bool down=(buttons&x)!=0;
    const bool pressed=down&&!held;
    if(pressed) {
      interact=known&&!can_reload;
      if(interact) pulse_until=now+100;
    }
    held=down;
    // Preserve a tap across repeated polling and a quick physical release.
    // Never extend it while X is held: physical A alone controls held cover.
    if(now<pulse_until) return uint16_t((buttons&~x)|a);
    if(!down) interact=false;
    return interact?uint16_t(buttons&~x):buttons;
  }
};
}
