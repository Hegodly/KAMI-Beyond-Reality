#pragma once
#include "region.h"
#include <cstdint>
#include <string>
#include <vector>
namespace kami::world {
// Decoded simulator event representation. Network transport and LLUDP
// LandLayer decompression are separate components and are not yet implemented.
struct RegionHandshake {
  uint64_t handle = 0;
  uint32_t width_meters = 256;
  uint32_t height_meters = 256;
  std::string name;
};
struct TerrainPatch {
  uint32_t x = 0, y = 0;
  uint32_t width = 0, height = 0;
  std::vector<float> heights;
};
bool validate_handshake(const RegionHandshake& info);
bool validate_patch(const TerrainPatch& patch, const Heightfield& destination);
}
