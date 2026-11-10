#include "scene_objects.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace kami::world {
namespace {
constexpr uint32_t full_id=12,compressed_id=13,cached_id=14,kill_id=16;
struct Cursor {
  const std::vector<uint8_t>& data;
  size_t pos=0;
  size_t remain() const {return pos<=data.size()?data.size()-pos:0;}
  bool skip(size_t n) {if(n>remain())return false;pos+=n;return true;}
  bool u8(uint8_t& v) {if(!remain())return false;v=data[pos++];return true;}
  bool u16(uint16_t& v) {
    if(remain()<2)return false;
    v=uint16_t(data[pos])|(uint16_t(data[pos+1])<<8);pos+=2;return true;
  }
  bool u32(uint32_t& v) {
    if(remain()<4)return false;v=0;
    for(int j=0;j<4;++j)v|=uint32_t(data[pos+j])<<(j*8);
    pos+=4;return true;
  }
  bool f32(float& f) {
    uint32_t bits=0;if(!u32(bits))return false;
    std::memcpy(&f,&bits,4);return std::isfinite(f);
  }
  bool vec3(float& x,float& y,float& z) {
    return f32(x)&&f32(y)&&f32(z);
  }
  bool var1() {uint8_t n=0;return u8(n)&&skip(n);}
  bool var2() {uint16_t n=0;return u16(n)&&skip(n);}
};
bool normalize_rotation(SceneObject& ob) {
  const double length=double(ob.qx)*ob.qx+double(ob.qy)*ob.qy+
                      double(ob.qz)*ob.qz;
  if (!std::isfinite(length)||length>1.2) return false;
  ob.qw=static_cast<float>(std::sqrt(std::max(0.,1.-length)));
  const float n=std::sqrt(ob.qx*ob.qx+ob.qy*ob.qy+ob.qz*ob.qz+ob.qw*ob.qw);
  if(n<0.00001f)return false;
  ob.qx/=n;ob.qy/=n;ob.qz/=n;ob.qw/=n;return true;
}
bool plausible(const SceneObject& ob) {
  auto size=[](float v){return std::isfinite(v)&&v>0.f&&v<=64.f;};
  return ob.local_id!=0 && ob.pcode==9 &&
    size(ob.sx)&&size(ob.sy)&&size(ob.sz) &&
    std::isfinite(ob.x)&&std::isfinite(ob.y)&&std::isfinite(ob.z) &&
    ob.x>=-256&&ob.x<=4096&&ob.y>=-256&&ob.y<=4096 &&
    ob.z>=-4096&&ob.z<=8192;
}
bool parse_region_prefix(Cursor& c,uint8_t& count) {
  // RegionData: region handle U64, time dilation U16, variable block count U8.
  return c.skip(10)&&c.u8(count)&&count<=128;
}
bool parse_full(const std::vector<uint8_t>& data,size_t offset,
                std::vector<SceneObject>& objects,size_t& skipped) {
  Cursor c{data,offset};uint8_t blocks=0;
  if(!parse_region_prefix(c,blocks))return false;
  for(unsigned i=0;i<blocks;++i) {
    SceneObject ob;uint8_t state=0,material=0,click=0,length=0;
    if(!c.u32(ob.local_id)||!c.u8(state)||!c.skip(16)||!c.skip(4)||
       !c.u8(ob.pcode)||!c.u8(material)||!c.u8(click)||
       !c.vec3(ob.sx,ob.sy,ob.sz)||!c.u8(length))return false;
    if(length>c.remain())return false;
    const size_t start=c.pos;
    if ((length==60||length==124) && ob.pcode==9) {
      if(!c.vec3(ob.x,ob.y,ob.z)||!c.skip(24)||
         !c.vec3(ob.qx,ob.qy,ob.qz))return false;
    }
    c.pos=start+length;
    uint32_t flags=0;
    if(!c.u32(ob.parent_id)||!c.u32(flags)||!c.skip(23)||
       !c.var2()||!c.var1()||!c.var2()||!c.var2()||
       !c.var1()||!c.skip(4)||!c.var1()||!c.var1()||
       !c.var1()||!c.skip(16+16+4+1+4+1+12+12)) return false;
    if ((length==60||length==124) && plausible(ob) && normalize_rotation(ob))
      objects.push_back(ob);
    else ++skipped;
  }
  return c.remain()==0;
}
bool parse_compressed(const std::vector<uint8_t>& data,size_t offset,
                      std::vector<SceneObject>& objects,size_t& skipped) {
  Cursor c{data,offset};uint8_t blocks=0;
  if(!parse_region_prefix(c,blocks))return false;
  for(unsigned i=0;i<blocks;++i) {
    uint32_t flags=0;uint16_t size=0;
    if(!c.u32(flags)||!c.u16(size)||size>c.remain())return false;
    const size_t end=c.pos+size;
    // UUID, local ID, PCode, state, CRC, material, click action,
    // scale, position, rotation (xyz), special code, owner UUID.
    // See LLViewerObject::processUpdateMessage(OUT_FULL_COMPRESSED).
    SceneObject ob;
    uint8_t state=0,material=0,click=0;
    uint32_t special=0;
    bool known=size>=84 && c.skip(16)&&c.u32(ob.local_id)&&
      c.u8(ob.pcode)&&c.u8(state)&&c.skip(4)&&c.u8(material)&&
      c.u8(click)&&c.vec3(ob.sx,ob.sy,ob.sz)&&
      c.vec3(ob.x,ob.y,ob.z)&&c.vec3(ob.qx,ob.qy,ob.qz)&&
      c.u32(special)&&c.skip(16);
    if (known && (special&0x80u)) known=c.skip(12);
    if (known && (special&0x20u)) known=c.u32(ob.parent_id);
    // Unknown optional trailing fields are not interpreted as geometry.
    if(known && c.pos<=end && plausible(ob) && normalize_rotation(ob))
      objects.push_back(ob);
    else ++skipped;
    c.pos=end;
  }
  return c.remain()==0;
}
bool parse_cached(const std::vector<uint8_t>& data,size_t offset,
                  std::vector<uint32_t>& ids) {
  Cursor c{data,offset};uint8_t blocks=0;
  if(!parse_region_prefix(c,blocks))return false;
  for(unsigned i=0;i<blocks;++i) {
    uint32_t id=0,crc=0,flags=0;
    if(!c.u32(id)||!c.u32(crc)||!c.u32(flags))return false;
    if(id)ids.push_back(id);
  }
  return c.remain()==0;
}
bool parse_kill(const std::vector<uint8_t>& data,size_t offset,
                std::vector<uint32_t>& ids) {
  Cursor c{data,offset};uint8_t count=0;
  if(!c.u8(count)||count>128)return false;
  for(unsigned i=0;i<count;++i) {
    uint32_t id=0;if(!c.u32(id))return false;
    if(id)ids.push_back(id);
  }
  return c.remain()==0;
}
void rotate(const SceneObject& ob,float& x,float& y,float& z) {
  // Unit quaternion, rotate prim-local SL XYZ before projecting SL Z up.
  const float tx=2.f*(ob.qy*z-ob.qz*y);
  const float ty=2.f*(ob.qz*x-ob.qx*z);
  const float tz=2.f*(ob.qx*y-ob.qy*x);
  const float xx=x+ob.qw*tx+(ob.qy*tz-ob.qz*ty);
  const float yy=y+ob.qw*ty+(ob.qz*tx-ob.qx*tz);
  const float zz=z+ob.qw*tz+(ob.qx*ty-ob.qy*tx);
  x=xx;y=yy;z=zz;
}
}
const SceneObject* SceneObjects::get(uint32_t id) const {
  const auto it=objects_.find(id);
  return it==objects_.end()?nullptr:&it->second;
}
void SceneObjects::reset() {
  objects_.clear();missing_.clear();stats_={};revision_=0;
}
size_t SceneObjects::visible_roots() const {
  size_t total=0;
  for(const auto& [id,ob]:objects_)if(ob.parent_id==0&&ob.pcode==9)++total;
  return total;
}
std::vector<uint32_t> SceneObjects::take_cache_misses(size_t limit) {
  std::vector<uint32_t> ids;
  ids.reserve(std::min(limit,missing_.size()));
  // Repeated probes can re-enqueue an id if a requested full update is lost.
  for(auto it=missing_.begin();it!=missing_.end()&&ids.size()<limit;) {
    ids.push_back(*it);it=missing_.erase(it);
  }
  stats_.requested_cache_misses+=ids.size();
  return ids;
}
bool SceneObjects::ingest(const kami::sl::Packet& packet) {
  uint32_t id=0;size_t offset=0;
  if(!kami::sl::decode_message_id(packet,id,offset))return false;
  if(id!=full_id&&id!=compressed_id&&id!=cached_id&&id!=kill_id)return false;
  std::vector<SceneObject> received;std::vector<uint32_t> keys;
  size_t skipped=0;bool parsed=false;
  if(id==full_id) {++stats_.full_packets;parsed=parse_full(packet.payload,offset,received,skipped);}
  if(id==compressed_id) {++stats_.compressed_packets;parsed=parse_compressed(packet.payload,offset,received,skipped);}
  if(id==cached_id) {++stats_.cached_packets;parsed=parse_cached(packet.payload,offset,keys);}
  if(id==kill_id) {++stats_.kill_packets;parsed=parse_kill(packet.payload,offset,keys);}
  if(!parsed){++stats_.invalid_packets;return false;}
  stats_.unsupported_objects+=skipped;
  if(id==cached_id) {
    for(uint32_t key:keys)
      if(!objects_.contains(key)&&missing_.size()<8192)missing_.insert(key);
    return true;
  }
  if(id==kill_id) {
    for(uint32_t key:keys) {
      missing_.erase(key);
      if(objects_.erase(key))++revision_;
    }
    return true;
  }
  for(const auto& ob:received) {
    missing_.erase(ob.local_id);
    auto it=objects_.find(ob.local_id);
    if(it==objects_.end()) {
      if(objects_.size()>=8192)continue;
      objects_.emplace(ob.local_id,ob);
    } else it->second=ob;
    ++revision_;++stats_.updated_objects;
  }
  return true;
}
bool SceneObjects::build_proxy_mesh(TerrainMesh& output,size_t limit) const {
  TerrainMesh scene;
  size_t count=0;
  for(const auto& [id,ob]:objects_) {
    if(ob.pcode!=9||ob.parent_id||count>=limit)continue;
    const float hx=ob.sx*0.5f,hy=ob.sy*0.5f,hz=ob.sz*0.5f;
    TerrainMesh prim=make_box_mesh(0,0,0,ob.sx,ob.sz,ob.sy);
    for(auto& vertex:prim.vertices) {
      float x=vertex.x,y=vertex.z,z=vertex.y;
      rotate(ob,x,y,z);
      vertex.x=ob.x+x-127.5f;
      vertex.y=ob.z+z;
      vertex.z=ob.y+y-32.f;
      vertex.r=0.47f;vertex.g=0.47f;vertex.b=0.55f;
    }
    if(!append_mesh(scene,prim))return false;
    ++count;
  }
  if(scene.indices.empty())return false;
  output=std::move(scene);
  return true;
}
}
