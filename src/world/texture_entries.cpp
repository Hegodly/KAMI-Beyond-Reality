#include "texture_entries.h"
#include <algorithm>
#include <array>
#include <cstring>
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
  }
}
bool nonzero_uuid(const std::array<uint8_t,16>& value) {
  return std::any_of(value.begin(),value.end(),[](uint8_t b){return b!=0;});
}
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
  // All other legacy fields still need to be parsed in the correct order;
  // ignoring them without consuming their exception blocks would shift the
  // material UUID read and misinterpret arbitrary bytes as colors.
  for(const size_t width: {4u,4u,2u,2u,2u,1u,1u,1u}) {
    if(!read_field(data,bytes,pos,width,face_count,field))return false;
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
