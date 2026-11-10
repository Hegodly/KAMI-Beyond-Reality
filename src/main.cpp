#include "dx12.h"
#include <windows.h>
#include <exception>
namespace {
kami::Dx12* active_renderer = nullptr;
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  try {
    WNDCLASSW wc{};
    wc.hInstance = inst;
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"KAMIWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"KAMI - Beyond Reality | DX12 bootstrap", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, inst, nullptr);
    if (!window) return 1;
    kami::Dx12 renderer;
    renderer.init(window);
    active_renderer = &renderer;
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
