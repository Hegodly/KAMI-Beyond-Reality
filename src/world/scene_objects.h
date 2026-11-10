#pragma once
#include "lludp.h"
#include "terrain.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kami::world {
// A visible local scene object from official LLUDP ObjectUpdate formats.
// Mesh assets, textures and child-link transformations are not resolved yet;
// only known root volume prims are rendered as neutral geometry proxies.
struct SceneObject {
  uint32_t local_id=0;
  uint32_t parent_id=0;
  uint8_t pcode=0;
  float x=0,y=0,z=0; // Second Life region coordinates (z is elevation).
  float sx=1,sy=1,sz=1;
  float qx=0,qy=0,qz=0,qw=1;
};
struct SceneStats {
  uint64_t full_packets=0;
  uint64_t compressed_packets=0;
  uint64_t cached_packets=0;
  uint64_t kill_packets=0;
  uint64_t invalid_packets=0;
  uint64_t unsupported_objects=0;
  uint64_t updated_objects=0;
  uint64_t requested_cache_misses=0;
};
class SceneObjects {
public:
  // Decodes full, compressed, cached-probe and kill messages.
  // Returns true only when the message belongs to this subsystem and
  // is well-formed; ignored other message IDs do not count as errors.
  bool ingest(const kami::sl::Packet& packet);
  bool build_proxy_mesh(TerrainMesh& output,size_t limit=2000) const;
  size_t visible_roots() const;
  size_t total_objects() const {return objects_.size();}
  uint64_t revision() const {return revision_;}
  const SceneStats& stats() const {return stats_;}
  const SceneObject* get(uint32_t local_id) const;
  std::vector<uint32_t> take_cache_misses(size_t limit=32);
  void reset();
private:
  std::unordered_map<uint32_t,SceneObject> objects_;
  std::unordered_set<uint32_t> missing_;
  SceneStats stats_;
  uint64_t revision_=0;
};
}
