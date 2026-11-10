#include "login_protocol.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace kami::sl {
namespace {
std::string hex_md5(const uint8_t* bytes, size_t count) {
  BCRYPT_ALG_HANDLE algorithm=nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_MD5_ALGORITHM,nullptr,0)<0)
    return {};
  std::array<uint8_t,16> digest{};
  const auto status=BCryptHash(algorithm,nullptr,0,
    const_cast<PUCHAR>(bytes),static_cast<ULONG>(count),
    digest.data(),static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(algorithm,0);
  if(status<0) return {};
  const char digits[]="0123456789abcdef";
  std::string out;
  out.reserve(32);
  for(uint8_t b:digest) {
    out.push_back(digits[b>>4]);
    out.push_back(digits[b&15]);
  }
  SecureZeroMemory(digest.data(),digest.size());
  return out;
}
bool usable(const IP_ADAPTER_ADDRESSES* adapter) {
  if(adapter->PhysicalAddressLength!=6) return false;
  if (adapter->OperStatus!=IfOperStatusUp) return false;
  return std::any_of(adapter->PhysicalAddress,
    adapter->PhysicalAddress+6,[](uint8_t b){return b!=0;});
}
}
std::string hardware_identity_for_login() {
  ULONG count=0;
  constexpr ULONG flags=GAA_FLAG_SKIP_ANYCAST |
                         GAA_FLAG_SKIP_MULTICAST |
                         GAA_FLAG_SKIP_DNS_SERVER;
  if (GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,nullptr,&count)!=ERROR_BUFFER_OVERFLOW ||
      count==0 || count>1024*1024) return {};
  std::vector<uint8_t> space(count);
  auto* adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(space.data());
  if (GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,adapters,&count)!=NO_ERROR)
    return {};
  for(int preferred=1;preferred>=0;--preferred) {
    for(const auto* adapter=adapters;adapter;adapter=adapter->Next) {
      if (!usable(adapter)) continue;
      const bool physical=(adapter->IfType==IF_TYPE_ETHERNET_CSMACD ||
                           adapter->IfType==IF_TYPE_IEEE80211);
      if (physical != (preferred==1)) continue;
      return hex_md5(adapter->PhysicalAddress,6);
    }
  }
  return {};
}
}
