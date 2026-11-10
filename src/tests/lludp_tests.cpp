#include "world/lludp.h"
#include <cassert>
#include <cstdint>
int main() {
  using kami::sl::Packet;
  using kami::sl::parse_packet;
  Packet p;
  const uint8_t plain[] = {0,0,0,0,1,0,0xAA,0xBB};
  assert(parse_packet(plain,sizeof(plain),p));
  assert(p.sequence==1 && p.payload.size()==2 && p.payload[0]==0xAA);
  const uint8_t encoded[] = {0x80,0,0,0,2,0,0x11,0,3,0x22};
  assert(parse_packet(encoded,sizeof(encoded),p));
  assert(p.payload.size()==5 && p.payload[1]==0 && p.payload[3]==0);
  const uint8_t ack[] = {0x10,0,0,0,3,0,0x77,0,0,0,9,1};
  assert(parse_packet(ack,sizeof(ack),p));
  assert(p.appended_acks.size()==1 && p.appended_acks[0]==9 && p.payload.size()==1);
  const uint8_t invalid[] = {0x80,0,0,0,4,0,0};
  assert(!parse_packet(invalid,sizeof(invalid),p));
  const uint8_t land[] = {0,0,0,0,5,0,11,76,3,0,0xA1,0xB2,0xC3};
  assert(parse_packet(land,sizeof(land),p));
  assert(kami::sl::is_land_layer(p));
  std::vector<uint8_t> compressed;
  assert(kami::sl::extract_land_layer_data(p,compressed));
  assert(compressed.size()==3 && compressed[0]==0xA1 && compressed[2]==0xC3);
  p.payload = {11,76,4,0,0xA1,0xB2,0xC3};
  assert(!kami::sl::extract_land_layer_data(p,compressed));
  p.payload = {11,76,0,0};
  assert(!kami::sl::extract_land_layer_data(p,compressed));
  p.payload = {11,76,1,0,0xAA,0xBB};
  assert(!kami::sl::extract_land_layer_data(p,compressed));
  p.payload = {11,55,1,0,0xAA};
  assert(!kami::sl::extract_land_layer_data(p,compressed));
  p.payload = {0xFF};
  uint32_t message_id = 0;
  size_t body_offset = 0;
  assert(!kami::sl::decode_message_id(p,message_id,body_offset));
  p.payload = {0x12,0x77};
  assert(kami::sl::decode_message_id(p,message_id,body_offset));
  assert(message_id == 0x12 && body_offset == 1);
  p.payload = {0xFF,0x42,0x77};
  assert(kami::sl::decode_message_id(p,message_id,body_offset));
  assert(message_id == 0x142 && body_offset == 2);
  p.payload = {0xFF,0xFF,0x01,0x02,0x77};
  assert(kami::sl::decode_message_id(p,message_id,body_offset));
  assert(message_id == 0x10102 && body_offset == 4);
  p.payload = {0x12,0xAA,0xBB};
  std::vector<uint8_t> message_body;
  assert(kami::sl::extract_message_body(p,message_body));
  assert(message_body.size()==2 && message_body[0]==0xAA);
  assert(!kami::sl::is_land_layer(p));
  const uint8_t short_packet[] = {0,0,0};
  assert(!parse_packet(short_packet,sizeof(short_packet),p));
}
