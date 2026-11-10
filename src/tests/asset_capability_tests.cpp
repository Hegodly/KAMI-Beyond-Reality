#include "world/asset_caps.h"
#include <cassert>
#include <string>
#include <vector>
int main() {
  using namespace kami::world;
  const std::string seed="https://simhost-0.agni.lindenlab.com:12046/CAPS/seed";
  const std::string cap="https://asset-1.agni.lindenlab.com:443/CAPS/fetch";
  assert(valid_secure_capability_url(seed));
  assert(valid_secure_capability_url(cap));
  assert(!valid_secure_capability_url("http://simhost-0.agni.lindenlab.com/CAPS/x"));
  assert(!valid_secure_capability_url("https://127.0.0.1/CAPS/x"));
  assert(!valid_secure_capability_url("https://localhost/CAPS/x"));
  assert(!valid_secure_capability_url("https://example.com/CAPS/x"));
  assert(!valid_secure_capability_url("https://evil-lindenlab.com/CAPS/x"));
  assert(!valid_secure_capability_url("https://alice:secret@simhost-0.agni.lindenlab.com/CAPS/x"));
  assert(!valid_secure_capability_url("https://simhost-0.agni.lindenlab.com/CAPS/x#fragment"));
  assert(!valid_secure_capability_url("https://simhost-0.agni.lindenlab.com/CAPS/x\r\nHeader: bad"));
  const std::string xml=
    "<?xml version=\"1.0\"?><llsd><map>"
    "<key>EventQueueGet</key><string>https://simhost-0.agni.lindenlab.com/event</string>"
    "<key>ViewerAsset</key><string>"+cap+"</string>"
    "</map></llsd>";
  ViewerAssetCapabilities result;
  assert(parse_seed_capabilities(xml,result));
  assert(result.viewer_asset==cap);
  assert(!parse_seed_capabilities("<llsd><map><key>ViewerAsset</key><string>http://bad.invalid</string></map></llsd>",result));
  assert(result.viewer_asset==cap);
  assert(!parse_seed_capabilities("<!DOCTYPE llsd [<!ENTITY ext SYSTEM 'file:///etc/passwd'>]>"
    "<llsd><map><key>ViewerAsset</key><string>&ext;</string></map></llsd>",result));
  assert(!parse_seed_capabilities("<llsd><map><key>GetMesh</key><string>"+cap+"</string></map></llsd>",result));
  assert(!parse_seed_capabilities("<llsd><map><key>ViewerAsset</key><integer>10</integer></map></llsd>",result));
  AssetId texture{};
  texture[0]=0x12;texture[1]=0x34;texture[15]=0x56;
  const auto url=texture_asset_url(cap,texture);
  assert(url==cap+"/?texture_id=12340000-0000-0000-0000-000000000056");
  assert(texture_asset_url(cap,AssetId{}).empty());
  assert(texture_asset_url(cap+"?token=secret",texture).empty());
  assert(texture_asset_url("http://example.com/x",texture).empty());
}
