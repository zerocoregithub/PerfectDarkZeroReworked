#include "../src/pdz_aim_retention.h"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
 using namespace pdz::aim;
 Retention state;
 const RetentionRect original{100,100,20,40};
 auto rect=original;
 assert(!state.Expand(7,100,110,120,0,0,rect)); // No initial acquisition boost.
 state.Observe(7,true,100);
 assert(!state.Expand(8,110,110,120,0,0,rect)); // No boost to other enemies.
 assert(state.Expand(7,110,123,120,0,0,rect));
 assert(rect.width==30&&rect.height==60);
 rect=original;
 assert(state.Expand(7,120,127,120,-4,0,rect)); // Reticle offset, not camera centre.
 rect=original;
 assert(state.Expand(7,130,128,120,0,0,rect)); // Brief outer-edge grace.
 state.Observe(7,true,200); // Continuous acceptance does not extend outer grace.
 rect=original;
 assert(!state.Expand(7,280,128,120,0,0,rect));
 assert(!state.target);
 state.Observe(7,true,300);rect=original;
 assert(!state.Expand(7,310,131,120,0,0,rect)); // Deliberate aim away.
 state.Observe(7,true,400);state.Observe(7,false,410);assert(state.Active(7,410));
 state.Observe(7,false,649);assert(state.Active(7,649)); // Failure does not refresh timeout.
 state.Observe(7,false,651);assert(!state.target);
 state.Observe(7,true,660);state.Observe(8,false,670);assert(!state.target);
 state.Observe(7,true,680);state.Observe(0,false,690);assert(!state.target); // Selection veto.
 assert(AssistanceOwnsAim(true,7,7,true,100,110));
 assert(!AssistanceOwnsAim(false,7,7,true,100,110));
 assert(!AssistanceOwnsAim(true,7,8,true,100,110));
 assert(!AssistanceOwnsAim(true,7,7,false,100,110));
 assert(!AssistanceOwnsAim(true,7,7,true,100,201));
 assert(!AssistanceOwnsAim(true,7,7,true,100,99));
 state.Observe(7,true,500);rect=original;
 assert(!state.Expand(7,751,110,120,0,0,rect)); // Stale state.
 state.Observe(7,true,800);rect=original;
 assert(!state.Expand(7,810,std::numeric_limits<double>::quiet_NaN(),120,0,0,rect));
 assert(!state.target);
 std::cout<<"PASS: acquisition unchanged, bounded same-target rejection grace, selection veto, aim ownership and invalid/stale rejection\n";
}
