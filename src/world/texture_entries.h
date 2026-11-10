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
bool parse_texture_entries(const uint8_t* data,size_t bytes,
                           TextureEntries& out,size_t face_count=TextureEntries::max_faces);
}
