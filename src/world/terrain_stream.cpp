#include "terrain_stream.h"
namespace kami::world {
TerrainStream::TerrainStream(uint32_t width, uint32_t height)
  : terrain_(width, height) {}
bool TerrainStream::ingest_datagram(const uint8_t* bytes, size_t count) {
  ++stats_.packets;
  kami::sl::Packet packet;
  if (!kami::sl::parse_packet(bytes, count, packet)) {
    ++stats_.invalid;
    return false;
  }
  return ingest_packet(packet);
}
bool TerrainStream::ingest_packet(const kami::sl::Packet& packet) {
  if (!kami::sl::is_land_layer(packet)) {
    ++stats_.ignored;
    return false;
  }
  if (!terrain_.apply_land_packet(packet)) {
    ++stats_.invalid;
    return false;
  }
  ++stats_.decoded;
  ++revision_;
  return true;
}
bool TerrainStream::build_mesh(TerrainMesh& mesh) const {
  return terrain_.build_partial_render_mesh(mesh);
}
void TerrainStream::reset() {
  terrain_.reset();
  stats_ = {};
  revision_ = 0;
}
}
