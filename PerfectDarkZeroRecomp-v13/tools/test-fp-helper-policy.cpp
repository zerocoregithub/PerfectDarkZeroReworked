#include <immintrin.h>
#include <cstdint>
#include <cstdio>

// Validate both the cached and actual x86 control state, including rounding,
// denormals, pending status flags, and initially unmasked exceptions.
int main() {
  const uint32_t saved = _mm_getcsr();
  for (uint32_t initial = 0; initial != 65536; ++initial) {
    _mm_setcsr(initial);
    uint32_t old_cached = _mm_getcsr();
    old_cached |= 0x1f80;
    _mm_setcsr(old_cached);
    const uint32_t old_actual = _mm_getcsr();

    _mm_setcsr(initial);
    const uint32_t current = _mm_getcsr();
    uint32_t new_cached = current | 0x1f80;
    if (new_cached != current) _mm_setcsr(new_cached);
    const uint32_t new_actual = _mm_getcsr();
    _mm_setcsr(saved);
    if (old_cached != new_cached || old_actual != new_actual) {
      std::printf("Mismatch at %04x\n", initial);
      return 1;
    }
  }
  std::puts("PASS: all 65536 valid MXCSR states produce identical host and cached state");
}
