#include "texture_atlas.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace kami::world {
bool DecodedImage::valid() const {
  return width>0&&height>0&&width<=2048&&height<=2048&&
    rgba.size()==size_t(width)*height*4;
}
TextureAtlas::TextureAtlas() { clear(); }
void TextureAtlas::clear() {
  slots_.clear();
  pixels_.assign(size_t(width)*height*4,255);
  ++revision_;
}
bool TextureAtlas::insert(const AssetId& id,const DecodedImage& image) {
  if(!valid_asset_uuid(id)||!image.valid())return false;
  auto found=slots_.find(id);
  if(found==slots_.end() && slots_.size()>=max_images)return false;
  const uint32_t index=found==slots_.end()?
    static_cast<uint32_t>(slots_.size()):found->second;
  const uint32_t origin_x=(index%columns)*tile;
  const uint32_t origin_y=(index/columns)*tile;
  // Nearest-neighbour reduction uses a clamped (0..width-1) source
  // coordinate. Tiles have no dependencies on unsafe image dimensions.
  for(uint32_t y=0;y<tile;++y)
    for(uint32_t x=0;x<tile;++x) {
      const uint32_t sx=std::min(image.width-1,
        static_cast<uint32_t>((uint64_t(x)*image.width)/tile));
      const uint32_t sy=std::min(image.height-1,
        static_cast<uint32_t>((uint64_t(y)*image.height)/tile));
      const size_t src=(size_t(sy)*image.width+sx)*4;
      const size_t dst=(size_t(origin_y+y)*width+(origin_x+x))*4;
      for(unsigned c=0;c<4;++c)pixels_[dst+c]=image.rgba[src+c];
    }
  if(found==slots_.end())slots_.emplace(id,index);
  ++revision_;
  return true;
}
bool TextureAtlas::shader_slot(const AssetId& id,float& one_based_slot) const {
  const auto it=slots_.find(id);
  if(it==slots_.end())return false;
  one_based_slot=static_cast<float>(it->second+1);
  return true;
}
bool TextureAtlas::uv(const AssetId& id,float u,float v,
                      float& out_u,float& out_v) const {
  auto it=slots_.find(id);
  if(it==slots_.end()||!std::isfinite(u)||!std::isfinite(v))return false;
  const uint32_t index=it->second;
  // Half-texel padding keeps bilinear samples within a tile, rather than
  // bleeding adjacent prim images. Repeating/rotation is not supported yet.
  out_u=((index%columns)*tile+0.5f+
    std::clamp(u,0.0f,1.0f)*float(tile-1))/float(width);
  out_v=((index/columns)*tile+0.5f+
    std::clamp(v,0.0f,1.0f)*float(tile-1))/float(height);
  return true;
}
}
