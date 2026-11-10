#pragma once
#include "simulator_transport.h"
#include <cstdint>
#include <functional>
#include <vector>
namespace kami::sl {
// Caller must supply authenticated session information obtained through a
// trusted Second Life login flow. Never infer circuit secrets from a region IP.
struct AuthenticatedSession {
  SimulatorEndpoint endpoint;
  uint32_t circuit_code = 0;
  uint8_t session_id[16]{};
  uint8_t agent_id[16]{};
};
bool valid_session(const AuthenticatedSession& session);
std::vector<uint8_t> make_use_circuit_code(const AuthenticatedSession& session, uint32_t sequence);
std::vector<uint8_t> make_complete_agent_movement(const AuthenticatedSession& session, uint32_t sequence);
std::vector<uint8_t> make_packet_ack(uint32_t sequence, uint32_t received_sequence);
class SimulatorSession {
public:
  using Receiver = SimulatorTransport::Receiver;
  bool connect(const AuthenticatedSession& session);
  size_t poll(const Receiver& receiver, size_t limit = 32);
  void disconnect();
private:
  SimulatorTransport transport_;
};
}
