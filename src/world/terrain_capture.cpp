#include "terrain_capture.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
namespace kami::world {
bool replay_capture(std::istream& input, TerrainStream& output, CaptureSummary& summary) {
  constexpr char magic[8] = {'K','A','M','I','U','D','P','1'};
  char header[8]{};
  if (!input.read(header, sizeof(header)) ||
      std::memcmp(header, magic, sizeof(header)) != 0) return false;
  // A bounded, transactional replay cannot damage a currently drawn region.
  TerrainStream candidate(output.terrain().heightfield().width,
                          output.terrain().heightfield().height);
  CaptureSummary candidate_summary;
  size_t total_bytes = 0;
  for (;;) {
    const int next = input.peek();
    if (next == std::char_traits<char>::eof()) {
      if (!input.eof()) return false;
      break;
    }
    std::array<uint8_t,4> length{};
    if (!input.read(reinterpret_cast<char*>(length.data()), length.size())) return false;
    const size_t size = size_t(length[0]) | (size_t(length[1])<<8) |
                        (size_t(length[2])<<16) | (size_t(length[3])<<24);
    if (size < 6 || size > 65535 || candidate_summary.records >= 2048 ||
        size > 16u*1024u*1024u - total_bytes) return false;
    total_bytes += size;
    std::vector<uint8_t> datagram(size);
    if (!input.read(reinterpret_cast<char*>(datagram.data()), size)) return false;
    ++candidate_summary.records;
    if (candidate.ingest_datagram(datagram.data(), datagram.size()))
      ++candidate_summary.terrain_records;
  }
  if (!candidate_summary.terrain_records || !candidate.terrain().any_renderable_cells())
    return false;
  output = std::move(candidate);
  summary = candidate_summary;
  return true;
}
}
