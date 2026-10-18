// HDR transport of the existing display-referred SDR image, not scene HDR recovery.
// The current SDR presentation contract is implicit sRGB. Decode exactly once at
// this final boundary; guest gamma/grading and AA remain in their existing path.
Texture2D<float4> source_texture : register(t0);
SamplerState source_sampler : register(s0);
cbuffer HdrWhite : register(b0) { float white_scale; };

float3 DecodeSrgb(float3 value) {
  value = saturate(value);
  return float3(value.r <= 0.04045 ? value.r / 12.92 : pow((value.r + 0.055) / 1.055, 2.4),
                value.g <= 0.04045 ? value.g / 12.92 : pow((value.g + 0.055) / 1.055, 2.4),
                value.b <= 0.04045 ? value.b / 12.92 : pow((value.b + 0.055) / 1.055, 2.4));
}
float4 FullscreenVS(uint id : SV_VertexID) : SV_Position {
  return float4(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0, 0.0, 1.0);
}
float4 OutputPS(float4 position : SV_Position) : SV_Target {
  return float4(DecodeSrgb(source_texture.Load(int3(int2(position.xy), 0)).rgb) * white_scale, 1.0);
}
float4 OverlayPS(float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_Target {
  // Preserve the existing UI alpha contract; RGB blends in linear scRGB now.
  float4 sample_color = source_texture.SampleLevel(source_sampler, uv, 0) * color;
  return float4(DecodeSrgb(sample_color.rgb) * white_scale, sample_color.a);
}
