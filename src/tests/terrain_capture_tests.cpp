#include "world/terrain_capture.h"
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include "world/lludp.h"
namespace {
std::vector<uint8_t> flat_patch_packet() {
  std::vector<uint8_t> group;
  size_t offset = 0;
  auto put=[&](uint32_t value,unsigned count) {
    for (unsigned bit=count;bit>0;--bit) {
      if ((offset%8)==0) group.push_back(0);
      if ((value>>(bit-1))&1u) group.back() |= uint8_t(1u<<(7-(offset%8)));
      ++offset;
    }
  };
  put(0,8);put(0,8);put(16,8);put(76,8);
  put(0,8);
  put(0,8);put(0,8);put(0xe0,8);put(0xc0,8);
  put(16,8);put(0,8);put(0,8);put(0,2);
  put(1,1);put(0,1);
  put(97,8);
  std::vector<uint8_t> packet{0,0,0,0,1,0,11,76,
    static_cast<uint8_t>(group.size()),static_cast<uint8_t>(group.size()>>8)};
  packet.insert(packet.end(),group.begin(),group.end());
  return packet;
}
std::string record(const std::vector<uint8_t>& packet) {
  std::string data("KAMIUDP1",8);
  uint32_t length=static_cast<uint32_t>(packet.size());
  for(int i=0;i<4;++i) data.push_back(static_cast<char>(length>>(i*8)));
  for (auto value:packet) data.push_back(static_cast<char>(value));
  return data;
}
}
int main() {
  kami::world::TerrainStream stream;
  kami::world::CaptureSummary summary;
  auto packet=flat_patch_packet();
  std::istringstream good(record(packet),std::ios::binary);
  assert(kami::world::replay_capture(good,stream,summary));
  assert(summary.records==1 && summary.terrain_records==1);
  assert(stream.terrain().received_samples()==256);
  const auto old=stream.revision();
  std::istringstream wrong("BAD!UDP1",std::ios::binary);
  assert(!kami::world::replay_capture(wrong,stream,summary));
  assert(stream.revision()==old);
  auto truncated=record(packet);
  truncated.pop_back();
  std::istringstream broken(truncated,std::ios::binary);
  assert(!kami::world::replay_capture(broken,stream,summary));
  assert(stream.revision()==old);
  std::istringstream empty("KAMIUDP1",std::ios::binary);
  assert(!kami::world::replay_capture(empty,stream,summary));
  assert(stream.revision()==old);
  // A live LayerData packet can be reserialized without recording the
  // source flags, packet ACKs or any other potentially sensitive messages.
  kami::sl::Packet land;
  assert(kami::sl::parse_packet(packet.data(),packet.size(),land));
  kami::world::TerrainOnlyCapture sanitized;
  assert(kami::world::record_land_packet(sanitized,land));
  assert(sanitized.datagrams.size()==1);
  assert(sanitized.total_bytes==packet.size());
  assert(sanitized.datagrams[0][0]==0);
  assert(sanitized.datagrams[0][4]==0);
  kami::sl::Packet unrelated;
  unrelated.payload={0x12,0x01,0x02};
  assert(!kami::world::record_land_packet(sanitized,unrelated));
  assert(sanitized.datagrams.size()==1);
  std::ostringstream exported(std::ios::out|std::ios::binary);
  assert(kami::world::write_land_capture(exported,sanitized));
  // The exported packet intentionally has a zeroed sequence, unlike the
  // original simulator datagram. Verify the sanitized serialization.
  assert(exported.str()==record(sanitized.datagrams[0]));
  kami::world::TerrainStream restored;
  kami::world::CaptureSummary restored_summary;
  std::istringstream imported(exported.str(),std::ios::in|std::ios::binary);
  assert(kami::world::replay_capture(imported,restored,restored_summary));
  assert(restored.terrain().received_samples()==256);
  // Writers must not accidentally persist unrelated simulator datagrams.
  sanitized.datagrams[0][6]=0x03; // UseCircuitCode, not LandLayer
  std::ostringstream refused(std::ios::binary);
  assert(!kami::world::write_land_capture(refused,sanitized));
  assert(refused.str().empty());
  sanitized.clear();
  assert(sanitized.datagrams.empty() && sanitized.total_bytes==0);
}
