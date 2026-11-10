#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace kami::sl {
// LLUDP framing only. Not a login, encryption, simulator connection or LandLayer decoder.
struct Packet {
  uint8_t flags = 0;
  uint32_t sequence = 0;
  uint8_t extra_header_length = 0;
  std::vector<uint8_t> payload;
  std::vector<uint32_t> appended_acks;
};
bool parse_packet(const uint8_t* data, size_t size, Packet& output);
// LLUDP frequency-prefixed message identifier, independent of payload framing.
bool decode_message_id(const Packet& packet, uint32_t& id, size_t& body_offset);
bool is_message(const Packet& packet, uint32_t id);
bool extract_message_body(const Packet& packet, std::vector<uint8_t>& body);
constexpr uint32_t land_layer_message_id = 0x10000u + 11u;
bool is_land_layer(const Packet& packet);
}
