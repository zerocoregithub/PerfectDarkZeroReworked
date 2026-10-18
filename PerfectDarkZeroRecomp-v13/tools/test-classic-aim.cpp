#include "../src/pdz_classic_aim_math.h"
#include "../src/pdz_aim_assist_policy.h"
#include <cassert>
#include <limits>
#include <iostream>

using namespace pdz::aim;
static bool Near(double a, double b) { return std::abs(a - b) < 1e-11; }
int main() {
  assert(Near(EdgeZoomScale(60,60,1),1));
  assert(Near(EdgeZoomScale(60,6,1),.1));
  assert(Near(EdgeZoomScale(60,1,.5),1./120));
  assert(Near(EdgeZoomScale(60,1,2),1./30));
  assert(Near(EdgeZoomScale(60,90,1),1));
  assert(Near(EdgeZoomScale(0,1,.5),.5));
  assert(EdgeZoomScale(60,1,std::numeric_limits<double>::quiet_NaN())==0);
  for(int hz:{30,60,120,240}) {
    FreeAim free; free.point={1,-1}; Point camera,total;
    for(int i=0;i<hz;++i) {
      assert(free.Update({},.5,1./hz,camera));
      total.x+=camera.x*90*EdgeZoomScale(60,1,.5);
      total.y-=camera.y*90*EdgeZoomScale(60,1,.5);
    }
    assert(Near(total.x,.75)&&Near(total.y,.75));
    assert(free.point.x==1&&free.point.y==-1);
  }
  for(double fov:{.001,.01,.049,.05,.051,1.,3.09})assert(ValidAimFov(fov));
  for(double fov:{0.,-.01,3.1,4.,std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::quiet_NaN()})assert(!ValidAimFov(fov));
  // Weapon render easing: equivalent results across frame rates, both axes,
  // continuous release, and no mutation of the authoritative aim point.
  for(int hz:{30,60,120,240}) {
    CursorVisual weapon;
    const Point target{-.3,.2};
    weapon.Update({},0.,.05);
    for(int i=0;i<hz/5;++i) weapon.Update(target,1./hz,.05);
    assert(Near(weapon.point.x,target.x*(1-std::exp(-4.))));
    assert(Near(weapon.point.y,target.y*(1-std::exp(-4.))));
    const Point before=weapon.point;
    weapon.Update({},1./hz,.05);
    assert(Near(weapon.point.x,before.x*std::exp(-1./hz/.05)));
    assert(Near(weapon.point.y,before.y*std::exp(-1./hz/.05)));
    assert(target.x==-.3&&target.y==.2);
  }
  assert(ManualSwayActive(true,true));
  assert(ManualSwayActive(false,false));
  assert(!ManualSwayActive(false,true));
  assert(FreeAimCamera(0,false));
  assert(FreeAimCamera(3,true));
  assert(!FreeAimCamera(3,false));
  for(uint8_t mode:{1,2,4,5,6,7,255}) assert(!FreeAimCamera(mode,true));
  // Render state eases a snap without mutating the raw target/shot state.
  const Point raw{.3,-.2};
  for(int hz:{30,60,120,240}) {
    CursorVisual visual;
    visual.Update({},0,.04);
    const auto first=visual.Update(raw,1./hz,.04);
    assert(first.x>0&&first.x<raw.x&&first.y<0&&first.y>raw.y);
    for(int i=1;i<hz;++i) visual.Update(raw,1./hz,.04);
    assert(Near(visual.point.x,raw.x*(1-std::exp(-25.))));
    assert(raw.x==.3&&raw.y==-.2);
    assert(visual.Update(raw,.016,0).x==raw.x);
    visual.Reset(); assert(!visual.initialized);
    assert(visual.Update(raw,.5,.04).x==raw.x);
  }
  assert(CloseRangeAttraction(0,1.5)==1.5);
  assert(CloseRangeAttraction(500,1.5)==1.5);
  assert(CloseRangeAttraction(1250,1.5)==1.25);
  assert(CloseRangeAttraction(2000,1.5)==1);
  assert(CloseRangeAttraction(100000,2)==1);
  assert(CloseRangeAttraction(-1,2)==1);
  assert(CloseRangeAttraction(0,1)==1);
  // Exact native stationary-input failure: both acceptance radii are zero.
  // The adapter restores native profile radii, without a blanket acceptance.
  const double native_radius=.02, native_outer=.05+.075;
  assert(!(.01<native_radius*OverlayAimStrength(0,true,false,false)));
  assert(.01<native_radius*OverlayAimStrength(0,true,true,false));
  assert(!(.2<native_outer*OverlayAimStrength(0,true,true,false)));
  assert(!FreeAimActive(false,false,true));
  assert(!FreeAimActive(true,true,false));
  assert(FreeAimActive(true,true,true));
  assert(FreeAimActive(true,false,false));
  assert(FreeAimActive(true,false,true));
  const auto quiet=SwayDeadzone({.005,-.01},.01);
  assert(quiet.x==0&&quiet.y==0);
  const auto outside=SwayDeadzone({.02,-.025},.01);
  assert(Near(outside.x,.01)&&Near(outside.y,-.015));
  const auto unchanged=SwayDeadzone({.02,-.025},0);
  assert(Near(unchanged.x,.02)&&Near(unchanged.y,-.025));
  assert(Near(EdgeTurn(.4,.5),0));
  assert(Near(EdgeTurn(.75,.5),.5));
  assert(Near(EdgeTurn(-.75,.5),-.5));
  assert(Near(EdgeTurn(1,.5),1));
  assert(Near(EdgeTurn(1,1),0));
  for(int hz:{30,60,120,240}) {
    FreeAim free;
    Point camera,total;
    for(int i=0;i<hz;++i) {
      assert(free.Update({.3/hz,-.2/hz},.5,1./hz,camera));
      total.x+=camera.x;total.y+=camera.y;
    }
    assert(Near(free.point.x,.3)&&Near(free.point.y,-.2));
    assert(Near(total.x,0)&&Near(total.y,0));
    assert(free.Update({2,-2},.5,1./hz,camera));
    assert(free.point.x==1&&free.point.y==-1);
    assert(Near(camera.x,1./hz)&&Near(camera.y,-1./hz));
    assert(free.Update({},1,1./hz,camera));
    assert(Near(camera.x,0)&&Near(camera.y,0));
    free.Reset();assert(free.point.x==0&&free.point.y==0);
    assert(!free.Update({},.5,.5,camera));
  }
  for (double yaw : {-.48, -.24, -.06, .06, .24, .48}) {
    Point offset;
    assert(WeaponEulerOffset({yaw, .03}, {std::tan(.5),16./9.}, offset));
    assert(offset.x*yaw < 0);
    Ray ray;
    assert(ToRay(offset,{std::tan(.5),16./9.},ray));
    assert(Near(std::atan2(ray.x,ray.z),-yaw));
    assert(Near(std::atan2(ray.y,ray.z),-.03));
  }
  for (double pitch : {-.48, -.24, -.06, .06, .24, .48}) {
    Point offset;
    assert(WeaponEulerOffset({0, pitch}, {std::tan(.5),16./9.}, offset));
    assert(offset.y*pitch < 0);
    Ray ray;
    assert(ToRay(offset,{std::tan(.5),16./9.},ray));
    assert(Near(std::atan2(ray.y,ray.z),-pitch));
    assert(Near(ray.x,0));
  }
  for(double aspect:{4./3.,16./9.,32./9.}) {
    Projection projection{std::tan(.5),aspect};
    Point offset;
    assert(SwayOffset({.24,-.12},projection,offset));
    Ray ray;
    assert(ToRay(offset,projection,ray));
    assert(Near(std::atan2(ray.x,ray.z),.24));
    assert(Near(std::atan2(ray.y,ray.z),-.12));
    // Native spread is added to the same aim center, not replaced.
    Point spread{.01,-.02};
    Point shot{offset.x+spread.x,offset.y+spread.y};
    assert(Near(shot.x-offset.x,spread.x));
    assert(Near(shot.y-offset.y,spread.y));
  }
  for (double aspect : {4./3., 16./9., 32./9.})
    for (double fov : {.1, .5, 1., 2., 2.864, 3., 40., 70., 110.})
      for (Point p : {Point{}, Point{.3, -.2}, Point{-.8, .7}}) {
        Projection projection{std::tan(fov * 3.141592653589793 / 360.), aspect};
        Ray ray;
        Point back;
        assert(ToRay(p, projection, ray));
        assert(Near(std::hypot(ray.x, ray.y, ray.z), 1.));
        assert(ToPoint(ray, projection, back));
        assert(Near(p.x, back.x) && Near(p.y, back.y));
      }
  State reference;
  assert(Update(reference, {.2, -.1}, .963, .979, 240., 1., .5));
  for (int hz : {30, 60, 120, 240}) {
    State s;
    for (int i = 0; i < hz; ++i)
      assert(Update(s, {.2, -.1}, .963, .979, 240., 1./hz, .5));
    assert(Near(s.firing.x, reference.firing.x));
    assert(Near(s.weapon.y, reference.weapon.y));
    for (int i = 0; i < hz * 3; ++i)
      assert(Update(s, {}, .963, .979, 240., 1./hz, .5));
    assert(std::abs(s.firing.x) < 1e-10);
    assert(std::abs(s.weapon.x) < 1e-7);
  }
  State s;
  assert(Update(s, {5, -5}, 0, 0, 240, .01, .25));
  assert(s.firing.x == .25 && s.firing.y == -.25);
  const State before = s;
  assert(!Update(s, {}, .5, -1, 240, .01, .25));
  assert(s.firing.x == before.firing.x && s.weapon.y == before.weapon.y);
  assert(Update(s, {}, 0, 0, 240, 0, .25));
  assert(s.firing.x == before.firing.x);
  Ray ray{1, 2, 3};
  assert(!ToRay({}, {0, 1}, ray));
  assert(ray.x == 1 && ray.y == 2 && ray.z == 3);
  Point point{1, 2};
  assert(!ToPoint({0, 0, -1}, {}, point));
  assert(point.x == 1 && point.y == 2);
  assert(!Update(s, {std::numeric_limits<double>::quiet_NaN(), 0},
                 .5, .5, 240, .01, .25));
  s.Reset();
  assert(s.firing.x == 0 && s.weapon.y == 0);
  std::cout << "Classic aim math: all tests passed\n";
}
