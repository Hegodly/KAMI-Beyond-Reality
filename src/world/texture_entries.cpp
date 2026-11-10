#include "texture_entries.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <utility>
#include <initializer_list>
namespace kami::world {
namespace {
using Field=std::array<std::array<uint8_t,16>,TextureEntries::max_faces>;
bool read_field(const uint8_t* data,size_t length,size_t& pos,
                size_t width,size_t faces,Field& output,bool last=false) {
  if(width<1||width>16||faces<1||faces>TextureEntries::max_faces ||
     pos>length||width>length-pos)return false;
  for(size_t i=0;i<faces;++i) {
    std::memcpy(output[i].data(),data+pos,width);
  }
  pos+=width;
  // The final legacy material UUID group can lack the trailing zero.
  if(last && pos==length)return true;
  for(;;) {
    uint64_t flags=0;
    uint8_t octet=0;
    unsigned count=0;
    do {
      if(pos>=length||++count>10)return false;
      octet=data[pos++];
      if(flags>(UINT64_MAX>>7))return false;
      flags=(flags<<7)|(octet&0x7fu);
    }while(octet&0x80u);
    if(!flags)return true;
    if(pos>length||width>length-pos)return false;
    for(size_t i=0;i<faces;++i)
      if(flags&(uint64_t(1)<<i))
        std::memcpy(output[i].data(),data+pos,width);
    pos+=width;
    // The last material-UUID field produced by LLPrimitive::packTEField
    // is not zero terminated, even if it contains exception values.
    if(last && pos==length)return true;
  }
}
float little_float(const std::array<uint8_t,16>& value) {
  const uint32_t bits=uint32_t(value[0])|(uint32_t(value[1])<<8)|
    (uint32_t(value[2])<<16)|(uint32_t(value[3])<<24);
  float f=0;
  std::memcpy(&f,&bits,sizeof(f));
  return f;
}
float normalized_short(const std::array<uint8_t,16>& value) {
  const auto bits=uint16_t(value[0])|(uint16_t(value[1])<<8);
  return float(static_cast<int16_t>(bits))/32767.f;
}
bool nonzero_uuid(const std::array<uint8_t,16>& value) {
  return std::any_of(value.begin(),value.end(),[](uint8_t b){return b!=0;});
}
}
bool transform_texture_uv(const TextureFace& face,float u,float v,
                          float& output_u,float& output_v) {
  if(!std::isfinite(u)||!std::isfinite(v)||
     std::abs(u)>10000.f||std::abs(v)>10000.f)return false;
  const float x=(u-0.5f)*face.scale_s;
  const float y=(v-0.5f)*face.scale_t;
  const float cosine=std::cos(face.rotation),sine=std::sin(face.rotation);
  const float out_s=0.5f+face.offset_s+x*cosine-y*sine;
  const float out_t=0.5f+face.offset_t+x*sine+y*cosine;
  if(!std::isfinite(out_s)||!std::isfinite(out_t)||
     std::abs(out_s)>100000.f||std::abs(out_t)>100000.f)return false;
  output_u=out_s;output_v=out_t;
  return true;
}
bool parse_texture_entries(const uint8_t* data,size_t bytes,TextureEntries& out,
                           size_t face_count) {
  if(!data||bytes==0||bytes>4096||face_count==0||
     face_count>TextureEntries::max_faces)return false;
  TextureEntries parsed{};
  size_t pos=0;
  Field field{};
  if(!read_field(data,bytes,pos,16,face_count,field))return false;
  for(size_t i=0;i<face_count;++i) {
    parsed.faces[i].texture_id=field[i];
    parsed.has_asset_references |= nonzero_uuid(field[i]);
  }
  if(!read_field(data,bytes,pos,4,face_count,field))return false;
  for(size_t i=0;i<face_count;++i)
    for(size_t k=0;k<4;++k)
      parsed.faces[i].rgba[k]=static_cast<uint8_t>(255u-field[i][k]);
  // Official LLTextureEntry fields, in exact wire order. The two scales
  // are little-endian float32; offset and rotation are signed little-endian
  // shorts. The simulator encodes rotation in 1/32768 of a full turn.
  constexpr float two_pi=6.2831853071795864769f;
  for(unsigned group=0;group<8;++group) {
    const size_t width=group<2?4u:group<5?2u:1u;
    if(!read_field(data,bytes,pos,width,face_count,field))return false;
    for(size_t face=0;face<face_count;++face) {
      auto& entry=parsed.faces[face];
      switch(group) {
        case 0:entry.scale_s=little_float(field[face]);break;
        case 1:entry.scale_t=little_float(field[face]);break;
        case 2:entry.offset_s=normalized_short(field[face]);break;
        case 3:entry.offset_t=normalized_short(field[face]);break;
        case 4:entry.rotation=normalized_short(field[face])*
                               two_pi*32767.f/32768.f;break;
        case 5:entry.bump_fullbright=field[face][0];break;
        case 6:entry.media_flags=field[face][0];break;
        case 7:entry.glow=field[face][0];break;
      }
      if((group<2 && (!std::isfinite(group==0?entry.scale_s:entry.scale_t)||
          std::abs(group==0?entry.scale_s:entry.scale_t)>1024.f)))
        return false;
    }
  }
  // Older objects may omit the trailing material ID field.
  if(pos<bytes) {
    if(!read_field(data,bytes,pos,16,face_count,field,true))return false;
    for(size_t i=0;i<face_count;++i) {
      parsed.faces[i].legacy_material_id=field[i];
      parsed.faces[i].has_legacy_material=nonzero_uuid(field[i]);
    }
  }
  if(pos!=bytes)return false;
  parsed.valid=true;
  out=std::move(parsed);
  return true;
}
}
