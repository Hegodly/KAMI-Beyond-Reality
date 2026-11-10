#pragma once
namespace kami {
// Embedded HLSL: the development executable requires no external shader file.
// The vertex shader uses row-vector multiplication to match the CPU-side
// DirectXMath transpose and HLSL's default column-major constant layout.
inline constexpr const char* terrain_dx12_shader = R"(
    cbuffer Camera : register(b0) { float4x4 view_projection; };
    Texture2D atlas_texture : register(t0);
    SamplerState atlas_sampler : register(s0);
    struct VSInput {
      float3 position : POSITION;
      float3 color : COLOR;
      float2 uv : TEXCOORD0;
      float texture_mix : TEXCOORD1;
      float alpha : TEXCOORD2;
    };
    struct VSOutput {
      float4 position : SV_POSITION;
      float3 color : COLOR;
      float3 world_position : TEXCOORD0;
      float2 uv : TEXCOORD1;
      float texture_mix : TEXCOORD2;
      float alpha : TEXCOORD3;
    };
    VSOutput VSMain(VSInput input) {
      VSOutput output;
      // DirectXMath supplies a transposed row-vector view/projection matrix.
      // Match that storage with vector * matrix (not matrix * vector).
      output.position = mul(float4(input.position, 1.0), view_projection);
      output.color = input.color;
      output.world_position = input.position;
      output.uv = input.uv;
      output.texture_mix = input.texture_mix;
      output.alpha = input.alpha;
      return output;
    }
    float4 PSMain(VSOutput input) : SV_TARGET {
      float3 dpdx = ddx(input.world_position);
      float3 dpdy = ddy(input.world_position);
      float3 normal = normalize(cross(dpdy, dpdx));
      // Two-sided terrain shading uses the normal facing the light hemisphere.
      float3 sun_direction = normalize(float3(0.35, 0.8, -0.4));
      float diffuse = saturate(abs(dot(normal, sun_direction)));
      float lighting = 0.30 + 0.70 * diffuse;
      float distance_fog = saturate((length(input.world_position.xz) - 120.0) / 260.0);
      float3 sky = float3(0.07, 0.12, 0.20);
      // A zero slot represents untextured geometry. Slots 1..64 select
      // one of the 8x8 decoded atlas images. Keep UVs unwrapped until
      // the pixel stage: vertex-level frac would break repeated walls.
      float4 sampled = float4(1.0, 1.0, 1.0, 1.0);
      if (input.texture_mix > 0.5) {
        float tile = floor(input.texture_mix - 0.5);
        float2 cell = float2(fmod(tile, 8.0), floor(tile / 8.0));
        float2 tiled_uv = frac(input.uv);
        float2 atlas_uv = (cell * 64.0 + 0.5 + tiled_uv * 63.0) / 512.0;
        sampled = atlas_texture.Sample(atlas_sampler, atlas_uv);
      }
      // Respect transparent holes in mesh foliage and alpha-masked prims.
      // Semi-transparent surface blending/sorting needs a separate pass.
      clip(input.alpha * sampled.a - 0.025);
      float3 albedo = input.color * sampled.rgb;
      float3 lit = albedo * lighting;
      float3 color = lerp(lit, sky, distance_fog * 0.72);
      color = color / (1.0 + color);
      color = pow(saturate(color), 1.0 / 2.2);
      return float4(color, 1.0);
    }
  )";
}
