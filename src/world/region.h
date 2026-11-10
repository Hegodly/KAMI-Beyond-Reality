#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace kami::world {
// A decoded terrain grid. This is an internal representation, not an SL wire decoder.
// The eventual region protocol layer must validate and decode simulator terrain patches.
struct Heightfield {
  uint32_t width = 0;
  uint32_t height = 0;
  float spacing = 1.0f;
  std::vector<float> elevations;
  float at(uint32_t x, uint32_t y) const {
    if (x >= width || y >= height || elevations.size() != size_t(width) * height)
      throw std::out_of_range("Invalid terrain heightfield");
    return elevations[size_t(y) * width + x];
  }
  bool valid() const {
    return width >= 2 && height >= 2 && width <= 4096 && height <= 4096 &&
           std::isfinite(spacing) && spacing > 0.0f &&
           elevations.size() == size_t(width) * height;
  }
};
struct Region {
  uint64_t handle = 0;
  uint32_t width_meters = 256;
  uint32_t height_meters = 256;
  Heightfield terrain;
};
// Region sizes and parcel boundaries must come from simulator metadata;
// a resident's parcel is not necessarily the size of the region.
}
