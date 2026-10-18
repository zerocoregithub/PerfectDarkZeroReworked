#include "../src/pdz_startup_hint.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main() {
  pdz::ui::StartupHint hint;
  assert(!hint.Visible(0,false));
  assert(!hint.Visible(std::numeric_limits<double>::quiet_NaN(),true));
  assert(hint.Visible(10,true));
  assert(hint.Visible(14.999,true));
  assert(!hint.Visible(15,true));
  assert(!hint.Visible(20,true));
  assert(!hint.Visible(30,false));
  assert(!hint.Visible(31,true));
  pdz::ui::StartupHint next_launch;
  assert(next_launch.Visible(0,true));
  puts("PASS: hint begins on first valid display, lasts five seconds, never reappears during the session and resets next launch.");
}
