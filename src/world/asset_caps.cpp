#include "asset_caps.h"
#include <windows.h>
#include <winhttp.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>
namespace kami::world {
namespace {
struct HttpHandle {
  HINTERNET h=nullptr;
  explicit HttpHandle(HINTERNET value):h(value){}
  ~HttpHandle(){if(h)WinHttpCloseHandle(h);}
  operator HINTERNET() const{return h;}
};
struct Bstr {
  BSTR value=nullptr;
  explicit Bstr(const wchar_t* v):value(SysAllocString(v)){}
  ~Bstr(){if(value)SysFreeString(value);}
  operator BSTR()const{return value;}
};
bool to_wide(const std::string& utf8,std::wstring& output) {
  if(utf8.empty()||utf8.size()>16384)return false;
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
      utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
  if(n<=0)return false;
  output.resize(n);
  return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
      utf8.data(),static_cast<int>(utf8.size()),output.data(),n)==n;
}
bool to_utf8(BSTR value,std::string& output) {
  if(!value)return false;
  int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,
      SysStringLen(value),nullptr,0,nullptr,nullptr);
  if(n<=0)return false;
  output.resize(n);
  return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,
      SysStringLen(value),output.data(),n,nullptr,nullptr)==n;
}
struct ParsedUrl {
  std::wstring host,path;
  INTERNET_PORT port=443;
};
bool parse_url(const std::string& url,ParsedUrl& result) {
  std::wstring wide;
  if(!to_wide(url,wide) || wide.size()>4096)return false;
  for(wchar_t c:wide) if(c<32 || c==127) return false;
  URL_COMPONENTS parts{};
  parts.dwStructSize=sizeof(parts);
  parts.dwHostNameLength=static_cast<DWORD>(-1);
  parts.dwUrlPathLength=static_cast<DWORD>(-1);
  parts.dwExtraInfoLength=static_cast<DWORD>(-1);
  parts.dwUserNameLength=static_cast<DWORD>(-1);
  parts.dwPasswordLength=static_cast<DWORD>(-1);
  if(!WinHttpCrackUrl(wide.data(),static_cast<DWORD>(wide.size()),
                       0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||
     !parts.lpszHostName||parts.dwHostNameLength<4||
     parts.dwHostNameLength>253||parts.dwUserNameLength||
     parts.dwPasswordLength)return false;
  std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
  // Prevent bearer-capability tokens from reaching loopback, literal IPs
  // or arbitrary hostnames. Simulator caps normally use Linden Lab DNS.
  std::transform(host.begin(),host.end(),host.begin(),
                 [](wchar_t c){return static_cast<wchar_t>(towlower(c));});
  const auto ends=[&](const wchar_t* suffix) {
    std::wstring value(suffix);
    return host.size()>=value.size() &&
      host.compare(host.size()-value.size(),value.size(),value)==0;
  };
  if(!(ends(L".lindenlab.com")||ends(L".secondlife.com")))return false;
  if(parts.dwExtraInfoLength>0 && parts.lpszExtraInfo[0]==L'#')return false;
  std::wstring path=parts.lpszUrlPath?
    std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength):L"/";
  if(path.empty())path=L"/";
  if(parts.lpszExtraInfo&&parts.dwExtraInfoLength)
    path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
  if(path.size()>4096||path.find(L'#')!=std::wstring::npos)return false;
  result={std::move(host),std::move(path),parts.nPort};
  return true;
}
bool post_or_get(const std::string& url,const wchar_t* method,
                 const std::string& body,size_t max_response,
                 std::vector<uint8_t>& response) {
  ParsedUrl parsed;
  if(!parse_url(url,parsed)||max_response>TextureAssetQueue::max_asset_bytes)
    return false;
  HttpHandle session(WinHttpOpen(L"KAMI Beyond Reality/0.1",
         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,
         WINHTTP_NO_PROXY_BYPASS,0));
  if(!session.h)return false;
  WinHttpSetTimeouts(session,8000,8000,12000,12000);
  HttpHandle connection(WinHttpConnect(session,parsed.host.c_str(),parsed.port,0));
  if(!connection.h)return false;
  HttpHandle request(WinHttpOpenRequest(connection,method,parsed.path.c_str(),
      nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
  if(!request.h)return false;
  DWORD disable=WINHTTP_DISABLE_REDIRECTS;
  if(!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,
                       &disable,sizeof(disable)))return false;
  const wchar_t* headers=body.empty()?L"":
      L"Content-Type: application/llsd+xml\r\n";
  if(!WinHttpSendRequest(request,headers,static_cast<DWORD>(-1L),
       body.empty()?nullptr:const_cast<char*>(body.data()),
       static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0)||
     !WinHttpReceiveResponse(request,nullptr))return false;
  DWORD status=0,length=sizeof(status);
  if(!WinHttpQueryHeaders(request,
      WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)||
     status!=200)return false;
  std::vector<uint8_t> downloaded;
  for(;;) {
    DWORD available=0;
    if(!WinHttpQueryDataAvailable(request,&available))return false;
    if(available==0)break;
    if(available>max_response-downloaded.size())return false;
    const size_t pos=downloaded.size();
    downloaded.resize(pos+available);
    DWORD got=0;
    if(!WinHttpReadData(request,downloaded.data()+pos,available,&got)||
       got==0)return false;
    downloaded.resize(pos+got);
  }
  if(downloaded.empty())return false;
  response=std::move(downloaded);
  return true;
}
}
bool valid_secure_capability_url(const std::string& url) {
  ParsedUrl ignored;
  return parse_url(url,ignored);
}
std::string texture_asset_url(const std::string& cap,const AssetId& id) {
  if(!valid_asset_uuid(id)||!valid_secure_capability_url(cap))return {};
  const std::string suffix="/?texture_id="+format_asset_uuid(id);
  if(cap.size()+suffix.size()>4096)return {};
  // ViewerAsset is generally a path without query parameters. Reject
  // ambiguous preexisting query strings instead of corrupting a token.
  if(cap.find('?')!=std::string::npos)return {};
  return cap+suffix;
}
bool parse_seed_capabilities(const std::string& xml,ViewerAssetCapabilities& result) {
  if(xml.empty()||xml.size()>65536)return false;
  std::wstring wide;
  if(!to_wide(xml,wide))return false;
  HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return false;
  const bool uninit=SUCCEEDED(init);
  bool good=false;
  do {
    Microsoft::WRL::ComPtr<IXMLDOMDocument2> doc;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&doc))))break;
    doc->put_async(VARIANT_FALSE);
    doc->put_validateOnParse(VARIANT_FALSE);
    doc->put_resolveExternals(VARIANT_FALSE);
    VARIANT disable;VariantInit(&disable);
    disable.vt=VT_BOOL;disable.boolVal=VARIANT_TRUE;
    Bstr property(L"ProhibitDTD");
    if(FAILED(doc->setProperty(property,disable)))break;
    BSTR input=SysAllocStringLen(wide.data(),static_cast<UINT>(wide.size()));
    if(!input)break;
    VARIANT_BOOL loaded=VARIANT_FALSE;
    HRESULT hr=doc->loadXML(input,&loaded);
    SysFreeString(input);
    if(FAILED(hr)||loaded!=VARIANT_TRUE)break;
    Microsoft::WRL::ComPtr<IXMLDOMNode> node;
    Bstr expression(L"/llsd/map/key[.='ViewerAsset']/following-sibling::*[1]");
    if(FAILED(doc->selectSingleNode(expression,&node))||!node)break;
    BSTR tag=nullptr;
    if(FAILED(node->get_nodeName(&tag))||!tag)break;
    const bool is_string=wcscmp(tag,L"string")==0;
    SysFreeString(tag);
    if(!is_string)break;
    BSTR value=nullptr;
    if(FAILED(node->get_text(&value))||!value)break;
    std::string cap;
    bool parsed=to_utf8(value,cap);
    SysFreeString(value);
    if(!parsed||!valid_secure_capability_url(cap))break;
    result.viewer_asset=std::move(cap);
    good=true;
  }while(false);
  if(uninit)CoUninitialize();
  return good;
}
bool request_viewer_asset_capability(const std::string& seed,
                                     ViewerAssetCapabilities& result) {
  static const std::string request=
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<llsd><array><string>ViewerAsset</string></array></llsd>";
  std::vector<uint8_t> response;
  if(!post_or_get(seed,L"POST",request,65536,response))return false;
  return parse_seed_capabilities(std::string(response.begin(),response.end()),result);
}
bool download_encoded_texture(const std::string& cap,const AssetId& id,
                              std::vector<uint8_t>& output) {
  const auto url=texture_asset_url(cap,id);
  if(url.empty())return false;
  std::vector<uint8_t> encoded;
  if(!post_or_get(url,L"GET",{},TextureAssetQueue::max_asset_bytes,encoded)||
     !looks_like_jpeg2000(encoded))return false;
  output=std::move(encoded);
  return true;
}
}
