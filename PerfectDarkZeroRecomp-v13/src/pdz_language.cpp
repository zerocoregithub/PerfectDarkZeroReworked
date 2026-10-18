// Selectively integrated from upstream 4cda209 game_patches.cpp.
// Keep separate: our app/input own the other upstream patch and mouse sites.
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/memory/utils.h>
#include <rex/system/flags.h>
#include "game_constants.h"

REX_EXTERN(__imp__sub_82394750);
REX_HOOK_RAW(sub_82394750) {
  const auto patch = GameConstants::PatchConstants::Language();
  if (ctx.lr == patch.call_site && base) {
    uint32_t language = REXCVAR_GET(user_language);
    if (language < 1 || language > patch.max_language) language = 1;
    rex::memory::store_and_swap<uint32_t>(base + patch.address, language);
  }
  __imp__sub_82394750(ctx, base);
}
