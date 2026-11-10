#include "texture_asset_queue.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>
namespace kami::world {
bool valid_asset_uuid(const AssetId& id) {
  return std::any_of(id.begin(),id.end(),[](uint8_t b){return b!=0;});
}
std::string format_asset_uuid(const AssetId& id) {
  if(!valid_asset_uuid(id))return {};
  static constexpr char hex[]="0123456789abcdef";
  std::string value;
  value.reserve(36);
  for(size_t i=0;i<id.size();++i) {
    if(i==4||i==6||i==8||i==10)value.push_back('-');
    value.push_back(hex[id[i]>>4]);value.push_back(hex[id[i]&0x0fu]);
  }
  return value;
}
bool looks_like_jpeg2000(const std::vector<uint8_t>& bytes) {
  if(bytes.size()<12||bytes.size()>TextureAssetQueue::max_asset_bytes)
    return false;
  // Raw J2K main header: SOC (FF 4F), SIZ (FF 51) marker required.
  if(bytes[0]==0xff&&bytes[1]==0x4f&&
     bytes[2]==0xff&&bytes[3]==0x51) return true;
  static constexpr uint8_t jp2[12]={
    0,0,0,12,'j','P',' ',' ',0x0d,0x0a,0x87,0x0a};
  return std::equal(std::begin(jp2),std::end(jp2),bytes.begin());
}
void TextureAssetQueue::reconcile(const std::vector<AssetId>& referenced) {
  std::map<AssetId,TextureAsset> next;
  for(const auto& id:referenced) {
    if(!valid_asset_uuid(id)||next.size()>=max_entries)continue;
    auto it=entries_.find(id);
    if(it!=entries_.end())next.emplace(id,std::move(it->second));
    else next.try_emplace(id);
  }
  entries_=std::move(next);
}
std::vector<AssetId> TextureAssetQueue::take_pending(size_t limit) {
  std::vector<AssetId> ids;
  if(!limit)return ids;
  for(auto& [id,entry]:entries_) {
    if(ids.size()>=limit)break;
    if(entry.state!=TextureAssetState::Queued)continue;
    entry.state=TextureAssetState::InFlight;
    ++entry.attempts;
    ids.push_back(id);
  }
  return ids;
}
bool TextureAssetQueue::finish(const AssetId& id,const std::vector<uint8_t>& bytes) {
  auto it=entries_.find(id);
  if(it==entries_.end()||it->second.state!=TextureAssetState::InFlight)
    return false;
  size_t current=0;
  for(const auto& [asset,entry]:entries_)
    current+=entry.encoded.size();
  if(!looks_like_jpeg2000(bytes)||bytes.size()>max_total_bytes-current) {
    it->second.state=TextureAssetState::Failed;
    return false;
  }
  it->second.encoded=bytes;
  it->second.state=TextureAssetState::EncodedReady;
  return true;
}
void TextureAssetQueue::fail(const AssetId& id,bool retryable) {
  auto it=entries_.find(id);
  if(it==entries_.end()||it->second.state!=TextureAssetState::InFlight)
    return;
  it->second.state=retryable&&it->second.attempts<3?
    TextureAssetState::Queued:TextureAssetState::Failed;
}
void TextureAssetQueue::clear(){entries_.clear();}
const TextureAsset* TextureAssetQueue::get(const AssetId& id) const {
  auto it=entries_.find(id);
  return it==entries_.end()?nullptr:&it->second;
}
TextureQueueStats TextureAssetQueue::stats() const {
  TextureQueueStats s;
  for(const auto& [id,entry]:entries_) {
    switch(entry.state) {
      case TextureAssetState::Queued:++s.queued;break;
      case TextureAssetState::InFlight:++s.in_flight;break;
      case TextureAssetState::EncodedReady:++s.encoded_ready;break;
      case TextureAssetState::Failed:++s.failed;break;
    }
    s.encoded_bytes+=entry.encoded.size();
  }
  return s;
}
}
