#pragma once
#include <rex/graphics/format/dxbc.h>
namespace rex::graphics {
// Shared by the guest translator and an isolated GPU bytecode/readback test.
inline void EmitPdzSceneGrade(dxbc::Assembler& a, uint32_t color, uint32_t temp,
                             dxbc::Src constants) {
  // Bit-exact neutral route: don't touch color or alpha if 1/1/1/0.
  a.OpNE(dxbc::Dest::R(temp), constants, dxbc::Src::LF(1,1,1,0));
  a.OpOr(dxbc::Dest::R(temp,1), dxbc::Src::R(temp).Select(0), dxbc::Src::R(temp).Select(1));
  a.OpOr(dxbc::Dest::R(temp,4), dxbc::Src::R(temp).Select(2), dxbc::Src::R(temp).Select(3));
  a.OpOr(dxbc::Dest::R(temp,1), dxbc::Src::R(temp).Select(0), dxbc::Src::R(temp).Select(2));
  a.OpIf(true, dxbc::Src::R(temp).Select(0));
  // Gamma acts on the final scene-composite RGB, never HUD or alpha.
  a.OpNE(dxbc::Dest::R(temp,1), constants.Select(0), dxbc::Src::LF(1));
  a.OpIf(true, dxbc::Src::R(temp).Select(0));
  a.OpMov(dxbc::Dest::R(color,7), dxbc::Src::R(color), true);
  a.OpLog(dxbc::Dest::R(color,7), dxbc::Src::R(color));
  a.OpMul(dxbc::Dest::R(color,7), dxbc::Src::R(color), constants.Select(0));
  a.OpExp(dxbc::Dest::R(color,7), dxbc::Src::R(color));
  a.OpEndIf();
  a.OpAdd(dxbc::Dest::R(color,7), dxbc::Src::R(color), dxbc::Src::LF(-.5f));
  a.OpMAd(dxbc::Dest::R(color,7), dxbc::Src::R(color), constants.Select(1), dxbc::Src::LF(.5f));
  a.OpDP3(dxbc::Dest::R(temp,1), dxbc::Src::R(color), dxbc::Src::LF(.299f,.587f,.114f,0));
  a.OpAdd(dxbc::Dest::R(color,7), dxbc::Src::R(color), -dxbc::Src::R(temp).Select(0));
  a.OpMAd(dxbc::Dest::R(color,7), dxbc::Src::R(color), constants.Select(2), dxbc::Src::R(temp).Select(0));
  a.OpMAd(dxbc::Dest::R(temp,7), constants.Select(3),
           dxbc::Src::LF(1,-.413f/.587f,1,0), dxbc::Src::LF(1));
  a.OpMul(dxbc::Dest::R(color,7), dxbc::Src::R(color), dxbc::Src::R(temp), true);
  a.OpEndIf();

}
}  // namespace rex::graphics
