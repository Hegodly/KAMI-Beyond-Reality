#include "dx12.h"
#include <windows.h>
#include <exception>
#include <vector>
namespace {
kami::Dx12* active_renderer = nullptr;
HWND login_panel = nullptr;
HWND username_input = nullptr;
HWND status_text = nullptr;
std::vector<HWND> ui_controls;
HFONT ui_font = nullptr;
HBRUSH dark_brush = nullptr;
constexpr int LOGIN_BUTTON = 1001;
constexpr int DEMO_BUTTON = 1002;
void show_demo(HWND window) {
  for (HWND c : ui_controls) ShowWindow(c, SW_HIDE);
  if (login_panel) ShowWindow(login_panel, SW_HIDE);
  if (active_renderer) SetFocus(window);
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
  dark_brush = CreateSolidBrush(RGB(20, 24, 35));
  ui_font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  login_panel = CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"",
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 48, 48, 650, 380,
    window, nullptr, instance, nullptr);
  auto add = [&](const wchar_t* cls, const wchar_t* caption, DWORD flags,
                 int x, int y, int w, int h, int id = 0) {
    HWND c = CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | flags,
      x, y, w, h, login_panel,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (c) {
      SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      ui_controls.push_back(c);
    }
    return c;
  };
  add(L"STATIC", L"KAMI  /  BEYOND REALITY", SS_LEFT, 24, 24, 590, 40);
  add(L"STATIC", L"SECOND LIFE  -  DEVELOPMENT PREVIEW", SS_LEFT, 24, 76, 590, 30);
  add(L"STATIC", L"Avatar username", SS_LEFT, 24, 130, 520, 26);
  username_input = add(L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
    24, 163, 550, 36);
  add(L"BUTTON", L"Connect (coming soon)", WS_TABSTOP | BS_PUSHBUTTON,
    24, 222, 265, 42, LOGIN_BUTTON);
  add(L"BUTTON", L"Explore DX12 demo", WS_TABSTOP | BS_PUSHBUTTON,
    303, 222, 271, 42, DEMO_BUTTON);
  status_text = add(L"STATIC",
    L"Authentication is not connected. Do not enter a password.",
    SS_LEFT, 24, 294, 580, 60);
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
      if (IsIconic(window)) { WaitMessage(); continue; }
      renderer.frame();
    }
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "KAMI DX12 error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
