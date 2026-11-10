#pragma once
#include "mesh_asset_header.h"
#include "terrain.h"
#include <cstdint>
#include <vector>
namespace kami::world {
// Decoded, local-space, per-material-face mesh; no avatar skinning or PBR.
// SL X/Y/Z local coordinates map to KAMI X/Z/up (Y). The owner object's
// rotation and scale are applied later on the DX12 scene thread.
enum class MeshLodQuality {Lowest=0,Low=1,Medium=2,High=3};
struct DecodedMeshGeometry {
  std::vector<TerrainMesh> faces;
  size_t vertices=0,triangles=0;
  MeshLodQuality quality=MeshLodQuality::Lowest;
};
// Highest available valid LOD; attempts lower detail on missing/broken blocks.
bool decode_mesh_best_lod(const std::vector<uint8_t>& asset,
                          const MeshAssetHeader& header,
                          DecodedMeshGeometry& geometry);
// Explicit high-only decoder for fixtures and callers needing this detail.
bool decode_mesh_high_lod(const std::vector<uint8_t>& asset,
                          const MeshAssetHeader& header,
                          DecodedMeshGeometry& geometry);
}
