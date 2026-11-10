#include "region_protocol.h"
#include <cmath>
namespace kami::world {
bool validate_handshake(const RegionHandshake& info) {
  // Region dimensions may vary; do not confuse a parcel with a region.
  return info.handle != 0 &&
         info.width_meters >= 16 && info.height_meters >= 16 &&
         info.width_meters <= 4096 && info.height_meters <= 4096 &&
         info.name.size() <= 255;
}
bool validate_patch(const TerrainPatch& p, const Heightfield& h) {
  if (!h.valid() || p.width == 0 || p.height == 0 ||
      p.x >= h.width || p.y >= h.height ||
      p.width > h.width - p.x || p.height > h.height - p.y ||
      p.heights.size() != size_t(p.width) * p.height) return false;
  for (float value : p.heights)
    if (!std::isfinite(value)) return false;
  return true;
}
}
