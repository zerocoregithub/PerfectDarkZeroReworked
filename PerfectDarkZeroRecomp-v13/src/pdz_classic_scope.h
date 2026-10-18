#pragma once
#include "pdz_classic_hud.h"
#include <string_view>

namespace pdz::hud {
inline bool ScopePresentationAllowed(bool fresh,bool owner_matches,bool weapon_matches,
                                     bool aiming_artwork,bool ads_held) {
  return fresh&&owner_matches&&weapon_matches&&(!aiming_artwork||ads_held);
}
inline bool ShowAuxiliaryThirdPersonMarker(bool scope_visible,bool scope_aiming) {
  return !(scope_visible&&scope_aiming);
}
inline bool ValidScopeOwner(uint32_t handle,uint32_t owner,uint32_t registered,
                            uint16_t generation,bool active,uint32_t self) {
  return handle&&active&&(handle>>16)==generation&&registered==owner&&self==owner;
}
inline std::string_view ScopeResourceName(std::string_view name) {
  constexpr std::string_view scheme="DarkXUIAsset://";
  if(name.starts_with(scheme)) name.remove_prefix(scheme.size());
  if(name.ends_with("_xur")) name.remove_suffix(4);
  return name;
}
inline bool ConventionalScopeResource(std::string_view name) {
  name=ScopeResourceName(name);
  constexpr std::string_view small_scope="aid_darkxui_ingame_small_scopes_";
  constexpr std::string_view large_scope="aid_darkxui_ingame_large_scopes_";
  if(name.starts_with(small_scope)) name.remove_prefix(small_scope.size());
  else if(name.starts_with(large_scope)) name.remove_prefix(large_scope.size());
  else return false;
  return name=="sniperrifle" || name=="magsec" || name=="superdragon" ||
         name=="fac16" || name=="dwp5" || name=="shockwave" || name=="rcp90";
}
// This shared asset was verified live for the scoped pistol (native kind 2).
// Do not treat every generic scope/gadget as a conventional weapon sight.
inline bool VerifiedGenericScopeResource(std::string_view name, uint32_t kind) {
  name=ScopeResourceName(name);
  return kind==2 && (name=="aid_darkxui_ingame_large_scopes_basicscope" ||
                    name=="aid_darkxui_ingame_small_scopes_basicscope");
}
// Independent reconstruction of sightDrawZoom/sightDrawAimer geometry in
// DabDavis/perfect-dark-dabs-mod/src/game/sight.c. 320x240 logical viewport;
// center/aspect alignment matches the port, without stretching the aimer box.
inline Mesh ClassicScope(float width, float height, float fov_ratio,
                         bool aiming, Point cursor, Color ink = {0,255,0,255}) {
  Mesh mesh;
  if (!std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1 ||
      !std::isfinite(fov_ratio) || !std::isfinite(cursor.x) || !std::isfinite(cursor.y)) return mesh;
  const float scale=height/240.f;
  const float halfwidth=160.f, halfheight=120.f;
  const float frac=std::clamp(fov_ratio,0.f,1.f);
  const float aspect=(width/height)/(4.f/3.f);
  const float left=width*.5f-(halfwidth-48)*scale*aspect*frac;
  const float right=width*.5f+(halfwidth-48)*scale*aspect*frac;
  const float top=height*.5f-(halfheight-10)*scale*frac;
  const float bottom=height*.5f+(halfheight-10)*scale*frac;
  const float cw=std::min(std::max(5.f,(halfwidth*.5f-60)*std::max(.2f,frac))*scale,right-left);
  const float ch=std::min(std::max(5.f,(halfheight*.5f-22)*std::max(.2f,frac))*scale,bottom-top);
  const float thick=std::max(1.f,scale),half=thick*.5f;
  const auto rect=[&](float l,float t,float r,float b) {
    if(r>l&&b>t) Quad(mesh,{l,t},{r,t},{r,b},{l,b},ink);
  };
  // Horizontal strokes own the corners; vertical strokes meet, never overlap.
  rect(left-half,top-half,left+cw,top+half);
  rect(left-half,top+half,left+half,top+ch);
  rect(right-cw,top-half,right+half,top+half);
  rect(right-half,top+half,right+half,top+ch);
  rect(left-half,bottom-half,left+cw,bottom+half);
  rect(left-half,bottom-ch,left+half,bottom-half);
  rect(right-cw,bottom-half,right+half,bottom+half);
  rect(right-half,bottom-ch,right+half,bottom-half);
  if(!aiming) return mesh;
  const float r=8.f*scale, overlap=2.f*scale;
  const float x=cursor.x,y=cursor.y;
  // Endpoints remain screen-relative; never reverse a segment at an edge.
  const float fixed_left=width*.5f-(halfwidth-48)*scale*aspect;
  const float fixed_right=width*.5f+(halfwidth-48)*scale*aspect;
  const float fixed_top=10.f*scale,fixed_bottom=height-10.f*scale;
  rect(fixed_left,y-half,x-r-half,y+half);
  rect(x+r+half,y-half,fixed_right,y+half);
  rect(x-half,fixed_top,x+half,y-r-half);
  rect(x-half,y+r+half,x+half,fixed_bottom);
  rect(x-r-half,y-r-half,x+r+half,y-r+half);
  rect(x-r-half,y+r-half,x+r+half,y+r+half);
  rect(x-r-half,y-r+half,x-r+half,y+r-half);
  rect(x+r-half,y-r+half,x+r+half,y+r-half);
  // Preserve equal inward tips, partitioned around the square's stroke.
  rect(x-r+half,y-half,x-r+overlap,y+half);
  rect(x+r-overlap,y-half,x+r-half,y+half);
  rect(x-half,y-r+half,x+half,y-r+overlap);
  rect(x-half,y+r-overlap,x+half,y+r-half);
  return mesh;
}
} // namespace pdz::hud
