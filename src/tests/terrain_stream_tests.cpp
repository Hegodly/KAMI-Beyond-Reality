#include "world/terrain_stream.h"
#include <cassert>
#include <cstdint>
#include <vector>
int main() {
  using kami::world::TerrainStream;
  TerrainStream stream(256,256);
  assert(!stream.terrain().any_renderable_cells());
  assert(!stream.ingest_datagram(nullptr,0));
  assert(stream.stats().invalid==1);
  const uint8_t other[]={0,0,0,0,1,0,0x12,0x00};
  assert(!stream.ingest_datagram(other,sizeof(other)));
  assert(stream.stats().ignored==1);
  // Encode one 16x16 flat LandLayer patch using the same MSB-first
  // bit framing as the simulator; no local terrain fabrication in product.
  std::vector<uint8_t> group;
  auto bits=[&](uint32_t value,unsigned count) {
    static_cast<void>(value);
    static_cast<void>(count);
  };
  size_t written=0;
  auto put=[&](uint32_t value,unsigned count) {
    for (unsigned bit=count;bit>0;--bit) {
      if ((written&7u)==0) group.push_back(0);
      if ((value>>(bit-1))&1u) group.back() |= uint8_t(1u<<(7u-(written&7u)));
      ++written;
    }
  };
  (void)bits;
  put(0,16); put(16,8); put(76,8);
  put(0,8); put(0x3f800000u,32); put(16,16); put(0,10);
  for (unsigned i=0;i<256;++i) { put(0,1); put(0,1); }
  put(97,8);
  std::vector<uint8_t> datagram={0,0,0,0,2,0,11,76,
    static_cast<uint8_t>(group.size()),static_cast<uint8_t>(group.size()>>8)};
  datagram.insert(datagram.end(),group.begin(),group.end());
  assert(stream.ingest_datagram(datagram.data(),datagram.size()));
  assert(stream.stats().decoded==1 && stream.revision()==1);
  assert(stream.terrain().received_samples()==256);
  assert(stream.terrain().renderable_cells()==225);
  kami::world::TerrainMesh mesh;
  assert(stream.build_mesh(mesh));
  assert(mesh.indices.size()==1350);
  float elevation=0;
  assert(stream.terrain().sample_height(0,0,elevation) && elevation==1.0f);
  assert(!stream.terrain().complete());
  datagram[7]=55;
  assert(!stream.ingest_datagram(datagram.data(),datagram.size()));
  assert(stream.stats().invalid==1 && stream.stats().ignored==2);
  stream.reset();
  assert(stream.stats().packets==0 && stream.revision()==0);
  assert(stream.terrain().received_samples()==0);
}
