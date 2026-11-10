#include "world/terrain_stream.h"
#include "world/land_layer.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
namespace {
struct BitWriter {
  std::vector<uint8_t> bytes;
  size_t bit=0;
  void put(uint32_t v,unsigned count) {
    for (unsigned n=count;n>0;--n) {
      if (!(bit&7u)) bytes.push_back(0);
      if ((v>>(n-1))&1u) bytes.back() |= static_cast<uint8_t>(1u<<(7-(bit&7)));
      ++bit;
    }
  }
  void u16le(unsigned v) {put(v&255u,8);put((v>>8)&255u,8);}
  void f32le(uint32_t bits) {
    put(bits&255u,8);put((bits>>8)&255u,8);
    put((bits>>16)&255u,8);put((bits>>24)&255u,8);
  }
  void header(unsigned quant,uint32_t min_height_bits,unsigned range,unsigned patch_id) {
    put(quant,8);
    f32le(min_height_bits);
    u16le(range);
    put(patch_id&255u,8);
    put((patch_id>>8)&3u,2);
  }
};
std::vector<uint8_t> zerocode(const std::vector<uint8_t>& bytes) {
  std::vector<uint8_t> result;
  for(size_t i=0;i<bytes.size();) {
    if(bytes[i]!=0) {result.push_back(bytes[i++]);continue;}
    uint8_t run=0;
    while(i<bytes.size() && bytes[i]==0 && run<255) {++i;++run;}
    result.push_back(0);result.push_back(run);
  }
  return result;
}
}
int main() {
  BitWriter bits;
  bits.u16le(256);bits.put(16,8);bits.put('L',8);
  // Two 16x16 patches at different X locations in the same LL group.
  // DC offset -7.0 (LE), range 16: flat patch renders exactly 1m.
  bits.header(0,0xc0e00000u,16,(5u<<5)|3u);
  bits.put(1,1);bits.put(0,1); // EOB
  // Second patch has nonzero horizontal frequency, not just flat samples.
  // DC offset -5.0f, word_bits=4, one zero then +3, then EOB.
  bits.header(2,0xc0a00000u,16,(7u<<5)|3u);
  bits.put(0,1);
  bits.put(1,1);bits.put(1,1);bits.put(0,1);bits.put(3,4);
  bits.put(1,1);bits.put(0,1);
  bits.put(97,8);
  std::vector<kami::sl::DecodedLandPatch> patches;
  assert(kami::sl::decode_land_group(bits.bytes.data(),bits.bytes.size(),patches));
  assert(patches.size()==2);
  assert(patches[0].x==80 && patches[0].y==48);
  assert(patches[1].x==112 && patches[1].y==48);
  assert(patches[0].heights.size()==256 && patches[1].heights.size()==256);
  for (float h:patches[0].heights) assert(std::fabs(h-1.0f)<0.01f);
  assert(std::fabs(patches[1].heights[0]-patches[1].heights[15])>0.01f);
  std::vector<uint8_t> body{11,'L',
    static_cast<uint8_t>(bits.bytes.size()),static_cast<uint8_t>(bits.bytes.size()>>8)};
  body.insert(body.end(),bits.bytes.begin(),bits.bytes.end());
  const auto coded=zerocode(body);
  std::vector<uint8_t> packet{0x80,0,0,0,7,0};
  packet.insert(packet.end(),coded.begin(),coded.end());
  kami::world::TerrainStream stream;
  assert(stream.ingest_datagram(packet.data(),packet.size()));
  assert(stream.stats().decoded==1 && stream.stats().land_packets==1);
  assert(stream.terrain().received_samples()==512);
  float first=0,last=0;
  assert(stream.terrain().sample_height(80,48,first));
  assert(std::fabs(first-1.0f)<0.01f);
  assert(stream.terrain().sample_height(112,48,first));
  assert(stream.terrain().sample_height(127,48,last));
  assert(std::fabs(first-last)>0.01f);
  kami::world::TerrainMesh mesh;
  assert(stream.build_mesh(mesh));
  assert(!mesh.indices.empty());
}
