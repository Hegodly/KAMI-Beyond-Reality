#include "world/scene_objects.h"
#include "world/lludp.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
namespace {
void u8(std::vector<uint8_t>& b,uint8_t v){b.push_back(v);}
void zeros(std::vector<uint8_t>& b,size_t n){b.insert(b.end(),n,uint8_t{0});}
void u16(std::vector<uint8_t>& b,uint16_t v){u8(b,uint8_t(v));u8(b,uint8_t(v>>8));}
void u32(std::vector<uint8_t>& b,uint32_t v){
  for(unsigned i=0;i<4;++i)u8(b,uint8_t(v>>(8*i)));
}
void f32(std::vector<uint8_t>& b,float v){
  uint32_t bits=0;std::memcpy(&bits,&v,4);u32(b,bits);
}
void vec(std::vector<uint8_t>& b,float x,float y,float z){f32(b,x);f32(b,y);f32(b,z);}
void region(std::vector<uint8_t>& b){zeros(b,10);u8(b,1);}
kami::sl::Packet make_compressed(uint32_t id=100,uint32_t parent=0,uint8_t pcode=9,float root_z_rotation=0,uint8_t profile=255) {
  std::vector<uint8_t> b{13};region(b);u32(b,0);
  std::vector<uint8_t> content;
  zeros(content,16);u32(content,id);u8(content,pcode);u8(content,0);
  u32(content,0);u8(content,0);u8(content,0);
  vec(content,2,4,6); // scale (SL X,Y,Z)
  vec(content,parent?5.f:128.f,parent?0.f:128.f,parent?2.f:25.f);
  vec(content,0,0,root_z_rotation); // SL quaternion xyz; w reconstructed
  u32(content,parent?0x20u:0);zeros(content,16);
  if(parent)u32(content,parent);
  if(profile!=255) {
    // Plain compressed volume with no extra params, followed by the
    // same 23-byte Path/Profile descriptor as ObjectUpdate.
    u8(content,0); // ExtraParams count
    u8(content,0x10);u8(content,profile);
    u16(content,0);u16(content,0);
    u8(content,100);u8(content,100);
    zeros(content,9);
    u16(content,0);u16(content,0);u16(content,0);
  }
  u16(b,static_cast<uint16_t>(content.size()));
  b.insert(b.end(),content.begin(),content.end());
  kami::sl::Packet p;p.payload=std::move(b);return p;
}
kami::sl::Packet make_full(uint32_t id,uint8_t path_curve=0,uint8_t profile_curve=0,uint16_t path_cut=0) {
  std::vector<uint8_t> b{12};region(b);
  u32(b,id);u8(b,0);zeros(b,16);u32(b,0);
  u8(b,9);u8(b,0);u8(b,0);vec(b,2,4,6);
  u8(b,60);vec(b,128,128,25);zeros(b,24);
  vec(b,0,0,0);zeros(b,12);
  u32(b,0);u32(b,0);
  u8(b,path_curve);u8(b,profile_curve);
  u16(b,path_cut);u16(b,0);
  u8(b,100);u8(b,100);
  zeros(b,9); // shear/twist/radius/taper/revolutions/skew
  u16(b,0);u16(b,0);u16(b,0); // profile begin/end/hollow
  u16(b,0);u8(b,0);u16(b,0);u16(b,0);
  u8(b,0);zeros(b,4);u8(b,0);u8(b,0);u8(b,0);
  zeros(b,16+16+4+1+4+1+12+12);
  kami::sl::Packet p;p.payload=std::move(b);return p;
}
kami::sl::Packet make_cached(uint32_t id) {
  std::vector<uint8_t> b{14};region(b);
  u32(b,id);u32(b,0);u32(b,0);
  kami::sl::Packet p;p.payload=std::move(b);return p;
}
kami::sl::Packet make_kill(uint32_t id) {
  std::vector<uint8_t> b{16};u8(b,1);u32(b,id);
  kami::sl::Packet p;p.payload=std::move(b);return p;
}
}
int main() {
  kami::world::SceneObjects scene;
  assert(!scene.ingest(kami::sl::Packet{}));
  assert(scene.ingest(make_compressed()));
  assert(scene.revision()==1 && scene.total_objects()==1);
  assert(scene.visible_roots()==1 && scene.stats().compressed_packets==1);
  const auto* ob=scene.get(100);
  assert(ob && ob->x==128 && ob->y==128 && ob->z==25);
  assert(ob->sx==2 && ob->sy==4 && ob->sz==6);
  assert(ob->qw==1 && ob->parent_id==0);
  kami::world::TerrainMesh proxy;
  assert(scene.build_proxy_mesh(proxy));
  assert(proxy.vertices.size()==8 && proxy.indices.size()==36);
  float miny=1000,maxy=-1000,minx=1000,maxx=-1000;
  for(auto& v:proxy.vertices){
    miny=std::min(miny,v.y);maxy=std::max(maxy,v.y);
    minx=std::min(minx,v.x);maxx=std::max(maxx,v.x);
  }
  assert(std::abs(miny-22)<0.01f && std::abs(maxy-28)<0.01f);
  assert(std::abs(minx+0.5f)<0.01f && std::abs(maxx-1.5f)<0.01f);
  assert(scene.ingest(make_cached(111)));
  assert(scene.take_cache_misses(32)==std::vector<uint32_t>{111});
  assert(scene.take_cache_misses(32).empty());
  assert(scene.ingest(make_cached(100)));
  assert(scene.take_cache_misses(32).empty());
  assert(scene.ingest(make_full(120)));
  assert(scene.get(120) && scene.visible_roots()==2);
  assert(scene.ingest(make_compressed(130,100)));
  assert(scene.get(130) && scene.visible_roots()==2);
  const auto prev=scene.revision();
  auto bad=make_full(99);
  bad.payload.pop_back();
  assert(!scene.ingest(bad));
  assert(scene.stats().invalid_packets==1);
  assert(scene.revision()==prev && !scene.get(99));
  assert(scene.ingest(make_kill(120)));
  assert(!scene.get(120) && scene.visible_roots()==1);
  auto avatar=make_compressed(150,0,47);
  assert(scene.ingest(avatar)&&!scene.get(150));
  assert(scene.stats().unsupported_objects>=1);
  // Shapes are determined by actual official full-update PathCurve
  // and ProfileCurve bytes, not guessed from object scale alone.
  kami::world::SceneObjects primitives;
  assert(primitives.ingest(make_full(300,0x10,0x00)));
  auto* cylinder=primitives.get(300);
  assert(cylinder && cylinder->shape.valid);
  assert(cylinder->shape.path_curve==0x10);
  assert(cylinder->shape.profile_curve==0);
  assert(cylinder->shape.path_scale_x==100);
  kami::world::TerrainMesh round_mesh;
  assert(primitives.build_proxy_mesh(round_mesh));
  assert(round_mesh.vertices.size()==50); // 24-sided cylinder + cap centers
  assert(round_mesh.indices.size()==288);
  assert(primitives.ingest(make_kill(300)));
  // Compressed format carries the same shape descriptor after the zero
  // extra-parameter count, when no optional sound/sculpt/text fields exist.
  assert(primitives.ingest(make_compressed(305,0,9,0,0)));
  auto* packed_cylinder=primitives.get(305);
  assert(packed_cylinder && packed_cylinder->shape.valid);
  assert(packed_cylinder->shape.path_curve==0x10);
  assert(packed_cylinder->shape.profile_curve==0);
  kami::world::TerrainMesh packed_mesh;
  assert(primitives.build_proxy_mesh(packed_mesh));
  assert(packed_mesh.indices.size()==288);
  assert(primitives.ingest(make_kill(305)));
  assert(primitives.ingest(make_full(301,0x20,0x05)));
  kami::world::TerrainMesh sphere;
  assert(primitives.build_proxy_mesh(sphere));
  assert(sphere.vertices.size()==325);
  assert(sphere.indices.size()==1728);
  assert(primitives.ingest(make_kill(301)));
  assert(primitives.ingest(make_full(302,0x10,0x03)));
  kami::world::TerrainMesh triangle;
  assert(primitives.build_proxy_mesh(triangle));
  assert(triangle.vertices.size()==8);
  assert(triangle.indices.size()==36);
  assert(primitives.ingest(make_kill(302)));
  // Nonzero path cut cannot be safely modeled by the uncut primitive mesh.
  assert(primitives.ingest(make_full(303,0x10,0x00,500)));
  kami::world::TerrainMesh fallback;
  assert(primitives.build_proxy_mesh(fallback));
  assert(fallback.vertices.size()==8);
  assert(fallback.indices.size()==36);
  // Parent rotation must rotate a child's local offset before moving
  // it into region space; orphaned or cyclic links must not be drawn.
  kami::world::SceneObjects links;
  assert(links.ingest(make_compressed(501,500)));
  assert(links.renderable_prims()==0);
  assert(links.ingest(make_compressed(500,0,9,0.70710678f)));
  assert(links.renderable_prims()==2);
  kami::world::TerrainMesh pair;
  assert(links.build_proxy_mesh(pair));
  assert(pair.vertices.size()==16 && pair.indices.size()==72);
  bool found_root=false,found_child=false;
  for(size_t base=0;base<16;base+=8) {
    float ax=0,ay=0,az=0;
    for(size_t v=base;v<base+8;++v) {
      ax+=pair.vertices[v].x;ay+=pair.vertices[v].y;az+=pair.vertices[v].z;
    }
    ax/=8;ay/=8;az/=8;
    if(std::fabs(ax-.5f)<.02f && std::fabs(ay-25)<.02f &&
       std::fabs(az-96)<.02f)found_root=true;
    if(std::fabs(ax-.5f)<.02f && std::fabs(ay-27)<.02f &&
       std::fabs(az-101)<.02f)found_child=true;
  }
  assert(found_root && found_child);
  assert(links.ingest(make_kill(500)));
  assert(links.renderable_prims()==0); // orphan is safely hidden
  kami::world::SceneObjects cycles;
  assert(cycles.ingest(make_compressed(700,701)));
  assert(cycles.ingest(make_compressed(701,700)));
  assert(cycles.renderable_prims()==0);
  kami::world::TerrainMesh invalid_mesh;
  assert(!cycles.build_proxy_mesh(invalid_mesh));
  // Compose two +135-degree rotations (total +270 degrees). The
  // composite quaternion legitimately has negative W; replacing it
  // with positive sqrt(1-xyz^2) would point the grandchild the wrong way.
  kami::world::SceneObjects nested;
  constexpr float sin_67_5=0.923879533f;
  assert(nested.ingest(make_compressed(900,0,9,sin_67_5)));
  assert(nested.ingest(make_compressed(901,900,9,sin_67_5)));
  assert(nested.ingest(make_compressed(902,901)));
  assert(nested.renderable_prims()==3);
  kami::world::TerrainMesh nested_mesh;
  assert(nested.build_proxy_mesh(nested_mesh));
  assert(nested_mesh.vertices.size()==24);
  bool oriented_grandchild=false;
  for(size_t base=0;base<24;base+=8) {
    float x=0,y=0,z=0;
    for(size_t v=base;v<base+8;++v) {
      x+=nested_mesh.vertices[v].x;
      y+=nested_mesh.vertices[v].y;
      z+=nested_mesh.vertices[v].z;
    }
    x/=8;y/=8;z/=8;
    if(std::fabs(x+3.0355f)<0.04f &&
       std::fabs(y-29.f)<0.04f &&
       std::fabs(z-94.5355f)<0.04f)
      oriented_grandchild=true;
  }
  assert(oriented_grandchild);
  scene.reset();
  assert(scene.total_objects()==0 && scene.revision()==0);
}
