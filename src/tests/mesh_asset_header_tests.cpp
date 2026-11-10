#include "world/mesh_asset_header.h"
#include <cassert>
#include <vector>
#include <string>
using namespace kami::world;
namespace {
void be32(std::vector<uint8_t>& data,uint32_t n){
  data.push_back(uint8_t(n>>24));data.push_back(uint8_t(n>>16));
  data.push_back(uint8_t(n>>8));data.push_back(uint8_t(n));
}
void key(std::vector<uint8_t>& d,const std::string& s){
  d.push_back('k');be32(d,uint32_t(s.size()));
  d.insert(d.end(),s.begin(),s.end());
}
void integer(std::vector<uint8_t>& d,uint32_t x){d.push_back('i');be32(d,x);}
std::vector<uint8_t> fixture(){
  const std::string pre="<?llsd/binary?>\n";
  std::vector<uint8_t> d(pre.begin(),pre.end());
  d.push_back('{');be32(d,3);
  key(d,"version");integer(d,1);
  key(d,"creator");d.push_back('u');
  for(int i=0;i<16;i++)d.push_back(static_cast<uint8_t>(i));
  key(d,"high_lod");d.push_back('{');be32(d,2);
  key(d,"offset");integer(d,0);
  key(d,"size");integer(d,8);
  d.push_back('}');d.push_back('}');
  for(int i=0;i<8;++i)d.push_back(0xa0+i);
  return d;
}
}
int main(){
  auto blob=fixture();
  MeshAssetHeader h;
  assert(parse_mesh_asset_header(blob,h));
  assert(h.high.present&&h.high.offset==0&&h.high.size==8);
  assert(h.header_bytes+8==blob.size());
  assert(!h.medium.present);
  const auto intact=h;
  blob.pop_back();
  assert(!parse_mesh_asset_header(blob,h));
  assert(h.header_bytes==intact.header_bytes);
  blob=fixture();
  blob[0]='x';
  assert(!parse_mesh_asset_header(blob,h));
  blob=fixture();
  assert(!parse_mesh_asset_header(nullptr,blob.size(),h));
  assert(!parse_mesh_asset_header(blob.data(),16u*1024u*1024u+1,h));
}
