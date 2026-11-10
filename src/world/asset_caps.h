#pragma once
#include "texture_asset_queue.h"
#include <string>
#include <vector>
namespace kami::world {
// ViewerAsset is an opaque HTTPS bearer capability supplied by the active
// simulator's seed capability, not a hardcoded generic CDN URL.
struct ViewerAssetCapabilities {
  std::string viewer_asset;
};
bool parse_seed_capabilities(const std::string& llsd_xml,
                             ViewerAssetCapabilities& result);
bool valid_secure_capability_url(const std::string& url);
std::string texture_asset_url(const std::string& viewer_asset,
                              const AssetId& texture_id);
// Both functions perform bounded synchronous HTTPS operations. Invoke off
// the GUI/render thread and never log any full capability URL.
bool request_viewer_asset_capability(const std::string& seed_cap,
                                     ViewerAssetCapabilities& result);
bool download_encoded_texture(const std::string& viewer_asset,
                              const AssetId& id,std::vector<uint8_t>& output);
}
