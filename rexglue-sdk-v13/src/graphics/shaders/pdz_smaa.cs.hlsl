cbuffer Constants : register(b0) { uint2 image_size; float2 image_size_inv; };
#define SMAA_RT_METRICS float4(image_size_inv, image_size)
#define SMAA_HLSL_4 1
#define SMAA_PRESET_HIGH 1
#include "../../../thirdparty/smaa/SMAA.hlsl"
Texture2D colorTex : register(t0);
Texture2D auxiliaryTex : register(t1);
Texture2D areaTex : register(t2);
Texture2D searchTex : register(t3);
RWTexture2D<float4> outputTex : register(u0);
[numthreads(16,8,1)]
void edges(uint3 tid : SV_DispatchThreadID) {
  if(any(tid.xy>=image_size))return;
  float2 uv=(float2(tid.xy)+0.5)*image_size_inv;
  float4 offsets[3];SMAAEdgeDetectionVS(uv,offsets);
  outputTex[tid.xy]=float4(SMAALumaEdgeDetectionPS(uv,offsets,colorTex),0,0);
}
[numthreads(16,8,1)]
void weights(uint3 tid : SV_DispatchThreadID) {
  if(any(tid.xy>=image_size))return;
  float2 uv=(float2(tid.xy)+0.5)*image_size_inv;
  float4 offsets[3];float2 pixel;
  SMAABlendingWeightCalculationVS(uv,pixel,offsets);
  outputTex[tid.xy]=SMAABlendingWeightCalculationPS(uv,pixel,offsets,auxiliaryTex,areaTex,searchTex,float4(0,0,0,0));
}
[numthreads(16,8,1)]
void blend(uint3 tid : SV_DispatchThreadID) {
  if(any(tid.xy>=image_size))return;
  float2 uv=(float2(tid.xy)+0.5)*image_size_inv;float4 offset;
  SMAANeighborhoodBlendingVS(uv,offset);
  outputTex[tid.xy]=float4(SMAANeighborhoodBlendingPS(uv,offset,colorTex,auxiliaryTex).rgb,1);
}
[numthreads(16,8,1)]
void copy(uint3 tid : SV_DispatchThreadID) {
  if(any(tid.xy>=image_size))return;
  outputTex[tid.xy]=float4(colorTex.Load(int3(tid.xy,0)).rgb,1);
}
