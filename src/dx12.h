#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
namespace kami {
class Dx12 {
public:
  void init(HWND hwnd);
  void frame();
  ~Dx12();
private:
  void sync();
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
};
}
