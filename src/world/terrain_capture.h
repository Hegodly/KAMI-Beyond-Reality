#pragma once
#include "terrain_stream.h"
#include <cstddef>
#include <istream>
namespace kami::world {
// KAMIUDP1 is an offline diagnostic packet container, not an SL login.
// Each record is a little-endian uint32 length followed by one raw LLUDP packet.
struct CaptureSummary {
  size_t records = 0;
  size_t terrain_records = 0;
};
bool replay_capture(std::istream& input, TerrainStream& output, CaptureSummary& summary);
}
