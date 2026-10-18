#include <rex/graphics/display_grade.h>
#include <cassert>
#include <cstdio>
#include <limits>
using rex::gpu::ApplyDisplayGrade;
using rex::gpu::DisplayGrade;
static bool Near(float a, float b) { return std::abs(a-b) < 0.00001f; }
int main() {
  size_t checks = 0;
  for (int r=0;r<=16;++r) for (int g=0;g<=16;++g) for (int b=0;b<=16;++b) {
    std::array<float,3> rgb = {r/16.f,g/16.f,b/16.f};
    assert(ApplyDisplayGrade(rgb, {}) == rgb);
    auto grey = ApplyDisplayGrade(rgb, {1,0,0});
    assert(Near(grey[0],grey[1]) && Near(grey[1],grey[2]));
    assert(Near(grey[0],rgb[0]*.299f+rgb[1]*.587f+rgb[2]*.114f));
    for (float contrast : {0.f,1.f,2.f}) for (float saturation : {0.f,1.f,2.f})
      for (float tint : {-.5f,0.f,.5f}) {
        auto result = ApplyDisplayGrade(rgb,{contrast,saturation,tint});
        for (auto channel : result) assert(std::isfinite(channel) && channel >= 0 && channel <= 1);
        ++checks;
      }
  }
  std::array<float,3> grey = {.5f,.5f,.5f};
  auto magenta = ApplyDisplayGrade(grey,{1,1,.25f});
  auto green = ApplyDisplayGrade(grey,{1,1,-.25f});
  assert(magenta[0] > magenta[1] && green[1] > green[0]);
  assert(Near(.299f*magenta[0]+.587f*magenta[1]+.114f*magenta[2],.5f));
  assert(Near(ApplyDisplayGrade({.25f,.5f,.75f},{2,1,0})[0],0));
  assert(Near(ApplyDisplayGrade({.25f,.5f,.75f},{2,1,0})[2],1));
  for (double invalid : {-1.,3.,std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity()})
    assert(rex::gpu::SanitizeGradeValue(invalid,0,2,1) == 1);
  std::printf("PASS: %zu grading combinations, identity, greyscale, contrast, tint, bounds, invalid values\n", checks);
}
