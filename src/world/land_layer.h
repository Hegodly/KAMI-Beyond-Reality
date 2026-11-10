#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace kami::sl {
class LandBitReader {
public:
  LandBitReader(const uint8_t* bytes, size_t size);
  bool read_bits(unsigned count, uint32_t& value);
  bool read_signed_bits(unsigned count, int32_t& value);
  bool peek_bits(unsigned count, uint32_t& value) const;
  bool skip_bits(size_t count);
  bool align_to_byte();
  bool read_u8(uint8_t& value);
  bool read_u16(uint16_t& value);
  bool read_u32(uint32_t& value);
  bool read_float32(float& value);
  size_t bits_remaining() const;
  size_t bit_position() const { return bit_position_; }
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
struct LandPatchHeader {
  uint8_t quant_wbits = 0;
  float dc_offset = 0;
  uint16_t range = 0;
  uint16_t patch_id = 0;
};
bool read_land_patch_header(LandBitReader& reader, LandPatchHeader& header);
bool read_land_patch_end(LandBitReader& reader, bool& end);
struct DecodedLandPatch {
  uint32_t x=0,y=0;
  uint32_t size=0;
  std::vector<float> heights;
};
bool decode_land_patch(LandBitReader& reader,unsigned patch_size,DecodedLandPatch& patch);
bool decode_land_group(const uint8_t* bytes,size_t size,std::vector<DecodedLandPatch>& patches);
bool reconstruct_land_patch(const std::vector<int32_t>& coefficients, const LandPatchHeader& header, unsigned patch_size, std::vector<float>& heights);
bool scale_land_heights(const std::vector<float>& frequency_heights, const LandPatchHeader& header, unsigned patch_size, std::vector<float>& heights);
bool inverse_land_dct(const std::vector<int32_t>& coefficients, unsigned patch_size, std::vector<float>& samples);
bool decode_land_patch_coordinates(uint16_t patch_id, unsigned patch_size, uint32_t& x, uint32_t& y);
bool decode_land_patch_coefficients(LandBitReader& reader, unsigned patch_size, const LandPatchHeader& header, std::vector<int32_t>& coefficients);
bool decode_land_coefficients(LandBitReader& reader, unsigned patch_size, unsigned word_bits, std::vector<int32_t>& coefficients);
}
