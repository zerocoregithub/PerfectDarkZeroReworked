#include "../src/pdz_weapon_accuracy.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <initializer_list>
int main() {
  for(auto kind:{2u,13u,20u}) {
    assert(pdz::accuracy::Spread(.12,true,kind)==.06);
    assert(pdz::accuracy::Spread(.12,false,kind)==.12);
    assert(pdz::accuracy::Spread(0,true,kind)==0);
    assert(pdz::accuracy::Spread(-1,true,kind)==-1);
  }
  for(auto kind:{0u,15u,26u,UINT32_MAX}) assert(pdz::accuracy::Spread(.12,true,kind)==.12);
  assert(std::isnan(pdz::accuracy::Spread(std::numeric_limits<double>::quiet_NaN(),true,20)));
  std::puts("PASS: exact half spread, vanilla toggle and unknown/nonfinite safety");
}
