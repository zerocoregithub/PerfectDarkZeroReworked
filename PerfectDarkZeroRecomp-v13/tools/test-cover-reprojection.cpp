#include "../src/pdz_cover_reprojection.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
  unsigned cases=0;
  for(float yaw:{-80.f,0.f,75.f}) for(float pitch:{-40.f,0.f,35.f})
  for(double distance:{5.,50.,500.,5000.})
  for(double x:{-.8,0.,.8}) for(double y:{-.7,0.,.7})
  for(float side:{-30.f,0.f,30.f}) for(double tangent:{.02,.55}) {
    std::array<float,16> view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    assert(pdz::RotateCoverView(view,yaw,pitch));
    pdz::SetCoverViewEye(view,{100,20,-300});
    const pdz::aim::Projection projection{tangent,1.777777};
    const pdz::aim::Point original{x,y};
    std::array<float,3> target{};
    assert(pdz::CoverUnproject(view,projection,original,distance,target));
    auto display=view;
    pdz::SetCoverViewEye(display,{100+side,20,-300+side*.2f});
    if(side!=0) assert(pdz::RotateCoverView(display,3,-2));
    auto cursor=original;
    pdz::aim::Point expected;double depth;
    const bool visible=pdz::CoverProject(display,projection,target,expected,depth);
    assert(pdz::ReprojectCoverCursor(view,projection,distance,display,projection,cursor)==visible);
    if(visible) {
      assert(std::abs(expected.x-cursor.x)<1e-6 && std::abs(expected.y-cursor.y)<1e-6);
      std::array<float,3> restored{};
      assert(pdz::CoverUnproject(display,projection,cursor,depth,restored));
      for(unsigned i=0;i<3;++i) assert(std::abs(target[i]-restored[i])<.01f);
    }
    ++cases;
  }
  std::array<float,16> view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  pdz::aim::Point cursor{.2,.3};const auto original=cursor;
  assert(!pdz::ReprojectCoverCursor(view,{1,1},0,view,{1,1},cursor));
  assert(cursor.x==original.x&&cursor.y==original.y);
  assert(!pdz::ReprojectCoverCursor(view,{1,1},-1,view,{1,1},cursor));
  view[0]=std::numeric_limits<float>::quiet_NaN();
  assert(!pdz::ReprojectCoverCursor(view,{1,1},100,view,{1,1},cursor));
  std::printf("PASS: %u cover world-point reprojection cases; invalid-depth/view rejection\n",cases);
}
