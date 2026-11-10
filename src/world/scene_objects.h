#pragma once
#include "lludp.h"
#include "terrain.h"
#include "texture_entries.h"
#include "texture_atlas.h"
#include "texture_asset_queue.h"
#include "mesh_discovery.h"
#include "mesh_asset_queue.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kami::world {
// A visible local scene object from official LLUDP ObjectUpdate formats.
// Mesh assets, textures and child-link transformations are not resolved yet;
// only known root volume prims are rendered as neutral geometry proxies.
// Volume parameters use the wire quantization in the Second Life
// message_template.msg (no guessed geometry for unsupported cuts/sculpts).
struct PrimitiveShape {
  uint8_t path_curve=0, profile_curve=0;
  uint16_t path_begin=0, path_end=0;
  uint8_t path_scale_x=100, path_scale_y=100;
  int8_t path_shear_x=0,path_shear_y=0,path_twist=0,path_twist_begin=0;
  int8_t radius_offset=0,taper_x=0,taper_y=0;
  uint8_t revolutions=0;
  int8_t skew=0;
  uint16_t profile_begin=0,profile_end=0,profile_hollow=0;
  bool valid=false;
};
struct SceneObject {
  uint32_t local_id=0;
  uint32_t parent_id=0;
  uint8_t pcode=0;
  float x=0,y=0,z=0; // Second Life region coordinates (z is elevation).
  float sx=1,sy=1,sz=1;
  float qx=0,qy=0,qz=0,qw=1;
  PrimitiveShape shape{};
  TextureEntries textures{};
  MeshExtra mesh{};
};
struct SceneStats {
  uint64_t full_packets=0;
  uint64_t compressed_packets=0;
  uint64_t cached_packets=0;
  uint64_t kill_packets=0;
  uint64_t invalid_packets=0;
  uint64_t unsupported_objects=0;
  uint64_t updated_objects=0;
  uint64_t textured_objects=0;
  uint64_t invalid_texture_entries=0;
  uint64_t requested_cache_misses=0;
};
class SceneObjects {
public:
  // Decodes full, compressed, cached-probe and kill messages.
  // Returns true only when the message belongs to this subsystem and
  // is well-formed; ignored other message IDs do not count as errors.
  bool ingest(const kami::sl::Packet& packet);
  bool build_proxy_mesh(TerrainMesh& output,size_t limit=2000,
                        const TextureAtlas* atlas=nullptr,
                        const MeshAssetQueue* assets=nullptr) const;
  size_t visible_roots() const;
  size_t renderable_prims() const;
  size_t objects_with_face_colors() const;
  size_t objects_with_mesh_asset() const;
  std::vector<AssetId> referenced_mesh_ids() const;
  size_t unique_texture_references() const;
  std::vector<AssetId> referenced_texture_ids() const;
  size_t total_objects() const {return objects_.size();}
  uint64_t revision() const {return revision_;}
  const SceneStats& stats() const {return stats_;}
  const SceneObject* get(uint32_t local_id) const;
  std::vector<uint32_t> take_cache_misses(size_t limit=32);
  void requeue_cache_misses(const std::vector<uint32_t>& ids);
  void reset();
private:
  std::unordered_map<uint32_t,SceneObject> objects_;
  std::unordered_set<uint32_t> missing_;
  SceneStats stats_;
  uint64_t revision_=0;
};
}
