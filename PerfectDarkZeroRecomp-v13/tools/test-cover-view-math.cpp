#include "../src/pdz_cover_view_math.h"
#include <cassert>
#include <cstdio>
#include <limits>

static std::array<float,16> View(float yaw, float pitch) {
  const float y=yaw*.017453292519943295f, p=pitch*.017453292519943295f;
  const float sy=std::sin(y),cy=std::cos(y),sp=std::sin(p),cp=std::cos(p);
  std::array<float,16> m{sy,-cy*sp,-cy*cp,0, 0,cp,-sp,0,
                        cy,sy*sp,sy*cp,0, 0,0,0,1};
  // Camera looking down is positive pitch: the third column points backwards.
  m[1]=cy*sp; m[6]=sp; m[9]=-sy*sp;
  for(int a=0;a<3;++a) m[12+a]=-(12*m[a]+34*m[4+a]+56*m[8+a]);
  return m;
}
int main() {
  assert(pdz::CanCorrectCoverView(1, 0));
  assert(!pdz::CanCorrectCoverView(1, 1)); // Exit still reports cover active.
  assert(!pdz::CanCorrectCoverView(0, 0));
  assert(!pdz::CanCorrectCoverView(1, 2));
  int cases=0;
  for(float y : {-68.f,-30.f,0.f,30.f,68.f})
    for(float p : {-65.f,-15.f,0.f,15.f,65.f})
      for(float dy : {-20.f,0.f,20.f})
        for(float dp : {-10.f,0.f,10.f}) {
          auto m=View(y,p), expected=View(y+dy,p+dp);
          assert(pdz::RotateCoverView(m,dy,dp));
          for(int i=0;i<16;++i) assert(std::abs(m[i]-expected[i])<.0001f);
          ++cases;
        }
  auto invalid=View(0,0); invalid[0]=2;
  auto unchanged=invalid;
  assert(!pdz::RotateCoverView(invalid,5,5) && invalid==unchanged);
  invalid=View(0,0); invalid[0]=std::numeric_limits<float>::quiet_NaN();
  assert(!pdz::RotateCoverView(invalid,5,5));
  invalid=View(0,0);
  assert(!pdz::RotateCoverView(invalid,std::numeric_limits<float>::infinity(),0));
  std::printf("PASS: %d yaw/pitch/position cases and invalid-input rejection\n",cases);
}
