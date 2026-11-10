#pragma once
#include "texture_asset_queue.h"
#include <cstdint>
#include <map>
#include <vector>
namespace kami::world {
struct DecodedImage {
  uint32_t width=0,height=0;
  // Unpremultiplied RGBA 8-bit pixel data, row-major.
  std::vector<uint8_t> rgba;
  bool valid() const;
};
class TextureAtlas {
public:
  static constexpr uint32_t tile=64,columns=8,rows=8;
  static constexpr uint32_t width=tile*columns,height=tile*rows;
  static constexpr size_t max_images=columns*rows;
  TextureAtlas();
  bool insert(const AssetId& id,const DecodedImage& image);
  bool uv(const AssetId& id,float local_u,float local_v,
          float& out_u,float& out_v) const;
  const std::vector<uint8_t>& pixels() const{return pixels_;}
  size_t size() const{return slots_.size();}
  uint64_t revision() const{return revision_;}
  void clear();
private:
  std::map<AssetId,uint32_t> slots_;
  std::vector<uint8_t> pixels_;
  uint64_t revision_=0;
};
}
