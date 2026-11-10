#pragma once
#include "mesh_asset_header.h"
#include "texture_asset_queue.h"
#include <cstddef>
#include <map>
#include <vector>
namespace kami::world {
enum class MeshDownloadState {Queued,InFlight,HeaderReady,Failed};
struct MeshAssetRecord {
  MeshDownloadState state=MeshDownloadState::Queued;
  MeshAssetHeader header{};
  std::vector<uint8_t> bytes;
  unsigned attempts=0;
};
struct MeshQueueStats {
  size_t queued=0,in_flight=0,header_ready=0,failed=0,bytes=0;
};
class MeshAssetQueue {
public:
  static constexpr size_t max_entries=128;
  static constexpr size_t max_bytes=32u*1024u*1024u;
  static constexpr size_t max_one_asset=12u*1024u*1024u;
  void reconcile(const std::vector<AssetId>& ids);
  std::vector<AssetId> take_pending(size_t count=1);
  bool finish(const AssetId& id,std::vector<uint8_t> asset);
  void fail(const AssetId& id,bool retryable);
  const MeshAssetRecord* get(const AssetId& id) const;
  MeshQueueStats stats() const;
  void clear(){entries_.clear();}
private:
  std::map<AssetId,MeshAssetRecord> entries_;
};
}
