#include "scene_objects.h"
#include "primitive_shapes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
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
  bool texture(TextureEntries& value,bool& malformed) {
    uint16_t count=0;
    if(!u16(count)||count>remain())return false;
    const uint8_t* start=data.data()+pos;
    pos+=count;
    if(count==0)return true;
    if(!parse_texture_entries(start,count,value)) {
      malformed=true; // Keep object geometry, but never use wrong color.
    }
    return true;
  }
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
bool normalize_composed_rotation(SceneObject& ob) {
  const double norm=double(ob.qx)*ob.qx+double(ob.qy)*ob.qy+
                    double(ob.qz)*ob.qz+double(ob.qw)*ob.qw;
  if(!std::isfinite(norm)||norm<1.e-12||norm>4.0)return false;
  const float inverse=static_cast<float>(1.0/std::sqrt(norm));
  ob.qx*=inverse;ob.qy*=inverse;
  ob.qz*=inverse;ob.qw*=inverse;
  return true;
}
bool plausible(const SceneObject& ob) {
  auto size=[](float v){return std::isfinite(v)&&v>0.f&&v<=64.f;};
  return ob.local_id!=0 && ob.pcode==9 &&
    size(ob.sx)&&size(ob.sy)&&size(ob.sz) &&
    std::isfinite(ob.x)&&std::isfinite(ob.y)&&std::isfinite(ob.z) &&
    ob.x>=-256&&ob.x<=4096&&ob.y>=-256&&ob.y<=4096 &&
    ob.z>=-4096&&ob.z<=8192;
}
bool read_shape(Cursor& c,PrimitiveShape& shape) {
  uint8_t signed_byte=0;
  if (!c.u8(shape.path_curve)||!c.u8(shape.profile_curve)||
      !c.u16(shape.path_begin)||!c.u16(shape.path_end)||
      !c.u8(shape.path_scale_x)||!c.u8(shape.path_scale_y)||
      !c.u8(signed_byte)) return false;
  shape.path_shear_x=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.path_shear_y=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.path_twist=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.path_twist_begin=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.radius_offset=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.taper_x=static_cast<int8_t>(signed_byte);
  if(!c.u8(signed_byte))return false;
  shape.taper_y=static_cast<int8_t>(signed_byte);
  if(!c.u8(shape.revolutions)||!c.u8(signed_byte))return false;
  shape.skew=static_cast<int8_t>(signed_byte);
  if(!c.u16(shape.profile_begin)||!c.u16(shape.profile_end)||
     !c.u16(shape.profile_hollow))return false;
  shape.valid=shape.path_begin<=50000&&shape.path_end<=50000&&
    shape.profile_begin<=50000&&shape.profile_end<=50000&&
    shape.profile_hollow<=50000;
  return true;
}
bool parse_region_prefix(Cursor& c,uint8_t& count) {
  // RegionData: region handle U64, time dilation U16, variable block count U8.
  return c.skip(10)&&c.u8(count)&&count<=128;
}
bool parse_full(const std::vector<uint8_t>& data,size_t offset,
                std::vector<SceneObject>& objects,size_t& skipped,
                size_t& malformed_textures) {
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
    bool malformed_texture=false;
    if(!c.u32(ob.parent_id)||!c.u32(flags)||!read_shape(c,ob.shape)||
       !c.texture(ob.textures,malformed_texture)||!c.var1()||!c.var2()||!c.var2()||
       !c.var1()||!c.skip(4)||!c.var1()||!c.var1()||
       !c.var1()||!c.skip(16+16+4+1+4+1+12+12)) return false;
    if(malformed_texture)++malformed_textures;
    if ((length==60||length==124) && plausible(ob) && normalize_rotation(ob))
      objects.push_back(ob);
    else ++skipped;
  }
  return c.remain()==0;
}
bool parse_compressed(const std::vector<uint8_t>& data,size_t offset,
                      std::vector<SceneObject>& objects,size_t& skipped,
                size_t& malformed_textures) {
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
    // Following the fixed fields, a conventional compressed volume contains
    // an ExtraParams count, then its 23-byte Path/Profile descriptor.
    // Handle the common zero-extra-parameters case only. Do not guess byte
    // offsets across optional sounds, sculpt params, particles or text.
    // Those objects deliberately keep shape.valid=false until fully parsed.
    if(known && c.pos<end && (special&~0x20u)==0) {
      const size_t begin=c.pos;
      uint8_t extras=0;
      if(c.u8(extras) && extras==0 && end-c.pos>=23) {
        PrimitiveShape shape;
        if(read_shape(c,shape) && c.pos<=end) {
          ob.shape=shape;
          // A compressed data-packer stores the TextureEntry field as
          // a little-endian U16 length followed by its packed contents.
          if(end-c.pos>=2) {
            uint16_t count=0;
            if(c.u16(count) && count<=end-c.pos) {
              if(count!=0) {
                TextureEntries parsed;
                if(parse_texture_entries(data.data()+c.pos,count,parsed))
                  ob.textures=std::move(parsed);
                else ++malformed_textures;
              }
              c.pos+=count;
            }
          }
        }
      } else c.pos=begin;
    }
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
namespace {
bool resolve_world_pose(const std::unordered_map<uint32_t,SceneObject>& all,
                        const SceneObject& object,SceneObject& result) {
  std::array<const SceneObject*,16> chain{};
  size_t count=0;
  const SceneObject* current=&object;
  while (current) {
    if(count>=chain.size())return false; // unreasonable nesting
    for(size_t i=0;i<count;++i)
      if(chain[i]->local_id==current->local_id)return false; // cycle
    chain[count++]=current;
    if(!current->parent_id)break;
    const auto it=all.find(current->parent_id);
    if(it==all.end())return false; // do not place orphaned links at fake origins
    current=&it->second;
  }
  if(!count||chain[count-1]->parent_id)return false;
  result=*chain[count-1]; // true root pose is region-local
  for(size_t i=count-1;i>0;--i) {
    const SceneObject& child=*chain[i-1];
    float x=child.x,y=child.y,z=child.z;
    rotate(result,x,y,z); // child offset is root-local, in SL XYZ
    result.x+=x;result.y+=y;result.z+=z;
    const float ax=result.qx,ay=result.qy,az=result.qz,aw=result.qw;
    result.qx=aw*child.qx+ax*child.qw+ay*child.qz-az*child.qy;
    result.qy=aw*child.qy-ax*child.qz+ay*child.qw+az*child.qx;
    result.qz=aw*child.qz+ax*child.qy-ay*child.qx+az*child.qw;
    result.qw=aw*child.qw-ax*child.qx-ay*child.qy-az*child.qz;
    // The sign of W matters after composing multiple link rotations.
    // The wire-level positive-W reconstruction is only appropriate for
    // individual compact quaternion updates, not composed quaternions.
    if(!normalize_composed_rotation(result))return false;
  }
  return std::isfinite(result.x)&&std::isfinite(result.y)&&std::isfinite(result.z);
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
size_t SceneObjects::renderable_prims() const {
  size_t count=0;
  SceneObject pose;
  for(const auto& [id,ob]:objects_)
    if(ob.pcode==9&&resolve_world_pose(objects_,ob,pose))++count;
  return count;
}
size_t SceneObjects::objects_with_face_colors() const {
  size_t count=0;
  for(const auto& [id,object]:objects_)
    if(object.textures.valid)++count;
  return count;
}
std::vector<AssetId> SceneObjects::referenced_texture_ids() const {
  std::set<AssetId> ids;
  for(const auto& [key,ob]:objects_) {
    if(!ob.textures.valid)continue;
    for(const auto& face:ob.textures.faces) {
      if(valid_asset_uuid(face.texture_id)) ids.insert(face.texture_id);
      if(ids.size()>=TextureAssetQueue::max_entries)break;
    }
    if(ids.size()>=TextureAssetQueue::max_entries)break;
  }
  return {ids.begin(),ids.end()};
}
size_t SceneObjects::unique_texture_references() const {
  // Catalog metadata only, not fetched image pixels. Deduplicate the same
  // UUID used by many prim faces, bounded for memory and UI responsiveness.
  std::set<std::array<uint8_t,16>> ids;
  for(const auto& [id,object]:objects_) {
    if(!object.textures.valid)continue;
    for(const auto& face:object.textures.faces) {
      if(std::any_of(face.texture_id.begin(),face.texture_id.end(),
                     [](uint8_t b){return b!=0;}))
        ids.insert(face.texture_id);
      if(ids.size()>=4096)return ids.size();
    }
  }
  return ids.size();
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
void SceneObjects::requeue_cache_misses(const std::vector<uint32_t>& ids) {
  for(uint32_t id:ids)
    if(id!=0 && !objects_.contains(id)&&missing_.size()<8192)
      missing_.insert(id);
}
bool SceneObjects::ingest(const kami::sl::Packet& packet) {
  uint32_t id=0;size_t offset=0;
  if(!kami::sl::decode_message_id(packet,id,offset))return false;
  if(id!=full_id&&id!=compressed_id&&id!=cached_id&&id!=kill_id)return false;
  std::vector<SceneObject> received;std::vector<uint32_t> keys;
  size_t skipped=0,malformed_texture=0;bool parsed=false;
  if(id==full_id) {++stats_.full_packets;parsed=parse_full(packet.payload,offset,received,skipped,malformed_texture);}
  if(id==compressed_id) {++stats_.compressed_packets;parsed=parse_compressed(packet.payload,offset,received,skipped,malformed_texture);}
  if(id==cached_id) {++stats_.cached_packets;parsed=parse_cached(packet.payload,offset,keys);}
  if(id==kill_id) {++stats_.kill_packets;parsed=parse_kill(packet.payload,offset,keys);}
  if(!parsed){++stats_.invalid_packets;return false;}
  stats_.unsupported_objects+=skipped;
  stats_.invalid_texture_entries+=malformed_texture;
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
    if(ob.textures.valid) ++stats_.textured_objects;
    missing_.erase(ob.local_id);
    auto it=objects_.find(ob.local_id);
    if(it==objects_.end()) {
      if(objects_.size()>=8192)continue;
      objects_.emplace(ob.local_id,ob);
    } else {
      // Some simulator updates omit optional TE or shape blocks. Never
      // replace a previously validated appearance with an empty placeholder
      // just because an incremental packet lacks material information.
      SceneObject updated=ob;
      if(!updated.textures.valid && it->second.textures.valid)
        updated.textures=it->second.textures;
      if(!updated.shape.valid && it->second.shape.valid)
        updated.shape=it->second.shape;
      it->second=std::move(updated);
    }
    ++revision_;++stats_.updated_objects;
  }
  return true;
}
bool SceneObjects::build_proxy_mesh(TerrainMesh& output,size_t limit) const {
  TerrainMesh scene;
  size_t count=0;
  for(const auto& [id,ob]:objects_) {
    if(ob.pcode!=9||count>=limit)continue;
    SceneObject pose;
    if(!resolve_world_pose(objects_,ob,pose))continue;
    TerrainMesh prim=make_primitive_proxy(ob);
    tint_primitive_proxy(ob,prim);
    for(auto& vertex:prim.vertices) {
      float x=vertex.x,y=vertex.z,z=vertex.y;
      rotate(pose,x,y,z);
      vertex.x=pose.x+x-127.5f;
      vertex.y=pose.z+z;
      vertex.z=pose.y+y-32.f;
      if(!ob.textures.valid) {
        vertex.r=0.47f;vertex.g=0.47f;vertex.b=0.55f;
      }
    }
    if(!append_mesh(scene,prim))return false;
    ++count;
  }
  if(scene.indices.empty())return false;
  output=std::move(scene);
  return true;
}
}
