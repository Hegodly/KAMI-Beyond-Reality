#include "dx12.h"
#include <stdexcept>
namespace {
void ok(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D12 operation failed"); }
}
namespace kami {
void Dx12::init(HWND hwnd) {
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
  ok(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
  ok(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  D3D12_COMMAND_QUEUE_DESC q{};
  q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  ok(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)));
  RECT rc{};
  GetClientRect(hwnd, &rc);
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = static_cast<UINT>(rc.right - rc.left);
  desc.Height = static_cast<UINT>(rc.bottom - rc.top);
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = count;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> base;
  ok(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &desc, nullptr, nullptr, &base));
  ok(base.As(&swap));
  frame_index = swap->GetCurrentBackBufferIndex();
  D3D12_DESCRIPTOR_HEAP_DESC hd{};
  hd.NumDescriptors = count;
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  ok(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
  rtv_step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < count; ++i) {
    ok(swap->GetBuffer(i, IID_PPV_ARGS(&back[i])));
    device->CreateRenderTargetView(back[i].Get(), nullptr, cpu);
    cpu.ptr += rtv_step;
  }
  ok(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  ok(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)));
  ok(commands->Close());
  ok(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!event_handle) throw std::runtime_error("CreateEvent failed");
}
void Dx12::sync() {
  if (!queue || !fence) return;
  auto value = ++fence_value;
  ok(queue->Signal(fence.Get(), value));
  if (fence->GetCompletedValue() < value) {
    ok(fence->SetEventOnCompletion(value, event_handle));
    WaitForSingleObject(event_handle, INFINITE);
  }
}
void Dx12::frame() {
  sync(); // One in-flight frame: simple, safe foundation; optimize later.
  ok(allocator->Reset());
  ok(commands->Reset(allocator.Get(), nullptr));
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = back[frame_index].Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commands->ResourceBarrier(1, &barrier);
  auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
  cpu.ptr += frame_index * rtv_step;
  const float clear[] = {0.015f, 0.025f, 0.045f, 1.0f};
  commands->ClearRenderTargetView(cpu, clear, 0, nullptr);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  commands->ResourceBarrier(1, &barrier);
  ok(commands->Close());
  ID3D12CommandList* lists[] = {commands.Get()};
  queue->ExecuteCommandLists(1, lists);
  ok(swap->Present(1, 0));
  frame_index = swap->GetCurrentBackBufferIndex();
}
Dx12::~Dx12() {
  if (queue && fence && event_handle) sync();
  if (event_handle) CloseHandle(event_handle);
}
}
