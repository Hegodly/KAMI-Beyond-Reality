#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace kami::world {
struct MeshLodBlock {size_t offset=0,size=0;bool present=false;};
struct MeshAssetHeader {
  size_t header_bytes=0;
  MeshLodBlock high,medium,low,lowest;
};
// Second Life meshes begin with an uncompressed binary LLSD header.
// This bounded reader validates the high_lod range; it does not yet
// inflate gzip-compressed LOD geometry or interpret vertex buffers.
bool parse_mesh_asset_header(const uint8_t* bytes,size_t length,
                             MeshAssetHeader& result);
inline bool parse_mesh_asset_header(const std::vector<uint8_t>& data,
                                   MeshAssetHeader& result) {
  return parse_mesh_asset_header(data.data(),data.size(),result);
}
}
