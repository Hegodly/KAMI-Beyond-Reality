#include "simulator_session.h"
#include <algorithm>
#include <windows.h>
#include <cstring>
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
std::vector<uint8_t> make_packet_ack(uint32_t sequence, uint32_t received_sequence) {
  // PacketAck: fixed frequency 0xFFFFFFFB, one little-endian packet sequence.
  return {0x00,
    static_cast<uint8_t>(sequence >> 24), static_cast<uint8_t>(sequence >> 16),
    static_cast<uint8_t>(sequence >> 8), static_cast<uint8_t>(sequence), 0x00,
    0xFF,0xFF,0xFF,0xFB,0x01,
    static_cast<uint8_t>(received_sequence),
    static_cast<uint8_t>(received_sequence >> 8),
    static_cast<uint8_t>(received_sequence >> 16),
    static_cast<uint8_t>(received_sequence >> 24)};
}

std::vector<uint8_t> make_region_handshake_reply(const AuthenticatedSession& s,uint32_t sequence) {
  if (!valid_session(s)) return {};
  std::vector<uint8_t> out={0,
    static_cast<uint8_t>(sequence>>24),static_cast<uint8_t>(sequence>>16),
    static_cast<uint8_t>(sequence>>8),static_cast<uint8_t>(sequence),
    0,0xFF,0xFF,0x00,0x95};
  out.insert(out.end(),s.agent_id,s.agent_id+16);
  out.insert(out.end(),s.session_id,s.session_id+16);
  append_u32_le(out,0);
  return out;
}
std::vector<uint8_t> make_agent_update(const AuthenticatedSession& s,uint32_t sequence) {
  if (!valid_session(s)) return {};
  std::vector<uint8_t> raw={0x04};
  raw.insert(raw.end(),s.agent_id,s.agent_id+16);
  raw.insert(raw.end(),s.session_id,s.session_id+16);
  auto f32=[&](float value) {
    uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));
    append_u32_le(raw,bits);
  };
  // LLQuaternion transmits xyz only; missing w is reconstructed by the sim.
  for (int q=0;q<6;++q) f32(0);
  raw.push_back(0); // AgentState
  f32(128);f32(128);f32(40); // CameraCenter (region local)
  f32(1);f32(0);f32(0); // CameraAt
  f32(0);f32(1);f32(0); // CameraLeft
  f32(0);f32(0);f32(1); // CameraUp
  f32(192); // draw distance in meters
  append_u32_le(raw,0); // ControlFlags: no autonomous movement
  raw.push_back(0); // AgentUpdate flags
  std::vector<uint8_t> out={0x80,
    static_cast<uint8_t>(sequence>>24),static_cast<uint8_t>(sequence>>16),
    static_cast<uint8_t>(sequence>>8),static_cast<uint8_t>(sequence),0};
  for (size_t i=0;i<raw.size();) {
    if (raw[i]) {out.push_back(raw[i++]);continue;}
    uint8_t run=0;
    while (i<raw.size() && !raw[i] && run<255) {++i;++run;}
    out.push_back(0);out.push_back(run);
  }
  return out;
}
std::vector<uint8_t> make_request_multiple_objects(const AuthenticatedSession& s,
                                                  uint32_t sequence,
                                                  const std::vector<uint32_t>& ids) {
  // Official message_template.msg: RequestMultipleObjects Medium 3
  // AgentData (AgentID, SessionID), ObjectData variable count, each
  // (CacheMissType=0 for full object, ID U32).
  if(!valid_session(s)||ids.empty()||ids.size()>64)return {};
  std::vector<uint8_t> out={0x40,
    static_cast<uint8_t>(sequence>>24),static_cast<uint8_t>(sequence>>16),
    static_cast<uint8_t>(sequence>>8),static_cast<uint8_t>(sequence),
    0,0xFF,0x03};
  out.insert(out.end(),s.agent_id,s.agent_id+16);
  out.insert(out.end(),s.session_id,s.session_id+16);
  out.push_back(static_cast<uint8_t>(ids.size()));
  for(uint32_t id:ids) {
    if(!id)return {};
    out.push_back(0); // CacheMissType = 0 -> full update
    append_u32_le(out,id);
  }
  return out;
}
bool SimulatorSession::connect(const AuthenticatedSession& s) {
  disconnect();
  if (!valid_session(s) || !transport_.open(s.endpoint)) return false;
  next_sequence_ = 3;
  const auto handshake = make_use_circuit_code(s,1);
  if (!transport_.send(handshake.data(),handshake.size())) {
    disconnect(); return false;
  }
  const auto movement=make_complete_agent_movement(s,2);
  if (movement.empty() || !transport_.send(movement.data(),movement.size())) {
    disconnect(); return false;
  }
  active_=s;
  active_session_=true;
  last_agent_update_ms_=0;
  return true;
}
size_t SimulatorSession::poll(const Receiver& receiver, size_t limit) {
  if (!receiver) return 0;
  return transport_.poll([&](const Packet& packet,const MessageId& id) {
    if ((packet.flags & 0x40u)!=0) {
      auto ack=make_packet_ack(next_sequence_++,packet.sequence);
      transport_.send(ack.data(),ack.size());
    }
    if (active_session_ && id.value==0xFFFF0094u) {
      auto reply=make_region_handshake_reply(active_,next_sequence_++);
      transport_.send(reply.data(),reply.size());
    }
    receiver(packet,id);
  },limit);
}
bool SimulatorSession::request_objects(const std::vector<uint32_t>& ids) {
  if(!active_session_)return false;
  const auto packet=make_request_multiple_objects(active_,next_sequence_++,ids);
  return !packet.empty()&&transport_.send(packet.data(),packet.size());
}
void SimulatorSession::tick() {
  if (!active_session_) return;
  const uint64_t now=GetTickCount64();
  if (last_agent_update_ms_ && now-last_agent_update_ms_<150) return;
  const auto bytes=make_agent_update(active_,next_sequence_++);
  if (!bytes.empty() && transport_.send(bytes.data(),bytes.size()))
    last_agent_update_ms_=now;
}
void SimulatorSession::disconnect() {
  transport_.close();
  next_sequence_=3;
  last_agent_update_ms_=0;
  active_session_=false;
  active_={};
}
}
