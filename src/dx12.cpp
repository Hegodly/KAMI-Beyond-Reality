#include "dx12.h"
#include "world/terrain.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <cstdint>
#include <DirectXMath.h>
#include <algorithm>
#include <cstdio>
#pragma comment(lib, "d3dcompiler.lib")
namespace {
void ok(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D12 operation failed"); }
}
namespace kami {
void Dx12::init(HWND hwnd) {
  window_handle = hwnd;
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
    cbuffer Camera : register(b0) { float4x4 view_projection; };
    struct VSInput { float3 position : POSITION; float3 color : COLOR; };
    struct VSOutput {
      float4 position : SV_POSITION;
      float3 color : COLOR;
      float3 world_position : TEXCOORD0;
    };
    VSOutput VSMain(VSInput input) {
      VSOutput output;
      output.position = mul(view_projection, float4(input.position, 1.0));
      output.color = input.color;
      output.world_position = input.position;
      return output;
    }
    float4 PSMain(VSOutput input) : SV_TARGET {
      float3 dpdx = ddx(input.world_position);
      float3 dpdy = ddy(input.world_position);
      float3 normal = normalize(cross(dpdy, dpdx));
      // Two-sided terrain shading uses the normal facing the light hemisphere.
      float3 sun_direction = normalize(float3(0.35, 0.8, -0.4));
      float diffuse = saturate(abs(dot(normal, sun_direction)));
      float lighting = 0.30 + 0.70 * diffuse;
      float distance_fog = saturate((length(input.world_position.xz) - 120.0) / 260.0);
      float3 sky = float3(0.07, 0.12, 0.20);
      float3 lit = input.color * lighting;
      float3 color = lerp(lit, sky, distance_fog * 0.72);
      color = color / (1.0 + color);
      color = pow(saturate(color), 1.0 / 2.2);
      return float4(color, 1.0);
    }
  )";
  Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errors;
  HRESULT vs_result = D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vs, &errors);
  if (FAILED(vs_result)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Vertex shader compilation failed");
  errors.Reset();
  HRESULT ps_result = D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &ps, &errors);
  if (FAILED(ps_result)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Pixel shader compilation failed");
  D3D12_ROOT_SIGNATURE_DESC root_desc{};
  D3D12_ROOT_PARAMETER camera_param{};
  camera_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  camera_param.Constants.ShaderRegister = 0;
  camera_param.Constants.Num32BitValues = 16;
  camera_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  root_desc.NumParameters = 1;
  root_desc.pParameters = &camera_param;
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
  if (!set_world_mesh(world::make_terrain_mesh(world::make_demo_heightfield(129))))
    throw std::runtime_error("Failed to initialize world mesh");
  D3D12_DESCRIPTOR_HEAP_DESC dsv_desc{};
  dsv_desc.NumDescriptors = 1;
  dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  ok(device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&dsv_heap)));
  create_depth(desc.Width, desc.Height);
}
bool Dx12::set_world_mesh(const world::TerrainMesh& mesh) {
  if (!device || mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.vertices.size() > UINT_MAX / sizeof(world::TerrainVertex) ||
      mesh.indices.size() > UINT_MAX / sizeof(uint32_t)) return false;
  for (const auto& v : mesh.vertices) {
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
        !std::isfinite(v.r) || !std::isfinite(v.g) || !std::isfinite(v.b)) return false;
  }
  if (mesh.indices.size() % 3 != 0) return false;
  for (uint32_t i : mesh.indices) if (i >= mesh.vertices.size()) return false;
  sync();
  const auto& vertices = mesh.vertices;
  const auto& indices = mesh.indices;
  const UINT new_index_count = static_cast<UINT>(indices.size());
  D3D12_HEAP_PROPERTIES upload{};
  upload.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC resource{};
  resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource.Width = vertices.size() * sizeof(world::TerrainVertex);
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
  vertex_view.StrideInBytes = sizeof(world::TerrainVertex);
  resource.Width = indices.size() * sizeof(uint32_t);
  ok(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &resource,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&index_buffer)));
  ok(index_buffer->Map(0, &read_range, &mapped));
  std::memcpy(mapped, indices.data(), static_cast<size_t>(resource.Width));
  index_buffer->Unmap(0, nullptr);
  index_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
  index_view.SizeInBytes = static_cast<UINT>(resource.Width);
  index_view.Format = DXGI_FORMAT_R32_UINT;
  index_count = new_index_count;
  return true;
}
void Dx12::create_depth(UINT width, UINT height) {
  depth_buffer.Reset();
  D3D12_HEAP_PROPERTIES default_heap{};
  default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC depth_desc{};
  depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depth_desc.Width = width;
  depth_desc.Height = height;
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
void Dx12::resize(UINT width, UINT height) {
  if (!width || !height || !swap || (viewport.Width == width && viewport.Height == height)) return;
  if (width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    throw std::runtime_error("DX12 viewport exceeds supported dimensions");
  sync();
  for (auto& buffer : back) buffer.Reset();
  depth_buffer.Reset();
  ok(swap->ResizeBuffers(count, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
  frame_index = swap->GetCurrentBackBufferIndex();
  auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < count; ++i) {
    ok(swap->GetBuffer(i, IID_PPV_ARGS(&back[i])));
    device->CreateRenderTargetView(back[i].Get(), nullptr, cpu);
    cpu.ptr += rtv_step;
  }
  viewport.Width = static_cast<float>(width);
  viewport.Height = static_cast<float>(height);
  scissor.right = static_cast<LONG>(width);
  scissor.bottom = static_cast<LONG>(height);
  create_depth(width, height);
}
void Dx12::sync() {
  if (!queue || !fence) return;
  auto value = ++fence_value;
  ok(queue->Signal(fence.Get(), value));
  if (fence->GetCompletedValue() == UINT64_MAX) throw std::runtime_error("DX12 device removed");
  if (fence->GetCompletedValue() < value) {
    ok(fence->SetEventOnCompletion(value, event_handle));
    if (WaitForSingleObject(event_handle, INFINITE) != WAIT_OBJECT_0)
      throw std::runtime_error("DX12 GPU fence wait failed");
  }
}
void Dx12::key(UINT key_code, bool pressed) {
  if (key_code < 256) keys[key_code] = pressed;
}
void Dx12::frame() {
  RECT rect{};
  if (window_handle && GetClientRect(window_handle, &rect)) {
    UINT width = static_cast<UINT>(std::max<LONG>(0, rect.right - rect.left));
    UINT height = static_cast<UINT>(std::max<LONG>(0, rect.bottom - rect.top));
    if (!width || !height) return;
    resize(width, height);
  }
  const ULONGLONG now = GetTickCount64();
  static ULONGLONG fps_start = now;
  static UINT fps_frames = 0;
  static UINT measured_fps = 0;
  ++fps_frames;
  if (now - fps_start >= 1000) {
    measured_fps = static_cast<UINT>(fps_frames * 1000ULL / (now - fps_start));
    fps_frames = 0;
    fps_start = now;
  }
  float dt = last_tick ? std::min(0.05f, static_cast<float>(now - last_tick) * 0.001f) : 0.016f;
  last_tick = now;
  const float speed = ((keys[VK_SHIFT]) ? 24.0f : 10.0f) * dt;
  const float forward_x = std::sin(yaw), forward_z = std::cos(yaw);
  const float right_x = std::cos(yaw), right_z = -std::sin(yaw);
  if (keys['W'] || keys[VK_UP]) { camera_x += forward_x * speed; camera_z += forward_z * speed; }
  if (keys['S'] || keys[VK_DOWN]) { camera_x -= forward_x * speed; camera_z -= forward_z * speed; }
  if (keys['D'] || keys[VK_RIGHT]) { camera_x += right_x * speed; camera_z += right_z * speed; }
  if (keys['A'] || keys[VK_LEFT]) { camera_x -= right_x * speed; camera_z -= right_z * speed; }
  if (keys['E']) camera_y += speed;
  if (keys['Q']) camera_y -= speed;
  static ULONGLONG last_title = 0;
  if (now - last_title > 250 && window_handle) {
    wchar_t title[256]{};
    swprintf_s(title, L"KAMI DX12 | %u FPS | WASD/Arrows move, Shift sprint, Q/E fly | Camera: %.1f, %.1f, %.1f",
               measured_fps, camera_x, camera_y, camera_z);
    SetWindowTextW(window_handle, title);
    last_title = now;
  }
  POINT mouse{};
  if (window_handle && GetForegroundWindow() == window_handle &&
      (GetAsyncKeyState(VK_RBUTTON) & 0x8000) && GetCursorPos(&mouse)) {
    if (mouse_tracking) {
      yaw += static_cast<float>(mouse.x - previous_mouse.x) * 0.003f;
      pitch = std::clamp(pitch - static_cast<float>(mouse.y - previous_mouse.y) * 0.003f, -1.45f, 1.45f);
    }
    previous_mouse = mouse;
    mouse_tracking = true;
  } else mouse_tracking = false;
  using namespace DirectX;
  XMVECTOR eye = XMVectorSet(camera_x, camera_y, camera_z, 1.0f);
  XMVECTOR direction = XMVectorSet(std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                  std::cos(yaw) * std::cos(pitch), 0.0f);
  XMMATRIX view = XMMatrixLookToLH(eye, direction, XMVectorSet(0, 1, 0, 0));
  XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(75.0f),
    viewport.Width / viewport.Height, 0.1f, 500.0f);
  XMFLOAT4X4 matrix{};
  XMStoreFloat4x4(&matrix, XMMatrixTranspose(view * projection));
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
  const float clear[] = {0.07f, 0.12f, 0.20f, 1.0f};
  commands->ClearRenderTargetView(cpu, clear, 0, nullptr);
  auto depth_handle = dsv_heap->GetCPUDescriptorHandleForHeapStart();
  commands->ClearDepthStencilView(depth_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  commands->OMSetRenderTargets(1, &cpu, FALSE, &depth_handle);
  commands->RSSetViewports(1, &viewport);
  commands->RSSetScissorRects(1, &scissor);
  commands->SetGraphicsRootSignature(root_signature.Get());
  commands->SetGraphicsRoot32BitConstants(0, 16, &matrix, 0);
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
  HRESULT present_result = swap->Present(1, 0);
  if (present_result == DXGI_ERROR_DEVICE_REMOVED || present_result == DXGI_ERROR_DEVICE_RESET)
    throw std::runtime_error("DX12 device lost during Present");
  ok(present_result);
  frame_index = swap->GetCurrentBackBufferIndex();
}
Dx12::~Dx12() {
  if (queue && fence && event_handle) sync();
  if (event_handle) CloseHandle(event_handle);
}
}
