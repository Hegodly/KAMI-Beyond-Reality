#include "lludp.h"
#include <utility>
namespace kami::sl {
namespace {
uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
}
bool extract_land_layer_data(const Packet& packet, std::vector<uint8_t>& compressed) {
  if (!is_land_layer(packet)) return false;
  std::vector<uint8_t> body;
  if (!extract_message_body(packet, body) || body.size() < 2) return false;
  const size_t length = size_t(body[0]) | (size_t(body[1]) << 8);
  if (!length || length != body.size() - 2) return false;
  compressed.assign(body.begin() + 2, body.end());
  return true;
}
bool is_land_layer(const Packet& packet) {
  return is_message(packet, land_layer_message_id);
}
bool extract_message_body(const Packet& packet, std::vector<uint8_t>& body) {
  uint32_t id = 0;
  size_t offset = 0;
  if (!decode_message_id(packet, id, offset)) return false;
  body.assign(packet.payload.begin() + offset, packet.payload.end());
  return true;
}
bool is_message(const Packet& packet, uint32_t expected) {
  uint32_t actual = 0;
  size_t offset = 0;
  return decode_message_id(packet, actual, offset) && actual == expected;
}
bool decode_message_id(const Packet& packet, uint32_t& id, size_t& body_offset) {
  const auto& p = packet.payload;
  if (p.empty()) return false;
  if (p[0] != 0xFF) { id = p[0]; body_offset = 1; return true; }
  if (p.size() < 2) return false;
  if (p[1] != 0xFF) { id = 0x100u + p[1]; body_offset = 2; return true; }
  if (p.size() < 4) return false;
  id = 0x10000u + (uint32_t(p[2]) << 8) + p[3];
  body_offset = 4;
  return true;
}
bool parse_packet(const uint8_t* data, size_t size, Packet& output) {
  // LLUDP fixed header: flags (1), sequence (4), extra header size (1).
  if (!data || size < 6 || size > 65535) return false;
  constexpr uint8_t zero_coded = 0x80, appended_ack = 0x10;
  Packet p;
  p.flags = data[0];
  p.sequence = be32(data + 1);
  p.extra_header_length = data[5];
  const size_t start = 6u + p.extra_header_length;
  if (start > size) return false;
  size_t end = size;
  if (p.flags & appended_ack) {
    const uint8_t count = data[size - 1];
    const size_t bytes = size_t(count) * 4 + 1;
    if (bytes > size - start) return false;
    end = size - bytes;
    for (size_t i = 0; i < count; ++i)
      p.appended_acks.push_back(be32(data + end + i * 4));
  }
  if (p.flags & zero_coded) {
    for (size_t i = start; i < end; ++i) {
      const uint8_t b = data[i];
      if (b != 0) { p.payload.push_back(b); continue; }
      if (++i >= end) return false;
      const uint8_t repeat = data[i];
      if (!repeat || p.payload.size() + repeat > 65535) return false;
      p.payload.insert(p.payload.end(), repeat, uint8_t(0));
    }
  } else p.payload.assign(data + start, data + end);
  output = std::move(p);
  return true;
}
}
