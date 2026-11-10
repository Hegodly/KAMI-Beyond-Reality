#include "world/texture_atlas.h"
#include <cassert>
#include <cmath>
using namespace kami::world;
int main() {
  TextureAtlas atlas;
  DecodedImage two{2,2,{255,0,0,255, 0,255,0,255,
                        0,0,255,255, 255,255,0,255}};
  assert(two.valid());
  assert(!(DecodedImage{}).valid());
  AssetId red{},green{};red[0]=1;green[0]=2;
  float u=-1,v=-1;
  assert(!atlas.uv(red,0.5f,0.5f,u,v));
  assert(!atlas.insert(AssetId{},two));
  assert(atlas.insert(red,two));
  assert(atlas.size()==1);
  assert(atlas.uv(red,0,0,u,v) && u>0&&u<1.f/8.f&&v>0&&v<1.f/8.f);
  assert(atlas.uv(red,1,1,u,v) && u<1.f/8.f && v<1.f/8.f);
  auto pixel=[&](unsigned x,unsigned y,unsigned channel) {
    return atlas.pixels()[(size_t(y)*TextureAtlas::width+x)*4+channel];
  };
  assert(pixel(0,0,0)==255 && pixel(0,0,1)==0);
  assert(pixel(63,0,1)==255);
  assert(pixel(0,63,2)==255);
  assert(pixel(63,63,0)==255 && pixel(63,63,1)==255);
  assert(atlas.insert(green,two));
  assert(atlas.uv(green,0,0,u,v) && u>1.f/8.f&&u<2.f/8.f);
  const auto rev=atlas.revision();
  assert(atlas.insert(red,two) && atlas.size()==2&&atlas.revision()>rev);
  DecodedImage invalid{8192,8192,{1,2,3,4}};
  assert(!atlas.insert(red,invalid));
  for(unsigned i=3;i<=64;++i) {
    AssetId id{};
    id[0]=static_cast<uint8_t>(i);
    assert(atlas.insert(id,DecodedImage{1,1,{0,0,0,255}}));
  }
  AssetId full{};
  full[0]=100;
  assert(atlas.size()==64);
  assert(!atlas.insert(full,two));
  atlas.clear();
  assert(atlas.size()==0);
  assert(!atlas.uv(red,0,0,u,v));
  return 0;
}
