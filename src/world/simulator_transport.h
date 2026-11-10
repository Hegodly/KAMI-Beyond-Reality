#pragma once
#include "lludp.h"
#include <cstdint>
#include <functional>
#include <string>
namespace kami::sl {
// Accepts only an explicitly supplied simulator endpoint from an authenticated
// login response. This transport does not perform authentication or circuit setup.
struct SimulatorEndpoint { std::string ipv4; uint16_t port = 0; };
struct MessageId { uint32_t value = 0; size_t header_bytes = 0; };
bool decode_message_id(const Packet& packet, MessageId& out);
class SimulatorTransport {
public:
  using Receiver = std::function<void(const Packet&, const MessageId&)>;
  SimulatorTransport();
  ~SimulatorTransport();
  SimulatorTransport(const SimulatorTransport&) = delete;
  SimulatorTransport& operator=(const SimulatorTransport&) = delete;
  bool open(const SimulatorEndpoint& endpoint);
  bool send(const uint8_t* bytes, size_t count);
  size_t poll(const Receiver& receiver, size_t max_packets = 32);
  void close();
private:
  uintptr_t socket_ = ~uintptr_t(0);
  uint32_t peer_ip_ = 0;
  uint16_t peer_port_ = 0;
  bool winsock_started_ = false;
};
}
