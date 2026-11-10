#pragma once
#include "texture_atlas.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace kami::world {
// Windows Imaging Component decoder bridge. Windows installations without
// a registered JPEG2000 codec fail cleanly; do not substitute fake textures.
bool decode_image_wic(const uint8_t* bytes,size_t count,DecodedImage& output);
bool decode_jpeg2000_wic(const std::vector<uint8_t>& encoded,DecodedImage& output);
}
