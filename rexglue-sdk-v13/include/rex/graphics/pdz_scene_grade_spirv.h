#pragma once
#include <rex/graphics/pipeline/shader/spirv_builder.h>
#include <SPIRV/GLSL.std.450.h>

namespace rex::graphics {
// Shared actual epilogue for guest translation and isolated Vulkan GPU readback.
inline void EmitPdzSceneGradeSpirv(SpirvBuilder& b, spv::Id color_variable,
                                  spv::Id grade, spv::Id ext_inst_glsl_std_450_) {
  const auto type_float_ = b.makeFloatType(32);
  const auto type_bool_ = b.makeBoolType();
  const auto type_float4_ = b.makeVectorType(type_float_, 4);
  const auto const_float_0_ = b.makeFloatConstant(0.f);
  const auto const_float_1_ = b.makeFloatConstant(1.f);
  spv::Id g[4];
  spv::Id changed = spv::NoResult;
  for (unsigned i = 0; i < 4; ++i) {
    g[i] = b.createCompositeExtract(grade, type_float_, i);
    const auto differs = b.createBinOp(spv::OpFOrdNotEqual, type_bool_, g[i],
                                      i == 3 ? const_float_0_ : const_float_1_);
    changed = i ? b.createBinOp(spv::OpLogicalOr, type_bool_, changed, differs) : differs;
  }
  SpirvBuilder::IfBuilder grading_if(changed, spv::SelectionControlMaskNone, b);
  const spv::Id original = b.createLoad(color_variable, spv::NoPrecision);
  const spv::Id gamma_changed = b.createBinOp(spv::OpFOrdNotEqual, type_bool_, g[0], const_float_1_);
  const spv::Id half = b.makeFloatConstant(.5f);
  spv::Id rgb[3];
  for (unsigned i = 0; i < 3; ++i) {
    const auto channel = b.createCompositeExtract(original, type_float_, i);
    const auto clamped = b.createTriBuiltinCall(type_float_, ext_inst_glsl_std_450_,
        GLSLstd450FClamp, channel, const_float_0_, const_float_1_);
    const auto gamma = b.createBinBuiltinCall(type_float_, ext_inst_glsl_std_450_,
                                             GLSLstd450Pow, clamped, g[0]);
    const auto corrected = b.createTriOp(spv::OpSelect, type_float_, gamma_changed, gamma, channel);
    rgb[i] = b.createBinOp(spv::OpFAdd, type_float_, half,
        b.createBinOp(spv::OpFMul, type_float_, g[1],
            b.createBinOp(spv::OpFSub, type_float_, corrected, half)));
  }
  spv::Id luma = const_float_0_;
  const float weights[3] = {.299f, .587f, .114f};
  for (unsigned i = 0; i < 3; ++i) {
    luma = b.createBinOp(spv::OpFAdd, type_float_, luma,
        b.createBinOp(spv::OpFMul, type_float_, rgb[i], b.makeFloatConstant(weights[i])));
  }
  std::vector<spv::Id> result;
  for (unsigned i = 0; i < 3; ++i) {
    const auto saturated = b.createBinOp(spv::OpFAdd, type_float_, luma,
        b.createBinOp(spv::OpFMul, type_float_, g[2],
            b.createBinOp(spv::OpFSub, type_float_, rgb[i], luma)));
    const auto gain = b.createBinOp(spv::OpFAdd, type_float_, const_float_1_,
        b.createBinOp(spv::OpFMul, type_float_, g[3],
            b.makeFloatConstant(i == 1 ? -.413f / .587f : 1.f)));
    result.push_back(b.createTriBuiltinCall(type_float_, ext_inst_glsl_std_450_,
        GLSLstd450FClamp, b.createBinOp(spv::OpFMul, type_float_, saturated, gain),
        const_float_0_, const_float_1_));
  }
  result.push_back(b.createCompositeExtract(original, type_float_, 3));
  b.createStore(b.createCompositeConstruct(type_float4_, result), color_variable);
  grading_if.makeEndIf();
}
}  // namespace rex::graphics
