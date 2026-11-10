#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace kami::sl {
class LandBitReader {
public:
  LandBitReader(const uint8_t* bytes, size_t size);
  bool read_bits(unsigned count, uint32_t& value);
  size_t bits_remaining() const;
private:
  const uint8_t* bytes_;
  size_t size_;
  size_t bit_position_ = 0;
};
struct LandGroupHeader {
  uint16_t stride = 0;
  uint8_t patch_size = 0;
  uint8_t layer_type = 0;
};
bool read_land_group_header(LandBitReader& reader, LandGroupHeader& header);
}
