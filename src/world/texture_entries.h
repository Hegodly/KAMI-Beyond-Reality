#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace kami::world {
struct TextureFace {
  std::array<uint8_t,16> texture_id{};
  std::array<uint8_t,16> legacy_material_id{};
  // Original SL RGBA color. Not a decoded JPEG2000/glTF texture.
  std::array<uint8_t,4> rgba{255,255,255,255};
  bool has_legacy_material=false;
  float scale_s=1.f,scale_t=1.f;
  float offset_s=0.f,offset_t=0.f,rotation=0.f;
  uint8_t bump_fullbright=0,media_flags=0,glow=0;
};
struct TextureEntries {
  static constexpr size_t max_faces=45;
  std::array<TextureFace,max_faces> faces{};
  bool valid=false;
  bool has_asset_references=false;
};
// Parse TextureEntry Variable 2, including default values and 7-bit
// per-face exception masks. Transactional: failed payloads cannot corrupt
// previously decoded colors. No external asset requests are performed.
// Rotate and scale about the texture center, then translate; do not
// clamp repeated UVs. The DX12 pixel shader wraps them per pixel.
bool transform_texture_uv(const TextureFace& face,float u,float v,
                          float& output_u,float& output_v);
bool parse_texture_entries(const uint8_t* data,size_t bytes,
                           TextureEntries& out,size_t face_count=TextureEntries::max_faces);
}
