#include "simulator_transport.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
namespace kami::sl {
bool decode_message_id(const Packet& packet, MessageId& out) {
  const auto& p = packet.payload;
  if (p.empty()) return false;
  MessageId candidate{};
  if (p[0] != 0xFF) { candidate.value = p[0]; candidate.header_bytes = 1; }
  else if (p.size() >= 2 && p[1] != 0xFF) {
    candidate.value = 0xFF00u | p[1]; candidate.header_bytes = 2;
  } else if (p.size() >= 4 && p[1] == 0xFF) {
    candidate.value = 0xFFFF0000u | (uint32_t(p[2]) << 8) | p[3];
    candidate.header_bytes = 4;
  } else return false;
  out = candidate;
  return true;
}
SimulatorTransport::SimulatorTransport() = default;
SimulatorTransport::~SimulatorTransport() { close(); }
bool SimulatorTransport::open(const SimulatorEndpoint& endpoint) {
  close();
  if (!endpoint.port || endpoint.ipv4.empty()) return false;
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;
  winsock_started_ = true;
  in_addr ip{};
  if (InetPtonA(AF_INET, endpoint.ipv4.c_str(), &ip) != 1) { close(); return false; }
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) { close(); return false; }
  socket_ = static_cast<uintptr_t>(s);
  u_long nonblocking = 1;
  if (ioctlsocket(s, FIONBIO, &nonblocking) != 0) { close(); return false; }
  peer_ip_ = ip.s_addr;
  peer_port_ = htons(endpoint.port);
  return true;
}
bool SimulatorTransport::send(const uint8_t* bytes, size_t count) {
  if (socket_ == ~uintptr_t(0) || !bytes || !count || count > 65507) return false;
  sockaddr_in peer{}; peer.sin_family = AF_INET;
  peer.sin_addr.s_addr = peer_ip_; peer.sin_port = peer_port_;
  return sendto(static_cast<SOCKET>(socket_), reinterpret_cast<const char*>(bytes),
                static_cast<int>(count), 0, reinterpret_cast<sockaddr*>(&peer),
                sizeof(peer)) == static_cast<int>(count);
}
size_t SimulatorTransport::poll(const Receiver& receiver, size_t max_packets) {
  if (socket_ == ~uintptr_t(0) || !receiver) return 0;
  size_t processed = 0;
  std::array<uint8_t, 65535> bytes{};
  while (processed < max_packets) {
    sockaddr_in from{}; int length = sizeof(from);
    int count = recvfrom(static_cast<SOCKET>(socket_), reinterpret_cast<char*>(bytes.data()),
                         static_cast<int>(bytes.size()), 0,
                         reinterpret_cast<sockaddr*>(&from), &length);
    if (count == SOCKET_ERROR) break;
    // Never accept unsolicited packets from another endpoint.
    if (from.sin_addr.s_addr != peer_ip_ || from.sin_port != peer_port_) continue;
    Packet packet; MessageId id;
    if (parse_packet(bytes.data(), static_cast<size_t>(count), packet) &&
        decode_message_id(packet, id)) { receiver(packet, id); ++processed; }
  }
  return processed;
}
void SimulatorTransport::close() {
  if (socket_ != ~uintptr_t(0)) {
    closesocket(static_cast<SOCKET>(socket_)); socket_ = ~uintptr_t(0);
  }
  if (winsock_started_) { WSACleanup(); winsock_started_ = false; }
  peer_ip_ = 0; peer_port_ = 0;
}
}
