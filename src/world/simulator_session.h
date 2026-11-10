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
std::vector<uint8_t> make_region_handshake_reply(const AuthenticatedSession& session,uint32_t sequence);
std::vector<uint8_t> make_agent_update(const AuthenticatedSession& session,uint32_t sequence);
std::vector<uint8_t> make_request_multiple_objects(const AuthenticatedSession& session,
                                                  uint32_t sequence,
                                                  const std::vector<uint32_t>& local_ids);
class SimulatorSession {
public:
  using Receiver = SimulatorTransport::Receiver;
  bool connect(const AuthenticatedSession& session);
  size_t poll(const Receiver& receiver, size_t limit = 32);
  void tick();
  // Request full object updates for uncached local IDs advertised by the sim.
  // Best-effort only until reliable retransmission handling is implemented.
  bool request_objects(const std::vector<uint32_t>& ids);
  void disconnect();
private:
  SimulatorTransport transport_;
  uint32_t next_sequence_ = 3;
  uint64_t last_agent_update_ms_ = 0;
  AuthenticatedSession active_{};
  bool active_session_ = false;
};
}
