#include "world/texture_entries.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdint>
#include <vector>
namespace {
void group(std::vector<uint8_t>& bytes,const std::vector<uint8_t>& value,
           uint8_t mask=0,const std::vector<uint8_t>& override_value={},
           bool trailing_zero=true) {
  bytes.insert(bytes.end(),value.begin(),value.end());
  if(mask) {
    bytes.push_back(mask);
    bytes.insert(bytes.end(),override_value.begin(),override_value.end());
  }
  if(trailing_zero)bytes.push_back(0);
}
std::vector<uint8_t> sample(bool material=true) {
  std::vector<uint8_t> te;
  std::vector<uint8_t> image(16,0);
  image[0]=0x12;
  std::vector<uint8_t> image_other(16,0);
  image_other[0]=0x34;
  group(te,image,0x04,image_other); // face 2 has different image UUID
  group(te,{0,127,255,0},0x0A,{255,0,0,127}); // faces 1 and 3
  group(te,{0,0,128,63},0x04,{0,0,0,64}); // face 2: S repeat 2
  group(te,{0,0,128,63}); // T scale 1.f
  group(te,{0,0},0x04,{0,64}); // face 2: +0.5 S offset
  group(te,{0,0}); // T offset
  group(te,{0,0},0x04,{0,32}); // face 2: +90-degree rotation
  group(te,{0},0x04,{32}); // face 2: fullbright bit 0x20
  group(te,{0}); // media
  group(te,{0}); // glow
  if(material) {
    std::vector<uint8_t> id(16,0);
    id[3]=1;
    group(te,id,0,{},false); // last field need not be terminated
  }
  return te;
}
}
int main() {
  using kami::world::TextureEntries;
  using kami::world::parse_texture_entries;
  auto data=sample();
  TextureEntries result;
  assert(parse_texture_entries(data.data(),data.size(),result,6));
  assert(result.valid && result.has_asset_references);
  assert(result.faces[0].texture_id[0]==0x12);
  assert(result.faces[1].texture_id[0]==0x12);
  assert(result.faces[2].texture_id[0]==0x34);
  assert(result.faces[0].rgba[0]==255&&result.faces[0].rgba[1]==128);
  assert(result.faces[0].rgba[2]==0&&result.faces[0].rgba[3]==255);
  assert(result.faces[1].rgba[0]==0&&result.faces[1].rgba[1]==255);
  assert(result.faces[1].rgba[2]==255&&result.faces[1].rgba[3]==128);
  assert(result.faces[3].rgba==result.faces[1].rgba);
  assert(result.faces[4].rgba==result.faces[0].rgba);
  assert(std::fabs(result.faces[0].scale_s-1.f)<0.001f);
  assert(std::fabs(result.faces[2].scale_s-2.f)<0.001f);
  assert(std::fabs(result.faces[2].offset_s-0.5f)<0.001f);
  assert(std::fabs(result.faces[2].rotation-1.5707963f)<0.001f);
  assert(result.faces[2].bump_fullbright==0x20);
  assert(result.faces[0].bump_fullbright==0);
  float u=0,v=0;
  assert(kami::world::transform_texture_uv(result.faces[0],0.75f,0.5f,u,v));
  assert(std::fabs(u-0.75f)<0.001f && std::fabs(v-0.5f)<0.001f);
  assert(kami::world::transform_texture_uv(result.faces[2],0.75f,0.5f,u,v));
  assert(std::fabs(u-1.0f)<0.001f && std::fabs(v-1.0f)<0.001f);
  const float before=u;
  assert(!kami::world::transform_texture_uv(result.faces[2],
      std::numeric_limits<float>::quiet_NaN(),0.2f,u,v));
  assert(u==before);
  assert(result.faces[0].has_legacy_material);
  assert(result.faces[0].legacy_material_id[3]==1);
  auto older=sample(false);
  TextureEntries no_material;
  assert(parse_texture_entries(older.data(),older.size(),no_material,5));
  assert(no_material.valid&&!no_material.faces[0].has_legacy_material);
  // Material UUID override for face 1. Official packTEField does not
  // append a terminating zero at the very end of the message.
  auto material_overrides=sample(false);
  std::vector<uint8_t> default_id(16,0);
  default_id[3]=1;
  material_overrides.insert(material_overrides.end(),
                            default_id.begin(),default_id.end());
  material_overrides.push_back(0x02); // exception mask: face 1
  std::vector<uint8_t> override_id(16,0);
  override_id[3]=2;
  material_overrides.insert(material_overrides.end(),
                            override_id.begin(),override_id.end());
  TextureEntries entries;
  assert(parse_texture_entries(material_overrides.data(),
          material_overrides.size(),entries,6));
  assert(entries.faces[0].legacy_material_id[3]==1);
  assert(entries.faces[1].legacy_material_id[3]==2);
  assert(entries.faces[2].legacy_material_id[3]==1);
  material_overrides.pop_back();
  assert(!parse_texture_entries(material_overrides.data(),
          material_overrides.size(),entries,6));
  assert(entries.faces[1].legacy_material_id[3]==2);
  assert(!parse_texture_entries(nullptr,data.size(),result,6));
  assert(!parse_texture_entries(data.data(),data.size(),result,0));
  assert(!parse_texture_entries(data.data(),data.size(),result,46));
  assert(!parse_texture_entries(data.data(),4097,result,6));
  // Failed parse must preserve a valid previously decoded color.
  const auto preserved=result.faces[1].rgba;
  data.pop_back();
  assert(!parse_texture_entries(data.data(),data.size(),result,6));
  assert(result.faces[1].rgba==preserved);
  std::vector<uint8_t> continuation(16,0);
  continuation.insert(continuation.end(),{0x80,0x80,0x80,0x80,
    0x80,0x80,0x80,0x80,0x80,0x80,0x00});
  assert(!parse_texture_entries(continuation.data(),continuation.size(),result,5));
  // Bitfield overrides for face 10 use two 7-bit chunks (0x80|8, 0).
  auto more=sample(false);
  // The simple decoder is bounded and rejects malformed repeated or
  // truncated exception masks before accepting any colors.
  more[16]=0x80; // index flags continuation without correct terminator
  assert(!parse_texture_entries(more.data(),more.size(),result,6));
}
