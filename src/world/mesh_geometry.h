#pragma once
#include "mesh_asset_header.h"
#include "terrain.h"
#include <cstdint>
#include <vector>
namespace kami::world {
// Decoded, local-space, per-material-face mesh; no avatar skinning or PBR.
// SL X/Y/Z local coordinates map to KAMI X/Z/up (Y). The owner object's
// rotation and scale are applied later on the DX12 scene thread.
struct DecodedMeshGeometry {
  std::vector<TerrainMesh> faces;
  size_t vertices=0,triangles=0;
};
bool decode_mesh_high_lod(const std::vector<uint8_t>& asset,
                          const MeshAssetHeader& header,
                          DecodedMeshGeometry& geometry);
}
