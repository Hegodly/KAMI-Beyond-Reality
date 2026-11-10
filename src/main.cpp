#include "dx12.h"
#include "world/terrain_capture.h"
#include "world/terrain_stream.h"
#include "world/scene_objects.h"
#include "world/asset_caps.h"
#include "world/login_protocol.h"
#include "world/simulator_session.h"
#include <future>
#include <chrono>
#include <memory>
#include <algorithm>
#include <string>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <exception>
#include <vector>
namespace {
kami::Dx12* active_renderer = nullptr;
HWND login_panel = nullptr;
HWND username_input = nullptr;
HWND password_input = nullptr;
HWND mfa_input = nullptr;
std::future<kami::sl::LoginResult> login_future;
std::unique_ptr<kami::sl::SimulatorSession> connected_session;
std::unique_ptr<kami::world::TerrainStream> live_terrain;
std::unique_ptr<kami::world::SceneObjects> live_objects;
kami::world::TextureAssetQueue live_texture_assets;
uint64_t last_texture_catalog_revision=0;
struct CapabilityReply {
  uint64_t generation=0;
  kami::world::ViewerAssetCapabilities cap;
  bool success=false;
};
struct TextureDownload {
  uint64_t generation=0;
  kami::world::AssetId id{};
  std::vector<uint8_t> bytes;
  bool success=false;
};
std::future<CapabilityReply> cap_future;
std::future<TextureDownload> texture_future;
std::string pending_seed_cap;
std::string live_viewerasset_cap;
bool caps_discovery_failed=false;
uint64_t world_generation=0;
ULONGLONG last_texture_request_ms=0;
kami::world::TerrainOnlyCapture live_land_capture;
uint64_t live_uploaded_revision = 0;
uint64_t live_uploaded_object_revision = 0;
ULONGLONG live_last_object_request_ms = 0;
ULONGLONG live_last_upload_ms = 0;
ULONGLONG live_last_status_ms = 0;
HWND status_text = nullptr;
std::vector<HWND> ui_controls;
HFONT ui_font = nullptr;
HBRUSH dark_brush = nullptr;
constexpr int LOGIN_BUTTON = 1001;
constexpr int DEMO_BUTTON = 1002;
constexpr int CAPTURE_BUTTON = 1003;
std::wstring control_text(HWND control, int limit) {
  const int n = GetWindowTextLengthW(control);
  if (n<1 || n>limit) return {};
  std::wstring buffer(size_t(n)+1,L'\0');
  const int copied = GetWindowTextW(control,buffer.data(),n+1);
  buffer.resize(static_cast<size_t>(std::max(0,copied)));
  return buffer;
}
std::string to_utf8(const std::wstring& input) {
  if (input.empty()) return {};
  const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),
    static_cast<int>(input.size()),nullptr,0,nullptr,nullptr);
  if (n<=0) return {};
  std::string output(size_t(n),'\0');
  if (WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),
    static_cast<int>(input.size()),output.data(),n,nullptr,nullptr)!=n) return {};
  return output;
}
void start_login() {
  if (login_future.valid()) {
    SetWindowTextW(status_text,L"Login is already in progress.");
    return;
  }
  std::wstring name=control_text(username_input,100);
  std::wstring secret=control_text(password_input,1024);
  SetWindowTextW(password_input,L"");
  kami::sl::LoginCredentials credentials;
  credentials.username=to_utf8(name);
  credentials.password=to_utf8(secret);
  credentials.token=to_utf8(control_text(mfa_input,6));
  SetWindowTextW(mfa_input,L"");
  if (!secret.empty()) SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));
  std::string first,last;
  if (!kami::sl::split_avatar_name(credentials.username,first,last) ||
      credentials.password.empty()) {
    SetWindowTextW(status_text,L"Enter a valid avatar name and password.");
    std::fill(credentials.password.begin(),credentials.password.end(),'\0');
    return;
  }
  ++world_generation;
  pending_seed_cap.clear();
  live_viewerasset_cap.clear();
  caps_discovery_failed=false;
  last_texture_request_ms=0;
  if (connected_session) connected_session->disconnect();
  connected_session.reset();
  live_terrain.reset();
  live_objects.reset();
  live_texture_assets.clear();
  last_texture_catalog_revision=0;
  live_land_capture.clear();
  live_uploaded_revision = 0;
  live_uploaded_object_revision = 0;
  live_last_object_request_ms = 0;
  live_last_upload_ms = 0;
  SetWindowTextW(status_text,L"Connecting securely to the official Second Life login server...");
  login_future=std::async(std::launch::async,
    [credentials=std::move(credentials)]() mutable {
      auto result=kami::sl::login_to_second_life(credentials);
      std::fill(credentials.password.begin(),credentials.password.end(),'\0');
      std::fill(credentials.token.begin(),credentials.token.end(),'\0');
      return result;
    });
}

void save_land_capture(HWND window) {
  if (live_land_capture.datagrams.empty()) {
    MessageBoxW(window,L"No land packets have arrived yet. Log in and let the terrain load before pressing F8.",
      L"KAMI - Terrain-only diagnostics",MB_OK|MB_ICONINFORMATION);
    return;
  }
  wchar_t filename[32768]=L"KAMI-land-debug.kudp";
  static constexpr wchar_t filter[]=L"KAMI terrain captures (*.kudp)\0*.kudp\0";
  OPENFILENAMEW dialog{};
  dialog.lStructSize=sizeof(dialog);
  dialog.hwndOwner=window;
  dialog.lpstrFilter=filter;
  dialog.lpstrFile=filename;
  dialog.nMaxFile=static_cast<DWORD>(sizeof(filename)/sizeof(filename[0]));
  dialog.lpstrDefExt=L"kudp";
  dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
  if (!GetSaveFileNameW(&dialog)) return;
  std::ofstream file(std::filesystem::path(filename),
                     std::ios::binary|std::ios::trunc);
  if (!file || !kami::world::write_land_capture(file,live_land_capture)) {
    MessageBoxW(window,L"Failed to save the terrain-only capture. Check your destination and available space.",
      L"KAMI",MB_OK|MB_ICONERROR);
    return;
  }
  MessageBoxW(window,
    L"Saved only simulator land-height packets, not credentials, avatar, chat, or inventory data.\n"
    L"The file still contains your region heightmap. Share it only if you are comfortable doing so.",
    L"KAMI - Terrain capture saved",MB_OK|MB_ICONINFORMATION);
}
void show_demo(HWND window) {
  for (HWND c : ui_controls) ShowWindow(c, SW_HIDE);
  if (login_panel) ShowWindow(login_panel, SW_HIDE);
  if (active_renderer) SetFocus(window);
}
void open_capture(HWND window) {
  wchar_t filename[32768]{};
  static constexpr wchar_t filter[] = L"KAMI terrain captures (*.kudp)\0*.kudp\0All files (*.*)\0*.*\0";
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = window;
  dialog.lpstrFilter = filter;
  dialog.lpstrFile = filename;
  dialog.nMaxFile = static_cast<DWORD>(sizeof(filename)/sizeof(filename[0]));
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&dialog)) return;
  std::ifstream capture(std::filesystem::path(filename), std::ios::binary);
  kami::world::TerrainStream replay(256, 256);
  kami::world::CaptureSummary summary;
  if (!capture || !kami::world::replay_capture(capture, replay, summary)) {
    SetWindowTextW(status_text,
      L"Invalid or empty KAMIUDP1 capture. No world geometry was replaced.");
    return;
  }
  if (active_renderer) active_renderer->prepare_recorded_region();
  if (!active_renderer || !active_renderer->set_region_terrain(replay.terrain())) {
    SetWindowTextW(status_text, L"Decoded terrain could not be uploaded to DX12.");
    return;
  }
  show_demo(window);
}
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_CTLCOLORSTATIC) {
    SetTextColor(reinterpret_cast<HDC>(wp), RGB(234, 238, 250));
    SetBkColor(reinterpret_cast<HDC>(wp), RGB(20, 24, 35));
    return reinterpret_cast<LRESULT>(dark_brush);
  }
  if (msg == WM_ERASEBKGND && dark_brush) {
    RECT rect{}; GetClientRect(hwnd, &rect);
    FillRect(reinterpret_cast<HDC>(wp), &rect, dark_brush);
    return 1;
  }
  if (msg == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
    if (LOWORD(wp) == DEMO_BUTTON) { show_demo(hwnd); return 0; }
    if (LOWORD(wp) == CAPTURE_BUTTON) { open_capture(hwnd); return 0; }
    if (LOWORD(wp) == LOGIN_BUTTON) {
      start_login();
      return 0;
    }
  }
  if (msg == WM_KEYDOWN && wp == VK_F8) {
    save_land_capture(hwnd);
    return 0;
  }
  if (active_renderer && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
                          msg == WM_KEYUP || msg == WM_SYSKEYUP)) {
    active_renderer->key(static_cast<UINT>(wp), msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
  }
  if (msg == WM_KILLFOCUS && active_renderer) {
    for (UINT key = 0; key < 256; ++key) active_renderer->key(key, false);
  }
  if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd, msg, wp, lp);
}
void create_login(HWND window, HINSTANCE instance) {
  dark_brush = CreateSolidBrush(RGB(20, 24, 35));
  ui_font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  login_panel = CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"",
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 48, 48, 650, 590,
    window, nullptr, instance, nullptr);
  auto add = [&](const wchar_t* cls, const wchar_t* caption, DWORD flags,
                 int x, int y, int w, int h, int id = 0) {
    HWND c = CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | flags,
      x + 48, y + 48, w, h, window,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (c) {
      SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      ui_controls.push_back(c);
    }
    return c;
  };
  add(L"STATIC", L"KAMI  /  BEYOND REALITY", SS_LEFT, 24, 24, 590, 40);
  add(L"STATIC", L"SECOND LIFE  -  EXPERIMENTAL LIVE LOGIN / DX12 PREVIEW", SS_LEFT, 24, 76, 590, 30);
  add(L"STATIC", L"Avatar username", SS_LEFT, 24, 130, 520, 26);
  username_input = add(L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
    24, 163, 550, 36);
  add(L"STATIC", L"Second Life password (never saved)", SS_LEFT,24,210,550,26);
  password_input = add(L"EDIT",L"",WS_TABSTOP | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
    24,242,550,36);
  add(L"STATIC",L"6-digit MFA code (if requested)",SS_LEFT,24,281,550,26);
  mfa_input=add(L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,24,309,550,36);
  add(L"BUTTON", L"Connect to Second Life", WS_TABSTOP | BS_PUSHBUTTON,
    24, 361, 265, 42, LOGIN_BUTTON);
  add(L"BUTTON", L"Explore DX12 demo", WS_TABSTOP | BS_PUSHBUTTON,
    303, 361, 271, 42, DEMO_BUTTON);
  add(L"BUTTON", L"Load recorded terrain (.kudp)", WS_TABSTOP | BS_PUSHBUTTON,
    24, 414, 550, 42, CAPTURE_BUTTON);
  status_text = add(L"STATIC",
    L"Experimental authentication; MFA and full world features are not complete.",
    SS_LEFT, 24, 476, 580, 86);
}
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  try {
    WNDCLASSW wc{};
    wc.hInstance = inst;
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"KAMIWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"KAMI - Beyond Reality | DX12 development", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, inst, nullptr);
    if (!window) return 1;
    kami::Dx12 renderer;
    renderer.init(window);
    active_renderer = &renderer;
    create_login(window, inst);
    ShowWindow(window, show);
    ShowWindow(login_panel, SW_SHOW);
    UpdateWindow(window);
    SetFocus(username_input);
    MSG msg{};
    for (;;) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { active_renderer = nullptr; return static_cast<int>(msg.wParam); }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      if (login_future.valid() &&
          login_future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        kami::sl::LoginResult outcome;
        try {
          outcome=login_future.get();
        } catch (const std::exception&) {
          SetWindowTextW(status_text,
            L"Login could not finish due to an internal client error. No session was started.");
          continue;
        }
        if (!outcome.authenticated) {
          std::string detail=outcome.error.empty() ? "Second Life rejected login without a reason." : outcome.error;
          if (detail.size()>180) detail.resize(180);
          std::wstring wide(detail.begin(),detail.end());
          SetWindowTextW(status_text,wide.c_str());
        } else {
          connected_session=std::make_unique<kami::sl::SimulatorSession>();
          if (!connected_session->connect(outcome.session)) {
            connected_session.reset();
            SetWindowTextW(status_text,L"Login accepted; simulator circuit connection failed.");
          } else {
            live_terrain=std::make_unique<kami::world::TerrainStream>(256,256);
            live_objects=std::make_unique<kami::world::SceneObjects>();
            live_texture_assets.clear();
            last_texture_catalog_revision=0;
            pending_seed_cap=std::move(outcome.seed_capability);
            live_viewerasset_cap.clear();
            caps_discovery_failed=false;
            live_land_capture.clear();
            live_uploaded_object_revision=0;
            live_last_object_request_ms=0;
            live_uploaded_revision=0;
            live_last_upload_ms=0;
            renderer.prepare_live_region();
            SetWindowTextW(status_text,L"Authenticated. Waiting for simulator terrain packets...");
          }
        }
      }
      // Capability discovery and JPEG2000 downloads run outside the frame
      // loop. Generation tags prevent stale responses from an old login from
      // reaching a new region or account. Opaque capability URLs are never
      // displayed or written to logs.
      if(cap_future.valid() &&
         cap_future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        try {
          auto reply=cap_future.get();
          if(reply.generation==world_generation && connected_session && live_objects) {
            if(reply.success) {
              live_viewerasset_cap=std::move(reply.cap.viewer_asset);
              caps_discovery_failed=false;
            } else {
              caps_discovery_failed=true;
            }
          }
        } catch(const std::exception&) {
          // Keep terrain and proxy rendering available if CAPS fail.
          caps_discovery_failed=true;
        }
      }
      if(texture_future.valid() &&
         texture_future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        try {
          auto response=texture_future.get();
          if(response.generation==world_generation && connected_session) {
            if(response.success)
              live_texture_assets.finish(response.id,response.bytes);
            else
              live_texture_assets.fail(response.id,true);
          }
        } catch(const std::exception&) {
          // A failed background request must not interrupt DX12.
        }
      }
      if(connected_session && live_objects && !pending_seed_cap.empty() &&
         !cap_future.valid()) {
        auto seed=std::move(pending_seed_cap);
        pending_seed_cap.clear();
        const uint64_t current=world_generation;
        cap_future=std::async(std::launch::async,
          [seed=std::move(seed),current]() mutable {
            CapabilityReply reply;
            reply.generation=current;
            reply.success=kami::world::request_viewer_asset_capability(seed,reply.cap);
            return reply;
          });
      }
      if (connected_session && live_terrain && live_objects) {
        connected_session->tick();
        connected_session->poll([&](const kami::sl::Packet& packet,
                                    const kami::sl::MessageId&) {
          kami::world::record_land_packet(live_land_capture,packet);
          live_terrain->ingest_packet(packet);
          live_objects->ingest(packet);
        },128);
        const ULONGLONG time=GetTickCount64();
        if (live_objects->revision()!=last_texture_catalog_revision) {
          live_texture_assets.reconcile(live_objects->referenced_texture_ids());
          last_texture_catalog_revision=live_objects->revision();
        }
        // Start at most one HTTP fetch per frame and per 350 ms.
        // The raw bytes remain in a bounded encoded cache; they are NOT
        // decoded to textures or submitted to the DX12 material pipeline.
        if(!live_viewerasset_cap.empty() && !texture_future.valid() &&
           time-last_texture_request_ms>=350) {
          auto next=live_texture_assets.take_pending(1);
          if(!next.empty()) {
            const auto id=next.front();
            const auto cap=live_viewerasset_cap;
            const uint64_t current=world_generation;
            texture_future=std::async(std::launch::async,
              [current,id,cap]() {
                TextureDownload result;
                result.generation=current;result.id=id;
                try {
                  result.success=kami::world::download_encoded_texture(
                    cap,id,result.bytes);
                } catch(const std::exception&) {
                  result.success=false;
                }
                return result;
              });
            last_texture_request_ms=time;
          }
        }
        // Cache probes require explicit full-object requests. Send bounded
        // batches, leaving the rest queued instead of flooding the simulator.
        if(time-live_last_object_request_ms>=250) {
          auto missing=live_objects->take_cache_misses(32);
          if(!missing.empty() && !connected_session->request_objects(missing))
            live_objects->requeue_cache_misses(missing);
          live_last_object_request_ms=time;
        }
        if ((live_terrain->revision()!=live_uploaded_revision ||
             live_objects->revision()!=live_uploaded_object_revision) &&
            time-live_last_upload_ms>=350 &&
            renderer.set_region_scene(live_terrain->terrain(),live_objects.get())) {
          live_uploaded_revision=live_terrain->revision();
          live_uploaded_object_revision=live_objects->revision();
          live_last_upload_ms=time;
          show_demo(window);
        }
      }
      if (connected_session && live_terrain && live_objects && GetTickCount64()-live_last_status_ms>=1000) {
        const auto& stats=live_terrain->stats();
        const auto& objects=live_objects->stats();
        const auto asset_counts=live_texture_assets.stats();
        wchar_t message[400]{};
        swprintf_s(message,
          L"UDP: %llu | Terrain: %llu decoded | Land: %.1f%%\n"
          L"Scene: %zu prims | %zu color entries | %zu unique texture IDs\n"
          L"Texture staging: %zu queued | %zu fetching | %zu encoded J2K | %zu failed\n"
          L"Object packets: %llu full, %llu compressed | Invalid: %llu",
          static_cast<unsigned long long>(stats.packets),
          static_cast<unsigned long long>(stats.decoded),
          live_terrain->terrain().completion_ratio()*100.0f,
          live_objects->renderable_prims(),
          live_objects->objects_with_face_colors(),
          live_objects->unique_texture_references(),
          asset_counts.queued,asset_counts.in_flight,
          asset_counts.encoded_ready,asset_counts.failed,
          static_cast<unsigned long long>(objects.full_packets),
          static_cast<unsigned long long>(objects.compressed_packets),
          static_cast<unsigned long long>(objects.invalid_packets));
        renderer.set_texture_stage_stats(asset_counts.encoded_ready,
                                         asset_counts.queued,
                                         asset_counts.in_flight);
        renderer.set_asset_cap_state(!live_viewerasset_cap.empty()?2:
          caps_discovery_failed?3:(cap_future.valid() || !pending_seed_cap.empty())?1:0);
        SetWindowTextW(status_text,message);
        live_last_status_ms=GetTickCount64();
      }
      if (IsIconic(window)) { WaitMessage(); continue; }
      renderer.frame();
    }
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "KAMI DX12 error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
