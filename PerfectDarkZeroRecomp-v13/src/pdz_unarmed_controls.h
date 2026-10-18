#pragma once
#include <cmath>
#include <cstdint>
#include <string_view>
namespace pdz {
inline uint32_t FistViewMask(uint32_t native_mask,bool verified_idle) {
  // User0's proven main-view bit only; retain other views and high flags.
  return verified_idle ? native_mask&~1u : native_mask;
}
// Verified resource name from the native first-person fist animation channel.
// Unknown, attack and outgoing attack clips must remain visible.
inline bool VerifiedIdleFists(std::string_view name) {
  return name=="aid_anim_chars_overlay_fists_fiststandidle"||
         name=="aid_anim_chars_overlay_fists_fistcrouchidle";
}
inline bool VerifiedUnarmedLocomotion(std::string_view name,uint8_t channel) {
  // Recorded holstered movement channels; never classify an overlay/action
  // as locomotion merely because its name contains "walk" or "run".
  if(channel<1||channel>4)return false;
  // Live idle/move/idle capture: these shared AimWalks clips are used by
  // holstered forward/back movement despite the resource's "male" prefix.
  // Every observed sample with these clips restored the cached hand mask.
  if(channel==1&&name=="aid_anim_chars_movement_male_walks_AimWalks_AimStandWalk")return true;
  if(channel==2&&name=="aid_anim_chars_movement_male_walks_AimWalks_AimStandJog")return true;
  if(channel==3&&name=="aid_anim_chars_movement_male_walks_AimWalks_AimStandRun")return true;
  constexpr std::string_view prefix="aid_anim_chars_movement_female_walks_";
  if(!name.starts_with(prefix))return false;
  name.remove_prefix(prefix.size());
  return name=="walk"||name=="jog"||name=="run2"||name=="unarmedsprint"||
         name=="backwardswalk"||name=="backwardsjog"||name=="backwardsrun"||
         name=="strafeleft"||name=="straferight"||
         name=="strafeleftfast"||name=="straferightfast";
}
// Clock-based timing tolerates repeated polls. Pulses remain level-held for
// native rising-edge detection, rather than firing again on every poll.
struct HolsterHold {
  bool held=false, armed=false, consumed=false;
  int64_t started=0, previous=0, pulse_until=0;
  uint16_t pulse=0;
  uint64_t owner=0;
  uint16_t Route(uint16_t buttons,bool enabled,bool gameplay,bool unarmed,
                 uint64_t session,int64_t now) {
    constexpr uint16_t y=0x8000, up=1;
    const bool down=(buttons&y)!=0;
    if(!enabled||!gameplay||owner!=session||now<previous||
       (previous&&now-previous>250)) {
      *this={}; owner=session; previous=now; held=down;
      return !enabled||!gameplay?buttons:uint16_t(buttons&~y);
    }
    previous=now;
    if(down&&!held) {
      started=now; armed=true; consumed=false; pulse=0; pulse_until=0;
    }
    if((down||held)&&armed&&!consumed&&now-started>=500) {
      consumed=true;
      if(!unarmed) { pulse=up; pulse_until=now+50; }
    }
    if(!down&&held) {
      if(armed&&!consumed) { pulse=y; pulse_until=now+50; }
      armed=false;
    }
    held=down;
    return uint16_t((buttons&~y)|(now<pulse_until?pulse:0));
  }
};
inline double BobAmplitude(double value,double neutral,double intensity) {
  if(!std::isfinite(value)||!std::isfinite(neutral)||
     !std::isfinite(intensity)||intensity<0||intensity>1||intensity==1)
    return value;
  return neutral+(value-neutral)*intensity;
}
inline bool ShowUnarmedHands(uint32_t action,bool known,bool trigger,
                             bool clip_present,double phase,bool blending) {
  if(trigger||!known) return true;
  if(action==8) return blending; // Idle, except an outgoing action blend.
  if(!clip_present||!std::isfinite(phase)||phase<0) return true;
  return phase<1||blending; // Preserve complete attack/raise/lower animations.
}
}
