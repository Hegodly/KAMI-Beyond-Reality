#include "texture_decode_wic.h"
#include "texture_asset_queue.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
namespace kami::world {
bool decode_image_wic(const uint8_t* bytes,size_t count,DecodedImage& output) {
  if(!bytes||count==0||count>TextureAssetQueue::max_asset_bytes)return false;
  HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE)return false;
  const bool cleanup=SUCCEEDED(apartment);
  bool succeeded=false;
  do {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,
      CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))break;
    Microsoft::WRL::ComPtr<IWICStream> stream;
    if(FAILED(factory->CreateStream(&stream)))break;
    if(FAILED(stream->InitializeFromMemory(
      const_cast<BYTE*>(bytes),static_cast<DWORD>(count))))break;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory->CreateDecoderFromStream(
      stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)))break;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(decoder->GetFrame(0,&frame)))break;
    UINT width=0,height=0;
    if(FAILED(frame->GetSize(&width,&height))||
       width==0||height==0||width>2048||height>2048)break;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter)))break;
    if(FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,
      WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom)))break;
    DecodedImage image;
    image.width=width;image.height=height;
    image.rgba.resize(size_t(width)*height*4);
    if(FAILED(converter->CopyPixels(nullptr,width*4,
      static_cast<UINT>(image.rgba.size()),image.rgba.data())))break;
    if(!image.valid())break;
    output=std::move(image);
    succeeded=true;
  }while(false);
  if(cleanup)CoUninitialize();
  return succeeded;
}
bool decode_jpeg2000_wic(const std::vector<uint8_t>& encoded,DecodedImage& output) {
  return looks_like_jpeg2000(encoded) &&
    decode_image_wic(encoded.data(),encoded.size(),output);
}
}
