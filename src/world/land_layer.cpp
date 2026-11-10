#include "land_layer.h"
namespace kami::sl {
LandBitReader::LandBitReader(const uint8_t* bytes,size_t size):bytes_(bytes),size_(size){}
size_t LandBitReader::bits_remaining() const {
  return bytes_ && bit_position_ <= size_*8 ? size_*8-bit_position_ : 0;
}
bool LandBitReader::read_signed_bits(unsigned count,int32_t& value) {
  if (!count || count>32) return false;
  uint32_t raw=0;
  if (!read_bits(count,raw)) return false;
  if (count==32) value=static_cast<int32_t>(raw);
  else {
    const uint32_t sign=uint32_t(1)<<(count-1);
    value=static_cast<int32_t>((raw^sign)-sign);
  }
  return true;
}
bool LandBitReader::read_bits(unsigned count,uint32_t& value) {
  if (count>32 || count>bits_remaining()) return false;
  uint32_t next=0;
  for (unsigned i=0;i<count;++i) {
    const size_t bit=bit_position_+i;
    next=(next<<1)|((bytes_[bit/8]>>(7-(bit%8)))&1u);
  }
  bit_position_+=count;
  value=next;
  return true;
}
bool read_land_group_header(LandBitReader& reader,LandGroupHeader& header) {
  uint32_t stride=0,size=0,type=0;
  if (!reader.read_bits(16,stride) || !reader.read_bits(8,size) ||
      !reader.read_bits(8,type)) return false;
  if (size!=16 && size!=32) return false;
  header={static_cast<uint16_t>(stride),static_cast<uint8_t>(size),static_cast<uint8_t>(type)};
  return true;
}
}
