#pragma once
#include <string>

namespace rex::gpu {
// Existing gamma-ramp pass, with optional presentation grading before FXAA luma.
// Gamma is already applied by the ramp; never apply it twice here.
inline std::string PdzSwapGradeGlsl(bool compute, bool pwl, bool luma, bool rb_swap) {
  std::string s = "#version 450\n#extension GL_EXT_samplerless_texture_functions : require\n";
  if (compute) s += "layout(local_size_x=16,local_size_y=8) in;\n";
  s += R"(
layout(push_constant) uniform Constants {
  uvec2 size;
  layout(offset=16) vec4 grade;
};
)";
  s += pwl ? "layout(set=0,binding=0) uniform utextureBuffer ramp;\n"
           : "layout(set=0,binding=0) uniform textureBuffer ramp;\n";
  s += "layout(set=1,binding=0) uniform texture2D source_image;\n";
  if (compute) s += luma
      ? "layout(set=2,binding=0,rgba16f) writeonly uniform image2D destination;\n"
      : "layout(set=2,binding=0,rgb10_a2) writeonly uniform image2D destination;\n";
  else s += "layout(location=0) out vec4 destination;\n";
  if (pwl) s += R"(
float ApplyPwl(uint value, uint channel) {
  uvec4 p = texelFetch(ramp, int((value >> 3u)*3u+channel));
  return clamp((float(p.x)+float((value&7u)*p.y)*0.125)*1.52737048e-05,0.0,1.0);
}
)";
  s += "void main() {\n";
  s += compute ? "ivec2 pixel=ivec2(gl_GlobalInvocationID.xy); if(any(greaterThanEqual(uvec2(pixel),size))) return;\n"
               : "ivec2 pixel=ivec2(gl_FragCoord.xy);\n";
  s += "uvec3 sample_value=uvec3(texelFetch(source_image,pixel,0).rgb*";
  s += pwl ? "1023.0" : "255.0";
  s += "+vec3(0.5))";
  if (rb_swap) s += ".bgr";
  s += ";\nvec3 rgb=";
  if (pwl) s += "vec3(ApplyPwl(sample_value.r,0u),ApplyPwl(sample_value.g,1u),ApplyPwl(sample_value.b,2u));\n";
  else if (rb_swap) s += "vec3(texelFetch(ramp,int(sample_value.r)).b,texelFetch(ramp,int(sample_value.g)).g,texelFetch(ramp,int(sample_value.b)).r);\n";
  else s += "vec3(texelFetch(ramp,int(sample_value.r)).r,texelFetch(ramp,int(sample_value.g)).g,texelFetch(ramp,int(sample_value.b)).b);\n";
  s += R"(
if (any(notEqual(grade.xyz,vec3(1.0,1.0,0.0)))) {
  rgb=(rgb-vec3(0.5))*grade.x+vec3(0.5);
  float y=dot(rgb,vec3(0.299,0.587,0.114));
  rgb=vec3(y)+(rgb-vec3(y))*grade.y;
  rgb=clamp(rgb*(vec3(1.0)+grade.z*vec3(1.0,-0.413/0.587,1.0)),0.0,1.0);
}
)";
  std::string result = luma ? "vec4(rgb,dot(rgb,vec3(0.299,0.587,0.114)))" : "vec4(rgb,1.0)";
  s += compute ? "imageStore(destination,pixel,"+result+");\n" : "destination="+result+";\n";
  return s+"}\n";
}
}  // namespace rex::gpu
