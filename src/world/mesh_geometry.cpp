#include "mesh_geometry.h"
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
namespace kami::world {
namespace {
constexpr size_t max_unpacked=8u*1024u*1024u;
struct Reader {
  const uint8_t* data=nullptr;
  size_t size=0,pos=0;
  bool has(size_t n)const{return pos<=size&&n<=size-pos;}
  bool next(uint8_t& v){if(!has(1))return false;v=data[pos++];return true;}
  bool skip_bytes(size_t n){if(!has(n))return false;pos+=n;return true;}
  bool be32(uint32_t& n){
    if(!has(4))return false;
    n=(uint32_t(data[pos])<<24)|(uint32_t(data[pos+1])<<16)|
      (uint32_t(data[pos+2])<<8)|uint32_t(data[pos+3]);
    pos+=4;return true;
  }
  bool name(std::string& s){
    uint8_t tag=0;uint32_t n=0;
    if(!next(tag)||tag!='k'||!be32(n)||n>256||!has(n))return false;
    s.assign(reinterpret_cast<const char*>(data+pos),n);
    pos+=n;return true;
  }
  bool binary(std::vector<uint8_t>& out){
    uint8_t tag=0;uint32_t n=0;
    if(!next(tag)||tag!='b'||!be32(n)||n>max_unpacked||!has(n))return false;
    out.assign(data+pos,data+pos+n);pos+=n;return true;
  }
  bool number(double& value){
    uint8_t tag=0;uint32_t bits32=0;
    if(!next(tag))return false;
    if(tag=='i') {
      if(!be32(bits32))return false;
      value=double(static_cast<int32_t>(bits32));
    }else if(tag=='r') {
      uint32_t high=0,low=0;
      if(!be32(high)||!be32(low))return false;
      uint64_t bits=(uint64_t(high)<<32)|low;
      std::memcpy(&value,&bits,sizeof(value));
    }else return false;
    return std::isfinite(value);
  }
  bool domain(std::array<double,3>& low,std::array<double,3>& high,
              unsigned dimensions){
    uint8_t tag=0;uint32_t entries=0;
    if(!next(tag)||tag!='{'||!be32(entries)||entries>12)return false;
    bool min_seen=false,max_seen=false;
    for(uint32_t i=0;i<entries;++i){
      std::string field;
      if(!name(field))return false;
      if(field=="Min"||field=="Max"){
        uint32_t count=0;
        if(!next(tag)||tag!='['||!be32(count)||count!=dimensions)return false;
        auto& dest=(field=="Min")?low:high;
        for(unsigned n=0;n<dimensions;++n)if(!number(dest[n]))return false;
        if(!next(tag)||tag!=']')return false;
        if(field=="Min")min_seen=true;else max_seen=true;
      }else if(!skip(3))return false;
    }
    return next(tag)&&tag=='}'&&min_seen&&max_seen;
  }
  bool skip(unsigned depth){
    if(depth>12)return false;
    uint8_t tag=0;uint32_t count=0;
    if(!next(tag))return false;
    switch(tag){
      case '!':case '0':case '1':return true;
      case 'i':return skip_bytes(4);
      case 'r':case 'd':return skip_bytes(8);
      case 'u':return skip_bytes(16);
      case 'l':case 'b':case 's':
        return be32(count)&&count<=max_unpacked&&skip_bytes(count);
      case '[':
        if(!be32(count)||count>65536)return false;
        for(uint32_t n=0;n<count;++n)if(!skip(depth+1))return false;
        return next(tag)&&tag==']';
      case '{':
        if(!be32(count)||count>1024)return false;
        for(uint32_t n=0;n<count;++n){
          std::string key;
          if(!name(key)||!skip(depth+1))return false;
        }
        return next(tag)&&tag=='}';
    }
    return false;
  }
};
bool inflate_lod(const uint8_t* data,size_t size,std::vector<uint8_t>& out) {
  if(!data||size<4||size>12u*1024u*1024u)return false;
  z_stream stream{};
  stream.next_in=const_cast<Bytef*>(data);
  stream.avail_in=static_cast<uInt>(size);
  // LLUZipHelper accepts both zlib and gzip wrappers.
  if(inflateInit2(&stream,15+32)!=Z_OK)return false;
  std::vector<uint8_t> decoded;
  bool okay=false;
  std::array<uint8_t,16384> buffer{};
  for(;;){
    stream.next_out=buffer.data();
    stream.avail_out=static_cast<uInt>(buffer.size());
    const int status=inflate(&stream,Z_NO_FLUSH);
    const size_t produced=buffer.size()-stream.avail_out;
    if(produced>max_unpacked-decoded.size())break;
    decoded.insert(decoded.end(),buffer.begin(),buffer.begin()+produced);
    if(status==Z_STREAM_END){
      okay=stream.avail_in==0&& !decoded.empty();
      break;
    }
    if(status!=Z_OK || (produced==0&&stream.avail_in==0))break;
  }
  inflateEnd(&stream);
  if(okay)out=std::move(decoded);
  return okay;
}
inline uint16_t le16(const std::vector<uint8_t>& data,size_t offset) {
  return uint16_t(data[offset])|(uint16_t(data[offset+1])<<8);
}
struct Submesh {
  std::vector<uint8_t> positions,indices,texcoords;
  std::array<double,3> pos_min{-.5,-.5,-.5},pos_max{.5,.5,.5};
  std::array<double,3> uv_min{0,0,0},uv_max{1,1,0};
  bool empty=false;
};
bool unpack_submesh(Reader& r,Submesh& s) {
  uint8_t tag=0;uint32_t count=0;
  if(!r.next(tag)||tag!='{'||!r.be32(count)||count>64)return false;
  for(unsigned i=0;i<count;++i){
    std::string field;
    if(!r.name(field))return false;
    if(field=="Position") {
      if(!r.binary(s.positions))return false;
    } else if(field=="TriangleList"){
      if(!r.binary(s.indices))return false;
    } else if(field=="TexCoord0"){
      if(!r.binary(s.texcoords))return false;
    } else if(field=="PositionDomain"){
      if(!r.domain(s.pos_min,s.pos_max,3))return false;
    } else if(field=="TexCoord0Domain"){
      if(!r.domain(s.uv_min,s.uv_max,2))return false;
    } else if(field=="NoGeometry") {
      if(!r.next(tag)||tag!='1')return false;
      s.empty=true;
    } else if(!r.skip(1))return false;
  }
  return r.next(tag)&&tag=='}';
}
bool build_face(const Submesh& s,TerrainMesh& out) {
  if(s.empty) return true;
  if(s.positions.empty()||s.positions.size()%6!=0||
     s.indices.empty()||s.indices.size()%6!=0)return false;
  const size_t count=s.positions.size()/6;
  if(count<3||count>65535||s.indices.size()/6>150000)return false;
  if(!s.texcoords.empty()&&s.texcoords.size()!=count*4)return false;
  for(unsigned d=0;d<3;++d)
    if(!std::isfinite(s.pos_min[d])||!std::isfinite(s.pos_max[d])||
       s.pos_min[d]>s.pos_max[d]||
       s.pos_min[d]<-2.0||s.pos_max[d]>2.0)return false;
  for(unsigned d=0;d<2;++d)
    if(!std::isfinite(s.uv_min[d])||!std::isfinite(s.uv_max[d])||
       std::abs(s.uv_min[d])>1000||std::abs(s.uv_max[d])>1000)return false;
  TerrainMesh face;
  face.vertices.reserve(count);
  face.indices.reserve(s.indices.size()/2);
  for(size_t i=0;i<count;++i) {
    double coord[3]{};
    for(int d=0;d<3;++d){
      const auto raw=le16(s.positions,i*6+d*2);
      coord[d]=s.pos_min[d]+double(raw)*(s.pos_max[d]-s.pos_min[d])/65535.;
    }
    TerrainVertex v{float(coord[0]),float(coord[2]),float(coord[1]),1,1,1};
    if(!s.texcoords.empty()) {
      v.u=float(s.uv_min[0]+double(le16(s.texcoords,i*4))*
                      (s.uv_max[0]-s.uv_min[0])/65535.);
      v.v=float(s.uv_min[1]+double(le16(s.texcoords,i*4+2))*
                      (s.uv_max[1]-s.uv_min[1])/65535.);
    }
    face.vertices.push_back(v);
  }
  // Some real SL mesh assets contain degenerate triangles (including
  // repeated indices). Discard those faces rather than losing the whole
  // object; malformed out-of-range indices still invalidate the asset.
  for(size_t i=0;i<s.indices.size();i+=6) {
    const uint16_t a=le16(s.indices,i);
    const uint16_t b=le16(s.indices,i+2);
    const uint16_t c=le16(s.indices,i+4);
    if(a>=count||b>=count||c>=count)return false;
    if(a==b||a==c||b==c)continue;
    const auto& va=face.vertices[a];
    const auto& vb=face.vertices[b];
    const auto& vc=face.vertices[c];
    const double abx=double(vb.x)-va.x,aby=double(vb.y)-va.y,abz=double(vb.z)-va.z;
    const double acx=double(vc.x)-va.x,acy=double(vc.y)-va.y,acz=double(vc.z)-va.z;
    const double cx=aby*acz-abz*acy,cy=abz*acx-abx*acz,cz=abx*acy-aby*acx;
    if(cx*cx+cy*cy+cz*cz<1e-20)continue;
    // SL (X,Y,Z) -> KAMI (X,Z,Y) reverses handedness.
    face.indices.insert(face.indices.end(),{a,c,b});
  }
  if(face.indices.empty())return true; // empty material face, not a fatal asset
  if(!validate_mesh(face))return false;
  out=std::move(face);
  return true;
}
}
static bool decode_mesh_lod_block(const std::vector<uint8_t>& asset,
                                  const MeshAssetHeader& header,
                                  const MeshLodBlock& block,
                                  MeshLodQuality quality,
                                  DecodedMeshGeometry& result) {
  if(!block.present||header.header_bytes>asset.size()||
     block.offset>asset.size()-header.header_bytes||
     block.size>asset.size()-header.header_bytes-block.offset)
    return false;
  const size_t start=header.header_bytes+block.offset;
  std::vector<uint8_t> plain;
  if(!inflate_lod(asset.data()+start,block.size,plain))return false;
  size_t pos=0;
  constexpr char preamble[]="<?llsd/binary?>\n";
  if(plain.size()>=sizeof(preamble)-1 &&
     std::memcmp(plain.data(),preamble,sizeof(preamble)-1)==0)
    pos=sizeof(preamble)-1;
  Reader r{plain.data(),plain.size(),pos};
  uint8_t tag=0;uint32_t faces=0;
  if(!r.next(tag)||tag!='['||!r.be32(faces)||faces>45||faces==0)
    return false;
  DecodedMeshGeometry geometry;
  geometry.faces.reserve(faces);
  for(unsigned n=0;n<faces;++n) {
    Submesh source;
    if(!unpack_submesh(r,source))return false;
    TerrainMesh converted;
    if(!build_face(source,converted))return false;
    if(geometry.vertices>200000-converted.vertices.size()||
       geometry.triangles>150000-converted.indices.size()/3)return false;
    geometry.vertices+=converted.vertices.size();
    geometry.triangles+=converted.indices.size()/3;
    geometry.faces.push_back(std::move(converted));
  }
  if(!r.next(tag)||tag!=']'||r.pos!=r.size||geometry.vertices==0)
    return false;
  geometry.quality=quality;
  result=std::move(geometry);
  return true;
}
bool decode_mesh_high_lod(const std::vector<uint8_t>& asset,
                          const MeshAssetHeader& header,
                          DecodedMeshGeometry& result) {
  return decode_mesh_lod_block(asset,header,header.high,MeshLodQuality::High,result);
}
bool decode_mesh_best_lod(const std::vector<uint8_t>& asset,
                          const MeshAssetHeader& header,
                          DecodedMeshGeometry& result) {
  if(decode_mesh_lod_block(asset,header,header.high,MeshLodQuality::High,result))
    return true;
  if(decode_mesh_lod_block(asset,header,header.medium,MeshLodQuality::Medium,result))
    return true;
  if(decode_mesh_lod_block(asset,header,header.low,MeshLodQuality::Low,result))
    return true;
  return decode_mesh_lod_block(asset,header,header.lowest,MeshLodQuality::Lowest,result);
}
}
