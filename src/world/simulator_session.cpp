#include "simulator_session.h"
#include <algorithm>
namespace kami::sl {
namespace {
void append_u32_le(std::vector<uint8_t>& out, uint32_t v) {
  for (int i=0;i<4;++i) out.push_back(static_cast<uint8_t>(v >> (8*i)));
}
bool nonzero_uuid(const uint8_t* bytes) {
  return std::any_of(bytes,bytes+16,[](uint8_t b){return b!=0;});
}
}
bool valid_session(const AuthenticatedSession& s) {
  return !s.endpoint.ipv4.empty() && s.endpoint.port != 0 &&
         s.circuit_code != 0 && nonzero_uuid(s.session_id) && nonzero_uuid(s.agent_id);
}
std::vector<uint8_t> make_use_circuit_code(const AuthenticatedSession& s, uint32_t sequence) {
  if (!valid_session(s)) return {};
  // LLUDP header (unreliable), high-frequency message UseCircuitCode = 3.
  std::vector<uint8_t> out = {
    0x00, static_cast<uint8_t>(sequence >> 24),
    static_cast<uint8_t>(sequence >> 16), static_cast<uint8_t>(sequence >> 8),
    static_cast<uint8_t>(sequence), 0x00, 0xFF, 0xFF, 0x00, 0x03
  };
  append_u32_le(out,s.circuit_code);
  out.insert(out.end(),s.session_id,s.session_id+16);
  out.insert(out.end(),s.agent_id,s.agent_id+16);
  return out;
}
std::vector<uint8_t> make_complete_agent_movement(const AuthenticatedSession& s, uint32_t sequence) {
  if (!valid_session(s)) return {};
  std::vector<uint8_t> out = {0x00,
    static_cast<uint8_t>(sequence >> 24),
    static_cast<uint8_t>(sequence >> 16),
    static_cast<uint8_t>(sequence >> 8),
    static_cast<uint8_t>(sequence),0x00,0xFF,0xFF,0x00,0xF9};
  out.insert(out.end(),s.agent_id,s.agent_id+16);
  out.insert(out.end(),s.session_id,s.session_id+16);
  append_u32_le(out,s.circuit_code);
  return out;
}
bool SimulatorSession::connect(const AuthenticatedSession& s) {
  disconnect();
  if (!valid_session(s) || !transport_.open(s.endpoint)) return false;
  const auto handshake = make_use_circuit_code(s,1);
  if (!transport_.send(handshake.data(),handshake.size())) {
    disconnect(); return false;
  }
  const auto movement=make_complete_agent_movement(s,2);
  if (movement.empty() || !transport_.send(movement.data(),movement.size())) {
    disconnect(); return false;
  }
  return true;
}
size_t SimulatorSession::poll(const Receiver& receiver, size_t limit) {
  return transport_.poll(receiver,limit);
}
void SimulatorSession::disconnect() { transport_.close(); }
}
