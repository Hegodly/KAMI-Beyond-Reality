#include "login_protocol.h"
#include <windows.h>
#include <winhttp.h>
#include <memory>
#include <string>
namespace kami::sl {
namespace {
struct Handle {
 HINTERNET handle=nullptr;
 explicit Handle(HINTERNET h):handle(h){}
 ~Handle(){if(handle) WinHttpCloseHandle(handle);}
 operator HINTERNET() const {return handle;}
};
}
bool post_login_xml(const std::string& body,std::string& response,std::string& error) {
  if (body.empty()||body.size()>65536) {error="Login request is invalid";return false;}
  Handle session(WinHttpOpen(L"KAMI Beyond Reality/0.1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  if (!session.handle) {error="Could not initialize Windows HTTPS";return false;}
  WinHttpSetTimeouts(session,10000,10000,15000,15000);
  Handle connection(WinHttpConnect(session,L"login.agni.lindenlab.com",
                                    INTERNET_DEFAULT_HTTPS_PORT,0));
  if (!connection.handle){error="Could not connect to Second Life";return false;}
  Handle request(WinHttpOpenRequest(connection,L"POST",L"/cgi-bin/login.cgi",
                                  nullptr,WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
  if (!request.handle){error="Could not create secure login request";return false;}
  DWORD disable=WINHTTP_DISABLE_REDIRECTS;
  WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable));
  if (!WinHttpSendRequest(request,L"Content-Type: text/xml; charset=utf-8\r\n",
      static_cast<DWORD>(-1L),const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),
      static_cast<DWORD>(body.size()),0) || !WinHttpReceiveResponse(request,nullptr)) {
    const DWORD code=GetLastError();
    if (code==ERROR_WINHTTP_NAME_NOT_RESOLVED) error="Login server DNS lookup failed";
    else if (code==ERROR_WINHTTP_TIMEOUT) error="Second Life login server timed out";
    else if (code==ERROR_WINHTTP_CANNOT_CONNECT) error="Could not establish login HTTPS connection";
    else if (code==ERROR_WINHTTP_SECURE_FAILURE) error="Windows rejected the login server TLS certificate";
    else error="Login HTTPS request failed (Windows error "+std::to_string(code)+")";
    return false;
  }
  DWORD code=0,size=sizeof(code);
  if (!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)||code!=200) {
    error="Second Life login HTTP returned status "+std::to_string(code);return false;
  }
  response.clear();
  for (;;) {
    DWORD available=0;
    if (!WinHttpQueryDataAvailable(request,&available)) {
      error="Failed to read secure login response";return false;
    }
    if (!available) break;
    if (available>2*1024*1024-response.size()) {
      error="Login response exceeded size limit";return false;
    }
    std::string part(available,'\0');
    DWORD count=0;
    if (!WinHttpReadData(request,part.data(),available,&count)||!count) {
      error="Secure login response was incomplete";return false;
    }
    response.append(part.data(),count);
  }
  return !response.empty();
}
}
