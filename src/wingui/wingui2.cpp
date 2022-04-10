/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "Layout.h"
#include "wingui2.h"

#include "utils/Log.h"

// this is experimantal win32 gui wrappers based on
// https://github.com/erengy/windows

namespace wg {

// window_map.h / window_map.cpp
struct WindowToHwnd {
    Wnd* window = nullptr;
    HWND hwnd = nullptr;
};

Vec<WindowToHwnd> gWindowToHwndMap;

static Wnd* WindowMapGetWindow(HWND hwnd) {
    for (auto& el : gWindowToHwndMap) {
        if (el.hwnd == hwnd) {
            return el.window;
        }
    }
    return nullptr;
}

static void WindowMapAdd(HWND hwnd, Wnd* w) {
    if (!hwnd || (WindowMapGetWindow(hwnd) != nullptr)) {
        return;
    }
    WindowToHwnd el = {w, hwnd};
    gWindowToHwndMap.Append(el);
}

/*
static bool WindowMapRemove(HWND hwnd) {
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
*/

static bool WindowMapRemove(Wnd* w) {
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

//- Taskbar.cpp

const DWORD WM_TASKBARCALLBACK = WM_APP + 0x15;
const DWORD WM_TASKBARCREATED = ::RegisterWindowMessage(L"TaskbarCreated");
const DWORD WM_TASKBARBUTTONCREATED = ::RegisterWindowMessage(L"TaskbarButtonCreated");

//- Window.h / Window.cpp

Wnd* gWindowBeingCreated = nullptr;
const WCHAR* kDefaultClassName = L"SumatraWgDefaultWinClass";

LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Wnd* window = WindowMapGetWindow(hwnd);

    if (!window) {
        // I think it's meant to ensure we associate Window with HWND
        // as early as possible given than CreateWindow
        window = gWindowBeingCreated;
        if (window) {
            window->SetWindowHandle(hwnd);
            WindowMapAdd(hwnd, window);
        }
    }

    if (window) {
        return window->WndProc(hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

Wnd::Wnd() {
    instance = GetModuleHandleW(nullptr);
    gWindowBeingCreated = nullptr;

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

Wnd::Wnd(HWND hwnd) {
    instance = GetModuleHandleW(nullptr);
    gWindowBeingCreated = nullptr;
    this->hwnd = hwnd;
}

Wnd::~Wnd() {
    Destroy();
}

void Wnd::Destroy() {
    HwndDestroyWindowSafe(&hwnd);
    if (font && parent) {
        DeleteFontSafe(&font);
    }
    DestroyIconSafe(&icon_large);
    DestroyIconSafe(&icon_small);

    if (prev_window_proc) {
        UnSubclass();
    }

    WindowMapRemove(this);
    hwnd = nullptr;
}

LRESULT Wnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

BOOL Wnd::OnCommand(WPARAM wparam, LPARAM lparam) {
    return FALSE;
}

// Called during window creation. Override this functions to perform tasks
// such as creating child windows.
int Wnd::OnCreate(HWND hwnd, CREATESTRUCT*) {
    // This function is called when a WM_CREATE message is received
    // Override it to automatically perform tasks during window creation.
    // Return 0 to continue creating the window.

    // Note: Window controls don't call OnCreate. They are sublcassed (attached)
    //  after their window is created.

    LOGFONT logfont;
    ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(logfont), &logfont);
    font = ::CreateFontIndirect(&logfont);
    ::SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);

    return 0;
}

void Wnd::OnDestroy() {
}

// Called when the background of the window's client area needs to be erased.
// Override this function in your derived class to perform drawing tasks.
// Return Value: Return FALSE to also permit default erasure of the background
//               Return TRUE to prevent default erasure of the background
BOOL Wnd::OnEraseBkgnd(HDC) {
    return FALSE;
}

void Wnd::OnClose() {
}

void Wnd::OnContextMenu(HWND hwnd, POINT pt) {
}

void Wnd::OnDropFiles(HDROP drop_info) {
}

void Wnd::OnGetMinMaxInfo(MINMAXINFO* mmi) {
}

LRESULT Wnd::OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam) {
    return -1;
}

void Wnd::OnMove(LPPOINTS pts) {
}

// Processes notification (WM_NOTIFY) messages from a child window.
LRESULT Wnd::OnNotify(int controlId, NMHDR* nmh) {
    // You can use either OnNotifyReflect or OnNotify to handle notifications
    // Override OnNotifyReflect to handle notifications in the CWnd class that
    //   generated the notification.   OR
    // Override OnNotify to handle notifications in the PARENT of the CWnd class
    //   that generated the notification.

    // Your overriding function should look like this ...

    // LPNMHDR pHeader = reinterpret_cast<LPNMHDR>(lparam);
    // switch (pHeader->code)
    // {
    //      Handle your notifications from the CHILD window here
    //      Return the value recommended by the Windows API documentation.
    //      For many notifications, the return value doesn't matter, but for some it does.
    // }

    // return 0 for unhandled notifications
    // The framework will call SetWindowLongPtr(DWLP_MSGRESULT, result) for dialogs.
    return 0;
}

// Processes the notification (WM_NOTIFY) messages in the child window that originated them.
LRESULT Wnd::OnNotifyReflect(WPARAM, LPARAM) {
    // Override OnNotifyReflect to handle notifications in the CWnd class that
    //   generated the notification.

    // Your overriding function should look like this ...

    // LPNMHDR pHeader = reinterpret_cast<LPNMHDR>(lparam);
    // switch (pHeader->code)
    // {
    //      Handle your notifications from this window here
    //      Return the value recommended by the Windows API documentation.
    // }

    // Return 0 for unhandled notifications.
    // The framework will call SetWindowLongPtr(DWLP_MSGRESULT, result) for dialogs.
    return 0;
}

void Wnd::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
}

void Wnd::OnSize(UINT msg, UINT type, SIZE size) {
}

void Wnd::OnTaskbarCallback(UINT msg, LPARAM lparam) {
}

void Wnd::OnTimer(UINT_PTR event_id) {
}

void Wnd::OnWindowPosChanging(WINDOWPOS* window_pos) {
}

// This function processes those special messages sent by some older controls,
// and reflects them back to the originating CWnd object.
// Override this function in your derived class to handle these special messages:
// WM_COMMAND, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORDLG, WM_CTLCOLORLISTBOX,
// WM_CTLCOLORSCROLLBAR, WM_CTLCOLORSTATIC, WM_CHARTOITEM,  WM_VKEYTOITEM,
// WM_HSCROLL, WM_VSCROLL, WM_DRAWITEM, WM_MEASUREITEM, WM_DELETEITEM,
// WM_COMPAREITEM, WM_PARENTNOTIFY.
LRESULT Wnd::OnMessageReflect(UINT, WPARAM, LPARAM) {
    // This function processes those special messages (see above) sent
    // by some older controls, and reflects them back to the originating CWnd object.
    // Override this function in your derived class to handle these special messages.

    // Your overriding function should look like this ...

    // switch (msg)
    // {
    //      Handle your reflected messages here
    // }

    // return 0 for unhandled messages
    return 0;
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT Wnd::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    HWND wnd = 0;
    switch (msg) {
        case WM_COMMAND:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
            wnd = reinterpret_cast<HWND>(lparam);
            break;

        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
            wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            break;

        case WM_PARENTNOTIFY:
            switch (LOWORD(wparam)) {
                case WM_CREATE:
                case WM_DESTROY:
                    wnd = reinterpret_cast<HWND>(lparam);
                    break;
            }
    }

    Wnd* pWnd = WindowMapGetWindow(wnd);

    if (pWnd != NULL)
        return pWnd->OnMessageReflect(msg, wparam, lparam);

    return 0;
}

LRESULT Wnd::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    switch (msg) {
        case WM_CLOSE: {
            OnClose();
            return 0;
        }

        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            Wnd* pWnd = WindowMapGetWindow(reinterpret_cast<HWND>(lparam));
            if (pWnd != NULL)
                result = pWnd->OnCommand(wparam, lparam);

            // Handle user commands.
            if (0 == result)
                result = OnCommand(wparam, lparam);

            if (0 != result)
                return 0;
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            OnCreate(hwnd, reinterpret_cast<CREATESTRUCT*>(lparam));
            break;
        }

        case WM_DESTROY: {
            OnDestroy();
            break; // Note: Some controls require default processing.
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            LPNMHDR pHeader = reinterpret_cast<LPNMHDR>(lparam);
            HWND from = pHeader->hwndFrom;
            Wnd* pWndFrom = WindowMapGetWindow(from);

            if (pWndFrom != NULL)
                if (::GetParent(from) == this->hwnd)
                    result = pWndFrom->OnNotifyReflect(wparam, lparam);

            // Handle user notifications
            if (result == 0)
                result = OnNotify((int)wparam, (NMHDR*)lparam);
            if (result != 0)
                return result;
            break;
        }

        case WM_PAINT: {
            if (!prev_window_proc) {
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
            return 0;
        }

        case WM_ERASEBKGND: {
            HDC dc = (HDC)(wparam);
            BOOL preventErasure;

            preventErasure = OnEraseBkgnd(dc);
            if (preventErasure)
                return TRUE;
        } break;

        // A set of messages to be reflected back to the control that generated them.
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            result = MessageReflect(msg, wparam, lparam);
            if (result != 0)
                return result; // Message processed so return.
        } break;               // Do default processing when message not already processed.

        case WM_DROPFILES: {
            OnDropFiles(reinterpret_cast<HDROP>(wparam));
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            SIZE size = {0};
            OnSize(msg, 0, size);
            break;
        }
        case WM_GETMINMAXINFO: {
            OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lparam));
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
            LRESULT lResult = OnMouseEvent(msg, wparam, lparam);
            if (lResult != -1)
                return lResult;
            break;
        }
        case WM_MOVE: {
            POINTS pts = MAKEPOINTS(lparam);
            OnMove(&pts);
            break;
        }

        case WM_CONTEXTMENU: {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            OnContextMenu(reinterpret_cast<HWND>(wparam), pt);
            break;
        }

        case WM_SIZE: {
            SIZE size = {LOWORD(lparam), HIWORD(lparam)};
            OnSize(msg, static_cast<UINT>(wparam), size);
            break;
        }
        case WM_TIMER: {
            OnTimer(static_cast<UINT>(wparam));
            break;
        }
        case WM_WINDOWPOSCHANGING: {
            OnWindowPosChanging(reinterpret_cast<LPWINDOWPOS>(lparam));
            break;
        }

        default: {
            if (msg == WM_TASKBARCREATED || msg == WM_TASKBARBUTTONCREATED || msg == WM_TASKBARCALLBACK) {
                OnTaskbarCallback(msg, lparam);
                return 0;
            }
            break;
        }
    }

    // Now hand all messages to the default procedure.
    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT Wnd::FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (prev_window_proc) {
        return ::CallWindowProc(prev_window_proc, hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

void Wnd::SetWindowHandle(HWND hwnd) {
    this->hwnd = hwnd;
}

void Wnd::PreCreate(CREATESTRUCT& cs) {
    create_struct.cx = cs.cx;
    create_struct.cy = cs.cy;
    create_struct.dwExStyle = cs.dwExStyle;
    create_struct.hInstance = instance;
    create_struct.hMenu = cs.hMenu;
    create_struct.hwndParent = cs.hwndParent;
    create_struct.lpCreateParams = cs.lpCreateParams;
    create_struct.lpszClass = cs.lpszClass;
    create_struct.lpszName = cs.lpszName;
    create_struct.style = cs.style;
    create_struct.x = cs.x;
    create_struct.y = cs.y;
}

void Wnd::PreRegisterClass(WNDCLASSEX& wc) {
    window_class.style = wc.style;
    window_class.lpfnWndProc = WindowProcStatic;
    window_class.cbClsExtra = wc.cbClsExtra;
    window_class.cbWndExtra = wc.cbWndExtra;
    window_class.hInstance = instance;
    window_class.hIcon = wc.hIcon;
    window_class.hCursor = wc.hCursor;
    window_class.hbrBackground = wc.hbrBackground;
    window_class.lpszMenuName = wc.lpszMenuName;
    window_class.lpszClassName = wc.lpszClassName;
}

bool Wnd::PreTranslateMessage(MSG& msg) {
    return false;
}

bool Wnd::RegisterClass(WNDCLASSEX& wc) const {
    WNDCLASSEX wc_existing = {0};
    if (::GetClassInfoEx(instance, wc.lpszClassName, &wc_existing)) {
        wc = wc_existing;
        return TRUE;
    }

    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProcStatic;

    auto res = ::RegisterClassEx(&wc);
    return ToBool(res);
}

void Wnd::Attach(HWND hwnd) {
    Detach();

    if (::IsWindow(hwnd)) {
        if (!WindowMapGetWindow(hwnd)) {
            WindowMapAdd(hwnd, this);
            Subclass(hwnd);
        }
    }
}

HWND Wnd::Detach() {
    HWND hwndRet = this->hwnd;

    if (prev_window_proc) {
        UnSubclass();
    }

    WindowMapRemove(this);

    this->hwnd = nullptr;

    return hwndRet;
}

HWND Wnd::Create(HWND parent) {
    PreRegisterClass(window_class);
    if (window_class.lpszClassName) {
        RegisterClass(window_class);
        create_struct.lpszClass = window_class.lpszClassName;
    }

    PreCreate(create_struct);
    if (!create_struct.lpszClass) {
        create_struct.lpszClass = kDefaultClassName;
    }
    if (!parent && create_struct.hwndParent) {
        parent = create_struct.hwndParent;
    }
    DWORD style;
    if (create_struct.style) {
        style = create_struct.style;
    } else {
        style = WS_VISIBLE | (parent ? WS_CHILD : WS_OVERLAPPEDWINDOW);
    }

    bool cx_or_cy = create_struct.cx || create_struct.cy;
    int x = cx_or_cy ? create_struct.x : CW_USEDEFAULT;
    int y = cx_or_cy ? create_struct.y : CW_USEDEFAULT;
    int cx = cx_or_cy ? create_struct.cx : CW_USEDEFAULT;
    int cy = cx_or_cy ? create_struct.cy : CW_USEDEFAULT;

    return Create(create_struct.dwExStyle, create_struct.lpszClass, create_struct.lpszName, style, x, y, cx, cy, parent,
                  create_struct.hMenu, create_struct.lpCreateParams);
}

HWND Wnd::Create(DWORD ex_style, LPCWSTR class_name, LPCWSTR window_name, DWORD style, int x, int y, int width,
                 int height, HWND parent, HMENU menu, LPVOID param) {
    Destroy();

    gWindowBeingCreated = this;

    menu = menu;
    parent = parent;

    hwnd =
        ::CreateWindowEx(ex_style, class_name, window_name, style, x, y, width, height, parent, menu, instance, param);

    WNDCLASSEX wc = {0};
    ::GetClassInfoEx(instance, class_name, &wc);
    if (wc.lpfnWndProc != reinterpret_cast<WNDPROC>(WindowProcStatic)) {
        Subclass(hwnd);
        OnCreate(hwnd, &create_struct);
    }

    gWindowBeingCreated = nullptr;

    return hwnd;
}

void Wnd::Subclass(HWND hwnd) {
    WNDPROC current_proc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hwnd, GWLP_WNDPROC));
    if (current_proc != reinterpret_cast<WNDPROC>(WindowProcStatic)) {
        prev_window_proc = reinterpret_cast<WNDPROC>(
            ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcStatic)));
        this->hwnd = hwnd;
    }
}

void Wnd::UnSubclass() {
    WNDPROC current_proc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hwnd, GWLP_WNDPROC));
    if (current_proc == reinterpret_cast<WNDPROC>(WindowProcStatic)) {
        ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prev_window_proc));
        prev_window_proc = nullptr;
    }
}

// application.cpp
bool PreTranslateMessage(MSG& msg) {
    bool shouldProcess = (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
                         (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST);
    if (!shouldProcess) {
        return false;
    }
    for (HWND hwnd = msg.hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd)) {
        if (auto window = WindowMapGetWindow(hwnd)) {
            if (window->PreTranslateMessage(msg)) {
                return true;
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

} // namespace wg