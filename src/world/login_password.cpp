#include "login_protocol.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#pragma comment(lib,"bcrypt.lib")
namespace kami::sl {
std::string legacy_password_hash(const std::string& password) {
  if (password.empty()) return {};
  BCRYPT_ALG_HANDLE algorithm=nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_MD5_ALGORITHM,nullptr,0)<0)
    return {};
  unsigned char digest[16]{};
  auto bytes=reinterpret_cast<PUCHAR>(const_cast<char*>(password.data()));
  const auto n=static_cast<ULONG>(std::min<size_t>(16,password.size()));
  const NTSTATUS status=BCryptHash(algorithm,nullptr,0,bytes,n,digest,16);
  BCryptCloseAlgorithmProvider(algorithm,0);
  if (status<0) return {};
  constexpr char hex[]="0123456789abcdef";
  std::string result="$1$";
  for (unsigned char b:digest) {
    result.push_back(hex[b>>4]);
    result.push_back(hex[b&15]);
  }
  SecureZeroMemory(digest,sizeof(digest));
  return result;
}
}
