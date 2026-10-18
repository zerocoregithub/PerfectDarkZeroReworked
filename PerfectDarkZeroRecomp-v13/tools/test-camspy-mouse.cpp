#include "../src/pdz_camspy_mouse.h"
#include <cassert>
#include <iostream>
int main() {
  assert(pdz::CamSpyControlled(0x820C93A0, 0x820C1090, 0x135AD00, 0x135AD00, 1));
  assert(!pdz::CamSpyControlled(0x820C93A0, 0x820C1090, 0x135AD00, 0x135AD00, 0));
  assert(!pdz::CamSpyControlled(0x820C93A0, 0x820C1090, 0x135AD00, 0x135AD00, 2));
  assert(!pdz::CamSpyControlled(0x820C93A0, 0x820C1090, 0x135AD00, 0x135AD80, 1));
  assert(!pdz::CamSpyControlled(0x820C93A0, 0x820C1358, 0x135AD00, 0x135AD00, 1));
  assert(!pdz::CamSpyControlled(0x820C0000, 0x820C1090, 0x135AD00, 0x135AD00, 1));
  assert(!pdz::CamSpyControlled(0x820C93A0, 0x820C1090, 0, 0, 1));
  int checks=0;
  for (float yaw : {-720.f, -179.f, -68.f, 0.f, 60.f, 179.f, 720.f}) {
    for (int counts=-1000; counts<=1000; ++counts) {
      float delta=float(counts)*.25f/8.40517241378f;
      const float next=pdz::CamSpyYaw(yaw,delta);
      assert(std::isfinite(next) && std::abs(next)<=180);
      const float angular_change=std::remainder(next-yaw,360.f);
      assert(std::abs(angular_change+delta)<0.0001f);
      ++checks;
    }
  }
  assert(pdz::CamSpyYaw(68, -30) == 98); // Not limited like cover.
  std::cout << "PASS: CamSpy ownership/type/activity gates and " << checks
            << " degree-based mouse deltas, yaw wrapping, unrestricted turn\n";
}
