#pragma once
#include "pdz_cover_view_math.h"
#include "pdz_classic_aim_math.h"

namespace pdz {
inline bool CoverProject(const std::array<float,16>& view,
                         aim::Projection projection,const std::array<float,3>& world,
                         aim::Point& screen,double& depth) {
  if(!ValidCoverView(view)||!aim::Valid(projection)) return false;
  std::array<double,3> local{};
  for(unsigned a=0;a<3;++a) {
    if(!std::isfinite(world[a])) return false;
    local[a]=view[12+a];
    for(unsigned r=0;r<3;++r) local[a]+=double(world[r])*view[r*4+a];
  }
  depth=-local[2];
  if(!std::isfinite(depth)||depth<=.001) return false;
  const aim::Point result{local[0]/(depth*projection.tan_half_vertical*projection.aspect),
                          local[1]/(depth*projection.tan_half_vertical)};
  if(!aim::Finite(result)) return false;
  screen=result; return true;
}
// Reconstruct on the actual collision-depth plane, not an invented aim distance.
// This allows unspread/manual cursor easing to remain separate from shot spread.
inline bool CoverUnproject(const std::array<float,16>& view,
                           aim::Projection projection,aim::Point screen,double depth,
                           std::array<float,3>& world) {
  if(!ValidCoverView(view)||!aim::Valid(projection)||!aim::Finite(screen)||
     !std::isfinite(depth)||depth<=.001) return false;
  const auto eye=CoverViewEye(view);
  const std::array<double,3> local{screen.x*depth*projection.tan_half_vertical*projection.aspect,
                                 screen.y*depth*projection.tan_half_vertical,-depth};
  std::array<float,3> result{};
  for(unsigned r=0;r<3;++r) {
    double value=eye[r];
    for(unsigned a=0;a<3;++a) value+=local[a]*view[r*4+a];
    result[r]=float(value);
    if(!std::isfinite(result[r])) return false;
  }
  world=result; return true;
}
inline bool ReprojectCoverCursor(const std::array<float,16>& aim_view,
                                 aim::Projection aim_projection,double depth,
                                 const std::array<float,16>& display_view,
                                 aim::Projection display_projection,aim::Point& cursor) {
  std::array<float,3> world{};
  aim::Point result; double display_depth=0;
  if(!CoverUnproject(aim_view,aim_projection,cursor,depth,world)||
     !CoverProject(display_view,display_projection,world,result,display_depth)) return false;
  // Do not clamp: clamping would falsely imply an off-screen target was on-screen.
  cursor=result; return true;
}
}
