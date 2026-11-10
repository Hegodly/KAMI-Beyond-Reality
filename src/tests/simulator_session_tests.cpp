#include "world/simulator_session.h"
#include <cassert>
int main() {
  kami::sl::AuthenticatedSession s{};
  assert(!kami::sl::valid_session(s));
  assert(kami::sl::make_use_circuit_code(s,1).empty());
  s.endpoint={"127.0.0.1",13000};
  s.circuit_code=42;
  s.agent_id[0]=1;
  s.session_id[0]=2;
  assert(kami::sl::valid_session(s));
  auto packet=kami::sl::make_use_circuit_code(s,0x01020304);
  assert(packet.size()==46);
  assert(packet[1]==1 && packet[2]==2 && packet[3]==3 && packet[4]==4);
  assert(packet[6]==0xFF && packet[7]==0xFF && packet[9]==3);
  assert(packet[10]==42 && packet[14]==2 && packet[30]==1);
  auto movement=kami::sl::make_complete_agent_movement(s,2);
  assert(movement.size()==46);
  assert(movement[6]==0xFF && movement[7]==0xFF && movement[9]==0xF9);
  assert(movement[10]==1 && movement[26]==2 && movement[42]==42);
  auto ack=kami::sl::make_packet_ack(3,0x11223344);
  assert(ack.size()==15);
  assert(ack[6]==0xFF && ack[7]==0xFF && ack[8]==0xFF && ack[9]==0xFB);
  assert(ack[10]==1 && ack[11]==0x44 && ack[12]==0x33 &&
         ack[13]==0x22 && ack[14]==0x11);
  auto reply=kami::sl::make_region_handshake_reply(s,5);
  assert(reply.size()==46 && reply[9]==0x95);
  assert(reply[10]==1 && reply[26]==2);
  auto update=kami::sl::make_agent_update(s,6);
  kami::sl::Packet decoded;
  assert(kami::sl::parse_packet(update.data(),update.size(),decoded));
  assert((decoded.flags & 0x80u)!=0);
  uint32_t update_id=0;
  size_t offset=0;
  assert(kami::sl::decode_message_id(decoded,update_id,offset));
  assert(update_id==4 && offset==1 && decoded.payload.size()==115);
  assert(decoded.payload[1]==s.agent_id[0] && decoded.payload[17]==s.session_id[0]);
}
