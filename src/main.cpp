#include "dx12.h"
#include "world/terrain_capture.h"
#include "world/terrain_stream.h"
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
uint64_t live_uploaded_revision = 0;
ULONGLONG live_last_upload_ms = 0;
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
  if (connected_session) connected_session->disconnect();
  connected_session.reset();
  live_terrain.reset();
  live_uploaded_revision = 0;
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
            live_uploaded_revision=0;
            live_last_upload_ms=0;
            renderer.prepare_live_region();
            SetWindowTextW(status_text,L"Authenticated. Waiting for simulator terrain packets...");
          }
        }
      }
      if (connected_session && live_terrain) {
        connected_session->tick();
        connected_session->poll([&](const kami::sl::Packet& packet,
                                    const kami::sl::MessageId&) {
          live_terrain->ingest_packet(packet);
        },128);
        const ULONGLONG time=GetTickCount64();
        if (live_terrain->revision()!=live_uploaded_revision &&
            time-live_last_upload_ms>=200 &&
            renderer.set_region_terrain(live_terrain->terrain())) {
          live_uploaded_revision=live_terrain->revision();
          live_last_upload_ms=time;
          show_demo(window);
        }
      }
      if (IsIconic(window)) { WaitMessage(); continue; }
      renderer.frame();
    }
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "KAMI DX12 error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
