#include "world/texture_asset_queue.h"
#include <cassert>
#include <vector>
int main() {
  using namespace kami::world;
  TextureAssetQueue queue;
  AssetId blank{},a{},b{},c{};
  a[0]=1;b[0]=2;c[0]=3;
  assert(!valid_asset_uuid(blank));
  assert(format_asset_uuid(blank).empty());
  assert(format_asset_uuid(a)=="01000000-0000-0000-0000-000000000000");
  queue.reconcile({blank,a,b,a});
  assert(queue.entries()==2);
  assert(queue.stats().queued==2);
  auto first=queue.take_pending(1);
  assert(first.size()==1&&first[0]==a);
  assert(queue.stats().in_flight==1&&queue.stats().queued==1);
  auto second=queue.take_pending(2);
  assert(second.size()==1&&second[0]==b);
  assert(queue.take_pending(20).empty());
  std::vector<uint8_t> jpeg={0xff,0x4f,0xff,0x51,
    0,0,0,0,0,0,0,0,0};
  assert(looks_like_jpeg2000(jpeg));
  assert(queue.finish(a,jpeg));
  assert(queue.get(a)->state==TextureAssetState::EncodedReady);
  assert(queue.stats().encoded_ready==1);
  assert(queue.stats().encoded_bytes==jpeg.size());
  assert(!queue.finish(a,jpeg));
  std::vector<uint8_t> jp2={0,0,0,12,'j','P',' ',' ',13,10,135,10,0};
  assert(looks_like_jpeg2000(jp2));
  assert(!looks_like_jpeg2000({1,2,3,4,5,6,7,8,9,10,11,12}));
  assert(!looks_like_jpeg2000(std::vector<uint8_t>(
     TextureAssetQueue::max_asset_bytes+1,0)));
  queue.fail(b,true);
  assert(queue.get(b)->state==TextureAssetState::Queued);
  assert(queue.take_pending(1)[0]==b);
  queue.fail(b,true);
  assert(queue.get(b)->state==TextureAssetState::Queued);
  assert(queue.take_pending(1)[0]==b);
  queue.fail(b,true);
  assert(queue.get(b)->state==TextureAssetState::Failed);
  assert(queue.stats().failed==1);
  queue.reconcile({a,c});
  assert(queue.get(b)==nullptr);
  assert(queue.get(a)->state==TextureAssetState::EncodedReady);
  assert(queue.get(c)->state==TextureAssetState::Queued);
  queue.reconcile({});
  assert(queue.entries()==0&&queue.stats().encoded_bytes==0);
  std::vector<AssetId> huge;
  for(unsigned i=1;i<=2200;i++){
    AssetId u{};
    u[0]=static_cast<uint8_t>(i);
    u[1]=static_cast<uint8_t>(i>>8);
    huge.push_back(u);
  }
  queue.reconcile(huge);
  assert(queue.entries()==TextureAssetQueue::max_entries);
  queue.clear();
  assert(queue.entries()==0);
}
