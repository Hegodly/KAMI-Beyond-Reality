#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
namespace kami::world {
using AssetId=std::array<uint8_t,16>;
enum class TextureAssetState {Queued, InFlight, EncodedReady, Failed};
struct TextureAsset {
  TextureAssetState state=TextureAssetState::Queued;
  std::vector<uint8_t> encoded;
  unsigned attempts=0;
};
struct TextureQueueStats {
  size_t queued=0,in_flight=0,encoded_ready=0,failed=0,encoded_bytes=0;
};
class TextureAssetQueue {
public:
  static constexpr size_t max_entries=2048;
  static constexpr size_t max_total_bytes=32u*1024u*1024u;
  static constexpr size_t max_asset_bytes=4u*1024u*1024u;
  // Reconcile currently referenced texture IDs, preserving completed
  // downloads for retained IDs and pruning everything else. No network I/O.
  void reconcile(const std::vector<AssetId>& referenced);
  std::vector<AssetId> take_pending(size_t limit=4);
  // Transport result only. Accepts JPEG2000 codestream or JP2 envelopes;
  // this is NOT image decompression or a GPU texture upload.
  bool finish(const AssetId& id,const std::vector<uint8_t>& bytes);
  void fail(const AssetId& id,bool retryable);
  void clear();
  const TextureAsset* get(const AssetId& id) const;
  TextureQueueStats stats() const;
  size_t entries() const {return entries_.size();}
private:
  std::map<AssetId,TextureAsset> entries_;
};
bool looks_like_jpeg2000(const std::vector<uint8_t>& encoded);
bool valid_asset_uuid(const AssetId& id);
std::string format_asset_uuid(const AssetId& id);
}
