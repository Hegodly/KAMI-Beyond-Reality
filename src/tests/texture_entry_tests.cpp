#include "world/texture_entries.h"
#include <cassert>
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
  group(te,{0,0,128,63}); // S scale 1.f
  group(te,{0,0,128,63}); // T scale 1.f
  group(te,{0,0}); // S offset
  group(te,{0,0}); // T offset
  group(te,{0,0}); // rotation
  group(te,{0}); // bump
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
  assert(result.faces[0].has_legacy_material);
  assert(result.faces[0].legacy_material_id[3]==1);
  auto older=sample(false);
  TextureEntries no_material;
  assert(parse_texture_entries(older.data(),older.size(),no_material,5));
  assert(no_material.valid&&!no_material.faces[0].has_legacy_material);
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
