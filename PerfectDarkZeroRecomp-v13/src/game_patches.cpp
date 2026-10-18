#include "generated/default/perfectdarkzerorecomp_init.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#include "game_cvars.h"
#include "game_constants.h"
#include "game_patches.h"

namespace game_patches
{
  bool IsEnabled(PatchId patch)
  {
    switch (patch)
    {
    case PatchId::Fps60Unlock:
      return REXCVAR_GET(pdz_fps60_unlock);
    case PatchId::AspectRatio16_9:
      return REXCVAR_GET(pdz_aspect_ratio_16_9);
    }
    return false;
  }
} // namespace game_patches

namespace
{
  bool WriteGuestBytes(uint32_t address, const uint8_t *bytes, uint32_t size)
  {
    if (!bytes || !size)
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: empty patch data", address);
      return false;
    }

    auto *rt = rex::Runtime::instance();
    auto *memory = rt ? rt->memory() : nullptr;
    auto *heap = memory ? memory->LookupHeap(address) : nullptr;
    if (!heap)
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: no guest heap", address);
      return false;
    }

    const uint64_t last_address = static_cast<uint64_t>(address) + size - 1;
    if (last_address > UINT32_MAX ||
        address / heap->page_size() != last_address / heap->page_size())
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: range crosses a guest page",
                  address);
      return false;
    }

    constexpr uint32_t writable = rex::memory::kMemoryProtectRead |
                                  rex::memory::kMemoryProtectWrite;
    uint32_t old_protect{};
    if (!heap->Protect(address, size, writable, &old_protect))
    {
      REXLOG_WARN("Cannot make guest range {:08X}+{:X} writable for game patch", address, size);
      return false;
    }

    std::memcpy(memory->TranslateVirtual(address), bytes, size);
    if (!heap->Protect(address, size, old_protect, nullptr))
    {
      REXLOG_WARN("Cannot restore guest protection for game patch at {:08X}", address);
      return false;
    }
    return true;
  }

  void ApplyAspectRatioPatch()
  {
    if (!game_patches::IsEnabled(game_patches::PatchId::AspectRatio16_9))
    {
      return;
    }

    const auto patch = GameConstants::PatchConstants::AspectRatio();
    const std::array<uint8_t, 4> bytes{
        static_cast<uint8_t>(patch.value >> 24),
        static_cast<uint8_t>(patch.value >> 16),
        static_cast<uint8_t>(patch.value >> 8),
        static_cast<uint8_t>(patch.value)};
    WriteGuestBytes(static_cast<uint32_t>(patch.address), bytes.data(),
                    static_cast<uint32_t>(bytes.size()));
  }
} // namespace

REX_EXTERN(__imp__sub_826CE6B8);

REX_HOOK_RAW(sub_826CE6B8)
{
  const auto patch = GameConstants::PatchConstants::Fps60();
  if (ctx.lr == patch.call_site &&
      game_patches::IsEnabled(game_patches::PatchId::Fps60Unlock))
  {
    ctx.r7.u32 = (ctx.r7.u32 & patch.preserve_mask) | patch.set_bits;
  }

  __imp__sub_826CE6B8(ctx, base);
}

REX_EXTERN(__imp__sub_82AC4CB8);

REX_HOOK_RAW(sub_82AC4CB8)
{
  const uint32_t state_address = ctx.r4.u32;
  __imp__sub_82AC4CB8(ctx, base);

  if (!base || !state_address || ctx.r3.u32 != 0)
  {
    return;
  }

  uint8_t *state = base + state_address;
  const auto read_be_i16 = [](const uint8_t *bytes)
  {
    const uint16_t value = (uint16_t(bytes[0]) << 8) | bytes[1];
    return static_cast<int16_t>(value);
  };
  const auto write_be_i16 = [](uint8_t *bytes, int16_t value)
  {
    const auto unsigned_value = static_cast<uint16_t>(value);
    bytes[0] = static_cast<uint8_t>(unsigned_value >> 8);
    bytes[1] = static_cast<uint8_t>(unsigned_value);
  };

  int16_t right_x = read_be_i16(state + 12);
  int16_t right_y = read_be_i16(state + 14);

  // The MNK driver drains its accumulated mouse delta on every poll
  // Hold the last nonzero sample and let it decay over a short window instead.
  static std::atomic<int64_t> last_nonzero_time_ms{0};
  static std::atomic<int16_t> held_x{0};
  static std::atomic<int16_t> held_y{0};
  constexpr int64_t kHoldWindowMs = 80;

  const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();

  if (right_x != 0 || right_y != 0)
  {
    held_x.store(right_x, std::memory_order_relaxed);
    held_y.store(right_y, std::memory_order_relaxed);
    last_nonzero_time_ms.store(now_ms, std::memory_order_relaxed);
  }
  else
  {
    const int64_t elapsed = now_ms - last_nonzero_time_ms.load(std::memory_order_relaxed);
    if (elapsed >= 0 && elapsed < kHoldWindowMs)
    {
      const double decay = 1.0 - static_cast<double>(elapsed) / static_cast<double>(kHoldWindowMs);
      right_x = static_cast<int16_t>(held_x.load(std::memory_order_relaxed) * decay);
      right_y = static_cast<int16_t>(held_y.load(std::memory_order_relaxed) * decay);
      write_be_i16(state + 12, right_x);
      write_be_i16(state + 14, right_y);
    }
  }
}

namespace game_patches
{
  void ApplyEnabledPatches()
  {
    ApplyAspectRatioPatch();
  }
} // namespace game_patches
