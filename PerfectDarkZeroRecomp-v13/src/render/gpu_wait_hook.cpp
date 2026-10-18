// sub_826E3968 is the XDK poll behind the command-ring space wait: it returns nonzero to
// keep waiting for the GPU and zero once the wait is over. The original poll does no
// pausing, so the render thread spins flat out while the emulated GPU is behind. Only the
// pause between polls changes; the result is untouched.
#include <chrono>
#include <thread>

#include <rex/hook.h>

#include "../game_cvars.h"

REX_EXTERN(__imp__sub_826E3968);

namespace
{
  enum GpuWaitMode : int32_t
  {
    kBusySpin = 0,
    kYield = 1,
    kSleep = 2,
  };

  constexpr std::chrono::microseconds kSleepPause{200};
} // namespace

REX_HOOK_RAW(sub_826E3968)
{
  __imp__sub_826E3968(ctx, base);
  if (ctx.r3.u32 == 0)
    return;

  switch (REXCVAR_GET(pdz_gpu_wait_mode))
  {
  case kYield:
    std::this_thread::yield();
    break;
  case kSleep:
    std::this_thread::sleep_for(kSleepPause);
    break;
  default:
    break;
  }
}
