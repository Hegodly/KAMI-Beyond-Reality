#include "texture_decode_openjpeg.h"
#include "texture_asset_queue.h"
#include <openjpeg.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
namespace kami::world {
namespace {
struct Input {
  const uint8_t* bytes=nullptr;
  size_t size=0,pos=0;
};
OPJ_SIZE_T read_mem(void* dest,OPJ_SIZE_T count,void* context) {
  auto* state=static_cast<Input*>(context);
  if(!state||!dest||state->pos>=state->size)return static_cast<OPJ_SIZE_T>(-1);
  const size_t length=std::min<size_t>(count,state->size-state->pos);
  std::memcpy(dest,state->bytes+state->pos,length);
  state->pos+=length;
  return length;
}
OPJ_OFF_T skip_mem(OPJ_OFF_T count,void* context) {
  auto* state=static_cast<Input*>(context);
  if(!state||count<0||uint64_t(count)>state->size-state->pos)return -1;
  state->pos+=size_t(count);
  return count;
}
OPJ_BOOL seek_mem(OPJ_OFF_T offset,void* context) {
  auto* state=static_cast<Input*>(context);
  if(!state||offset<0||uint64_t(offset)>state->size)return OPJ_FALSE;
  state->pos=size_t(offset);
  return OPJ_TRUE;
}
void silent(const char*,void*){}
uint8_t channel(const opj_image_comp_t& component,size_t pixel) {
  int64_t value=component.data[pixel];
  if(component.sgnd)value+=(int64_t(1)<<(component.prec-1));
  const int64_t range=(int64_t(1)<<component.prec)-1;
  value=std::clamp<int64_t>(value,0,range);
  return static_cast<uint8_t>((value*255+range/2)/range);
}
}
bool decode_jpeg2000_openjpeg(const std::vector<uint8_t>& bytes,
                              DecodedImage& output) {
  if(!looks_like_jpeg2000(bytes))return false;
  const OPJ_CODEC_FORMAT format=
    bytes[0]==0xff?OPJ_CODEC_J2K:OPJ_CODEC_JP2;
  using CodecPtr=std::unique_ptr<opj_codec_t,decltype(&opj_destroy_codec)>;
  using StreamPtr=std::unique_ptr<opj_stream_t,decltype(&opj_stream_destroy)>;
  using ImagePtr=std::unique_ptr<opj_image_t,decltype(&opj_image_destroy)>;
  CodecPtr codec(opj_create_decompress(format),&opj_destroy_codec);
  if(!codec)return false;
  opj_set_error_handler(codec.get(),silent,nullptr);
  opj_set_warning_handler(codec.get(),silent,nullptr);
  opj_dparameters_t parameters{};
  opj_set_default_decoder_parameters(&parameters);
  if(!opj_setup_decoder(codec.get(),&parameters))return false;
  Input input{bytes.data(),bytes.size(),0};
  StreamPtr stream(opj_stream_create(8192,OPJ_TRUE),&opj_stream_destroy);
  if(!stream)return false;
  opj_stream_set_user_data(stream.get(),&input,nullptr);
  opj_stream_set_user_data_length(stream.get(),bytes.size());
  opj_stream_set_read_function(stream.get(),read_mem);
  opj_stream_set_skip_function(stream.get(),skip_mem);
  opj_stream_set_seek_function(stream.get(),seek_mem);
  opj_image_t* raw=nullptr;
  if(!opj_read_header(stream.get(),codec.get(),&raw))return false;
  ImagePtr image(raw,&opj_image_destroy);
  if(!image||image->x1<=image->x0||image->y1<=image->y0)return false;
  const uint32_t width=image->x1-image->x0,height=image->y1-image->y0;
  if(width==0||height==0||width>2048||height>2048||
     image->numcomps<1||image->numcomps>4||
     image->color_space==OPJ_CLRSPC_SYCC)return false;
  for(OPJ_UINT32 i=0;i<image->numcomps;++i) {
    const auto& c=image->comps[i];
    if(c.w!=width||c.h!=height||c.dx!=1||c.dy!=1||
       c.prec<1||c.prec>16)return false;
  }
  if(!opj_decode(codec.get(),stream.get(),image.get())||
     !opj_end_decompress(codec.get(),stream.get()))return false;
  for(OPJ_UINT32 i=0;i<image->numcomps;++i)
    if(!image->comps[i].data)return false;
  DecodedImage decoded;
  decoded.width=width;decoded.height=height;
  decoded.rgba.resize(size_t(width)*height*4);
  const size_t count=size_t(width)*height;
  for(size_t i=0;i<count;++i) {
    const uint8_t red=channel(image->comps[0],i);
    uint8_t green=red,blue=red,alpha=255;
    if(image->numcomps>=3) {
      green=channel(image->comps[1],i);
      blue=channel(image->comps[2],i);
      if(image->numcomps==4)alpha=channel(image->comps[3],i);
    } else if(image->numcomps==2) {
      alpha=channel(image->comps[1],i);
    }
    decoded.rgba[4*i]=red;
    decoded.rgba[4*i+1]=green;
    decoded.rgba[4*i+2]=blue;
    decoded.rgba[4*i+3]=alpha;
  }
  if(!decoded.valid())return false;
  output=std::move(decoded);
  return true;
}
}
