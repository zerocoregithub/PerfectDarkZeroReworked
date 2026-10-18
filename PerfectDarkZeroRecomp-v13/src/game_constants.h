// game_constants.h - Game-specific constants
#pragma once

#include <cstdint>

namespace GameConstants
{
  constexpr uint32_t kCodeBase = 0x82140000;
  constexpr uint32_t kCodeEnd = 0x82CE62AC;
  constexpr uint32_t kImageBase = 0x82000000;
  constexpr uint32_t kImageSize = 0x013D0000;
}

namespace GameConstants::PatchConstants
{
  struct RegisterPatch
  {
    std::uintptr_t call_site;
    std::uint32_t preserve_mask;
    std::uint32_t set_bits;
  };

  struct Patch
  {
    std::uintptr_t address;
    std::uint32_t value;
  };

  constexpr RegisterPatch Fps60()
  {
    return RegisterPatch{
        0x826CFBB0,
        0x0000FFFF,
        0x00010000};
  }

  constexpr Patch AspectRatio()
  {
    return Patch{
        0x820EC158,
        0x4018E38E};
  }

  struct LanguagePatch
  {
    std::uintptr_t call_site;
    std::uint32_t address;
    std::uint32_t max_language;
  };

  // sub_82393540 -> sub_82394750, which builds the loc path from the language global.
  constexpr LanguagePatch Language()
  {
    return LanguagePatch{
        0x82393690,
        0x82D280E8,
        10};
  }
}
