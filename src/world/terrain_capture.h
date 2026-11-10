#pragma once
#include "terrain_stream.h"
#include <cstddef>
#include <istream>
#include <ostream>
#include <vector>
namespace kami::world {
// KAMIUDP1 is an offline diagnostic packet container, not an SL login.
// Each record is a little-endian uint32 length followed by one raw LLUDP packet.
struct CaptureSummary {
  size_t records = 0;
  size_t terrain_records = 0;
};
bool replay_capture(std::istream& input, TerrainStream& output, CaptureSummary& summary);

// Only the LayerData/land-heightfield message is stored. Do not record other
// simulator messages: they can include avatar, chat or session information.
// Capture stays in RAM until the user explicitly chooses a local file via F8.
struct TerrainOnlyCapture {
  size_t total_bytes = 0;
  std::vector<std::vector<uint8_t>> datagrams;
  void clear() { datagrams.clear(); total_bytes = 0; }
};
bool record_land_packet(TerrainOnlyCapture& capture,const kami::sl::Packet& packet);
bool write_land_capture(std::ostream& output,const TerrainOnlyCapture& capture);
}
