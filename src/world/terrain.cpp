#include "terrain.h"
#include "region_protocol.h"
#include <cmath>
#include <algorithm>
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
bool TerrainAssembler::ready_to_render() const {
  return complete() && heights_.valid();
}
bool TerrainAssembler::apply_decoded_patch(const TerrainPatch& patch) {
  if (!validate_patch(patch, heights_)) return false;
  return apply_patch(patch.x, patch.y, patch.width, patch.height, patch.heights);
}
size_t TerrainAssembler::missing_samples() const {
  return received_.size() - received_count_;
}
bool TerrainAssembler::has_sample(uint32_t x, uint32_t y) const {
  return x < heights_.width && y < heights_.height &&
         received_[size_t(y) * heights_.width + x] != 0;
}
void TerrainAssembler::reset() {
  std::fill(received_.begin(), received_.end(), uint8_t{0});
  std::fill(heights_.elevations.begin(), heights_.elevations.end(), 0.0f);
  received_count_ = 0;
}
float TerrainAssembler::completion_ratio() const {
  return received_.empty() ? 0.0f :
    static_cast<float>(received_count_) / static_cast<float>(received_.size());
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
bool mesh_bounds(const TerrainMesh& mesh, float min_xyz[3], float max_xyz[3]) {
  if (!min_xyz || !max_xyz || mesh.vertices.empty()) return false;
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return false;
  min_xyz[0] = max_xyz[0] = mesh.vertices[0].x;
  min_xyz[1] = max_xyz[1] = mesh.vertices[0].y;
  min_xyz[2] = max_xyz[2] = mesh.vertices[0].z;
  for (const auto& v : mesh.vertices) {
    const float p[3] = {v.x,v.y,v.z};
    for (int i = 0; i < 3; ++i) {
      if (p[i] < min_xyz[i]) min_xyz[i] = p[i];
      if (p[i] > max_xyz[i]) max_xyz[i] = p[i];
    }
  }
  return true;
}
bool offset_mesh_indices(TerrainMesh& mesh, uint32_t offset) {
  for (auto i : mesh.indices) if (i > UINT32_MAX - offset) return false;
  for (auto& i : mesh.indices) i += offset;
  return true;
}
bool set_mesh_colors(TerrainMesh& mesh, const std::vector<float>& rgb) {
  if (mesh.vertices.size() > SIZE_MAX / 3 || rgb.size() != mesh.vertices.size() * 3) return false;
  for (float c : rgb) if (!std::isfinite(c) || c < 0.0f) return false;
  for (size_t i = 0; i < mesh.vertices.size(); ++i) {
    mesh.vertices[i].r = rgb[3*i];
    mesh.vertices[i].g = rgb[3*i+1];
    mesh.vertices[i].b = rgb[3*i+2];
  }
  return true;
}
TerrainMesh make_indexed_mesh(const std::vector<float>& positions,
                              const std::vector<uint32_t>& indices) {
  if (positions.empty() || positions.size() % 3 != 0 ||
      positions.size() / 3 > UINT32_MAX || indices.empty() ||
      indices.size() % 3 != 0) throw std::invalid_argument("Invalid indexed mesh input");
  TerrainMesh result;
  result.vertices.reserve(positions.size() / 3);
  for (size_t i = 0; i < positions.size(); i += 3) {
    if (!std::isfinite(positions[i]) || !std::isfinite(positions[i+1]) ||
        !std::isfinite(positions[i+2])) throw std::invalid_argument("Invalid mesh position");
    result.vertices.push_back({positions[i],positions[i+1],positions[i+2],.8f,.8f,.8f});
  }
  result.indices = indices;
  if (!validate_mesh(result)) throw std::invalid_argument("Invalid mesh indices");
  return result;
}
bool validate_mesh(const TerrainMesh& mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.indices.size() % 3 != 0 || mesh.vertices.size() > UINT32_MAX ||
      mesh.indices.size() > UINT32_MAX) return false;
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
        !std::isfinite(v.r) || !std::isfinite(v.g) || !std::isfinite(v.b)) return false;
  for (auto i : mesh.indices) if (i >= mesh.vertices.size()) return false;
  return true;
}
bool append_mesh(TerrainMesh& destination, const TerrainMesh& source) {
  if (source.vertices.empty() || source.indices.empty() ||
      source.indices.size() % 3 != 0 ||
      source.vertices.size() > UINT32_MAX ||
      destination.vertices.size() > UINT32_MAX - source.vertices.size() ||
      destination.indices.size() > UINT32_MAX - source.indices.size()) return false;
  for (const auto& v : source.vertices)
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
        !std::isfinite(v.r) || !std::isfinite(v.g) || !std::isfinite(v.b)) return false;
  for (auto i : source.indices) if (i >= source.vertices.size()) return false;
  if (!destination.vertices.empty() || !destination.indices.empty()) {
    if (!validate_mesh(destination)) return false;
  }
  const auto offset = static_cast<uint32_t>(destination.vertices.size());
  destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
  destination.indices.reserve(destination.indices.size() + source.indices.size());
  for (auto i : source.indices) destination.indices.push_back(offset + i);
  return true;
}
bool rotate_mesh_y(TerrainMesh& mesh, float radians) {
  if (!std::isfinite(radians)) return false;
  const float c = std::cos(radians), s = std::sin(radians);
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.x * c + v.z * s) ||
        !std::isfinite(-v.x * s + v.z * c)) return false;
  for (auto& v : mesh.vertices) {
    const float x = v.x * c + v.z * s;
    v.z = -v.x * s + v.z * c;
    v.x = x;
  }
  return true;
}
bool scale_mesh(TerrainMesh& mesh, float sx, float sy, float sz) {
  if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz) ||
      sx <= 0 || sy <= 0 || sz <= 0) return false;
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.x * sx) || !std::isfinite(v.y * sy) ||
        !std::isfinite(v.z * sz)) return false;
  for (auto& v : mesh.vertices) { v.x *= sx; v.y *= sy; v.z *= sz; }
  return true;
}
bool tint_mesh(TerrainMesh& mesh, float r, float g, float b) {
  if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) ||
      r < 0 || g < 0 || b < 0) return false;
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.r * r) || !std::isfinite(v.g * g) ||
        !std::isfinite(v.b * b)) return false;
  for (auto& v : mesh.vertices) { v.r *= r; v.g *= g; v.b *= b; }
  return true;
}
bool translate_mesh(TerrainMesh& mesh, float dx, float dy, float dz) {
  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return false;
  for (const auto& v : mesh.vertices)
    if (!std::isfinite(v.x + dx) || !std::isfinite(v.y + dy) ||
        !std::isfinite(v.z + dz)) return false;
  for (auto& v : mesh.vertices) { v.x += dx; v.y += dy; v.z += dz; }
  return true;
}
TerrainMesh make_ground_plane(float size, float elevation) {
  if (!std::isfinite(size) || !std::isfinite(elevation) || size <= 0.0f)
    throw std::invalid_argument("Invalid ground plane");
  const float half = size * 0.5f;
  TerrainMesh m;
  m.vertices = {{-half,elevation,-half,.22f,.34f,.25f},
                {half,elevation,-half,.22f,.34f,.25f},
                {-half,elevation,half,.22f,.34f,.25f},
                {half,elevation,half,.22f,.34f,.25f}};
  m.indices = {0,2,1,1,2,3};
  return m;
}
TerrainMesh make_box_mesh(float x, float y, float z, float sx, float sy, float sz) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz) ||
      sx <= 0 || sy <= 0 || sz <= 0) throw std::invalid_argument("Invalid box geometry");
  const float a = sx * 0.5f, b = sy * 0.5f, c = sz * 0.5f;
  TerrainMesh m;
  m.vertices = {
    {x-a,y-b,z-c,.65f,.66f,.72f}, {x+a,y-b,z-c,.65f,.66f,.72f},
    {x-a,y+b,z-c,.65f,.66f,.72f}, {x+a,y+b,z-c,.65f,.66f,.72f},
    {x-a,y-b,z+c,.65f,.66f,.72f}, {x+a,y-b,z+c,.65f,.66f,.72f},
    {x-a,y+b,z+c,.65f,.66f,.72f}, {x+a,y+b,z+c,.65f,.66f,.72f}
  };
  m.indices = {0,2,1,1,2,3,4,5,6,5,7,6,0,4,2,4,6,2,
               1,3,5,5,3,7,2,6,3,3,6,7,0,1,4,1,5,4};
  return m;
}
TerrainMesh make_terrain_mesh(const Heightfield& h) {
  if (!h.valid() || !std::isfinite(h.spacing) || h.spacing <= 0.0f)
    throw std::invalid_argument("Invalid heightfield");
  TerrainMesh mesh;
  if (h.elevations.size() > UINT32_MAX)
    throw std::invalid_argument("Terrain mesh exceeds 32-bit index capacity");
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
