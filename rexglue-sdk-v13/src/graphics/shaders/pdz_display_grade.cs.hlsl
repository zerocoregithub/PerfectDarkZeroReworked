// Presentation-only: after the original gamma ramp, before optional FXAA
// and host UI composition. Each thread owns one pixel; no neighbor hazards.
cbuffer GradeConstants : register(b0) {
  uint2 image_size;
  float contrast;
  float saturation;
  float tint;
  uint fxaa_luma;
};
RWTexture2D<float4> image : register(u0);
[numthreads(16, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID) {
  if (any(thread.xy >= image_size)) return;
  float4 pixel = image[thread.xy];
  float3 rgb = (pixel.rgb - 0.5) * contrast + 0.5;
  float luma = dot(rgb, float3(0.299, 0.587, 0.114));
  rgb = luma + (rgb - luma) * saturation;
  rgb *= float3(1.0 + tint, 1.0 - tint * (0.413 / 0.587), 1.0 + tint);
  pixel.rgb = saturate(rgb);
  // The original FXAA input stores display-encoded luma in alpha.
  if (fxaa_luma) pixel.a = dot(pixel.rgb, float3(0.299, 0.587, 0.114));
  image[thread.xy] = pixel;
}
