#include "mesh_asset_header.h"
#include <limits>
#include <initializer_list>
#include <cstdint>
#include <string>
namespace kami::world {
namespace {
struct Reader {
  const uint8_t* data=nullptr;
  size_t length=0,pos=0;
  bool require(size_t n) const{return pos<=length&&n<=length-pos;}
  bool byte(uint8_t& v){if(!require(1))return false;v=data[pos++];return true;}
  bool take(size_t n){if(!require(n))return false;pos+=n;return true;}
  bool be32(uint32_t& n){
    if(!require(4))return false;
    n=(uint32_t(data[pos])<<24)|(uint32_t(data[pos+1])<<16)|
      (uint32_t(data[pos+2])<<8)|uint32_t(data[pos+3]);
    pos+=4;return true;
  }
  bool key(std::string& key){
    uint8_t marker=0;uint32_t len=0;
    if(!byte(marker)||marker!='k'||!be32(len)||len>256||!require(len))return false;
    key.assign(reinterpret_cast<const char*>(data+pos),len);
    pos+=len;return true;
  }
  bool skip(unsigned depth) {
    if(depth>12)return false;
    uint8_t marker=0;uint32_t n=0;
    if(!byte(marker))return false;
    switch(marker){
      case '!':case '0':case '1':return true;
      case 'i':return take(4);
      case 'r':case 'd':return take(8);
      case 'u':return take(16);
      case 's':case 'l':case 'b':
        return be32(n)&&n<=65536&&take(n);
      case '[':
        if(!be32(n)||n>2048)return false;
        for(uint32_t i=0;i<n;++i)if(!skip(depth+1))return false;
        {uint8_t end=0;return byte(end)&&end==']';}
      case '{':
        if(!be32(n)||n>2048)return false;
        for(uint32_t i=0;i<n;++i) {
          std::string key_name;
          if(!key(key_name)||!skip(depth+1))return false;
        }
        {uint8_t end=0;return byte(end)&&end=='}';}
    }
    return false;
  }
  bool lod(MeshLodBlock& out) {
    uint8_t start=0,finish=0;uint32_t entries=0;
    if(!byte(start)||start!='{'||!be32(entries)||entries>32)return false;
    int64_t offset=-1,size=-1;
    for(uint32_t i=0;i<entries;++i){
      std::string field;
      if(!key(field))return false;
      if(field=="offset"||field=="size"){
        uint8_t tag=0;uint32_t value=0;
        if(!byte(tag)||tag!='i'||!be32(value)||value>INT32_MAX)return false;
        if(field=="offset")offset=static_cast<int64_t>(value);
        else size=static_cast<int64_t>(value);
      }else if(!skip(2))return false;
    }
    if(!byte(finish)||finish!='}'||offset<0||size<=0)return false;
    out.offset=static_cast<size_t>(offset);
    out.size=static_cast<size_t>(size);
    out.present=true;
    return true;
  }
};
}
bool parse_mesh_asset_header(const uint8_t* data,size_t length,
                             MeshAssetHeader& out) {
  if(!data||length<6||length>16u*1024u*1024u)return false;
  // LLSDSerialize permits the binary document directly; some older
  // mesh assets carry a "<?llsd/binary?>" line first.
  constexpr char magic[]="<?llsd/binary?>\n";
  constexpr size_t prefix=sizeof(magic)-1;
  size_t begin=0;
  if(length>=prefix) {
    bool matches=true;
    for(size_t i=0;i<prefix;++i)
      if(data[i]!=uint8_t(magic[i])){matches=false;break;}
    if(matches)begin=prefix;
  }
  Reader r{data,length,begin};
  uint8_t marker=0;uint32_t entries=0;
  if(!r.byte(marker)||marker!='{'||!r.be32(entries)||entries>256)return false;
  MeshAssetHeader parsed;
  for(uint32_t i=0;i<entries;++i){
    if(r.pos>131072)return false;
    std::string name;
    if(!r.key(name))return false;
    if(name=="high_lod"){
      if(!r.lod(parsed.high))return false;
    } else if(name=="medium_lod"){
      if(!r.lod(parsed.medium))return false;
    }else if(name=="low_lod"){
      if(!r.lod(parsed.low))return false;
    }else if(name=="lowest_lod"){
      if(!r.lod(parsed.lowest))return false;
    }else if(!r.skip(1))return false;
  }
  if(!r.byte(marker)||marker!='}'||r.pos>131072||!parsed.high.present)
    return false;
  parsed.header_bytes=r.pos;
  for(const MeshLodBlock* block:{&parsed.high,&parsed.medium,&parsed.low,&parsed.lowest}) {
    if(!block->present)continue;
    if(block->offset>length-parsed.header_bytes ||
       block->size>length-parsed.header_bytes-block->offset)
      return false;
  }
  out=parsed;
  return true;
}
}
