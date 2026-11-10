#pragma once
#include "terrain.h"
#include "lludp.h"
#include <cstddef>
#include <cstdint>
namespace kami::world {
// Receives simulator LLUDP datagrams after an independently authenticated
// session has established the circuit. Does not invent terrain or log in.
struct TerrainStreamStats {
  uint64_t packets = 0;
  uint64_t decoded = 0;
  uint64_t ignored = 0;
  uint64_t invalid = 0;
};
class TerrainStream {
public:
  TerrainStream(uint32_t width = 256, uint32_t height = 256);
  // Returns true only when a LandLayer packet updates terrain.
  bool ingest_datagram(const uint8_t* bytes, size_t count);
  bool ingest_packet(const kami::sl::Packet& packet);
  const TerrainAssembler& terrain() const { return terrain_; }
  bool build_mesh(TerrainMesh& mesh) const;
  const TerrainStreamStats& stats() const { return stats_; }
  uint64_t revision() const { return revision_; }
  void reset();
private:
  TerrainAssembler terrain_;
  TerrainStreamStats stats_;
  uint64_t revision_ = 0;
};
}
