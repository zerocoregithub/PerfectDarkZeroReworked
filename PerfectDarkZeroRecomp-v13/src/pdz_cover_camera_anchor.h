#pragma once
#include "pdz_cover_view_math.h"
#include <cstdint>
#include <algorithm>

namespace pdz {
// Experimental horizontal anchor. Never changes player/body transforms.
// Native height and rotation remain authoritative. An identity change, large
// displacement, pause, stale update, or exit abandons the anchor immediately.
struct CoverCameraAnchor {
  uint32_t player = 0, camera = 0, attachment = 0;
  int64_t start_ms = 0, last_ms = 0;
  std::array<float, 3> anchor{}, body_anchor{};
  float follow_weight=0;
  void Reset() { *this = {}; }
  bool Apply(std::array<float, 16>& view, uint32_t p, uint32_t c, uint32_t a,
             const std::array<float, 3>& body, int64_t now, bool eligible,
             float follow=0) {
    if (!eligible || !p || !c || !a || !ValidCoverView(view)) {
      Reset(); return false;
    }
    for (float v : body) if (!std::isfinite(v)) { Reset(); return false; }
    const auto eye = CoverViewEye(view);
    for (float v : eye) if (!std::isfinite(v)) { Reset(); return false; }
    const bool changed = player != p || camera != c || attachment != a ||
                         now < last_ms || now - last_ms > 200;
    if (changed) {
      player = p; camera = c; attachment = a;
      start_ms = last_ms = now; anchor = eye; body_anchor = body;
      follow_weight=0;
      return false;
    }
    const auto elapsed=now-last_ms;
    last_ms = now;
    // Allow the native entry camera to settle before capturing its safe eye.
    if (now - start_ms < 200) { anchor = eye; body_anchor = body; return false; }
    // Units are native world units, not assumed metres. Refuse large camera
    // or body displacement instead of pinning the camera across traversal.
    const float eye_move = std::hypot(eye[0]-anchor[0], eye[2]-anchor[2]);
    const float body_move = std::hypot(body[0]-body_anchor[0], body[2]-body_anchor[2]);
    if (eye_move > 100.f || body_move > 100.f) {
      Reset(); return false;
    }
    if(!std::isfinite(follow)) { Reset(); return false; }
    follow=std::clamp(follow,0.f,.35f);
    // Ease only presentation; never feed this into aim/muzzle transforms.
    follow_weight=std::lerp(follow_weight,follow,float(-std::expm1(-elapsed/80.)));
    if (eye_move < .001f) return false;
    SetCoverViewEye(view, {std::lerp(anchor[0],eye[0],follow_weight),eye[1],
                           std::lerp(anchor[2],eye[2],follow_weight)});
    return true;
  }
};
}
