#include "dx12_shader.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <cassert>
#include <cmath>
#include <cstring>
int main() {
  // Verify exactly the HLSL source embedded into the shipping executable.
  assert(std::strstr(kami::terrain_dx12_shader,
    "mul(float4(input.position, 1.0), view_projection)") != nullptr);
  assert(std::strstr(kami::terrain_dx12_shader,
    "atlas_texture.Sample(atlas_sampler, atlas_uv)") != nullptr);
  ID3DBlob* vs=nullptr;
  ID3DBlob* ps=nullptr;
  ID3DBlob* errors=nullptr;
  HRESULT result=D3DCompile(kami::terrain_dx12_shader,
    std::strlen(kami::terrain_dx12_shader),nullptr,nullptr,nullptr,
    "VSMain","vs_5_0",0,0,&vs,&errors);
  if (errors) { errors->Release(); errors=nullptr; }
  assert(SUCCEEDED(result) && vs);
  vs->Release();
  result=D3DCompile(kami::terrain_dx12_shader,
    std::strlen(kami::terrain_dx12_shader),nullptr,nullptr,nullptr,
    "PSMain","ps_5_0",0,0,&ps,&errors);
  if (errors) { errors->Release(); errors=nullptr; }
  assert(SUCCEEDED(result) && ps);
  ps->Release();

  using namespace DirectX;
  const XMVECTOR eye=XMVectorSet(0.0f,146.0f,-108.0f,1.0f);
  const XMVECTOR target=XMVectorSet(0.0f,25.0f,110.0f,1.0f);
  const XMVECTOR forward=XMVector3Normalize(XMVectorSubtract(target,eye));
  const XMMATRIX view=XMMatrixLookToLH(eye,forward,XMVectorSet(0,1,0,0));
  const XMMATRIX proj=XMMatrixPerspectiveFovLH(
    XMConvertToRadians(75.0f),16.0f/9.0f,0.1f,500.0f);
  const XMMATRIX view_projection=view*proj;
  XMFLOAT4X4 cpu_constants{};
  XMStoreFloat4x4(&cpu_constants,XMMatrixTranspose(view_projection));
  const float* shader_constants=&cpu_constants._11;
  XMFLOAT4 world_point{0.0f,25.0f,110.0f,1.0f};
  XMFLOAT4 expected{};
  XMStoreFloat4(&expected,
    XMVector4Transform(XMLoadFloat4(&world_point),view_projection));
  const float original[4]={world_point.x,world_point.y,world_point.z,world_point.w};
  float actual[4]{};
  // HLSL receives column-major matrix constants; mul(v, M) uses
  // column-major packed rows provided by CPU's transpose.
  for (int c=0;c<4;++c)
    for (int r=0;r<4;++r)
      actual[c]+=original[r]*shader_constants[4*c+r];
  const float expected_values[4]={expected.x,expected.y,expected.z,expected.w};
  for (int i=0;i<4;++i) assert(std::fabs(actual[i]-expected_values[i])<0.01f);
  // The mesh center must be projected in front of the camera, not
  // stretched toward a false perspective vanishing point.
  assert(actual[3]>0);
  assert(std::fabs(actual[0])<actual[3]*0.01f);
  assert(std::fabs(actual[1])<actual[3]*0.01f);
  return 0;
}
