#include "land_layer.h"
#include "region_protocol.h"
#include <cstring>
#include <cmath>
#include <utility>
#include <algorithm>
namespace kami::sl {
LandBitReader::LandBitReader(const uint8_t* bytes,size_t size):bytes_(bytes),size_(size){}
size_t LandBitReader::bits_remaining() const {
  return bytes_ && size_ <= SIZE_MAX / 8 && bit_position_ <= size_*8 ? size_*8-bit_position_ : 0;
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
bool decode_land_group_to_world(const uint8_t* bytes,size_t size,uint32_t width,uint32_t height,std::vector<kami::world::TerrainPatch>& patches) {
  std::vector<DecodedLandPatch> decoded;
  if (!decode_land_group_for_region(bytes,size,width,height,decoded)) return false;
  std::vector<kami::world::TerrainPatch> converted;
  for (const auto& patch:decoded) {
    kami::world::TerrainPatch item;
    if (!land_patch_to_world(patch,item)) return false;
    converted.push_back(std::move(item));
  }
  patches=std::move(converted);
  return true;
}
bool land_patch_to_world(const DecodedLandPatch& source,kami::world::TerrainPatch& target) {
  if ((source.size!=16 && source.size!=32) ||
      source.heights.size()!=size_t(source.size)*source.size) return false;
  for (float value:source.heights) if (!std::isfinite(value)) return false;
  kami::world::TerrainPatch converted;
  converted.x=source.x;converted.y=source.y;
  converted.width=source.size;converted.height=source.size;
  converted.heights=source.heights;
  target=std::move(converted);
  return true;
}
bool decode_land_group_for_region(const uint8_t* bytes,size_t size,uint32_t width,uint32_t height,std::vector<DecodedLandPatch>& patches) {
  std::vector<DecodedLandPatch> decoded;
  if (!decode_land_group(bytes,size,decoded) || !land_group_within_region(decoded,width,height)) return false;
  patches=std::move(decoded);
  return true;
}
bool land_group_within_region(const std::vector<DecodedLandPatch>& patches,uint32_t region_width,uint32_t region_height) {
  for (const auto& patch:patches)
    if (!land_patch_within_region(patch,region_width,region_height)) return false;
  return true;
}
bool land_patch_within_region(const DecodedLandPatch& patch,uint32_t region_width,uint32_t region_height) {
  return patch.size>=2 && patch.x<region_width && patch.y<region_height &&
    patch.size<=region_width-patch.x && patch.size<=region_height-patch.y &&
    patch.heights.size()==size_t(patch.size)*patch.size;
}
bool decode_land_group(const uint8_t* bytes,size_t size,std::vector<DecodedLandPatch>& patches) {
  if (!bytes || !size || size>65535) return false;
  LandBitReader reader(bytes,size);
  LandGroupHeader group;
  if (!read_land_group_header(reader,group)) return false;
  std::vector<DecodedLandPatch> decoded;
  while (reader.bits_remaining()>=8) {
    uint32_t marker=0;
    if (!reader.peek_bits(8,marker)) return false;
    if (marker==97) {
      if (!reader.skip_bits(8)) return false;
      patches=std::move(decoded);
      return true;
    }
    if (decoded.size()>=1024) return false;
    DecodedLandPatch patch;
    if (!decode_land_patch(reader,group.patch_size,patch)) return false;
    decoded.push_back(std::move(patch));
  }
  return false;
}
bool decode_land_patch(LandBitReader& reader,unsigned patch_size,DecodedLandPatch& patch) {
  LandPatchHeader header;
  if (!read_land_patch_header(reader,header)) return false;
  std::vector<int32_t> coefficients;
  if (!decode_land_patch_coefficients(reader,patch_size,header,coefficients)) return false;
  std::vector<float> heights;
  if (!reconstruct_land_patch(coefficients,header,patch_size,heights)) return false;
  uint32_t x=0,y=0;
  if (!decode_land_patch_coordinates(header.patch_id,patch_size,x,y)) return false;
  patch={x,y,patch_size,std::move(heights)};
  return true;
}
bool reconstruct_land_patch(const std::vector<int32_t>& coefficients,const LandPatchHeader& header,unsigned patch_size,std::vector<float>& heights) {
  if ((patch_size!=16 && patch_size!=32) ||
      coefficients.size()!=size_t(patch_size)*patch_size) return false;
  // SL transmits DCT coefficients in zigzag order. Dequantize into
  // frequency-space row-major order before the inverse transform.
  std::vector<int32_t> frequency(coefficients.size(),0);
  size_t index=0;
  const int n=static_cast<int>(patch_size);
  for (int diagonal=0;diagonal<=2*(n-1);++diagonal) {
    const int low=std::max(0,diagonal-(n-1));
    const int high=std::min(n-1,diagonal);
    for (int step=low;step<=high;++step) {
      const int x=(diagonal&1)?high-(step-low):step;
      const int y=diagonal-x;
      frequency[size_t(y)*patch_size+x]=
        coefficients[index++]*(1+2*(x+y));
    }
  }
  std::vector<float> transformed;
  return inverse_land_dct(frequency,patch_size,transformed) &&
         scale_land_heights(transformed,header,patch_size,heights);
}
bool scale_land_heights(const std::vector<float>& frequency_heights,const LandPatchHeader& header,unsigned patch_size,std::vector<float>& heights) {
  if ((patch_size!=16 && patch_size!=32) ||
      frequency_heights.size()!=size_t(patch_size)*patch_size ||
      !std::isfinite(header.dc_offset)) return false;
  // See the Second Life patch_idct.cpp: prequant comes from the upper
  // nibble; the inverse-DCT output is scaled by range/2^prequant and
  // offset by the patch minimum plus half the range.
  const unsigned prequant=(header.quant_wbits>>4)+2u;
  const double mult=double(header.range)/double(uint32_t(1)<<prequant);
  const double add=double(header.dc_offset)+double(header.range)*0.5;
  std::vector<float> result;
  result.reserve(frequency_heights.size());
  for (float value:frequency_heights) {
    const double elevation=add+double(value)*mult;
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
  const unsigned n=patch_size;
  const double norm=2.0/double(n);
  const double halfroot=0.7071067811865475244;
  std::vector<double> cosines(size_t(n)*n);
  for (unsigned u=0;u<n;++u)
    for (unsigned x=0;x<n;++x)
      cosines[size_t(u)*n+x]=std::cos((2*x+1)*u*pi/(2.0*n));
  // Separable 2D inverse DCT (O(n^3) instead of O(n^4)).
  // The first pass matches the official viewer's column/line scaling.
  std::vector<double> intermediate(size_t(n)*n,0);
  for (unsigned v=0;v<n;++v)
    for (unsigned x=0;x<n;++x) {
      double sum=0;
      for (unsigned u=0;u<n;++u)
        sum+=(u==0?halfroot:1.0)*
          double(coefficients[size_t(v)*n+u])*cosines[size_t(u)*n+x];
      intermediate[size_t(v)*n+x]=sum*norm;
    }
  std::vector<float> result(coefficients.size(),0);
  for (unsigned y=0;y<n;++y)
    for (unsigned x=0;x<n;++x) {
      double sum=0;
      for (unsigned v=0;v<n;++v)
        sum+=(v==0?halfroot:1.0)*intermediate[size_t(v)*n+x]*cosines[size_t(v)*n+y];
      if (!std::isfinite(sum) || std::abs(sum)>1.0e8) return false;
      result[size_t(y)*n+x]=static_cast<float>(sum);
    }
  samples=std::move(result);
  return true;
}
bool decode_land_patch_coordinates(uint16_t patch_id,unsigned patch_size,uint32_t& x,uint32_t& y) {
  if (patch_size!=16 && patch_size!=32) return false;
  const uint32_t patch_x=(patch_id>>5)&0x1fu;
  const uint32_t patch_y=patch_id & 0x1fu;
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
  // Official LL patch_code.cpp: 0=single zero, 10=end of coefficients,
  // 110=positive value, 111=negative value. No zero-run length follows.
  for (size_t i=0;i<count;++i) {
    uint32_t flag=0;
    if (!reader.read_bits(1,flag)) return false;
    if (!flag) continue;
    uint32_t value_bit=0;
    if (!reader.read_bits(1,value_bit)) return false;
    if (!value_bit) break;
    uint32_t sign=0,magnitude=0;
    if (!reader.read_bits(1,sign)) return false;
    // LLBitPack::bitUnpack writes each group of up to eight bits to
    // successive bytes of a native little-endian integer. A 13-bit
    // coefficient is transmitted as eight low bits then five high bits.
    // Reading all 13 bits as a single big-endian word corrupts real land
    // coefficients and produces kilometer-scale-looking terrain spikes.
    for (unsigned shift=0;shift<word_bits;shift+=8) {
      const unsigned count=std::min(8u,word_bits-shift);
      uint32_t part=0;
      if (!reader.read_bits(count,part)) return false;
      magnitude|=part<<shift;
    }
    decoded[i]=sign?-static_cast<int32_t>(magnitude):
                     static_cast<int32_t>(magnitude);
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
  uint32_t quant=0;
  if (!reader.read_bits(8,quant) || quant==97u) return false;
  // LLBitPack serializes native little-endian float/word bytes, with
  // each individual byte sent most-significant bit first.
  uint32_t dc=0,range=0,patch_id=0;
  for (unsigned i=0;i<4;++i) {
    uint32_t b=0;
    if (!reader.read_bits(8,b)) return false;
    dc|=b<<(i*8);
  }
  for (unsigned i=0;i<2;++i) {
    uint32_t b=0;
    if (!reader.read_bits(8,b)) return false;
    range|=b<<(i*8);
  }
  uint32_t low=0,high=0;
  if (!reader.read_bits(8,low) || !reader.read_bits(2,high)) return false;
  patch_id=low|(high<<8);
  float offset=0;
  std::memcpy(&offset,&dc,sizeof(offset));
  if (!std::isfinite(offset)) return false;
  header={static_cast<uint8_t>(quant),offset,static_cast<uint16_t>(range),
          static_cast<uint16_t>(patch_id)};
  return true;
}
bool read_land_group_header(LandBitReader& reader,LandGroupHeader& header) {
  uint32_t stride=0,size=0,type=0;
  uint32_t low=0,high=0;
  if (!reader.read_bits(8,low) || !reader.read_bits(8,high) ||
      !reader.read_bits(8,size) || !reader.read_bits(8,type)) return false;
  stride=low|(high<<8);
  if ((size!=16 && size!=32) || type!=0x4c) return false;
  header={static_cast<uint16_t>(stride),static_cast<uint8_t>(size),static_cast<uint8_t>(type)};
  return true;
}
}
