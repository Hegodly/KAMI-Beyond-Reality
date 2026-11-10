#include "land_layer.h"
#include <cstring>
#include <cmath>
#include <utility>
namespace kami::sl {
LandBitReader::LandBitReader(const uint8_t* bytes,size_t size):bytes_(bytes),size_(size){}
size_t LandBitReader::bits_remaining() const {
  return bytes_ && bit_position_ <= size_*8 ? size_*8-bit_position_ : 0;
}
bool LandBitReader::read_float32(float& value) {
  uint32_t raw=0;
  if (!read_u32(raw)) return false;
  float decoded=0;
  std::memcpy(&decoded,&raw,sizeof(decoded));
  if (!std::isfinite(decoded)) return false;
  value=decoded;
  return true;
}
bool LandBitReader::read_u32(uint32_t& value) {
  return read_bits(32,value);
}
bool LandBitReader::read_u16(uint16_t& value) {
  uint32_t raw=0;
  if (!read_bits(16,raw)) return false;
  value=static_cast<uint16_t>(raw);
  return true;
}
bool LandBitReader::read_u8(uint8_t& value) {
  uint32_t raw=0;
  if (!read_bits(8,raw)) return false;
  value=static_cast<uint8_t>(raw);
  return true;
}
bool LandBitReader::align_to_byte() {
  const size_t padding=(8-bit_position_%8)%8;
  return skip_bits(padding);
}
bool LandBitReader::skip_bits(size_t count) {
  if (count>bits_remaining()) return false;
  bit_position_+=count;
  return true;
}
bool LandBitReader::peek_bits(unsigned count,uint32_t& value) const {
  if (count>32 || count>bits_remaining()) return false;
  uint32_t next=0;
  for (unsigned i=0;i<count;++i) {
    const size_t bit=bit_position_+i;
    next=(next<<1)|((bytes_[bit/8]>>(7-(bit%8)))&1u);
  }
  value=next;
  return true;
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
bool reconstruct_land_patch(const std::vector<int32_t>& coefficients,const LandPatchHeader& header,unsigned patch_size,std::vector<float>& heights) {
  std::vector<float> transformed;
  return inverse_land_dct(coefficients,patch_size,transformed) &&
         scale_land_heights(transformed,header,patch_size,heights);
}
bool scale_land_heights(const std::vector<float>& frequency_heights,const LandPatchHeader& header,unsigned patch_size,std::vector<float>& heights) {
  if ((patch_size!=16 && patch_size!=32) ||
      frequency_heights.size()!=size_t(patch_size)*patch_size ||
      !std::isfinite(header.dc_offset)) return false;
  std::vector<float> result;
  result.reserve(frequency_heights.size());
  for (float value:frequency_heights) {
    const double elevation=double(header.dc_offset)+double(value)*double(header.range)/double(patch_size);
    if (!std::isfinite(elevation) || std::abs(elevation)>1.0e8) return false;
    result.push_back(static_cast<float>(elevation));
  }
  heights=std::move(result);
  return true;
}
bool inverse_land_dct(const std::vector<int32_t>& coefficients,unsigned patch_size,std::vector<float>& samples) {
  if ((patch_size!=16 && patch_size!=32) ||
      coefficients.size()!=size_t(patch_size)*patch_size) return false;
  constexpr double pi=3.14159265358979323846;
  const double n=static_cast<double>(patch_size);
  std::vector<float> output(coefficients.size(),0.0f);
  for (unsigned y=0;y<patch_size;++y) for (unsigned x=0;x<patch_size;++x) {
    double sum=0;
    for (unsigned v=0;v<patch_size;++v) for (unsigned u=0;u<patch_size;++u) {
      const double au=u?1.0:0.7071067811865476;
      const double av=v?1.0:0.7071067811865476;
      sum+=au*av*double(coefficients[size_t(v)*patch_size+u])*
        std::cos((2*x+1)*u*pi/(2*n))*std::cos((2*y+1)*v*pi/(2*n));
    }
    const double height=2.0*sum/n;
    if (!std::isfinite(height) || std::abs(height)>1.0e8) return false;
    output[size_t(y)*patch_size+x]=static_cast<float>(height);
  }
  samples=std::move(output);
  return true;
}
bool decode_land_patch_coordinates(uint16_t patch_id,unsigned patch_size,uint32_t& x,uint32_t& y) {
  if (patch_size!=16 && patch_size!=32) return false;
  const uint32_t patch_x=patch_id & 0x1fu;
  const uint32_t patch_y=(patch_id>>5)&0x1fu;
  x=patch_x*patch_size;
  y=patch_y*patch_size;
  return true;
}
bool decode_land_patch_coefficients(LandBitReader& reader,unsigned patch_size,const LandPatchHeader& header,std::vector<int32_t>& coefficients) {
  const unsigned word_bits=unsigned(header.quant_wbits & 0x0f)+2u;
  return decode_land_coefficients(reader,patch_size,word_bits,coefficients);
}
bool decode_land_coefficients(LandBitReader& reader,unsigned patch_size,unsigned word_bits,std::vector<int32_t>& coefficients) {
  if ((patch_size!=16 && patch_size!=32) || word_bits<2 || word_bits>17) return false;
  const size_t count=size_t(patch_size)*patch_size;
  std::vector<int32_t> decoded(count,0);
  for (size_t i=0;i<count;) {
    uint32_t present=0;
    if (!reader.read_bits(1,present)) return false;
    if (!present) {
      uint32_t run=0;
      if (!reader.read_bits(1,run)) return false;
      if (!run) { ++i; continue; }
      uint32_t length=0;
      if (!reader.read_bits(5,length) || !length || length>count-i) return false;
      i+=length;
      continue;
    }
    uint32_t sign=0,magnitude=0;
    if (!reader.read_bits(1,sign) || !reader.read_bits(word_bits,magnitude)) return false;
    decoded[i++]=sign ? -static_cast<int32_t>(magnitude) : static_cast<int32_t>(magnitude);
  }
  coefficients=std::move(decoded);
  return true;
}
bool read_land_patch_end(LandBitReader& reader,bool& end) {
  uint32_t marker=0;
  if (!reader.read_bits(8,marker)) return false;
  end=(marker==97u);
  return true;
}
bool read_land_patch_header(LandBitReader& reader,LandPatchHeader& header) {
  uint32_t quant=0,dc=0,range=0,id=0;
  if (!reader.read_bits(8,quant) || !reader.read_bits(32,dc) ||
      !reader.read_bits(16,range) || !reader.read_bits(10,id)) return false;
  float offset=0;
  std::memcpy(&offset,&dc,sizeof(offset));
  if (!std::isfinite(offset)) return false;
  header={static_cast<uint8_t>(quant),offset,static_cast<uint16_t>(range),
          static_cast<uint16_t>(id)};
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
