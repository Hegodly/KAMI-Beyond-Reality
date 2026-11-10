#include "terrain.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace kami::world {
TerrainAssembler::TerrainAssembler(uint32_t width, uint32_t height, float spacing) {
  if (width < 2 || height < 2 || width > 4096 || height > 4096 ||
      !std::isfinite(spacing) || spacing <= 0.0f)
    throw std::invalid_argument("Invalid region dimensions");
  heights_.width = width;
  heights_.height = height;
  heights_.spacing = spacing;
  heights_.elevations.assign(size_t(width) * height, 0.0f);
  received_.assign(size_t(width) * height, 0);
}
bool TerrainAssembler::apply_patch(uint32_t x, uint32_t y, uint32_t width,
                                    uint32_t height, const std::vector<float>& samples) {
  if (!width || !height || x >= heights_.width || y >= heights_.height ||
      width > heights_.width - x || height > heights_.height - y ||
      samples.size() != size_t(width) * height) return false;
  for (float v : samples) if (!std::isfinite(v)) return false;
  for (uint32_t row = 0; row < height; ++row)
    for (uint32_t col = 0; col < width; ++col) {
      size_t dst = size_t(y + row) * heights_.width + x + col;
      if (!received_[dst]) { received_[dst] = 1; ++received_count_; }
      heights_.elevations[dst] = samples[size_t(row) * width + col];
    }
  return true;
}
TerrainMesh make_terrain_mesh(const Heightfield& h) {
  if (!h.valid() || !std::isfinite(h.spacing) || h.spacing <= 0.0f)
    throw std::invalid_argument("Invalid heightfield");
  TerrainMesh mesh;
  mesh.vertices.reserve(h.elevations.size());
  for (uint32_t z = 0; z < h.height; ++z)
    for (uint32_t x = 0; x < h.width; ++x) {
      float elevation = h.at(x, z);
      if (!std::isfinite(elevation)) throw std::invalid_argument("Nonfinite elevation");
      float green = 0.25f + 0.12f * (std::sin(elevation * 0.1f) + 1.0f);
      mesh.vertices.push_back({(float(x) - float(h.width - 1) * 0.5f) * h.spacing,
                               elevation, float(z) * h.spacing - 32.0f,
                               0.10f, green, 0.24f});
    }
  mesh.indices.reserve(size_t(h.width - 1) * (h.height - 1) * 6);
  for (uint32_t z = 0; z + 1 < h.height; ++z)
    for (uint32_t x = 0; x + 1 < h.width; ++x) {
      uint32_t i = z * h.width + x;
      mesh.indices.insert(mesh.indices.end(), {i, i + h.width, i + 1,
                                                i + 1, i + h.width, i + h.width + 1});
    }
  return mesh;
}
Heightfield make_demo_heightfield(uint32_t side) {
  TerrainAssembler assembler(side, side, 2.0f);
  std::vector<float> heights(size_t(side) * side);
  for (uint32_t z = 0; z < side; ++z)
    for (uint32_t x = 0; x < side; ++x) {
      float px = (float(x) - float(side - 1) * 0.5f) * 2.0f;
      float pz = float(z) * 2.0f - 32.0f;
      heights[size_t(z) * side + x] = 1.2f * std::sin(px * 0.23f) * std::cos(pz * 0.19f);
    }
  if (!assembler.apply_patch(0, 0, side, side, heights))
    throw std::runtime_error("Demo terrain assembly failed");
  return assembler.heightfield();
}
}
