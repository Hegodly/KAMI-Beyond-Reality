#include "world/simulator_transport.h"
#include <cassert>
int main() {
  kami::sl::Packet packet;
  kami::sl::MessageId id;
  assert(!kami::sl::decode_message_id(packet, id));
  packet.payload = {0x05};
  assert(kami::sl::decode_message_id(packet, id) && id.value == 5 && id.header_bytes == 1);
  packet.payload = {0xFF,0x08};
  assert(kami::sl::decode_message_id(packet, id) && id.value == 0xFF08 && id.header_bytes == 2);
  packet.payload = {0xFF,0xFF,0x12,0x34};
  assert(kami::sl::decode_message_id(packet, id) && id.value == 0xFFFF1234 && id.header_bytes == 4);
  packet.payload = {0xFF,0xFF,0x12};
  assert(!kami::sl::decode_message_id(packet, id));
  kami::sl::SimulatorTransport transport;
  assert(!transport.open({"", 0}));
  assert(!transport.open({"not-an-ip", 13000}));
}
