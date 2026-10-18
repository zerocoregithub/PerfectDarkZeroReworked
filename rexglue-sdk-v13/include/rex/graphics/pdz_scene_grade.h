#pragma once
#include <cstdint>
namespace rex::gpu {
// Proven by matched RenderDoc pixel histories, not texture-size heuristics.
// Both draws composite the scene before subsequent HUD/outline submission.
constexpr bool IsPdzSceneComposite(uint64_t hash) {
  return hash == UINT64_C(0x16C954AF5D9DEBD0) || hash == UINT64_C(0xEDCCE21F19A013D1);
}
// Dedicated unused modification bit prevents reuse of old unmodified pipelines.
constexpr uint64_t kPdzSceneGradeModification = UINT64_C(1) << 63;
}  // namespace rex::gpu
