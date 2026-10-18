#include "../src/pdz_npc_damage_math.h"
#include <cassert>
#include <cstdio>

int main() {
  assert(pdz::IsClassicNpcHealthPath(0x8251EBB0u, 0x8251EDE8u));
  assert(!pdz::IsClassicNpcHealthPath(0x8259B738u, 0x82599BF0u)); // Joanna.
  assert(!pdz::IsClassicNpcHealthPath(0x82537320u, 0x8251EDE8u)); // Base/unknown.
  assert(!pdz::IsClassicNpcHealthPath(0x8251EBB0u, 0));
  for (double damage : {.1, 1., 5., 10., 50., 100., 1000.}) {
    assert(pdz::ClassicNpcHealthLoss(damage, false, .5) == damage);
    assert(pdz::ClassicNpcHealthLoss(damage, true, 1) == damage);
    for (double scale : {.25, .35, .5, .75}) {
      const double loss = pdz::ClassicNpcHealthLoss(damage, true, scale);
      assert(std::abs(loss - damage / scale) < 1e-4);
      assert(loss >= damage);
      for (double health : {1., 10., 50., 100., 1000.})
        if (damage >= health) assert(loss >= health); // Never undo a lethal hit.
    }
  }
  for (double damage : {-100., -1., 0.})
    assert(pdz::ClassicNpcHealthLoss(damage, true, .5) == damage);
  assert(pdz::ClassicNpcHealthLoss(5, true, .1) == 5);
  assert(pdz::ClassicNpcHealthLoss(5, true, 2) == 5);
  assert(pdz::ClassicNpcHealthLoss(5, true, std::numeric_limits<double>::infinity()) == 5);
  assert(std::isnan(pdz::ClassicNpcHealthLoss(std::numeric_limits<double>::quiet_NaN(), true, .5)));
  const double huge = std::numeric_limits<float>::max();
  assert(pdz::ClassicNpcHealthLoss(huge, true, .25) == huge);
  std::puts("PASS: NPC/player type gates, disabled identity, health-loss scale, lethal-hit preservation, invalid values and overflow guards");
}
