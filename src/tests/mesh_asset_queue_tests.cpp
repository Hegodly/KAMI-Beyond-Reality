#include "world/mesh_asset_queue.h"
#include <cassert>
#include <string>
#include <vector>
using namespace kami::world;
namespace {
void be32(std::vector<uint8_t>& d,uint32_t n){
  d.push_back(uint8_t(n>>24));d.push_back(uint8_t(n>>16));
  d.push_back(uint8_t(n>>8));d.push_back(uint8_t(n));
}
void key(std::vector<uint8_t>& d,const char* s){
  std::string v(s);d.push_back('k');be32(d,uint32_t(v.size()));
  d.insert(d.end(),v.begin(),v.end());
}
std::vector<uint8_t> valid_asset(){
  std::vector<uint8_t> d{'{'};be32(d,1);
  key(d,"high_lod");d.push_back('{');be32(d,2);
  key(d,"offset");d.push_back('i');be32(d,0);
  key(d,"size");d.push_back('i');be32(d,5);
  d.push_back('}');d.push_back('}');
  d.insert(d.end(),{1,2,3,4,5});
  return d;
}
}
int main(){
  AssetId a{},b{};a[0]=1;b[0]=2;
  MeshAssetQueue queue;
  queue.reconcile({AssetId{},a,a,b});
  assert(queue.stats().queued==2);
  assert(queue.take_pending(1)==std::vector<AssetId>{a});
  assert(queue.stats().in_flight==1);
  auto data=valid_asset();
  assert(queue.finish(a,data));
  assert(queue.stats().header_ready==1);
  assert(queue.stats().decoded_ready==0); // synthetic header has no valid zlib LOD
  assert(queue.get(a)&&queue.get(a)->header.high.size==5);
  assert(queue.stats().bytes==data.size());
  assert(!queue.finish(a,data));
  assert(queue.take_pending(1)==std::vector<AssetId>{b});
  queue.fail(b,true);assert(queue.stats().queued==1);
  assert(queue.take_pending(1).size()==1);
  queue.fail(b,true);assert(queue.stats().queued==1);
  assert(queue.take_pending(1).size()==1);
  queue.fail(b,true);assert(queue.stats().failed==1);
  queue.reconcile({a});
  assert(!queue.get(b)&&queue.get(a));
  queue.reconcile({});
  assert(queue.stats().bytes==0&&queue.stats().header_ready==0);
  queue.reconcile({b});
  assert(queue.take_pending(1).size()==1);
  assert(!queue.finish(b,{0,1,2,3}));
  assert(queue.stats().failed==1);
}
