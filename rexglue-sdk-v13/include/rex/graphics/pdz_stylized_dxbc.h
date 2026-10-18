#pragma once
#include <rex/graphics/format/dxbc.h>
#include <rex/graphics/pdz_stylized.h>
namespace rex::graphics {
inline void EmitPdzLightingBands(dxbc::Assembler& a,uint32_t reg,uint32_t scratch,const dxbc::Src& input) {
  a.OpMov(dxbc::Dest::R(reg,7),input);
  const auto rgb=dxbc::Src::R(reg);
  const auto level=dxbc::Dest::R(scratch,1),band=dxbc::Dest::R(scratch,2);
  const auto x=dxbc::Src::R(scratch).Select(0),y=dxbc::Src::R(scratch).Select(1);
  a.OpMax(level,rgb.Select(0),rgb.Select(1));
  a.OpMax(level,x,rgb.Select(2));
  a.OpMax(level,x,dxbc::Src::LF(.0001f));
  // Logarithmic light-buffer steps: preserve HDR range and hue, never add a
  // brightness floor to unlit surfaces. Three levels per octave, not RGB posterization.
  a.OpLog(band,x);
  a.OpMul(band,y,dxbc::Src::LF(3.f));
  a.OpRoundNI(band,y);
  a.OpMul(band,y,dxbc::Src::LF(1.f/3.f));
  a.OpExp(band,y);
  a.OpDiv(band,y,x);
  // Keep only a quarter of the extra accumulated-light banding. The actual
  // per-light toon boundaries remain; avoid stacking harsh detail bands.
  a.OpMul(band,y,dxbc::Src::LF(.25f));
  a.OpAdd(band,y,dxbc::Src::LF(.75f));
  a.OpMul(dxbc::Dest::R(reg,7),rgb,y);
}
inline void EmitPdzStylizedVector(dxbc::Assembler& a,uint32_t reg,uint32_t mask,PdzStylizedVector site) {
  // Dot products are emitted in temporary X and replicated by StoreResult.
  // The guest destination mask (Z here) is not the temporary result mask.
  const auto dest=dxbc::Dest::R(reg,site==PdzStylizedVector::Diffuse?1:mask);
  const auto src=site==PdzStylizedVector::Diffuse?
      dxbc::Src::R(reg).Select(0):dxbc::Src::R(reg);
  if(site==PdzStylizedVector::NormalXY) a.OpMul(dest,src,dxbc::Src::LF(0.f));
  if(site==PdzStylizedVector::AmbientBlend) a.OpMov(dest,src,true);
  if(site==PdzStylizedVector::Diffuse) {
    a.OpMov(dest,src,true);
    // Narrow continuous transitions around the original .25/.75 thresholds.
    // X is the only consumed diffuse result; Y is local scratch, not guest Y.
    // This reduces threshold popping without blurring textures or silhouettes.
    const auto upper=dxbc::Dest::R(reg,2);
    const auto y=dxbc::Src::R(reg).Select(1);
    a.OpAdd(upper,src,dxbc::Src::LF(-.72f));
    a.OpMul(upper,y,dxbc::Src::LF(1.f/.06f),true);
    a.OpAdd(dest,src,dxbc::Src::LF(-.22f));
    a.OpMul(dest,src,dxbc::Src::LF(1.f/.06f),true);
    a.OpAdd(dest,src,y);
    a.OpMul(dest,src,dxbc::Src::LF(.5f));
  }
  if(site==PdzStylizedVector::AmbientColor) a.OpMul(dest,src,dxbc::Src::LF(.35f));
}
inline void EmitPdzStylizedSpecular(dxbc::Assembler& a,uint32_t reg) {
  const auto dest=dxbc::Dest::R(reg,1);const auto src=dxbc::Src::R(reg).Select(0);
  a.OpMin(dest,src,dxbc::Src::LF(.25f));
  a.OpMul(dest,src,dxbc::Src::LF(0.f));
}
}
