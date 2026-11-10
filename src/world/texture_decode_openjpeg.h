#pragma once
#include "texture_atlas.h"
#include <cstdint>
#include <vector>
namespace kami::world {
// OpenJPEG decoder for simulator JPEG2000 codestreams and JP2 wrappers.
// Fully decoded pixels are bounded; unsupported YCbCr/subsampled images
// deliberately fail instead of displaying incorrect color data.
bool decode_jpeg2000_openjpeg(const std::vector<uint8_t>& encoded,DecodedImage& output);
}
