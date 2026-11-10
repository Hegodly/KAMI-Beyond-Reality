#include "world/mesh_discovery.h"
#include <cassert>
#include <vector>
using namespace kami::world;
int main(){
  std::vector<uint8_t> mesh={2,
    0x20,0, 3,0,0,0, 11,22,33,
    0x60,0, 17,0,0,0};
  for(unsigned i=1;i<=16;++i)mesh.push_back(static_cast<uint8_t>(i));
  mesh.push_back(5);
  MeshExtra decoded;
  assert(find_mesh_extra(mesh.data(),mesh.size(),decoded));
  assert(decoded.valid && decoded.sculpt_type==5);
  assert(decoded.mesh_id[0]==1 && decoded.mesh_id[15]==16);
  std::vector<uint8_t> partial=mesh;
  partial.pop_back();
  assert(!find_mesh_extra(partial.data(),partial.size(),decoded));
  assert(decoded.valid && decoded.mesh_id[0]==1);
  auto sculpt=mesh;sculpt[11]=0x30;
  assert(find_mesh_extra(sculpt.data(),sculpt.size(),decoded)&&decoded.valid);
  sculpt.back()=6;
  assert(find_mesh_extra(sculpt.data(),sculpt.size(),decoded)&&!decoded.valid);
  auto impossible=mesh;impossible[12]=255;
  assert(!find_mesh_extra(impossible.data(),impossible.size(),decoded));
  assert(decoded.valid==false);
  assert(!find_mesh_extra(nullptr,17,decoded));
  assert(!find_mesh_extra(mesh.data(),16385,decoded));
  return 0;
}
