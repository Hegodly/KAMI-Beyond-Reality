#include "world/texture_decode_wic.h"
#include <cassert>
#include <vector>
using namespace kami::world;
int main(){
  // Minimal 2x2 24-bit BMP. Bottom row blue, white; top row red, green.
  // This validates WIC RGBA conversion independent of optional JP2 codecs.
  std::vector<uint8_t> bmp(70,0);
  auto le16=[&](size_t i,unsigned v){bmp[i]=uint8_t(v);bmp[i+1]=uint8_t(v>>8);};
  auto le32=[&](size_t i,unsigned v){
    for(int b=0;b<4;++b)bmp[i+b]=uint8_t(v>>(8*b));
  };
  bmp[0]='B';bmp[1]='M';le32(2,70);le32(10,54);le32(14,40);
  le32(18,2);le32(22,2);le16(26,1);le16(28,24);le32(34,16);
  // BMP rows are bottom-up, BGR 24-bit, 2 bytes padding.
  bmp[54]=255;bmp[55]=0;bmp[56]=0;
  bmp[57]=255;bmp[58]=255;bmp[59]=255;
  bmp[62]=0;bmp[63]=0;bmp[64]=255;
  bmp[65]=0;bmp[66]=255;bmp[67]=0;
  DecodedImage image;
  assert(decode_image_wic(bmp.data(),bmp.size(),image));
  assert(image.valid()&&image.width==2&&image.height==2);
  assert(image.rgba[0]==255&&image.rgba[1]==0&&image.rgba[2]==0);
  assert(image.rgba[4]==0&&image.rgba[5]==255&&image.rgba[6]==0);
  assert(image.rgba[8]==0&&image.rgba[9]==0&&image.rgba[10]==255);
  assert(image.rgba[12]==255&&image.rgba[13]==255&&image.rgba[14]==255);
  assert(!decode_image_wic(nullptr,bmp.size(),image));
  assert(!decode_image_wic(bmp.data(),0,image));
  std::vector<uint8_t> garbage(40,0);
  assert(!decode_image_wic(garbage.data(),garbage.size(),image));
  assert(!decode_jpeg2000_wic(bmp,image));
  assert(image.valid());
  return 0;
}
