#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "world/terrain.h"
namespace kami {
class Dx12 {
public:
  void init(HWND hwnd);
  void frame();
  void key(UINT key, bool pressed);
  // Uploads a decoded world mesh to the existing DX12 draw path.
  // Call from the render thread, after init().
  bool set_world_mesh(const world::TerrainMesh& mesh);
  bool set_region_terrain(const world::TerrainAssembler& terrain);
  bool showing_recorded_terrain() const { return recorded_terrain; }
  ~Dx12();
private:
  void sync();
  void resize(UINT width, UINT height);
  void create_depth(UINT width, UINT height);
  static constexpr UINT count = 2;
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
  Microsoft::WRL::ComPtr<ID3D12Resource> back[count];
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  HANDLE event_handle = nullptr;
  UINT frame_index = 0;
  UINT rtv_step = 0;
  UINT64 fence_value = 0;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
  Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
  Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
  Microsoft::WRL::ComPtr<ID3D12Resource> depth_buffer;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap;
  D3D12_INDEX_BUFFER_VIEW index_view{};
  UINT index_count = 0;
  D3D12_VERTEX_BUFFER_VIEW vertex_view{};
  D3D12_VIEWPORT viewport{};
  D3D12_RECT scissor{};
  float camera_x = 0.0f, camera_y = 5.0f, camera_z = -10.0f;
  float yaw = 0.0f, pitch = -0.22f;
  ULONGLONG last_tick = 0;
  POINT previous_mouse{};
  bool mouse_tracking = false;
  bool keys[256]{};
  HWND window_handle = nullptr;
  bool recorded_terrain = false;
};
}
