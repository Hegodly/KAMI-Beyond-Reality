#pragma once
#include "region.h"
#include <cstdint>
#include <vector>
namespace kami::world {
struct TerrainPatch;
struct TerrainMesh;
struct TerrainVertex { float x, y, z, r, g, b; };
struct TerrainMesh {
  std::vector<TerrainVertex> vertices;
  std::vector<uint32_t> indices;
};
// Simulator terrain patches are decoded elsewhere. This class applies decoded
// samples to a full region heightfield, handling out-of-order patch arrival.
class TerrainAssembler {
public:
  TerrainAssembler(uint32_t width, uint32_t height, float spacing = 1.0f);
  bool apply_patch(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                   const std::vector<float>& samples);
  const Heightfield& heightfield() const { return heights_; }
  bool complete() const { return received_count_ == received_.size(); }
  size_t received_samples() const { return received_count_; }
  float completion_ratio() const;
  void reset();
  bool has_sample(uint32_t x, uint32_t y) const;
  size_t missing_samples() const;
  bool apply_decoded_patch(const TerrainPatch& patch);
  bool ready_to_render() const;
  bool build_render_mesh(TerrainMesh& output) const;
  bool apply_patch_batch(const std::vector<TerrainPatch>& patches);
private:
  Heightfield heights_;
  std::vector<uint8_t> received_;
  size_t received_count_ = 0;
};
// Combines independently decoded region terrain and scene geometry into one draw mesh.
bool append_mesh(TerrainMesh& destination, const TerrainMesh& source);
TerrainMesh make_box_mesh(float x, float y, float z, float sx, float sy, float sz);
TerrainMesh make_ground_plane(float size, float elevation);
bool translate_mesh(TerrainMesh& mesh, float dx, float dy, float dz);
bool tint_mesh(TerrainMesh& mesh, float r, float g, float b);
bool scale_mesh(TerrainMesh& mesh, float sx, float sy, float sz);
bool rotate_mesh_y(TerrainMesh& mesh, float radians);
bool validate_mesh(const TerrainMesh& mesh);
// Scene geometry assembled from already decoded mesh asset positions and indices.
TerrainMesh make_indexed_mesh(const std::vector<float>& positions,
                              const std::vector<uint32_t>& indices);
bool set_mesh_colors(TerrainMesh& mesh, const std::vector<float>& rgb);
bool offset_mesh_indices(TerrainMesh& mesh, uint32_t offset);
bool mesh_bounds(const TerrainMesh& mesh, float min_xyz[3], float max_xyz[3]);
TerrainMesh make_terrain_mesh(const Heightfield& heights);
Heightfield make_demo_heightfield(uint32_t side = 129);
}
