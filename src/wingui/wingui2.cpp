/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/wingui2.h"

#include "utils/Log.h"

Kind kindWnd = "wnd";

// TODO:
// - if layout is set, do layout on WM_SIZE using LayoutToSize

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

LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Wnd* window = WindowMapGetWindow(hwnd);

    if (!window) {
        // I think it's meant to ensure we associate Window with HWND
        // as early as possible given than CreateWindow
        window = gWindowBeingCreated;
        if (window) {
            window->hwnd = hwnd;
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
    // instance = GetModuleHandleW(nullptr);
    kind = kindWnd;
}

Wnd::~Wnd() {
    Destroy();
    delete layout;
    DeleteObject(backgroundColorBrush);
}

Kind Wnd::GetKind() {
    return kind;
}

void Wnd::SetText(const WCHAR* s) {
    CrashIf(!hwnd);
    HwndSetText(hwnd, s);
    HwndInvalidate(hwnd); // TODO: move inside HwndSetText()?
}

void Wnd::SetText(const char* s) {
    if (!s) {
        s = "";
    }
    auto ws = ToWstrTemp(s);
    SetText(ws.Get());
}

void Wnd::SetText(std::string_view sv) {
    if (sv.empty()) {
        SetText(L"");
    }
    auto ws = ToWstrTemp(sv);
    SetText(ws.Get());
}

TempStr Wnd::GetText() {
    auto sw = win::GetTextTemp(hwnd);
    auto sa = ToUtf8Temp(sw.AsView());
    return sa;
}

void Wnd::SetVisibility(Visibility newVisibility) {
    CrashIf(!hwnd);
    visibility = newVisibility;
    bool isVisible = IsVisible();
    // TODO: a different way to determine if is top level vs. child window?
    if (GetParent(hwnd) == nullptr) {
        ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    } else {
        BOOL bIsVisible = toBOOL(isVisible);
        SetWindowStyle(hwnd, WS_VISIBLE, bIsVisible);
    }
}

Visibility Wnd::GetVisibility() {
    return visibility;
#if 0
    if (GetParent(hwnd) == nullptr) {
        // TODO: what to do for top-level window?
        CrashMe();
        return true;
    }
    bool isVisible = IsWindowStyleSet(hwnd, WS_VISIBLE);
    return isVisible;
#endif
}

void Wnd::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool Wnd::IsVisible() const {
    return visibility == Visibility::Visible;
}

void Wnd::Destroy() {
    HwndDestroyWindowSafe(&hwnd);
    /*
    if (prevWindowProc) {
        UnSubclass();
    }
    */
    Cleanup();
}

LRESULT Wnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

// This function is called when a window is attached to Wnd.
// Override it to automatically perform tasks when the window is attached.
// Note:  Window controls are attached.
void Wnd::OnAttach() {
}

// Override this to handle WM_COMMAND messages
bool Wnd::OnCommand(WPARAM wparam, LPARAM lparam) {
    //  UINT id = LOWORD(wparam);
    //  switch (id)
    //  {
    //  case IDM_FILE_NEW:
    //      OnFileNew();
    //      return true;   // return TRUE for handled commands
    //  }

    // return false for unhandled commands

    return false;
}

// Called during window creation. Override this functions to perform tasks
// such as creating child windows.
int Wnd::OnCreate(CREATESTRUCT*) {
    // This function is called when a WM_CREATE message is received
    // Override it to automatically perform tasks during window creation.
    // Return 0 to continue creating the window.

    // Note: Window controls don't call OnCreate. They are sublcassed (attached)
    //  after their window is created.

    /*
    LOGFONT logfont;
    ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(logfont), &logfont);
    font = ::CreateFontIndirect(&logfont);
    ::SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    */

    return 0;
}

// This function is called when a window is destroyed.
// Override it to do additional tasks, such as ending the application
//  with PostQuitMessage.
void Wnd::OnDestroy() {
}

// Called when the background of the window's client area needs to be erased.
// Override this function in your derived class to perform drawing tasks.
// Return Value: Return FALSE to also permit default erasure of the background
//               Return TRUE to prevent default erasure of the background
bool Wnd::OnEraseBkgnd(HDC) {
    return false;
}

// Called in response to WM_CLOSE, before the window is destroyed.
// Override this function to suppress destroying the window.
// WM_CLOSE is sent by SendMessage(WM_CLOSE, 0, 0) or by clicking X
//  in the top right corner.
// Child windows don't receive WM_CLOSE unless they are closed using
//  the Close function.
void Wnd::OnClose() {
    Destroy();
}

void Wnd::OnContextMenu(HWND hwnd, Point pt) {
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

Size Wnd::GetIdealSize() {
    return {};
}

Size Wnd::Layout(const Constraints bc) {
    dbglayoutf("WindowBase::Layout() %s ", GetKind());
    LogConstraints(bc, "\n");

    auto hinset = insets.left + insets.right;
    auto vinset = insets.top + insets.bottom;
    auto innerConstraints = bc.Inset(hinset, vinset);

    int dx = MinIntrinsicWidth(0);
    int dy = MinIntrinsicHeight(0);
    childSize = innerConstraints.Constrain(Size{dx, dy});
    auto res = Size{
        childSize.dx + hinset,
        childSize.dy + vinset,
    };
    return res;
}

int Wnd::MinIntrinsicHeight(int) {
#if 0
    auto vinset = insets.top + insets.bottom;
    Size s = GetIdealSize();
    return s.dy + vinset;
#else
    Size s = GetIdealSize();
    return s.dy;
#endif
}

int Wnd::MinIntrinsicWidth(int) {
#if 0
    auto hinset = insets.left + insets.right;
    Size s = GetIdealSize();
    return s.dx + hinset;
#else
    Size s = GetIdealSize();
    return s.dx;
#endif
}

void Wnd::Close() {
    CrashIf(!::IsWindow(hwnd));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void Wnd::SetPos(RECT* r) {
    ::MoveWindow(hwnd, r);
}

void Wnd::SetBounds(Rect bounds) {
    dbglayoutf("WindowBaseLayout:SetBounds() %s %d,%d - %d, %d\n", GetKind(), bounds.x, bounds.y, bounds.dx, bounds.dy);

    lastBounds = bounds;

    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);

    auto r = RectToRECT(bounds);
    ::MoveWindow(hwnd, &r);
    // TODO: optimize if doesn't change position
    ::InvalidateRect(hwnd, nullptr, TRUE);
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
    if (!wnd) {
        return 0;
    }

    Wnd* pWnd = WindowMapGetWindow(wnd);
    if (pWnd != nullptr) {
        auto res = pWnd->OnMessageReflect(msg, wparam, lparam);
        return res;
    }

    return 0;
}

// for interop with windows not wrapped in Wnd, run this at the beginning of message loop
LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // hwnd is a parent of control sending WM_NOTIFY message
    switch (msg) {
        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            Wnd* pWnd = WindowMapGetWindow(reinterpret_cast<HWND>(lparam));
            bool didHandle = false;
            if (pWnd != nullptr) {
                didHandle = pWnd->OnCommand(wparam, lparam);
            }
            if (didHandle) {
                return 1;
            }
        } break; // Note: Some MDI commands require default processing.
        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<LPNMHDR>(lparam);
            HWND from = hdr->hwndFrom;
            Wnd* wndFrom = WindowMapGetWindow(from);
            if (!wndFrom) {
                return 0;
            }
            if (hwnd == GetParent(wndFrom->hwnd)) {
                return wndFrom->OnNotifyReflect(wparam, lparam);
            }
        }
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
            Wnd* pWnd = WindowMapGetWindow(reinterpret_cast<HWND>(lparam));
            LRESULT result = 0;
            if (pWnd != nullptr) {
                result = pWnd->MessageReflect(msg, wparam, lparam);
            }
            if (result != 0) {
                return result; // Message processed so return.
            }
        } break; // Do default processing when message not already processed.
    }
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
            bool didHandle = false;
            if (pWnd != nullptr) {
                didHandle = pWnd->OnCommand(wparam, lparam);
            }

            // Handle user commands.
            if (!didHandle) {
                didHandle = OnCommand(wparam, lparam);
            }

            if (didHandle) {
                return 0;
            }
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            OnCreate((CREATESTRUCT*)lparam);
            break;
        }

        case WM_DESTROY: {
            OnDestroy();
            break; // Note: Some controls require default processing.
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            Wnd* wndFrom = WindowMapGetWindow(from);

            if (wndFrom != nullptr) {
                if (::GetParent(from) == this->hwnd) {
                    result = wndFrom->OnNotifyReflect(wparam, lparam);
                }
            }

            // Handle user notifications
            if (result == 0) {
                result = OnNotify((int)wparam, (NMHDR*)lparam);
            }
            if (result != 0) {
                return result;
            }
            break;
        }

        case WM_PAINT: {
            if (prevWindowProc) {
                // Allow window controls to do their default drawing.
                return FinalWindowProc(msg, wparam, lparam);
            }

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
            // No more drawing required
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
            OnGetMinMaxInfo(reinterpret_cast<MINMAXINFO*>(lparam));
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
            Point pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
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
    if (prevWindowProc) {
        return ::CallWindowProc(prevWindowProc, hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

bool Wnd::PreTranslateMessage(MSG& msg) {
    return false;
}

void Wnd::Attach(HWND hwnd) {
    CrashIf(!IsWindow(hwnd));
    CrashIf(WindowMapGetWindow(hwnd));

    this->hwnd = hwnd;
    Subclass();
    OnAttach();
}

// Attaches a CWnd object to a dialog item.
void Wnd::AttachDlgItem(UINT id, HWND parent) {
    CrashIf(!::IsWindow(parent));
    HWND wnd = ::GetDlgItem(parent, id);
    Attach(wnd);
}

HWND Wnd::Detach() {
    CrashIf(!prevWindowProc);
    if (IsWindow(hwnd)) {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)(prevWindowProc));
    }

    HWND wnd = this->hwnd;
    WindowMapRemove(this);
    this->hwnd = nullptr;
    prevWindowProc = nullptr;
    return wnd;
}

void Wnd::Cleanup() {
    WindowMapRemove(this);
    hwnd = nullptr;
    prevWindowProc = nullptr;
}

static void WndRegisterClass(const WCHAR* className) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    HINSTANCE inst = GetInstance();
    BOOL ok = ::GetClassInfoExW(inst, className, &wc);
    if (ok) {
        return;
    }
    wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.hInstance = inst;
    wc.lpszClassName = className;
    wc.lpfnWndProc = StaticWindowProc;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ATOM atom = ::RegisterClassExW(&wc);
    CrashIf(!atom);
}

HWND Wnd::CreateControl(const CreateControlArgs& args) {
    CrashIf(!args.parent);
    CrashIf(!args.className);

    DWORD style = args.style;
    style |= WS_CHILD;
    if (args.visible) {
        style |= WS_VISIBLE;
    } else {
        style &= ~WS_VISIBLE;
    }
    DWORD exStyle = args.exStyle;
    const WCHAR* className = args.className;
    int x = args.pos.x;
    int y = args.pos.y;
    int dx = args.pos.dx;
    int dy = args.pos.dy;
    HWND parent = args.parent;
    HMENU id = args.ctrlId;
    HINSTANCE inst = GetInstance();
    LPVOID* createParams = 0;
    hwnd = ::CreateWindowExW(exStyle, className, L"", style, x, y, dx, dy, parent, id, inst, createParams);
    CrashIf(!hwnd);
    Subclass();
    OnAttach();
    HFONT f = args.font;
    if (!f) {
        f = GetDefaultGuiFont();
    }
    HwndSetFont(hwnd, f);
    if (args.text) {
        SetText(args.text);
    }
    return hwnd;
}

HWND Wnd::CreateCustom(const CreateCustomArgs& args) {
    const WCHAR* className = args.className;
    if (className == nullptr) {
        className = kDefaultClassName;
    }
    WndRegisterClass(className);
    HWND parent = args.parent;

    DWORD style = args.style;
    if (style == 0) {
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    if (parent) {
        style |= WS_CHILD;
    } else {
        style &= ~WS_CHILD;
        style |= WS_CLIPCHILDREN;
    }
    if (args.visible) {
        style |= WS_VISIBLE;
    } else {
        style &= ~WS_VISIBLE;
    }

    int x = args.pos.x;
    int y = args.pos.y;
    int dx = args.pos.dx;
    int dy = args.pos.dy;
    if (!args.parent && args.pos.IsEmpty()) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        dx = CW_USEDEFAULT;
        dy = CW_USEDEFAULT;
    }

    DWORD tmpStyle = style & ~WS_VISIBLE;
    DWORD exStyle = args.exStyle;
    const WCHAR* title = args.title;
    HMENU m = (HMENU)args.menu;
    HINSTANCE inst = GetInstance();
    LPVOID* createParams = nullptr;

    // associate hwnd with this window as soon as possible
    // in StaticWndProc
    gWindowBeingCreated = this;
    HWND hwndTmp = ::CreateWindowExW(exStyle, className, title, style, x, y, dx, dy, parent, m, inst, createParams);
    gWindowBeingCreated = nullptr;
    CrashIf(!hwndTmp);
    // hwnd should be assigned in WM_CREATE
    CrashIf(hwndTmp != hwnd);
    CrashIf(this != WindowMapGetWindow(hwndTmp));

    HFONT f = args.font;
    if (!f) {
        f = GetDefaultGuiFont();
    }
    HwndSetFont(hwnd, f);

    if (style & WS_VISIBLE) {
        if (style & WS_MAXIMIZE)
            ::ShowWindow(hwnd, SW_MAXIMIZE);
        else if (style & WS_MINIMIZE)
            ::ShowWindow(hwnd, SW_MINIMIZE);
        else
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    return hwnd;
}

// if only top given, set them all to top
// if top, right given, set bottom to top and left to right
void Wnd::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(hwnd, top, right, bottom, left);
}

void Wnd::Subclass() {
    CrashIf(!IsWindow(hwnd));
    CrashIf(prevWindowProc); // don't subclass multiple times

    WindowMapAdd(hwnd, this);
    prevWindowProc = SubclassWindow(hwnd, StaticWindowProc);
    CrashIf(!prevWindowProc);
}

void Wnd::SetIsEnabled(bool isEnabled) const {
    CrashIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool Wnd::IsEnabled() const {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void Wnd::SetFocus() const {
    ::SetFocus(hwnd);
}

bool Wnd::IsFocused() const {
    BOOL isFocused = ::IsFocused(hwnd);
    return tobool(isFocused);
}

void Wnd::SetRtl(bool isRtl) const {
    CrashIf(!hwnd);
    SetWindowExStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRtl);
}

void Wnd::SetBackgroundColor(COLORREF col) {
    if (col == ColorNoChange) {
        return;
    }
    backgroundColor = col;
    if (backgroundColorBrush != nullptr) {
        DeleteObject(backgroundColorBrush);
        backgroundColorBrush = nullptr;
    }
    if (backgroundColor != ColorUnset) {
        backgroundColorBrush = CreateSolidBrush(backgroundColor);
    }
    // can be set before we create the window
    if (!hwnd) {
        return;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
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

} // namespace wg

static void SizeToIdealSize(wg::Wnd* wnd) {
    if (!wnd || !wnd->hwnd) {
        return;
    }
    auto size = wnd->GetIdealSize();
    // TODO: don't change x,y, only dx/dy
    RECT r{0, 0, size.dx, size.dy};
    wnd->SetBounds(r);
}

//- Static

// https://docs.microsoft.com/en-us/windows/win32/controls/static-controls

namespace wg {
Kind kindStatic = "static";

Static::Static() {
    kind = kindStatic;
}

HWND Static::Create(const StaticCreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = WC_STATICW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.style = WS_CHILD | WS_VISIBLE | SS_NOTIFY;
    cargs.text = args.text;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Static::GetIdealSize() {
    CrashIf(!hwnd);
    WCHAR* txt = win::GetTextTemp(hwnd);
    HFONT hfont = GetWindowFont(hwnd);
    return HwndMeasureText(hwnd, txt, hfont);
}

bool Static::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == STN_CLICKED && onClicked) {
        onClicked();
        return true;
    }
    return false;
}

#if 0
void Handle_WM_CTLCOLORSTATIC(void* user, WndEvent* ev) {
    auto w = (StaticCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_CTLCOLORSTATIC);
    HDC hdc = (HDC)ev->wp;
    if (w->textColor != ColorUnset) {
        SetTextColor(hdc, w->textColor);
    }
    // the brush we return is the background color for the whole
    // area of static control
    // SetBkColor() is just for the part where the text is
    // SetBkMode(hdc, TRANSPARENT) sets the part of the text to transparent
    // (but the whole background is still controlled by the bruhs
    auto bgBrush = w->backgroundColorBrush;
    if (bgBrush != nullptr) {
        SetBkColor(hdc, w->backgroundColor);
        ev->result = (LRESULT)bgBrush;
    } else {
        SetBkMode(hdc, TRANSPARENT);
    }
    ev->didHandle = true;
}
#endif

LRESULT Static::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CTLCOLORSTATIC) {
        // TODO: implement me
        return 0;
    }
    return 0;
}

} // namespace wg

//- Button

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons
namespace wg {

Kind kindButton = "button";

Button::Button() {
    kind = kindButton;
}

bool Button::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == BN_CLICKED && onClicked) {
        onClicked();
        return true;
    }
    return false;
}

LRESULT Button::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CTLCOLORBTN) {
        // TODO: implement me
        return 0;
    }
    return 0;
}

HWND Button::Create(const ButtonCreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = WC_BUTTONW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (isDefault) {
        cargs.style |= BS_DEFPUSHBUTTON;
    } else {
        cargs.style |= BS_PUSHBUTTON;
    }
    cargs.text = args.text;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Button::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
Size Button::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    Size size = this->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

Button* CreateButton(HWND parent, const WCHAR* s, const ClickedHandler& onClicked) {
    ButtonCreateArgs args;
    args.parent = parent;
    args.text = ToUtf8Temp(s).Get();

    auto b = new Button();
    b->onClicked = onClicked;
    b->Create(args);
    return b;
}

#define kButtonMargin DpiScale(8)

Button* CreateDefaultButton(HWND parent, const WCHAR* s) {
    ButtonCreateArgs args;
    args.parent = parent;
    args.text = ToUtf8Temp(s).Get();

    auto* b = new Button();
    b->Create(args);

    RECT r;
    GetClientRect(parent, &r);
    Size size = b->GetIdealSize();
    int x = RectDx(r) - size.dx - kButtonMargin;
    int y = RectDy(r) - size.dy - kButtonMargin;
    r.left = x;
    r.right = x + size.dx;
    r.top = y;
    r.bottom = y + size.dy;
    b->SetPos(&r);
    return b;
}

} // namespace wg

//- Edit

// https://docs.microsoft.com/en-us/windows/win32/controls/edit-controls

// TODO:
// - expose EN_UPDATE
// https://docs.microsoft.com/en-us/windows/win32/controls/en-update
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

namespace wg {

Kind kindEdit = "edit";

static bool EditSetCueText(HWND hwnd, std::string_view s) {
    if (!hwnd) {
        return false;
    }
    auto ws = ToWstrTemp(s);
    bool ok = Edit_SetCueBannerText(hwnd, ws.Get()) == TRUE;
    return ok;
}

Edit::Edit() {
    kind = kindEdit;
}

Edit::~Edit() {
    // DeleteObject(bgBrush);
}

void Edit::SetSelection(int start, int end) {
    Edit_SetSel(hwnd, start, end);
}

HWND Edit::Create(const EditCreateArgs& editArgs) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/edit-control-styles
    CreateControlArgs args;
    args.className = WC_EDITW;
    args.parent = editArgs.parent;
    args.font = editArgs.font;
    args.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;
    if (editArgs.withBorder) {
        args.exStyle = WS_EX_CLIENTEDGE;
        // Note: when using WS_BORDER, we would need to remember
        // we have border and use it in Edit::HasBorder
        // args.style |= WS_BORDER;
    }
    if (editArgs.isMultiLine) {
        args.style |= ES_MULTILINE | WS_VSCROLL | ES_WANTRETURN;
    } else {
        // ES_AUTOHSCROLL disable wrapping in multi-line setup
        args.style |= ES_AUTOHSCROLL;
    }
    idealSizeLines = editArgs.idealSizeLines;
    if (idealSizeLines < 1) {
        idealSizeLines = 1;
    }
    Wnd::CreateControl(args);
    SizeToIdealSize(this);

    if (hwnd) {
        if (editArgs.cueText) {
            EditSetCueText(hwnd, editArgs.cueText);
        }
    }
    return hwnd;
}

bool Edit::HasBorder() {
    DWORD exStyle = GetWindowExStyle(hwnd);
    bool res = bit::IsMaskSet<DWORD>(exStyle, WS_EX_CLIENTEDGE);
    return res;
}

Size Edit::GetIdealSize() {
    HFONT hfont = HwndGetFont(hwnd);
    Size s1 = HwndMeasureText(hwnd, L"Minimal", hfont);
    // logf("Edit::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    auto txt = win::GetTextTemp(hwnd);
    Size s2 = HwndMeasureText(hwnd, txt, hfont);
    // logf("Edit::GetIdealSize: s2.dx=%d, s2.dy=%d\n", (int)s2.cx, (int)s2.cy);

    int dx = std::max(s1.dx, s2.dx);
    if (maxDx > 0 && dx > maxDx) {
        dx = maxDx;
    }
    // for multi-line text, this measures multiple line.
    // TODO: maybe figure out better protocol
    int dy = std::min(s1.dy, s2.dy);
    if (dy == 0) {
        dy = std::max(s1.dy, s2.dy);
    }
    dy = dy * idealSizeLines;
    // logf("Edit::GetIdealSize: dx=%d, dy=%d\n", (int)dx, (int)dy);

    LRESULT margins = SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    dx += lm + rm;

    if (HasBorder()) {
        dx += DpiScale(hwnd, 4);
        dy += DpiScale(hwnd, 8);
    }
    // logf("Edit::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return {dx, dy};
}

// https://docs.microsoft.com/en-us/windows/win32/controls/en-change
bool Edit::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == EN_CHANGE && onTextChanged) {
        onTextChanged();
        return true;
    }
    return false;
}

#if 0
// https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcoloredit
static void Handle_WM_CTLCOLOREDIT(void* user, WndEvent* ev) {
    auto w = (EditCtrl*)user;
    CrashIf(ev->msg != WM_CTLCOLOREDIT);
    HWND hwndCtrl = (HWND)ev->lp;
    CrashIf(hwndCtrl != w->hwnd);
    if (w->bgBrush == nullptr) {
        return;
    }
    HDC hdc = (HDC)ev->wp;
    // SetBkColor(hdc, w->bgCol);
    SetBkMode(hdc, TRANSPARENT);
    if (w->textColor != ColorUnset) {
        ::SetTextColor(hdc, w->textColor);
    }
    ev->didHandle = true;
    ev->result = (INT_PTR)w->bgBrush;
}
#endif

LRESULT Edit::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CTLCOLOREDIT) {
        // TOOD: return brush
        return 0;
    }
    return 0;
}

} // namespace wg

namespace wg {

Kind kindListBox = "listbox";

ListBox::ListBox() {
    kind = kindListBox;
#if 0
    ctrlID = 0;
#endif
}

ListBox::~ListBox() {
    delete this->model;
}

HWND ListBox::Create(const ListBoxCreateArgs& args) {
    idealSizeLines = args.idealSizeLines;
    if (idealSizeLines < 0) {
        idealSizeLines = 0;
    }
    idealSize = {DpiScale(args.parent, 120), DpiScale(args.parent, 32)};

    CreateControlArgs cargs;
    cargs.className = L"LISTBOX";
    cargs.parent = args.parent;
    cargs.font = args.font;

    // https://docs.microsoft.com/en-us/windows/win32/controls/list-box-styles
    cargs.style = WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    cargs.style |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    // args.style |= WS_BORDER;
    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    if (hwnd) {
        if (model != nullptr) {
            FillWithItems(this->hwnd, model);
        }
    }

    return hwnd;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/lb-getitemheight
int ListBox::GetItemHeight(int idx) {
    // idx only valid for LBS_OWNERDRAWVARIABLE, otherwise should be 0
    int res = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, idx, 0);
    if (res == LB_ERR) {
        // if failed for some reason, fallback to measuring text in default font
        // HFONT f = GetFont();
        HFONT f = GetDefaultGuiFont();
        Size sz = HwndMeasureText(hwnd, L"A", f);
        res = sz.dy;
    }
    return res;
}

Size ListBox::GetIdealSize() {
    Size res = idealSize;
    if (idealSizeLines > 0) {
        int dy = GetItemHeight(0) * idealSizeLines + DpiScale(hwnd, 2 + 2); // padding of 2 at top and bottom
        res.dy = dy;
    }
    return res;
}

int ListBox::GetCount() {
    LRESULT res = ListBox_GetCount(hwnd);
    return (int)res;
}

int ListBox::GetCurrentSelection() {
    LRESULT res = ListBox_GetCurSel(hwnd);
    return (int)res;
}

// -1 to clear selection
// returns false on error
bool ListBox::SetCurrentSelection(int n) {
    if (n < 0) {
        ListBox_SetCurSel(hwnd, -1);
        return true;
    }
    int nItems = model->ItemsCount();
    if (n >= nItems) {
        return false;
    }
    LRESULT res = ListBox_SetCurSel(hwnd, n);
    return res != LB_ERR;
}

// for efficiency you can re-use model:
// get the model, change data, call SetModel() again
void ListBox::SetModel(ListBoxModel* model) {
    if (this->model && (this->model != model)) {
        delete this->model;
    }
    this->model = model;
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
    }
    SetCurrentSelection(-1);
    // TODO: update ideal size based on the size of the model
}

bool ListBox::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-selchange
    if (code == LBN_SELCHANGE && onSelectionChanged) {
        onSelectionChanged();
        return true;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-dblclk
    if (code == LBN_DBLCLK && onDoubleClick) {
        onDoubleClick();
        return true;
    }
    return false;
}

LRESULT ListBox::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorlistbox
    if (msg == WM_CTLCOLORLISTBOX) {
        // TOOD: implement me
        return 0;
    }
    return 0;
}

} // namespace wg

//- Checkbox

namespace wg {

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

static Kind kindCheckbox = "checkbox";

static CheckState GetButtonCheckState(HWND hwnd) {
    auto res = Button_GetCheck(hwnd);
    return (CheckState)res;
}

static void SetButtonCheckState(HWND hwnd, CheckState newState) {
    CrashIf(!hwnd);
    Button_SetCheck(hwnd, newState);
}

Checkbox::Checkbox() {
    kind = kindCheckbox;
}

HWND Checkbox::Create(const CheckboxCreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.text = args.text;
    cargs.className = WC_BUTTONW;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);
    return hwnd;
}

bool Checkbox::OnCommand(WPARAM wp, LPARAM) {
    auto code = HIWORD(wp);
    if (code == BN_CLICKED && onCheckStateChanged) {
        onCheckStateChanged();
        return true;
    }
    return false;
}

Size Checkbox::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

void Checkbox::SetCheckState(CheckState newState) {
    CrashIf(!hwnd);
    SetButtonCheckState(hwnd, newState);
}

CheckState Checkbox::GetCheckState() const {
    return GetButtonCheckState(hwnd);
}

void Checkbox::SetIsChecked(bool isChecked) {
    CrashIf(!hwnd);
    CheckState newState = isChecked ? CheckState::Checked : CheckState::Unchecked;
    SetButtonCheckState(hwnd, newState);
}

bool Checkbox::IsChecked() const {
    CrashIf(!hwnd);
    auto state = GetCheckState();
    return state == CheckState::Checked;
}

} // namespace wg

//- Progress

namespace wg {

// https://docs.microsoft.com/en-us/windows/win32/controls/progress-bar-control-reference

Kind kindProgress = "progress";

Progress::Progress() {
    kind = kindProgress;
}

HWND Progress::Create(const ProgressCreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.style = WS_CHILD | WS_VISIBLE;
    cargs.className = PROGRESS_CLASSW;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);
    if (hwnd && args.initialMax != 0) {
        SetMax(args.initialMax);
    }
    return hwnd;
}

Size Progress::GetIdealSize() {
    return {idealDx, idealDy};
}

void Progress::SetMax(int newMax) {
    int min = 0;
    SendMessageW(hwnd, PBM_SETRANGE32, min, newMax);
}

void Progress::SetCurrent(int newCurrent) {
    SendMessageW(hwnd, PBM_SETPOS, newCurrent, 0);
}

int Progress::GetMax() {
    auto max = (int)SendMessageW(hwnd, PBM_GETRANGE, FALSE /* get high limit */, 0);
    return max;
}

int Progress::GetCurrent() {
    auto current = (int)SendMessageW(hwnd, PBM_GETPOS, 0, 0);
    return current;
}

} // namespace wg

//- DropDown

namespace wg {

// https://docs.microsoft.com/en-us/windows/win32/controls/combo-boxes

Kind kindDropDown = "dropdown";

DropDown::DropDown() {
    kind = kindDropDown;
}

static void SetDropDownItems(HWND hwnd, Vec<std::string_view>& items) {
    ComboBox_ResetContent(hwnd);
    for (std::string_view s : items) {
        auto ws = ToWstrTemp(s);
        ComboBox_AddString(hwnd, ws);
    }
}

bool DropDown::OnCommand(WPARAM wp, LPARAM) {
    auto code = HIWORD(wp);
    if ((code == CBN_SELCHANGE) && onSelectionChanged) {
        onSelectionChanged();
        // must return false or else the drop-down list will not close
        return false;
    }
    return false;
}

HWND DropDown::Create(const DropDownCreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST;
    cargs.className = WC_COMBOBOX;

    Wnd::CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }

    // SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);

    SizeToIdealSize(this);
    return hwnd;
}

// -1 means no selection
int DropDown::GetCurrentSelection() {
    int res = (int)ComboBox_GetCurSel(hwnd);
    return res;
}

// -1 : no selection
void DropDown::SetCurrentSelection(int n) {
    if (n < 0) {
        ComboBox_SetCurSel(hwnd, -1);
        return;
    }
    int nItems = (int)items.size();
    CrashIf(n >= nItems);
    ComboBox_SetCurSel(hwnd, n);
}

void DropDown::SetCueBanner(std::string_view sv) {
    auto ws = ToWstrTemp(sv);
    ComboBox_SetCueBannerText(hwnd, ws.Get());
}

void DropDown::SetItems(Vec<std::string_view>& newItems) {
    items.Reset();
    for (std::string_view s : newItems) {
        items.Append(s);
    }
    SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
}

static void DropDownItemsFromStringArray(Vec<std::string_view>& items, const char* strings) {
    while (strings) {
        items.Append(strings);
        seqstrings::Next(strings);
    }
}

void DropDown::SetItemsSeqStrings(const char* items) {
    Vec<std::string_view> strings;
    DropDownItemsFromStringArray(strings, items);
    SetItems(strings);
}

Size DropDown::GetIdealSize() {
    HFONT hfont = GetWindowFont(hwnd);
    Size s1 = TextSizeInHwnd(hwnd, L"Minimal", hfont);
    for (std::string_view s : items) {
        auto ws = ToWstrTemp(s);
        Size s2 = TextSizeInHwnd(hwnd, ws, hfont);
        s1.dx = std::max(s1.dx, s2.dx);
        s1.dy = std::max(s1.dy, s2.dy);
    }
    // TODO: not sure if I want scrollbar. Only needed if a lot of items
    int dxPad = GetSystemMetrics(SM_CXVSCROLL);
    int dx = s1.dx + dxPad + DpiScale(hwnd, 8);
    // TODO: 5 is a guessed number.
    int dyPad = DpiScale(hwnd, 4);
    int dy = s1.dy + dyPad;
    Rect rc = WindowRect(hwnd);
    if (rc.dy > dy) {
        dy = rc.dy;
    }
    return {dx, dy};
}

} // namespace wg

//- Trackbar

namespace wg {

// https://docs.microsoft.com/en-us/windows/win32/controls/trackbar-control-reference

Kind kindTrackbar = "trackbar";

Trackbar::Trackbar() {
    kind = kindTrackbar;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
HWND Trackbar::Create(const TrackbarCreateArgs& args) {
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    dwStyle |= TBS_AUTOTICKS; // tick marks for each increment
    dwStyle |= TBS_TOOLTIPS;  // show current value when dragging in a tooltip
    if (args.isHorizontal) {
        dwStyle |= TBS_HORZ;
        idealSize.dx = 32;
        idealSize.dy = DpiScale(args.parent, 22);
    } else {
        dwStyle |= TBS_VERT;
        idealSize.dy = 32;
        idealSize.dx = DpiScale(args.parent, 22);
    }

    CreateControlArgs cargs;
    cargs.className = TRACKBAR_CLASS;
    cargs.parent = args.parent;

    // TODO: add initial size to CreateControlArgs
    // initialSize = idealSize;

    cargs.style = dwStyle;
    // args.style |= WS_BORDER;
    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    if (hwnd) {
        SetRange(args.rangeMin, args.rangeMax);
        SetValue(args.rangeMin);
    }
    return hwnd;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-hscroll--trackbar-
// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
LRESULT Trackbar::OnMessageReflect(UINT msg, WPARAM wp, LPARAM) {
    if (!onPosChanging) {
        return 0;
    }
    switch (msg) {
        case WM_VSCROLL:
        case WM_HSCROLL: {
            int pos = (int)HIWORD(wp);
            int code = (int)LOWORD(wp);
            switch (code) {
                case TB_THUMBPOSITION:
                case TB_THUMBTRACK:
                    // pos is HIWORD so do nothing
                    break;
                default:
                    pos = GetValue();
            }
            TrackbarPosChangingEvent a{};
            a.trackbar = this;
            a.pos = pos;
            onPosChanging(&a);
            // per https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
            // "if an application processes this message, it should return zero"
            return 0;
        }
    }

    return 0;
}

Size Trackbar::GetIdealSize() {
    return {idealSize.dx, idealSize.dy};
}

void Trackbar::SetRange(int min, int max) {
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM range = (LPARAM)MAKELONG(min, max);
    SendMessageW(hwnd, TBM_SETRANGE, redraw, range);
}

int Trackbar::GetRangeMin() {
    int res = SendMessageW(hwnd, TBM_GETRANGEMIN, 0, 0);
    return res;
}

int Trackbar::getRangeMax() {
    int res = SendMessageW(hwnd, TBM_GETRANGEMAX, 0, 0);
    return res;
}

void Trackbar::SetValue(int pos) {
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM p = (LPARAM)pos;
    SendMessageW(hwnd, TBM_SETPOS, redraw, p);
}

int Trackbar::GetValue() {
    int res = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
    return res;
}

} // namespace wg

//- Splitter

namespace wg {

// the technique for drawing the splitter for non-live resize is described
// at http://www.catch22.net/tuts/splitter-windows

Kind kindSplitter = "splitter";

static void OnSplitterPaint(HWND hwnd, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    AutoDeleteBrush br = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static void DrawXorBar(HDC hdc, HBRUSH br, int x1, int y1, int width, int height) {
    SetBrushOrgEx(hdc, x1, y1, nullptr);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, br);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(HWND hwnd, Rect& rc) {
    rc = ChildPosWithinParent(hwnd);
    HDC hdc = GetDC(GetParent(hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(HWND hwnd, HBRUSH br, int x) {
    Rect rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, x, rc.y, 4, rc.dy);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineH(HWND hwnd, HBRUSH br, int y) {
    Rect rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, rc.x, y, rc.dx, 4);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineVH(HWND hwnd, HBRUSH br, bool isVert, Point pos) {
    if (isVert) {
        DrawResizeLineV(hwnd, br, pos.x);
    } else {
        DrawResizeLineH(hwnd, br, pos.y);
    }
}

static void DrawResizeLine(HWND hwnd, HBRUSH br, SplitterType stype, bool erasePrev, bool drawCurr,
                           Point& prevResizeLinePos) {
    Point pos;
    GetCursorPosInHwnd(GetParent(hwnd), pos);
    bool isVert = stype != SplitterType::Horiz;

    if (erasePrev) {
        DrawResizeLineVH(hwnd, br, isVert, prevResizeLinePos);
    }
    if (drawCurr) {
        DrawResizeLineVH(hwnd, br, isVert, pos);
    }
    prevResizeLinePos = pos;
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

Splitter::Splitter() {
    kind = kindSplitter;
}

Splitter::~Splitter() {
    DeleteObject(brush);
    DeleteObject(bmp);
}

HWND Splitter::Create(const SplitterCreateArgs& args) {
    CrashIf(!args.parent);

    isLive = args.isLive;
    type = args.type;
    backgroundColor = args.backgroundColor;
    if (backgroundColor == ColorUnset) {
        backgroundColor = GetSysColor(COLOR_BTNFACE);
    }

    bmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    CrashIf(!bmp);
    brush = CreatePatternBrush(bmp);
    CrashIf(!brush);

    DWORD style = GetWindowLong(args.parent, GWL_STYLE);
    parentClipsChildren = bit::IsMaskSet<DWORD>(style, WS_CLIPCHILDREN);

    CreateCustomArgs cargs;
    // cargs.className = L"SplitterWndClass";
    cargs.parent = args.parent;
    cargs.style = WS_CHILDWINDOW;
    cargs.exStyle = 0;
    CreateCustom(cargs);

    return hwnd;
}

LRESULT Splitter::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (WM_ERASEBKGND == msg) {
        // TODO: should this be FALSE?
        return TRUE;
    }

    if (WM_LBUTTONDOWN == msg) {
        SetCapture(hwnd);
        if (!isLive) {
            if (parentClipsChildren) {
                SetWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, false);
            }
            DrawResizeLine(hwnd, brush, type, false, true, prevResizeLinePos);
        }
        return 1;
    }

    if (WM_LBUTTONUP == msg) {
        if (!isLive) {
            DrawResizeLine(hwnd, brush, type, true, false, prevResizeLinePos);
            if (parentClipsChildren) {
                SetWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        SplitterMoveEvent arg;
        arg.w = this;
        arg.done = true;
        onSplitterMove(&arg);
        ScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterType::Vert == type) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            SplitterMoveEvent arg;
            arg.w = this;
            arg.done = false;
            onSplitterMove(&arg);
            if (!arg.resizeAllowed) {
                curId = IDC_NO;
            } else if (!isLive) {
                DrawResizeLine(hwnd, brush, type, true, true, prevResizeLinePos);
            }
        }
        SetCursorCached(curId);
        return 0;
    }

    if (WM_PAINT == msg) {
        OnSplitterPaint(hwnd, backgroundColor);
        return 0;
    }

    return WndProcDefault(hwnd, msg, wparam, lparam);
}

} // namespace wg