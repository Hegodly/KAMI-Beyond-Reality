#include "mesh_discovery.h"
#include <cstring>
namespace kami::world {
bool find_mesh_extra(const uint8_t* data,size_t size,MeshExtra& result) {
  if(!data||size==0||size>16384)return false;
  size_t p=0;
  auto read16=[&](uint16_t& value)->bool {
    if(size-p<2)return false;
    value=uint16_t(data[p])|(uint16_t(data[p+1])<<8);
    p+=2;return true;
  };
  auto read32=[&](uint32_t& value)->bool {
    if(size-p<4)return false;
    value=uint32_t(data[p])|(uint32_t(data[p+1])<<8)|
      (uint32_t(data[p+2])<<16)|(uint32_t(data[p+3])<<24);
    p+=4;return true;
  };
  const uint8_t entries=data[p++];
  if(entries>64)return false;
  MeshExtra discovered{};
  for(unsigned index=0;index<entries;++index) {
    uint16_t type=0;uint32_t length=0;
    if(!read16(type)||!read32(length)||length>size-p)return false;
    if((type==0x60||type==0x30)&&length==17) {
      MeshExtra candidate{};
      std::memcpy(candidate.mesh_id.data(),data+p,16);
      candidate.sculpt_type=data[p+16];
      candidate.valid=(candidate.sculpt_type&0x07u)==5u &&
        valid_asset_uuid(candidate.mesh_id);
      if(candidate.valid)discovered=candidate;
    }
    p+=length;
  }
  if(p!=size)return false;
  result=discovered;
  return true;
}
}
