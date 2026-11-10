#include "dx12.h"
#include <windows.h>
#include <exception>
namespace {
kami::Dx12* active_renderer = nullptr;
HWND login_panel = nullptr;
HWND username_input = nullptr;
HWND status_text = nullptr;
constexpr int LOGIN_BUTTON = 1001;
constexpr int DEMO_BUTTON = 1002;
void show_demo(HWND window) {
  if (login_panel) ShowWindow(login_panel, SW_HIDE);
  if (active_renderer) SetFocus(window);
}
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
    if (LOWORD(wp) == DEMO_BUTTON) { show_demo(hwnd); return 0; }
    if (LOWORD(wp) == LOGIN_BUTTON) {
      // Never request or store an SL password before authenticated TLS login
      // and MFA support are implemented.
      SetWindowTextW(status_text,
        L"Second Life authentication is not connected yet. No credentials sent.");
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
  login_panel = CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
    24, 32, 570, 225, window, nullptr, instance, nullptr);
  CreateWindowW(L"STATIC", L"KAMI - Beyond Reality | Second Life",
    WS_CHILD | WS_VISIBLE, 14, 12, 520, 24, login_panel, nullptr, instance, nullptr);
  CreateWindowW(L"STATIC", L"Avatar username:",
    WS_CHILD | WS_VISIBLE, 14, 52, 130, 22, login_panel, nullptr, instance, nullptr);
  username_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
    150, 49, 320, 26, login_panel, nullptr, instance, nullptr);
  CreateWindowW(L"BUTTON", L"Log in (not yet available)",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP, 14, 92, 235, 32,
    window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(LOGIN_BUTTON)), instance, nullptr);
  CreateWindowW(L"BUTTON", L"Explore DX12 demo",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP, 260, 92, 210, 32,
    window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(DEMO_BUTTON)), instance, nullptr);
  status_text = CreateWindowW(L"STATIC",
    L"Login requires secure SL authentication and MFA integration. Do not enter a password.",
    WS_CHILD | WS_VISIBLE, 14, 148, 535, 52, login_panel, nullptr, instance, nullptr);
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
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"KAMI - Beyond Reality | DX12 development", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, inst, nullptr);
    if (!window) return 1;
    kami::Dx12 renderer;
    renderer.init(window);
    active_renderer = &renderer;
    create_login(window, inst);
    ShowWindow(window, show);
    MSG msg{};
    for (;;) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { active_renderer = nullptr; return static_cast<int>(msg.wParam); }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      if (IsIconic(window)) { WaitMessage(); continue; }
      renderer.frame();
    }
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "KAMI DX12 error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
