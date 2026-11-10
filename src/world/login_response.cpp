#include "login_protocol.h"
#include <windows.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <charconv>
namespace kami::sl {
namespace {
struct Bstr {
  BSTR value=nullptr;
  explicit Bstr(const wchar_t* s):value(SysAllocString(s)){}
  ~Bstr(){ if(value) SysFreeString(value); }
  Bstr(const Bstr&)=delete;
  operator BSTR() const {return value;}
};
bool utf8_to_wide(const std::string& text,std::wstring& out) {
  if (text.empty() || text.size()>2*1024*1024) return false;
  const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
      text.data(),static_cast<int>(text.size()),nullptr,0);
  if (!n) return false;
  out.resize(n);
  return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),
      static_cast<int>(text.size()),out.data(),n)==n;
}
bool wide_to_utf8(BSTR text,std::string& out) {
  if (!text) return false;
  const int chars=static_cast<int>(SysStringLen(text));
  int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,chars,
                           nullptr,0,nullptr,nullptr);
  if (n<0 || (n==0&&chars)) return false;
  out.resize(n);
  return !n || WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,chars,
                                    out.data(),n,nullptr,nullptr)==n;
}
bool query_text(IXMLDOMNode* parent,const wchar_t* xpath,std::string& out) {
  Microsoft::WRL::ComPtr<IXMLDOMNode> node;
  Bstr path(xpath);
  if (!path.value || FAILED(parent->selectSingleNode(path,&node)) || !node)
    return false;
  BSTR raw=nullptr;
  if (FAILED(node->get_text(&raw)) || !raw) return false;
  bool ok=wide_to_utf8(raw,out);
  SysFreeString(raw);
  return ok;
}
bool read_number(const std::string& value,uint32_t& result) {
  if (value.empty()) return false;
  auto r=std::from_chars(value.data(),value.data()+value.size(),result);
  return r.ec==std::errc{} && r.ptr==value.data()+value.size();
}
}
bool parse_login_response(const std::string& xml, LoginResult& result) {
  result={};
  std::wstring wide;
  if (!utf8_to_wide(xml,wide)) return false;
  HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  if (FAILED(init)&&init!=RPC_E_CHANGED_MODE) return false;
  const bool uninit=SUCCEEDED(init);
  bool parsed=false;
  do {
    Microsoft::WRL::ComPtr<IXMLDOMDocument2> document;
    if (FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&document)))) break;
    document->put_async(VARIANT_FALSE);
    document->put_validateOnParse(VARIANT_FALSE);
    document->put_resolveExternals(VARIANT_FALSE);
    VARIANT disabled;VariantInit(&disabled);
    disabled.vt=VT_BOOL;disabled.boolVal=VARIANT_TRUE;
    Bstr prohibit(L"ProhibitDTD");
    if (FAILED(document->setProperty(prohibit,disabled))) break;
    BSTR input=SysAllocStringLen(wide.data(),static_cast<UINT>(wide.size()));
    if (!input) break;
    VARIANT_BOOL loaded=VARIANT_FALSE;
    HRESULT hr=document->loadXML(input,&loaded);
    SysFreeString(input);
    if (FAILED(hr)||loaded!=VARIANT_TRUE) break;
    Microsoft::WRL::ComPtr<IXMLDOMNodeList> members;
    Bstr xpath(L"/methodResponse/params/param/value/struct/member");
    if (FAILED(document->selectNodes(xpath,&members)) || !members) break;
    long count=0;
    if (FAILED(members->get_length(&count)) || count<=0 || count>4096) break;
    std::unordered_map<std::string,std::string> properties;
    for (long i=0;i<count;++i) {
      Microsoft::WRL::ComPtr<IXMLDOMNode> node;
      if (FAILED(members->get_item(i,&node))||!node) continue;
      std::string key,value;
      if (query_text(node.Get(),L"name",key) &&
          query_text(node.Get(),L"value",value) && key.size()<128 && value.size()<8192)
        properties.emplace(std::move(key),std::move(value));
    }
    auto get=[&](const char* key)->std::string {
      auto it=properties.find(key);
      return it==properties.end()?std::string():it->second;
    };
    if (get("login")!="true") {
      result.error=get("message");
      if (result.error.empty()) result.error="Login was not accepted";
      parsed=true;
      break;
    }
    auto endpoint=get("sim_ip");
    uint32_t port=0,code=0;
    AuthenticatedSession session{};
    if (endpoint.empty()||endpoint.size()>64||
        !read_number(get("sim_port"),port)||port==0||port>65535||
        !read_number(get("circuit_code"),code)||code==0||
        !parse_uuid(get("session_id"),session.session_id)||
        !parse_uuid(get("agent_id"),session.agent_id)) break;
    session.endpoint={endpoint,static_cast<uint16_t>(port)};
    session.circuit_code=code;
    if (!valid_session(session)) break;
    result.session=session;
    result.seed_capability=get("seed_capability");
    result.authenticated=true;
    parsed=true;
  }while(false);
  if (uninit) CoUninitialize();
  return parsed;
}
}
