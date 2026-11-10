#include "login_protocol.h"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace kami::sl {
bool split_avatar_name(const std::string& text, std::string& first, std::string& last) {
  if (text.empty() || text.size()>100) return false;
  auto space=text.find(' ');
  if (space==std::string::npos) { first=text; last="Resident"; }
  else {
    if (text.find(' ',space+1)!=std::string::npos) return false;
    first=text.substr(0,space); last=text.substr(space+1);
  }
  auto valid=[](const std::string& name) {
    return !name.empty() && name.size()<=50 &&
      std::all_of(name.begin(),name.end(),[](unsigned char c) {
        return std::isalnum(c) || c=='_' || c=='-' || c=='.';
      });
  };
  return valid(first) && valid(last);
}
bool parse_uuid(const std::string& text, uint8_t out[16]) {
  if (text.size()!=36 || !out) return false;
  for (size_t i: {size_t(8),size_t(13),size_t(18),size_t(23)})
    if (text[i]!='-') return false;
  auto hex=[](char c)->int {
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='f') return c-'a'+10;
    if (c>='A'&&c<='F') return c-'A'+10;
    return -1;
  };
  uint8_t bytes[16]{};
  size_t n=0;
  for (size_t i=0;i<text.size();) {
    if (text[i]=='-') { ++i;continue; }
    if (i+1>=text.size() || n>=16) return false;
    int hi=hex(text[i++]),lo=hex(text[i++]);
    if (hi<0||lo<0) return false;
    bytes[n++]=static_cast<uint8_t>((hi<<4)|lo);
  }
  if (n!=16) return false;
  std::memcpy(out,bytes,16);
  return true;
}
std::string xml_escape(const std::string& s) {
  std::string out;out.reserve(s.size());
  for (unsigned char c:s) {
    switch(c) {
      case '&':out+="&amp;";break;
      case '<':out+="&lt;";break;
      case '>':out+="&gt;";break;
      case '"':out+="&quot;";break;
      case '\'':out+="&apos;";break;
      default: if (c>=32) out.push_back(static_cast<char>(c));
    }
  }
  return out;
}
std::string make_login_request(const LoginCredentials& credentials) {
  std::string first,last;
  if (!split_avatar_name(credentials.username,first,last) ||
      credentials.password.empty() || credentials.password.size()>1024 ||
      (credentials.start!="home"&&credentials.start!="last")) return {};
  if (!credentials.token.empty() &&
      (credentials.token.size()!=6 ||
       !std::all_of(credentials.token.begin(),credentials.token.end(),
         [](unsigned char c){return c>='0'&&c<='9';}))) return {};
  if (credentials.mfa_hash.size()>512) return {};
  const std::string hash=legacy_password_hash(credentials.password);
  if (hash.empty()) return {};
  auto value=[](const char* name,const std::string& content) {
    return std::string("<member><name>")+name+"</name><value><string>"+
      xml_escape(content)+"</string></value></member>";
  };
  auto integer=[](const char* name,int content) {
    return std::string("<member><name>")+name+"</name><value><int>"+
      std::to_string(content)+"</int></value></member>";
  };
  std::string out="<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<methodCall><methodName>login_to_simulator</methodName><params>"
    "<param><value><struct>";
  out+=value("first",first)+value("last",last)+value("passwd",hash);
  out+=value("start",credentials.start)+value("channel","KAMI Beyond Reality");
  out+=value("version","0.1.0")+value("platform","Win");
  out+=value("id0","00000000-0000-0000-0000-000000000000");
  out+=value("mac",hardware_identity_for_login());
  // The official viewer sends these as XML-RPC numeric values, not strings.
  out+=integer("agree_to_tos",0)+integer("read_critical",0);
  out+=integer("address_size",64)+integer("last_exec_event",0);
  out+=integer("last_exec_duration",0)+integer("extended_errors",1);
  out+=value("last_exec_session_id","00000000-0000-0000-0000-000000000000");
  out+=value("host_id","");
  // Both fields must be present for an MFA-capable third-party viewer.
  out+=value("token",credentials.token)+value("mfa_hash",credentials.mfa_hash);
  out+="<member><name>options</name><value><array><data>";
  for (const char* option : {
    "inventory-root", "inventory-skeleton", "inventory-lib-root",
    "inventory-lib-owner", "inventory-skel-lib", "gestures",
    "display_names", "event_categories", "event_notifications",
    "classified_categories", "adult_compliant", "buddy-list",
    "newuser-config", "ui-config", "max-agent-groups",
    "map-server-url", "login-flags", "global-textures"
  }) {
    out+="<value><string>";
    out+=option;
    out+="</string></value>";
  }
  out+="</data></array></value></member></struct></value></param></params></methodCall>";
  return out;
}
LoginResult login_to_second_life(const LoginCredentials& credentials) {
  LoginResult result;
  std::string body=make_login_request(credentials);
  if (body.empty()) {result.error="Invalid login details";return result;}
  std::string response;
  if (!post_login_xml(body,response,result.error)) return result;
  if (!parse_login_response(response,result) && result.error.empty())
    result.error="The Second Life login response was invalid";
  return result;
}
}
