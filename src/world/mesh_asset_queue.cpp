#include "mesh_asset_queue.h"
#include <algorithm>
#include <utility>
namespace kami::world {
void MeshAssetQueue::reconcile(const std::vector<AssetId>& ids) {
  std::map<AssetId,MeshAssetRecord> next;
  for(const auto& id:ids) {
    if(!valid_asset_uuid(id)||next.size()>=max_entries)continue;
    auto found=entries_.find(id);
    if(found==entries_.end())next.try_emplace(id);
    else next.emplace(id,std::move(found->second));
  }
  entries_=std::move(next);
}
std::vector<AssetId> MeshAssetQueue::take_pending(size_t count) {
  std::vector<AssetId> selected;
  for(auto& [id,entry]:entries_) {
    if(selected.size()>=count)break;
    if(entry.state!=MeshDownloadState::Queued)continue;
    entry.state=MeshDownloadState::InFlight;
    ++entry.attempts;
    selected.push_back(id);
  }
  return selected;
}
bool MeshAssetQueue::finish(const AssetId& id,std::vector<uint8_t> bytes) {
  auto it=entries_.find(id);
  if(it==entries_.end()||it->second.state!=MeshDownloadState::InFlight)
    return false;
  size_t occupied=0;
  for(const auto& [key,item]:entries_)
    occupied+=item.bytes.size();
  MeshAssetHeader header;
  if(bytes.empty()||bytes.size()>max_one_asset||
     bytes.size()>max_bytes-occupied||
     !parse_mesh_asset_header(bytes,header)) {
    it->second.state=MeshDownloadState::Failed;
    return false;
  }
  it->second.header=header;
  DecodedMeshGeometry geometry;
  if(decode_mesh_high_lod(bytes,header,geometry)) {
    // Each compressed mesh can expand dramatically. Bound total live
    // vertex/index storage independently of compressed asset bytes.
    size_t already_decoded=0;
    for(const auto& [key,entry]:entries_) {
      already_decoded+=entry.geometry.vertices*sizeof(TerrainVertex);
      already_decoded+=entry.geometry.triangles*3*sizeof(uint32_t);
    }
    const size_t requested=geometry.vertices*sizeof(TerrainVertex)+
      geometry.triangles*3*sizeof(uint32_t);
    if(already_decoded<=max_decoded_geometry_bytes &&
       requested<=max_decoded_geometry_bytes-already_decoded) {
      it->second.geometry=std::move(geometry);
      it->second.state=MeshDownloadState::DecodedReady;
    } else it->second.state=MeshDownloadState::HeaderReady;
  } else it->second.state=MeshDownloadState::HeaderReady;
  it->second.bytes=std::move(bytes);
  return true;
}
void MeshAssetQueue::fail(const AssetId& id,bool retryable) {
  auto it=entries_.find(id);
  if(it==entries_.end()||it->second.state!=MeshDownloadState::InFlight)return;
  it->second.state=retryable&&it->second.attempts<3?
    MeshDownloadState::Queued:MeshDownloadState::Failed;
}
const MeshAssetRecord* MeshAssetQueue::get(const AssetId& id) const {
  auto it=entries_.find(id);
  return it==entries_.end()?nullptr:&it->second;
}
MeshQueueStats MeshAssetQueue::stats() const {
  MeshQueueStats result;
  for(const auto& [id,entry]:entries_) {
    switch(entry.state) {
      case MeshDownloadState::Queued:++result.queued;break;
      case MeshDownloadState::InFlight:++result.in_flight;break;
      case MeshDownloadState::HeaderReady:++result.header_ready;break;
      case MeshDownloadState::DecodedReady:++result.decoded_ready;break;
      case MeshDownloadState::Failed:++result.failed;break;
    }
    result.bytes+=entry.bytes.size();
  }
  return result;
}
}
