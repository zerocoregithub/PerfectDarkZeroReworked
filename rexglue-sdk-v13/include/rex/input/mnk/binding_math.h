#pragma once
#include <cstdint>
namespace rex::input::mnk {
inline bool BindingModifiersMatch(uint8_t requested,uint8_t live,uint16_t key) {
  // Bare modifiers include their own bit, but still reject unrelated modifiers.
  const uint8_t own=key==0x10||key==0xA0||key==0xA1?1:
                    key==0x11||key==0xA2||key==0xA3?2:
                    key==0x12||key==0xA4||key==0xA5?4:0;
  return uint8_t(requested|own)==live;
}
}
