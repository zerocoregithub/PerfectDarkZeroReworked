#include "../src/pdz_cover_target_ray.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
  for(float offset : {-80.f,-40.f,0.f,40.f,80.f}) {
    std::array<float,3> eye{offset,25,30}, origin{10,20,30};
    std::array<float,3> start{12,23,40},end{210,320,1030};
    const auto a=start,b=end;
    assert(pdz::AlignCoverTargetRay(eye,origin,start,end));
    for(int i=0;i<3;++i) {
      assert(std::abs((start[i]-eye[i])-(a[i]-origin[i]))<.001f);
      assert(std::abs((end[i]-start[i])-(b[i]-a[i]))<.001f);
    }
    // Ray crosses the same camera-relative target at near and far distances;
    // no fixed screen-space correction or changed weapon spread.
    for(float t : {.01f,.2f,.8f})
      for(int i=0;i<3;++i)
        assert(std::abs((start[i]+t*(end[i]-start[i])-eye[i])-
                       (a[i]+t*(b[i]-a[i])-origin[i]))<.001f);
  }
  std::array<float,3> eye{0,0,0},origin{0,0,0},s{0,0,1},e{0,0,100};
  const auto original_s=s,original_e=e;
  assert(!pdz::AlignCoverTargetRay(eye,origin,s,e)&&s==original_s&&e==original_e);
  eye[0]=1000;
  assert(!pdz::AlignCoverTargetRay(eye,origin,s,e)&&s==original_s&&e==original_e);
  eye[0]=std::numeric_limits<float>::quiet_NaN();
  assert(!pdz::AlignCoverTargetRay(eye,origin,s,e)&&s==original_s&&e==original_e);
  std::puts("PASS: camera-origin target ray alignment, both sides, near/far targets, direction preservation and invalid input");
}
