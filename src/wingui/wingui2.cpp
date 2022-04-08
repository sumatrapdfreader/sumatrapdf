/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "Layout.h"

// #include "wingui2.h"

#include "utils/Log.h"

// this is experimantal win32 gui wrappers based on
// https://github.com/erengy/windows
// I'm using https://github.com/kjk/windows fork to track progress
// for now everything is in this one file

// definitions, will go into wingui2.h
namespace wg {

}


// implementation
namespace wg {

// window_map.h / window_map.cpp

struct Window;

struct WindowToHwnd {
  Window* window = nullptr;
  HWND hwnd = nullptr;
};

Vec<WindowToHwnd> gWindowToHwndMap;

Window* WindowMapGetWindow(HWND hwnd) {
  for (auto& el : gWindowToHwndMap) {
    if (el.hwnd == hwnd) {
      return el.window;
    }
  }
  return nullptr;
}

void WindowMapAdd(HWND hwnd, Window* w) {
  if (!hwnd || (WindowMapGetWindow(hwnd) != nullptr)) {
    return;
  }
  WindowToHwnd el = { w, hwnd };
  gWindowToHwndMap.Append(el);
}

bool WindowMapRemove(HWND hwnd) {
  int n = gWindowToHwndMap.isize();
  for (int i = 0; i < n; i++) {
    auto&& el = gWindowToHwndMap[i];
    if (el.hwnd == hwnd) {
      gWindowToHwndMap.RemoveAtFast(i);
      return true;
    }
  }
  return false;
}

bool WindowMapRemove(Window* w) {
  int n = gWindowToHwndMap.isize();
  for (int i = 0; i < n; i++) {
    auto&& el = gWindowToHwndMap[i];
    if (el.window == w) {
      gWindowToHwndMap.RemoveAtFast(i);
      return true;
    }
  }
  return false;
}

// Window.h / Window.cpp

enum WindowBorderStyle {
  kWindowBorderNone,
  kWindowBorderClient,
  kWindowBorderStatic
};

struct Window {
  Window();
  Window(HWND hwnd);
  virtual ~Window();
  virtual void Destroy();

  virtual bool PreTranslateMessage(MSG& msg);

  void SetWindowHandle(HWND hwnd);

  // message handlers
  virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual LRESULT WindowProcDefault(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  void Subclass(HWND hwnd);
  void UnSubclass();

  CREATESTRUCT create_struct = {};
  WNDCLASSEX   window_class = {};
  HFONT        font = nullptr;
  HICON        icon_large = nullptr;
  HICON icon_small = nullptr;
  HMENU        menu = nullptr;
  WNDPROC      prev_window_proc = nullptr;
  HWND parent = nullptr;
  HWND window = nullptr;
  HINSTANCE instance = nullptr;
};

Window* gCurrentWindow = nullptr;
const WCHAR* kDefaultClassName = L"SumatraWgDefaultWinClass";

LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam) {
  Window* window = WindowMapGetWindow(hwnd);

  if (!window) {
    // I think it's meant to ensure we associate Window with HWND
    // as early as possible given than CreateWindow
    window = gCurrentWindow;
    if (window) {
      window->SetWindowHandle(hwnd);
      WindowMapAdd(hwnd, window);
    }
  }

  if (window) {
    return window->WindowProc(hwnd, uMsg, wParam, lParam);
  } else {
    return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

Window::Window() {
  instance = GetModuleHandleW(nullptr);
  gCurrentWindow = nullptr; // TODO: why?

  // Create default window class
  WNDCLASSEX wc = {};
  if (!::GetClassInfoExW(instance, kDefaultClassName, &wc)) {
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProcStatic;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = kDefaultClassName;
    ::RegisterClassEx(&wc);
  }
}

Window::Window(HWND hwnd) {
  instance = GetModuleHandleW(nullptr);
  gCurrentWindow = nullptr; // TODO: why?
  window = hwnd;
}

Window::~Window() {
  Destroy();
}

void Window::Destroy() {
  HwndDestroyWindowSafe(&window);
  if (font && parent) {
    DeleteFontSafe(&font);
  }
  DestroyIconSafe(&icon_large);
  DestroyIconSafe(&icon_small);

  if (prev_window_proc) {
    UnSubclass();
  }

  WindowMapRemove(this);
  window = nullptr;
}

LRESULT Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return WindowProcDefault(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::WindowProcDefault(HWND hwnd, UINT uMsg,
                                  WPARAM wParam, LPARAM lParam) {
  return 0;
/*                                  
  switch (uMsg) {
    case WM_COMMAND: {
      if (OnCommand(wParam, lParam))
        return 0;
      break;
    }
    case WM_CONTEXTMENU: {
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      OnContextMenu(reinterpret_cast<HWND>(wParam), pt);
      break;
    }
    case WM_CREATE: {
      OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lParam));
      break;
    }
    case WM_DESTROY: {
      if (OnDestroy())
        return 0;
      break;
    }
    case WM_DROPFILES: {
      OnDropFiles(reinterpret_cast<HDROP>(wParam));
      break;
    }
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE: {
      SIZE size = {0};
      OnSize(uMsg, 0, size);
      break;
    }
    case WM_GETMINMAXINFO: {
      OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lParam));
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEACTIVATE:
    case WM_MOUSEHOVER:
    case WM_MOUSEHWHEEL:
    case WM_MOUSELEAVE:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL: {
      LRESULT lResult = OnMouseEvent(uMsg, wParam, lParam);
      if (lResult != -1)
        return lResult;
      break;
    }
    case WM_MOVE: {
      POINTS pts = MAKEPOINTS(lParam);
      OnMove(&pts);
      break;
    }
    case WM_NOTIFY: {
      LRESULT lResult = OnNotify(static_cast<int>(wParam),
                                 reinterpret_cast<LPNMHDR>(lParam));
      if (lResult)
        return lResult;
      break;
    }
    case WM_PAINT: {
      if (!prev_window_proc_) {
        if (::GetUpdateRect(hwnd, nullptr, FALSE)) {
          PAINTSTRUCT ps;
          HDC hdc = ::BeginPaint(hwnd, &ps);
          OnPaint(hdc, &ps);
          ::EndPaint(hwnd, &ps);
        } else {
          HDC hdc = ::GetDC(hwnd);
          OnPaint(hdc, nullptr);
          ::ReleaseDC(hwnd, hdc);
        }
      }
      break;
    }
    case WM_SIZE: {
      SIZE size = {LOWORD(lParam), HIWORD(lParam)};
      OnSize(uMsg, static_cast<UINT>(wParam), size);
      break;
    }
    case WM_TIMER: {
      OnTimer(static_cast<UINT>(wParam));
      break;
    }
    case WM_WINDOWPOSCHANGING: {
      OnWindowPosChanging(reinterpret_cast<LPWINDOWPOS>(lParam));
      break;
    }
    default: {
      if (uMsg == WM_TASKBARCREATED ||
          uMsg == WM_TASKBARBUTTONCREATED ||
          uMsg == WM_TASKBARCALLBACK) {
        OnTaskbarCallback(uMsg, lParam);
        return 0;
      }
      break;
    }
  }

  if (prev_window_proc_) {
    return ::CallWindowProc(prev_window_proc_, hwnd, uMsg, wParam, lParam);
  } else {
    return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  */
}


void Window::SetWindowHandle(HWND hwnd) {
  window = hwnd;
}

bool Window::PreTranslateMessage(MSG& msg) {
  return false;
}

void Window::Subclass(HWND hwnd) {
  WNDPROC current_proc = reinterpret_cast<WNDPROC>(
      ::GetWindowLongPtr(hwnd, GWLP_WNDPROC));
  if (current_proc != reinterpret_cast<WNDPROC>(WindowProcStatic)) {
    prev_window_proc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcStatic)));
    window = hwnd;
  }
}

void Window::UnSubclass() {
  WNDPROC current_proc = reinterpret_cast<WNDPROC>(
      ::GetWindowLongPtr(window, GWLP_WNDPROC));
  if (current_proc == reinterpret_cast<WNDPROC>(WindowProcStatic)) {
    ::SetWindowLongPtr(window, GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(prev_window_proc));
    prev_window_proc = nullptr;
  }
}

// application.cpp
bool PreTranslateMessage(MSG& msg) {
    if ((WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
        (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST)) {
      for (HWND hwnd = msg.hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd)) {
        if (auto window = WindowMapGetWindow(hwnd)) {
          if (window->PreTranslateMessage(msg)) {
            return true;
          }
        }
      }
    }
    return false;
}

int MessageLoop() {
  MSG msg;

  // TODO: add handling of accelerators
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    if (!PreTranslateMessage(msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  return static_cast<int>(LOWORD(msg.wParam));
}


}