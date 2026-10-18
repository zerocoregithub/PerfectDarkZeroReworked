#include "../src/pdz_cover_camera_anchor.h"
#include <cassert>
#include <cstdio>
#include <limits>

static std::array<float,16> View(float x, float y, float z, float yaw=0) {
  const float c=std::cos(yaw), s=std::sin(yaw);
  std::array<float,16> m{c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1};
  pdz::SetCoverViewEye(m,{x,y,z}); return m;
}
static bool Near(float a,float b) { return std::abs(a-b)<.001f; }
int main() {
  for (int hz : {30,60,120,240}) {
    pdz::CoverCameraAnchor a;
    auto v=View(10,20,30);
    assert(!a.Apply(v,1,2,3,{0,0,0},0,true));
    // Entry settles before an anchor is established.
    for(int t=1;t<200;t+=1000/hz) {
      v=View(10,20,30);
      assert(!a.Apply(v,1,2,3,{0,0,0},t,true));
    }
    for(int t=200;t<600;t+=1000/hz) {
      v=View(40,25,50,.4f);
      const auto orientation=v;
      assert(a.Apply(v,1,2,3,{30,0,20},t,true));
      const auto e=pdz::CoverViewEye(v);
      assert(Near(e[0],10)&&Near(e[1],25)&&Near(e[2],30));
      for(int i=0;i<12;++i) assert(v[i]==orientation[i]);
    }
    // Exit/disabled identity is exact, no animation or transform writes.
    v=View(40,25,50); const auto native=v;
    assert(!a.Apply(v,1,2,3,{30,0,20},600,false)&&v==native);
    assert(!a.player);
    assert(!a.Apply(v,1,2,3,{30,0,20},610,true));
    assert(!a.Apply(v,1,2,4,{30,0,20},620,true)); // New attachment.
    assert(!a.Apply(v,5,2,4,{30,0,20},630,true)); // New player.
    assert(!a.Apply(v,5,6,4,{30,0,20},640,true)); // New camera.
    assert(!a.Apply(v,5,6,4,{30,0,20},1000,true)); // Stale update.
    assert(!a.Apply(v,5,6,4,{30,0,20},900,true)); // Clock rollback.
    v=View(400,25,50);
    assert(!a.Apply(v,5,6,4,{30,0,20},1100,true)&&!a.player);
    v=View(0,0,0);
    a.Apply(v,1,2,3,{0,0,0},1200,true);
    assert(!a.Apply(v,1,2,3,{500,0,0},1400,true)&&!a.player);
    v[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!a.Apply(v,1,2,3,{0,0,0},1410,true)&&!a.player);
  }
  std::puts("PASS: cover anchor, height/rotation preservation, exit, identities, bounds and 30/60/120/240 Hz");
  for(int hz:{30,60,120,240}) for(float side:{-40.f,0.f,40.f}) {
    pdz::CoverCameraAnchor a;
    auto view=View(10,20,30);
    a.Apply(view,1,2,3,{0,0,0},0,true);
    for(int t=1;t<200;t+=1000/hz) {
      view=View(10,20,30);a.Apply(view,1,2,3,{0,0,0},t,true);
    }
    for(int t=200;t<1400;t+=1000/hz) {
      view=View(10+side,25,30,.4f);const auto native=view;
      a.Apply(view,1,2,3,{side,0,0},t,true,.35f);
      const auto eye=pdz::CoverViewEye(view);
      assert(std::abs(eye[0]-(10+side*.35f))<=std::abs(side*.35f)+.001f);
      for(int i=0;i<12;i++) assert(view[i]==native[i]);
      assert(Near(eye[1],25));
    }
    assert(std::abs(a.follow_weight-.35f)<.001f);
    view=View(10+side,25,30);
    a.Apply(view,1,2,4,{side,0,0},1400,true,.35f);
    assert(a.follow_weight==0); // Identity reset must discard presentation easing.
    a.Apply(view,1,2,4,{side,0,0},1410,false,.35f);
    assert(!a.player&&a.follow_weight==0);
  }
  std::puts("PASS: bounded follow, both directions/centre, native orientation/height, identities and disable");
}
