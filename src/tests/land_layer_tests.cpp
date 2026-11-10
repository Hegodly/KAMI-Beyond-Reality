#include "world/land_layer.h"
#include <cassert>
#include <cstdint>
int main() {
  using namespace kami::sl;
  const uint8_t bytes[]={0x12,0x34,0x10,0x4C};
  LandBitReader reader(bytes,sizeof(bytes));
  uint32_t v=0;
  assert(reader.bits_remaining()==32);
  assert(reader.read_bits(4,v)&&v==1);
  assert(reader.read_bits(12,v)&&v==0x234);
  assert(reader.read_bits(8,v)&&v==16);
  assert(reader.read_bits(8,v)&&v==0x4C);
  assert(reader.bits_remaining()==0);
  assert(!reader.read_bits(1,v));
  LandBitReader empty(nullptr,0);
  assert(empty.bits_remaining()==0 && !empty.read_bits(1,v));
  assert(empty.read_bits(0,v) && v==0);
  LandBitReader excessive(bytes,sizeof(bytes));
  assert(!excessive.read_bits(33,v));
  assert(excessive.bits_remaining()==32);
  LandBitReader header_reader(bytes,sizeof(bytes));
  LandGroupHeader header;
  assert(read_land_group_header(header_reader,header));
  assert(header.stride==0x1234 && header.patch_size==16 && header.layer_type==0x4C);
}
