#include "world/texture_decode_openjpeg.h"
#include "world/texture_asset_queue.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace {
std::vector<uint8_t> decode64(const std::string& text) {
  static const std::string alphabet=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> out;
  uint32_t value=0;
  int bits=-8;
  for(char c:text) {
    if(c=='=')break;
    auto pos=alphabet.find(c);
    assert(pos!=std::string::npos);
    value=(value<<6)|static_cast<uint32_t>(pos);
    bits+=6;
    if(bits>=0) {
      out.push_back(static_cast<uint8_t>((value>>bits)&255u));
      bits-=8;
    }
  }
  return out;
}
}
int main() {
  using namespace kami::world;
  // 32x32 lossless RGB J2K codestream; each pixel is (8*x,8*y,128).
  // Generated from a synthetic image, not extracted from a resident's land.
  const auto fixture=decode64("/0//UQAvAAAAAAAgAAAAIAAAAAAAAAAAAAAAIAAAACAAAAAAAAAAAAADBwEBBwEBBwEB/1IADAAAAAEABQQEAAH/XAATQEBISFBISFBISFBISFBISFD/ZAAlAAFDcmVhdGVkIGJ5IE9wZW5KUEVHIHZlcnNpb24gMi41LjT/kAAKAAAAAABwAAH/k8fUBAgvx9QECC+Az8AIBF+n4AQEX4DH2gYADQIFo+0DAAs9hIDD6gUAIhoIXY+h9QKAA3/dUZ+AwfOHADahma5jo1+g+cOAPaY0WxDcV4DA+QKAX6fHiLegfIFAnawpQQeA/9k=");
  assert(looks_like_jpeg2000(fixture));
  DecodedImage image;
  assert(decode_jpeg2000_openjpeg(fixture,image));
  assert(image.width==32&&image.height==32&&image.valid());
  const auto pixel=[&](size_t x,size_t y,size_t channel) {
    return image.rgba[(y*image.width+x)*4+channel];
  };
  assert(std::abs(int(pixel(0,0,0)))<=1);
  assert(std::abs(int(pixel(31,0,0))-248)<=1);
  assert(std::abs(int(pixel(0,31,1))-248)<=1);
  assert(std::abs(int(pixel(12,14,2))-128)<=1);
  assert(pixel(0,0,3)==255);
  const auto original=image.rgba;
  auto bad=fixture;
  bad.resize(30);
  assert(!decode_jpeg2000_openjpeg(bad,image));
  assert(image.rgba==original); // no partial frame on failed decode
  std::vector<uint8_t> junk(40,0);
  assert(!decode_jpeg2000_openjpeg(junk,image));
}
