#include "dx12.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <cstdint>
#pragma comment(lib, "d3dcompiler.lib")
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
  viewport = {0.0f, 0.0f, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0.0f, 1.0f};
  scissor = {0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
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
  // Original HLSL: vertex-colored geometry, compiled during initialization.
  const char* shader = R"(
    struct VSInput { float3 position : POSITION; float3 color : COLOR; };
    struct VSOutput { float4 position : SV_POSITION; float3 color : COLOR; };
    VSOutput VSMain(VSInput input) {
      VSOutput output;
      // World coordinates: X across terrain, Y elevation, Z distance.
      // Fixed perspective camera above the terrain, looking toward +Z.
      float3 p = input.position - float3(0.0, 5.0, -10.0);
      float depth = p.z + p.y * 0.22;
      float height = p.y - p.z * 0.22;
      float aspect = 1280.0 / 720.0;
      output.position = float4(p.x / aspect, height, depth * 1.001 - 0.1, depth);
      output.color = input.color;
      return output;
    }
    float4 PSMain(VSOutput input) : SV_TARGET {
      return float4(input.color, 1.0);
    }
  )";
  Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errors;
  ok(D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vs, &errors));
  errors.Reset();
  ok(D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &ps, &errors));
  D3D12_ROOT_SIGNATURE_DESC root_desc{};
  root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  Microsoft::WRL::ComPtr<ID3DBlob> root_blob;
  errors.Reset();
  ok(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, &errors));
  ok(device->CreateRootSignature(0, root_blob->GetBufferPointer(), root_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
  D3D12_INPUT_ELEMENT_DESC inputs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_signature.Get();
  pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pso.InputLayout = {inputs, 2};
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState.DepthClipEnable = TRUE;
  pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pso.SampleMask = UINT_MAX;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pso.SampleDesc.Count = 1;
  pso.DepthStencilState.DepthEnable = TRUE;
  pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  ok(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline)));
  struct Vertex { float x, y, z, r, g, b; };
  // Procedural terrain is a geometry pipeline milestone, NOT Second Life land.
  constexpr int side = 33;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(side * side);
  for (int z = 0; z < side; ++z) {
    for (int x = 0; x < side; ++x) {
      float px = (x - 16) * 1.25f;
      float pz = z * 1.25f + 2.0f;
      float y = 1.2f * std::sin(px * 0.23f) * std::cos(pz * 0.19f);
      float green = 0.28f + 0.15f * (y + 1.2f) / 2.4f;
      vertices.push_back({px, y, pz, 0.10f, green, 0.24f});
    }
  }
  for (int z = 0; z < side - 1; ++z) {
    for (int x = 0; x < side - 1; ++x) {
      uint32_t i = static_cast<uint32_t>(z * side + x);
      indices.insert(indices.end(), {i, i + side, i + 1, i + 1, i + side, i + side + 1});
    }
  }
  index_count = static_cast<UINT>(indices.size());
  D3D12_HEAP_PROPERTIES upload{};
  upload.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC resource{};
  resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource.Width = vertices.size() * sizeof(Vertex);
  resource.Height = 1;
  resource.DepthOrArraySize = 1;
  resource.MipLevels = 1;
  resource.SampleDesc.Count = 1;
  resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  ok(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &resource,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffer)));
  void* mapped = nullptr;
  D3D12_RANGE read_range{0, 0};
  ok(vertex_buffer->Map(0, &read_range, &mapped));
  std::memcpy(mapped, vertices.data(), static_cast<size_t>(resource.Width));
  vertex_buffer->Unmap(0, nullptr);
  vertex_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
  vertex_view.SizeInBytes = static_cast<UINT>(resource.Width);
  vertex_view.StrideInBytes = sizeof(Vertex);
  resource.Width = indices.size() * sizeof(uint32_t);
  ok(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &resource,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&index_buffer)));
  ok(index_buffer->Map(0, &read_range, &mapped));
  std::memcpy(mapped, indices.data(), static_cast<size_t>(resource.Width));
  index_buffer->Unmap(0, nullptr);
  index_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
  index_view.SizeInBytes = static_cast<UINT>(resource.Width);
  index_view.Format = DXGI_FORMAT_R32_UINT;
  D3D12_DESCRIPTOR_HEAP_DESC dsv_desc{};
  dsv_desc.NumDescriptors = 1;
  dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  ok(device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&dsv_heap)));
  D3D12_HEAP_PROPERTIES default_heap{};
  default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC depth_desc{};
  depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depth_desc.Width = desc.Width;
  depth_desc.Height = desc.Height;
  depth_desc.DepthOrArraySize = 1;
  depth_desc.MipLevels = 1;
  depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
  depth_desc.SampleDesc.Count = 1;
  depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  D3D12_CLEAR_VALUE depth_clear{};
  depth_clear.Format = DXGI_FORMAT_D32_FLOAT;
  depth_clear.DepthStencil.Depth = 1.0f;
  ok(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE,
    &depth_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_clear,
    IID_PPV_ARGS(&depth_buffer)));
  device->CreateDepthStencilView(depth_buffer.Get(), nullptr,
    dsv_heap->GetCPUDescriptorHandleForHeapStart());
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
  auto depth_handle = dsv_heap->GetCPUDescriptorHandleForHeapStart();
  commands->ClearDepthStencilView(depth_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  commands->OMSetRenderTargets(1, &cpu, FALSE, &depth_handle);
  commands->RSSetViewports(1, &viewport);
  commands->RSSetScissorRects(1, &scissor);
  commands->SetGraphicsRootSignature(root_signature.Get());
  commands->SetPipelineState(pipeline.Get());
  commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commands->IASetVertexBuffers(0, 1, &vertex_view);
  commands->IASetIndexBuffer(&index_view);
  commands->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
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
