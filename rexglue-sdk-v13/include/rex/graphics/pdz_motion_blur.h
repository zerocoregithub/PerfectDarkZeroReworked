#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
namespace rex::gpu {
// Native microcode independently disassembled from the one-session dump:
// c1..c6 only, c4 scale/bias maps to packed slot 3. Reject layout drift.
inline bool IsNativePdzMotionBlur(uint64_t hash, uint32_t count,
                                 bool dynamic, std::span<const uint64_t> bitmap) {
  return hash==0x1AEB45F93B4CC719ull && count==6 && !dynamic &&
      bitmap.size()==4 && bitmap[0]==0x7e &&
      bitmap[1]==0 && bitmap[2]==0 && bitmap[3]==0;
}
// Exact DXBC observed in stationary/turning PDZ captures, not a size heuristic.
// Fail closed for other backends, translations or shaders until verified.
inline bool IsCapturedPdzMotionBlur(std::span<const uint8_t> binary) {
  constexpr std::array<uint8_t,16> digest={0x09,0xfa,0x1c,0x96,0x72,0x44,0x83,0xbf,
    0x76,0x3d,0x7b,0xa7,0x7b,0x57,0x02,0xb6};
  return binary.size()>=32 && std::memcmp(binary.data(),"DXBC",4)==0 &&
      std::memcmp(binary.data()+4,digest.data(),digest.size())==0;
}
inline bool DisableCapturedBlurOffsets(void* packed,uint32_t float4_count,bool enabled) {
  if(!enabled||!packed||float4_count!=6)return false;
  // Captured packed CB3[3] transforms the motion vector before the 12 taps.
  // Zero scale AND bias: all sample coordinates collapse to their base UV.
  // Modify upload copy only; leave guest registers, weights and clamps intact.
  std::memset(static_cast<uint8_t*>(packed)+3*4*sizeof(float),0,4*sizeof(float));
  return true;
}
}
