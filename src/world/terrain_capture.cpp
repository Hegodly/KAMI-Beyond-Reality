#include "terrain_capture.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
#include <ostream>
namespace kami::world {
bool record_land_packet(TerrainOnlyCapture& capture,const kami::sl::Packet& packet) {
  // Accept only a well-framed LayerData message containing land heightfield
  // bytes. All other packet types are refused, including authentication,
  // avatar identifiers, chat, and inventory.
  std::vector<uint8_t> compressed;
  if (!kami::sl::extract_land_layer_data(packet,compressed)) return false;
  const size_t length=6u+packet.payload.size();
  if (length>65535 || capture.datagrams.size()>=2048 ||
      capture.total_bytes>16u*1024u*1024u ||
      length>16u*1024u*1024u-capture.total_bytes) return false;
  std::vector<uint8_t> datagram;
  datagram.reserve(length);
  // Replace the original flags/sequence/ACKs with a sanitized, uncompressed
  // LLUDP header. This retains only message ID + LayerData payload.
  datagram.insert(datagram.end(),{0,0,0,0,0,0});
  datagram.insert(datagram.end(),packet.payload.begin(),packet.payload.end());
  capture.total_bytes+=datagram.size();
  capture.datagrams.push_back(std::move(datagram));
  return true;
}
bool write_land_capture(std::ostream& output,const TerrainOnlyCapture& capture) {
  if (!output || capture.datagrams.empty() || capture.datagrams.size()>2048 ||
      capture.total_bytes>16u*1024u*1024u) return false;
  // Prevalidate every record before outputting any bytes.
  size_t checked_bytes=0;
  for (const auto& datagram:capture.datagrams) {
    kami::sl::Packet p;
    std::vector<uint8_t> layer;
    if (datagram.size()<6 || datagram.size()>65535 ||
        !kami::sl::parse_packet(datagram.data(),datagram.size(),p) ||
        !kami::sl::extract_land_layer_data(p,layer) ||
        datagram.size()>16u*1024u*1024u-checked_bytes) return false;
    checked_bytes+=datagram.size();
  }
  constexpr char magic[8]={'K','A','M','I','U','D','P','1'};
  output.write(magic,sizeof(magic));
  for (const auto& datagram:capture.datagrams) {
    const uint32_t size=static_cast<uint32_t>(datagram.size());
    const char le[4]={static_cast<char>(size),static_cast<char>(size>>8),
                      static_cast<char>(size>>16),static_cast<char>(size>>24)};
    output.write(le,4);
    output.write(reinterpret_cast<const char*>(datagram.data()),
                 static_cast<std::streamsize>(datagram.size()));
  }
  return bool(output.flush());
}
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
