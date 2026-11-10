#include "world/mesh_asset_header.h"
#include "world/mesh_geometry.h"
#include <zlib.h>
#include <cassert>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
using namespace kami::world;
namespace {
void be32(std::vector<uint8_t>& d,uint32_t n){
  d.insert(d.end(),{uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)});
}
void key(std::vector<uint8_t>& d,const std::string& s){
  d.push_back('k');be32(d,uint32_t(s.size()));
  d.insert(d.end(),s.begin(),s.end());
}
void binary(std::vector<uint8_t>& d,const std::vector<uint8_t>& bytes){
  d.push_back('b');be32(d,uint32_t(bytes.size()));
  d.insert(d.end(),bytes.begin(),bytes.end());
}
void le16(std::vector<uint8_t>& d,uint16_t n){
  d.push_back(uint8_t(n));d.push_back(uint8_t(n>>8));
}
std::vector<uint8_t> fixture(bool bad_triangle=false,bool medium_only=false) {
  std::vector<uint8_t> sub{'['};be32(sub,1);
  sub.push_back('{');be32(sub,3);
  key(sub,"Position");
  std::vector<uint8_t> positions;
  for(const uint16_t n:{0,0,0,65535,0,0,0,65535,0})le16(positions,n);
  binary(sub,positions);
  key(sub,"TriangleList");
  std::vector<uint8_t> tris;
  for(const uint16_t n:std::array<uint16_t,3>{0,1,static_cast<uint16_t>(bad_triangle?4:2)})
    le16(tris,n);
  binary(sub,tris);
  key(sub,"TexCoord0");
  std::vector<uint8_t> uv;
  for(const uint16_t n:{0,0,65535,0,0,65535})le16(uv,n);
  binary(sub,uv);
  sub.push_back('}');sub.push_back(']');
  uLongf bound=compressBound(static_cast<uLong>(sub.size()));
  std::vector<uint8_t> compressed(bound);
  assert(compress2(compressed.data(),&bound,sub.data(),
                   static_cast<uLong>(sub.size()),Z_BEST_SPEED)==Z_OK);
  compressed.resize(bound);
  std::vector<uint8_t> header{'{'};be32(header,medium_only?2:1);
  if(medium_only){
    key(header,"high_lod");header.push_back('{');be32(header,2);
    key(header,"offset");header.push_back('i');be32(header,UINT32_MAX);
    key(header,"size");header.push_back('i');be32(header,0);
    header.push_back('}');
  }
  key(header,medium_only?"medium_lod":"high_lod");
  header.push_back('{');be32(header,2);
  key(header,"offset");header.push_back('i');be32(header,0);
  key(header,"size");header.push_back('i');be32(header,uint32_t(compressed.size()));
  header.push_back('}');header.push_back('}');
  header.insert(header.end(),compressed.begin(),compressed.end());
  return header;
}
}
int main(){
  const auto data=fixture();
  MeshAssetHeader header;
  assert(parse_mesh_asset_header(data,header));
  DecodedMeshGeometry geom;
  assert(decode_mesh_high_lod(data,header,geom));
  assert(geom.faces.size()==1&&geom.vertices==3&&geom.triangles==1);
  assert(geom.faces[0].indices==std::vector<uint32_t>({0,1,2}));
  const auto& v=geom.faces[0].vertices;
  assert(std::fabs(v[0].x+0.5f)<.001f && std::fabs(v[0].y+0.5f)<.001f);
  assert(std::fabs(v[1].x-0.5f)<.001f && std::fabs(v[2].z-0.5f)<.001f);
  assert(v[1].u==1.f && v[2].v==1.f);
  assert(geom.quality==MeshLodQuality::High);
  auto sparse=fixture(false,true);
  MeshAssetHeader sparse_header;
  assert(parse_mesh_asset_header(sparse,sparse_header));
  assert(!sparse_header.high.present && sparse_header.medium.present);
  assert(!decode_mesh_high_lod(sparse,sparse_header,geom));
  assert(decode_mesh_best_lod(sparse,sparse_header,geom));
  assert(geom.quality==MeshLodQuality::Medium);
  assert(geom.vertices==3 && geom.triangles==1);
  auto broken=fixture(true);
  MeshAssetHeader bad_header;
  assert(parse_mesh_asset_header(broken,bad_header));
  const auto saved=geom.vertices;
  assert(!decode_mesh_high_lod(broken,bad_header,geom));
  assert(geom.vertices==saved);
  auto corrupt=data;
  corrupt[header.header_bytes+3]^=0x80;
  assert(!decode_mesh_high_lod(corrupt,header,geom));
  return 0;
}
