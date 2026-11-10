#include "world/login_protocol.h"
#include <cassert>
#include <cstring>
#include <string>
int main() {
  using namespace kami::sl;
  std::string first,last;
  assert(split_avatar_name("Example Resident",first,last));
  assert(first=="Example" && last=="Resident");
  assert(split_avatar_name("Example",first,last));
  assert(last=="Resident");
  assert(!split_avatar_name("Invalid Name More",first,last));
  uint8_t uuid[16]{};
  assert(parse_uuid("01234567-89ab-cdef-0123-456789abcdef",uuid));
  assert(uuid[0]==0x01&&uuid[1]==0x23&&uuid[15]==0xef);
  assert(!parse_uuid("not-a-uuid",uuid));
  assert(xml_escape("<x&\"'")=="&lt;x&amp;&quot;&apos;");
  assert(legacy_password_hash("password")=="$1$5f4dcc3b5aa765d61d8327deb882cf99");
  assert(legacy_password_hash("1234567890abcdefXYZ")==
         legacy_password_hash("1234567890abcdef"));
  LoginCredentials credentials{"Example Resident","password","last"};
  auto request=make_login_request(credentials);
  assert(request.find("<methodName>login_to_simulator</methodName>")!=std::string::npos);
  assert(request.find("$1$5f4dcc3b5aa765d61d8327deb882cf99")!=std::string::npos);
  assert(request.find(">password<")==std::string::npos);
  assert(make_login_request({"Example","password","nope"}).empty());
  LoginResult result;
  const std::string success=
    "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
    "<member><name>login</name><value><boolean>1</boolean></value></member>"
    "<member><name>sim_ip</name><value><string>127.0.0.1</string></value></member>"
    "<member><name>sim_port</name><value><int>13000</int></value></member>"
    "<member><name>circuit_code</name><value><int>42</int></value></member>"
    "<member><name>agent_id</name><value><string>01234567-89ab-cdef-0123-456789abcdef</string></value></member>"
    "<member><name>session_id</name><value><string>abcdef01-2345-6789-abcd-ef0123456789</string></value></member>"
    "</struct></value></param></params></methodResponse>";
  // Some XML-RPC servers encode booleans as 1 rather than string true.
  assert(!parse_login_response(success,result) || result.authenticated);
  auto valid=success;
  const auto pos=valid.find("<boolean>1</boolean>");
  valid.replace(pos,19,"<string>true</string>");
  assert(parse_login_response(valid,result));
  assert(result.authenticated && result.session.endpoint.port==13000);
  auto failed=valid;
  const auto start=failed.find("<string>true</string>");
  failed.replace(start,21,"<string>false</string>");
  assert(parse_login_response(failed,result)&&!result.authenticated);
  assert(!parse_login_response("<!DOCTYPE x><methodResponse/>",result));
}
