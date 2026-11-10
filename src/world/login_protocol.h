#pragma once
#include "simulator_session.h"
#include <cstdint>
#include <string>
namespace kami::sl {
struct LoginCredentials {
  std::string username;
  std::string password;
  std::string start = "last";
};
struct LoginResult {
  bool authenticated = false;
  AuthenticatedSession session{};
  std::string seed_capability;
  std::string error;
};
// Only the official Second Life production endpoint is supported.
bool split_avatar_name(const std::string& text, std::string& first, std::string& last);
bool parse_uuid(const std::string& text, uint8_t out[16]);
std::string xml_escape(const std::string& text);
std::string legacy_password_hash(const std::string& password);
std::string make_login_request(const LoginCredentials& credentials);
bool parse_login_response(const std::string& xml, LoginResult& result);
bool post_login_xml(const std::string& payload, std::string& response, std::string& error);
LoginResult login_to_second_life(const LoginCredentials& credentials);
}
