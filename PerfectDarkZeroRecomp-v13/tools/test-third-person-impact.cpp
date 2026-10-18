#include "../src/pdz_cover_reprojection.h"
#include <cassert>
#include <cstdio>
int main() {
  const pdz::aim::Projection projection{.55,16./9.};
  for(float shoulder:{-80.f,80.f}) for(double depth:{20.,100.,1000.}) {
    std::array<float,16> view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    pdz::SetCoverViewEye(view,{shoulder,0,0});
    pdz::aim::Point screen;double actual_depth;
    assert(pdz::CoverProject(view,projection,{0,0,float(-depth)},screen,actual_depth));
    assert(std::abs(screen.x+shoulder/(depth*.55*(16./9.)))<1e-5);
    assert(screen.y==0&&actual_depth==depth);
    for(double hz:{30.,60.,120.,240.}) {
      pdz::aim::CursorVisual visual;visual.Update({0,0},0,.035);
      double previous=0;
      for(int i=0;i<int(hz*.2);++i) {
        const auto p=visual.Update(screen,1/hz,.035);
        assert(std::abs(p.x)>=previous&&std::abs(p.x)<=std::abs(screen.x));
        previous=std::abs(p.x);
      }
      assert(std::abs(visual.point.x-screen.x)<.01*std::abs(screen.x));
      visual.Reset();assert(!visual.initialized);
    }
    assert(!pdz::CoverProject(view,projection,{0,0,10},screen,actual_depth));
  }
  puts("PASS: impact projection both shoulders/near-far, behind-camera rejection, monotonic render easing/reset at 30-240Hz");
}
