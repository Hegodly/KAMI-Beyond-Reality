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
}
