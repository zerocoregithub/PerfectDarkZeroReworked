#include "../src/pdz_qol.h"
#include <cassert>

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
#error This test checks ordinary builds with diagnostics disabled.
#endif

int main() {
  // Deliberately invalid input: release stubs must not inspect guest memory.
  auto* invalid = reinterpret_cast<uint8_t*>(1);
  assert(!PDZ_TraceDodgeRoute(invalid,0,0,0,0));
  PDZ_TraceStickCall(0,0,0,0);
  PDZ_TraceCameraRead(0,0);
  PDZ_TraceVisionState(invalid,0);
  PDZ_TraceRcpSubmission(invalid,0,0);
  PDZ_TraceCoverAimOrder(invalid,0,0,0,0,0,0);
}
