/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

static Kind kindWindow = "wnd";

TempStr WinMsgNameTemp(UINT msg) {
    return fmt("0x%x", (int)msg);
}

// TODO:
// - if layout is set, do layout on WM_SIZE using LayoutToSize

static Vec<HWND> gHwndDestroyed;

void MarkHWNDDestroyed(HWND hwnd) {
    gHwndDestroyed.Append(hwnd);
}

// a window can outlive its HWND: the object is still in a list, but messages
// for that HWND must no longer reach it
bool HwndWasDestroyed(HWND hwnd) {
    return gHwndDestroyed.Find(hwnd) >= 0;
}

static Vec<WindowBase*> gWindowList;

WindowBase* WindowBaseFromHwnd(HWND hwnd) {
    for (auto& wnd : gWindowList) {
        if (wnd->hwnd == hwnd) {
            if (gHwndDestroyed.Find(hwnd) >= 0) {
                return nullptr;
            }
            return wnd;
        }
    }
    return nullptr;
}

static bool WindowListRemove(WindowBase* w) {
    bool removed = false;
    while (gWindowList.RemoveFast(w) >= 0) {
        removed = true;
    }
    // logf("WndMapRemoveWnd: failed to remove w: 0x%p\n", w);
    return removed;
}

static void WindowListAdd(WindowBase* w) {
    bool report = WindowListRemove(w);
    ReportIfFast(report);
    gWindowList.Append(w);
}

//- Taskbar.cpp

const DWORD WM_TASKBARCALLBACK = WM_APP + 0x15;
const DWORD WM_TASKBARCREATED = ::RegisterWindowMessage(L"TaskbarCreated");
const DWORD WM_TASKBARBUTTONCREATED = ::RegisterWindowMessage(L"TaskbarButtonCreated");

//- Window.h / Window.cpp

static const WStr kDefaultClassName = L"SumatraWgDefaultWinClass";

static LRESULT CALLBACK WindowBaseWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // seen crashes in TabCtrl::WndProc() which might be caused by handling drag&drop messages
    // after parent window was destroyed. maybe this will fix it
    if (!IsWindow(hwnd)) {
        return 0;
    }

    WindowBase* wnd = WindowBaseFromHwnd(hwnd);

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)(lparam);
        ReportIf(wnd);
        wnd = (WindowBase*)(cs->lpCreateParams);
        wnd->hwnd = hwnd;
        WindowListAdd(wnd);
    }

    if (wnd) {
        return wnd->WndProc(hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

static LRESULT CALLBACK WindowBaseSubclassedWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                       UINT_PTR /*subclassId*/, DWORD_PTR /*data*/) {
    return WindowBaseWindowProc(hwnd, msg, wp, lp);
}

WindowBase::WindowBase() {
    // instance = GetModuleHandleW(nullptr);
    kind = kindWindow;
}

WindowBase::~WindowBase() {
    Destroy();
    // the tree first: a virtual control tells its root it's going away
    delete layout;
    delete vroot;
    DeleteBrushSafe(&bgBrush);
}

void WindowBase::SetText(Str s) {
    if (!s) {
        s = StrL("");
    }
    HwndSetText(hwnd, s);
    HwndRepaintNow(hwnd); // TODO: move inside HwndSetText()?
}

TempStr WindowBase::GetTextTemp() {
    return HwndGetTextTemp(hwnd);
}

void WindowBase::SetVisibility(Visibility newVisibility) {
    ReportIf(!hwnd);
    visibility = newVisibility;
    bool isVisible = IsVisible();
    // TODO: a different way to determine if is top level vs. child window?
    if (GetParent(hwnd) == nullptr) {
        ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    } else {
        BOOL bIsVisible = toBOOL(isVisible);
        HwndSetWindowStyle(hwnd, WS_VISIBLE, bIsVisible);
    }
}

Visibility WindowBase::GetVisibility() {
    return visibility;
#if 0
    if (GetParent(hwnd) == nullptr) {
        // TODO: what to do for top-level window?
        CrashMe();
        return true;
    }
    bool isVisible = HwndIsWindowStyleSet(hwnd, WS_VISIBLE);
    return isVisible;
#endif
}

void WindowBase::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool WindowBase::IsVisible() const {
    return visibility == Visibility::Visible;
}

void WindowBase::Destroy() {
    // the order is important
    // stop dispatching messages to this WindowBase
    WindowListRemove(this);
    // unsubclass while hwnd is still valid
    UnSubclass();
    // finally destroy hwnd
    HwndDestroyWindowSafe(&hwnd);
}

// over-ride those to hook into message processing
LRESULT WindowBase::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

// This function is called when a window is attached to WindowBase.
// Override it to automatically perform tasks when the window is attached.
// Note:  Window controls are attached.
void WindowBase::OnAttach() {}

void WindowBase::OnFocus() {}

// Override this to handle WM_COMMAND messages
bool WindowBase::OnCommand(WPARAM /*wparam*/, LPARAM /*lparam*/) {
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
int WindowBase::OnCreate(CREATESTRUCT* /*cs*/) {
    // This function is called when a WM_CREATE message is received
    // Override it to automatically perform tasks during window creation.
    // Return 0 to continue creating the window.

    // Note: Window controls don't call OnCreate. They are sublcassed (attached)
    //  after their window is created.

    /*
    LOGFONT logfont;
    ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(logfont), &logfont);
    font = ::CreateFontIndirectW(&logfont);
    ::SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    */

    return 0;
}

void WindowBase::OnDropFiles(HDROP drop_info) {}

void WindowBase::OnGetMinMaxInfo(MINMAXINFO* mmi) {}

LRESULT WindowBase::OnMouseEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    return -1;
}

void WindowBase::OnMove(POINTS* /*pts*/) {}

// Processes notification (WM_NOTIFY) messages from a child window.
LRESULT WindowBase::OnNotify(int /*controlId*/, NMHDR* /*nmh*/) {
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

void WindowBase::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
    if (vroot) {
        PaintVirtTree(vroot, hdc, ToRect(ps->rcPaint), bgColor);
        return;
    }
    auto* br = BackgroundBrush();
    if (br != nullptr) {
        HdcFillRect(hdc, ToRect(ps->rcPaint), br);
    }
}

void WindowBase::DoLayout(Size size) {
    LayoutTreeToSize(hwnd, layout, size, &vroot);
}

void WindowBase::DoLayout() {
    Rect rc = HwndClientRect(hwnd);
    DoLayout(rc.Size());
}

void WindowBase::SetFocusTo(ControlBase* c) {
    if (!c || !c->hwnd) {
        return;
    }
    // the win32 focus moving away from us clears the virtual focus (WM_KILLFOCUS)
    ::SetFocus(c->hwnd);
}

void WindowBase::SetFocusTo(VirtWnd* w) {
    if (!vroot || !w) {
        return;
    }
    // a virtual control has no HWND: we hold the focus on its behalf and route
    // the keys to it
    if (::GetFocus() != hwnd) {
        ::SetFocus(hwnd);
    }
    vroot->SetFocus(w);
}

bool WindowBase::TabNavigate(bool backwards) {
    if (!layout) {
        return false;
    }
    Vec<TabStop> stops;
    CollectTabStops(layout, stops);
    int n = len(stops);
    if (n == 0) {
        return false;
    }
    // where we are now: a virtual control if one has the focus, otherwise the
    // control that owns the win32 focus
    int idx = -1;
    VirtWnd* focusedVirt = vroot ? vroot->focused : nullptr;
    HWND focusedHwnd = ::GetFocus();
    for (int i = 0; i < n; i++) {
        TabStop& ts = stops[i];
        if (focusedVirt && ts.vwnd == focusedVirt) {
            idx = i;
            break;
        }
        if (!focusedVirt && ts.ctrl && ts.ctrl->hwnd == focusedHwnd) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        idx = backwards ? n - 1 : 0;
    } else {
        idx = (idx + (backwards ? -1 : 1) + n) % n;
    }
    TabStop& ts = stops[idx];
    if (ts.vwnd) {
        SetFocusTo(ts.vwnd);
    } else {
        if (vroot) {
            vroot->SetFocus(nullptr);
        }
        SetFocusTo(ts.ctrl);
    }
    return true;
}

void WindowBase::OnSize(UINT msg, UINT type, Size size) {}

void WindowBase::OnTaskbarCallback(UINT msg, LPARAM lparam) {}

void WindowBase::OnTimer(UINT_PTR timerId) {}

void WindowBase::OnWindowPosChanging(WINDOWPOS* window_pos) {}

void WindowBase::Close() {
    ReportIf(!::IsWindow(hwnd));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void WindowBase::SetPos(Rect* r) {
    HwndMoveWindow(hwnd, r);
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT WindowBase::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
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

        case WM_DRAWITEM: {
            // Get HWND directly from the struct since control ID may be 0
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
            wnd = dis->hwndItem;
            break;
        }
        case WM_MEASUREITEM: {
            // MEASUREITEMSTRUCT doesn't have hwnd, try GetDlgItem first
            wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            if (!wnd && wparam == 0) {
                // Control ID is 0, find owner-draw listbox child
                wnd = FindWindowExW(hwnd, nullptr, L"LISTBOX", nullptr);
            }
            break;
        }
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

    ControlBase* pWnd = ControlFromHwnd(wnd);
    if (pWnd != nullptr) {
        auto res = pWnd->OnMessageReflect(msg, wparam, lparam);
        return res;
    }

    return 0;
}

// for interop with windows not wrapped in WindowBase, run this at the beginning of message loop
LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // hwnd is a parent of control sending WM_NOTIFY message
    switch (msg) {
        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
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
            ControlBase* wndFrom = ControlFromHwnd(from);
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
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
            LRESULT result = 0;
            if (pWnd != nullptr) {
                result = pWnd->MessageReflect(msg, wparam, lparam);
            }
            if (result != 0) {
                return result;
            }
        } break;
        // owner-draw messages: lparam is a struct pointer, not an HWND
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
            ControlBase* pWnd = ControlFromHwnd(dis->hwndItem);
            if (pWnd != nullptr) {
                LRESULT result = pWnd->OnMessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
        case WM_MEASUREITEM: {
            HWND wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            if (!wnd && wparam == 0) {
                wnd = FindWindowExW(hwnd, nullptr, L"LISTBOX", nullptr);
            }
            ControlBase* pWnd = wnd ? ControlFromHwnd(wnd) : nullptr;
            if (pWnd != nullptr) {
                LRESULT result = pWnd->OnMessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
        case WM_DELETEITEM:
        case WM_COMPAREITEM: {
            HWND wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            ControlBase* pWnd = wnd ? ControlFromHwnd(wnd) : nullptr;
            if (pWnd != nullptr) {
                LRESULT result = pWnd->MessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
    }
    return 0;
}

LRESULT WindowBase::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    if (vroot) {
        LRESULT res = 0;
        if (VirtTreeOnMessage(hwnd, vroot, msg, wparam, lparam, res)) {
            return res;
        }
    }

    WmEvent e{hwnd, msg, wparam, lparam, this->userData, this};

    if (msg == WM_CLOSE) {
        if (onClose.IsValid()) {
            WindowBase::CloseEvent ev;
            ev.e = &e;
            onClose.Call(&ev);
            if (ev.e->didHandle) {
                return 0;
            }
        }
        // TODO: should only send WM_DESTROY, the rest should be hooked in OnDestroy
        Destroy();
        return 0;
    }

    if (msg == WM_DESTROY) {
        if (onDestroy.IsValid()) {
            DestroyEvent ev;
            ev.e = &e;
            onDestroy.Call(&ev);
        }
        // no break because some controls require default processing.
    }

    switch (msg) {
        // windows don't support WM_GETFONT / WM_SETFONT
        // only controls do. not sure if we won't interfere
        // with control handling
        // TODO: maybe when font is nullptr, ask the original proc
        case WM_GETFONT: {
            return (LRESULT)font;
        }

        case WM_SETFONT: {
            font = (HFONT)wparam;
            if (!subclassId) {
                return 0;
            }
            // a subclassed window is a real control (edit, tree view etc.) that
            // draws its own text: remembering the font here isn't enough, the
            // control itself has to see WM_SETFONT. Fall through to the original
            // wndproc.
            break;
        }

        case WM_COMMAND: {
            // Subclassed controls (like ComboBox) receive WM_COMMAND from their
            // internal children. Don't handle it here — let the original wndproc
            // process it and send proper notifications to the parent window.
            if (subclassId) {
                break;
            }
            // Reflect this message if it's from a control.
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
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

        case WM_SETFOCUS: {
            OnFocus();
            break;
        }

        case WM_KILLFOCUS: {
            // we were holding the focus for a virtual control; it's gone now
            if (vroot) {
                vroot->SetFocus(nullptr);
            }
            break;
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            ControlBase* wndFrom = ControlFromHwnd(from);

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
            if (subclassId) {
                // Allow window controls to do their default drawing.
                return FinalWindowProc(msg, wparam, lparam);
            }

            if (::GetUpdateRect(hwnd, nullptr, FALSE)) {
                PAINTSTRUCT ps;
                HDC hdc = ::BeginPaint(hwnd, &ps);
                OnPaint(hdc, &ps);
                ::EndPaint(hwnd, &ps);
            } else {
                // TODO: for now those are the same because some OnPaint()
                // implementations assume ps is provided
                PAINTSTRUCT ps;
                HDC hdc = ::BeginPaint(hwnd, &ps);
                OnPaint(hdc, &ps);
                ::EndPaint(hwnd, &ps);
#if 0
                HDC hdc = ::GetDC(hwnd);
                OnPaint(hdc, nullptr);
                ::ReleaseDC(hwnd, hdc);
#endif
            }
            // No more drawing required
            return 0;
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
            result = MessageReflect(msg, wparam, lparam);
            if (result != 0) return result; // Message processed so return.
        } break;                            // Do default processing when message not already processed.

        case WM_DROPFILES: {
            OnDropFiles(reinterpret_cast<HDROP>(wparam));
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            Size size{};
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
            if (lResult != -1) return lResult;
            break;
        }
        case WM_MOVE: {
            POINTS pts = MAKEPOINTS(lparam);
            OnMove(&pts);
            break;
        }

        case WM_SIZE: {
            Size size = {LOWORD(lparam), HIWORD(lparam)};
            if (autoLayout && (size.dx > 0) && (size.dy > 0)) {
                DoLayout(size);
                HwndInvalidate(hwnd);
            }
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

LRESULT WindowBase::FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (subclassId) {
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    } else {
        // TODO: also DefSubclassProc?
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

bool WindowBase::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_TAB || !layout) {
        return false;
    }
    if (IsCtrlPressed() || IsAltPressed()) {
        return false;
    }
    // a window of only HWND controls keeps the native dialog navigation
    // (IsDialogMessage() in the message loop); we take Tab over only when there
    // are virtual controls in the ring as well, as those it can't see
    if (!vroot) {
        return false;
    }
    return TabNavigate(IsShiftPressed());
}

void WindowBase::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(WindowBaseFromHwnd(hwnd));

    this->hwnd = hwnd;
    Subclass();
    OnAttach();
}

HWND WindowBase::Detach() {
    UnSubclass();

    HWND wnd = hwnd;
    WindowListRemove(this);
    hwnd = nullptr;
    return wnd;
}

static void WndRegisterClass(WStr className) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetInstance();
    wc.style = CS_DBLCLKS;
    wc.lpszClassName = className.s;
    wc.lpfnWndProc = WindowBaseWindowProc;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ::RegisterClassExW(&wc);
}

HWND WindowBase::CreateCustom(const CreateCustomArgs& args) {
    font = args.font;

    WStr className = args.className ? args.className : kDefaultClassName;
    // TODO: validate className is not win32 control class
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

    DWORD exStyle = args.exStyle;
    if (args.isRtl) {
        exStyle |= WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    }
    ReportIf(args.menu && args.cmdId);
    HMENU m = args.menu;
    if (m == nullptr) {
        m = (HMENU)(INT_PTR)args.cmdId;
    }
    HINSTANCE inst = GetInstance();
    void* createParams = this;
    WCHAR* titleW = CWStrTemp(args.title);

    HWND hwndTmp = ::CreateWindowExW(exStyle, className.s, titleW, style, x, y, dx, dy, parent, m, inst, createParams);

    ReportIf(!hwndTmp);
    // hwnd should be assigned in WM_CREATE
    ReportIf(hwndTmp != hwnd);
    ReportIf(this != WindowBaseFromHwnd(hwndTmp));
    if (!hwnd) {
        return nullptr;
    }

    // trigger creating a backgroundBrush
    SetColors(kColorNoChange, args.bgColor);
    if (args.icon) {
        HwndSetIcon(hwnd, args.icon);
    }
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

void WindowBase::Subclass() {
    ReportIf(!IsWindow(hwnd));
    ReportIf(subclassId); // don't subclass multiple times
    if (subclassId) {
        return;
    }
    WindowListAdd(this);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, WindowBaseSubclassedWindowProc, subclassId, (DWORD_PTR)this);
    if (!ok) {
        // can fail under low memory / desktop heap exhaustion (it allocates and
        // attaches a window property), so don't assert. Reset subclassId so that
        // `subclassId != 0` keeps meaning "is subclassed".
        logf("WindowBase::Subclass: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
        subclassId = 0;
    }
}

void WindowBase::UnSubclass() {
    if (!subclassId) {
        return;
    }
    RemoveWindowSubclass(hwnd, WindowBaseSubclassedWindowProc, subclassId);
    subclassId = 0;
}

HFONT WindowBase::GetFont() {
    return font;
}

// HwndSetFont() sends WM_SETFONT, which our wndproc records in `font` and (for
// subclassed controls) forwards to the control itself, so this both remembers
// and applies the font. Without it SetFont() was a no-op on screen.
void WindowBase::SetFont(HFONT fontIn) {
    font = fontIn;
    if (!hwnd) {
        return;
    }
    HwndSetFont(hwnd, fontIn);
}

void WindowBase::SetIsEnabled(bool isEnabled) const {
    ReportIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool WindowBase::IsEnabled() const {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void WindowBase::SetColors(COLORREF textCol, COLORREF bgCol) {
    if (textCol != kColorNoChange) {
        this->textColor = textCol;
    }
    if (bgCol == kColorNoChange) {
        return;
    }
    this->bgColor = bgCol;
    DeleteBrushSafe(&bgBrush); // will be re-created in BackgroundBrush()
    HwndScheduleRepaint(hwnd);
}

HBRUSH WindowBase::BackgroundBrush() {
    if (bgBrush == nullptr) {
        if (bgColor != kColorUnset) {
            bgBrush = CreateSolidBrush(bgColor);
        }
    }
    return bgBrush;
}

void WindowBase::SuspendRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
}

void WindowBase::ResumeRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
}

// application.cpp
bool PreTranslateMessage(MSG& msg) {
    bool shouldProcess = (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
                         (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST);
    if (!shouldProcess) {
        return false;
    }
    for (HWND hwnd = msg.hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd)) {
        auto* wnd = WindowBaseFromHwnd(hwnd);
        if (wnd && wnd->PreTranslateMessage(msg)) {
            return true;
        }
    }
    return false;
}

//--- misc code

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog) {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (PreTranslateMessage(msg)) {
            continue;
        }
        if (TranslateAccelerator(msg.hwnd, accelTable, &msg)) {
            continue;
        }
        if (hwndDialog && IsDialogMessage(hwndDialog, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

#if 0
// TODO: support accelerator table?
// TODO: a better way to stop the loop e.g. via shared
// atomic int to signal termination and sending WM_IDLE
// to trigger processing of the loop
void RunModalWindow(HWND hwndDialog, HWND hwndParent) {
    if (hwndParent != nullptr) {
        EnableWindow(hwndParent, FALSE);
    }

    MSG msg;
    bool isFinished = false;
    while (!isFinished) {
        BOOL ok = WaitMessage();
        if (!ok) {
            DWORD err = GetLastError();
            LogLastError(err);
            isFinished = true;
            continue;
        }
        while (!isFinished && PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                isFinished = true;
                break;
            }
            if (!IsDialogMessage(hwndDialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (hwndParent != nullptr) {
        EnableWindow(hwndParent, TRUE);
    }
}
#endif

#if 0
// sets initial position of w within hwnd. Assumes w->initialSize is set.
void PositionCloseTo(WindowBase* w, HWND hwnd) {
    ReportIf(!hwnd);
    Size is = w->initialSize;
    ReportIf(is.IsEmpty());
    RECT r{};
    BOOL ok = GetWindowRect(hwnd, &r);
    ReportIf(!ok);

    // position w in the the center of hwnd
    // if window is bigger than hwnd, let the system position
    // we don't want to hide it
    int offX = (RectDx(r) - is.dx) / 2;
    if (offX < 0) {
        return;
    }
    int offY = (RectDy(r) - is.dy) / 2;
    if (offY < 0) {
        return;
    }
    Point& ip = w->initialPos;
    ip.x = (int)r.left + (int)offX;
    ip.y = (int)r.top + (int)offY;
}
#endif

// http://www.guyswithtowels.com/blog/10-things-i-hate-about-win32.html#ModelessDialogs
// to implement a standard dialog navigation we need to call
// IsDialogMessage(hwnd) in message loop.
// hwnd has to be current top-level window that is modeless dialog
// we need to manually maintain this window
static HWND g_currentModelessDialog = nullptr;

// TODO: those are hacks
HWND GetCurrentModelessDialog() {
    return g_currentModelessDialog;
}

// set to nullptr to disable
void SetCurrentModelessDialog(HWND hwnd) {
    g_currentModelessDialog = hwnd;
}

// TODO: port from Window.cpp or figure out something better
#if 0
static LRESULT CALLBACK wndProcCustom(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // ...
    if (w->isDialog) {
        // TODO: should handle more messages as per
        // https://stackoverflow.com/questions/35688400/set-full-focus-on-a-button-setfocus-is-not-enough
        // and https://docs.microsoft.com/en-us/windows/win32/dlgbox/dlgbox-programming-considerations
        if (WM_ACTIVATE == msg) {
            if (wp == 0) {
                // becoming inactive
                SetCurrentModelessDialog(nullptr);
            } else {
                // becoming active
                SetCurrentModelessDialog(w->hwnd);
            }
        }
    }
}
#endif

void DrawCloseButton(const DrawCloseButtonArgs& args) {
    bool isHover = args.isHover;
    const Rect& r = args.r;
    Gdiplus::Graphics g(args.hdc);
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPageUnit(Gdiplus::UnitPixel);
    HWND hwnd = WindowFromDC(args.hdc);
    // GDI+ doesn't pick up the window's orientation through the device context,
    // so we have to explicitly mirror all rendering horizontally
    if (HwndIsRtl(hwnd) && !args.noMirror) {
        g.ScaleTransform(-1, 1);
        g.TranslateTransform((float)HwndClientRect(hwnd).dx, 0, Gdiplus::MatrixOrderAppend);
    }
    Gdiplus::Color c;

    // paint rectangular background if specified
    if (args.colBg != kColorNoChange) {
        c.SetFromCOLORREF(args.colBg);
        Gdiplus::SolidBrush bgBr(c);
        g.FillRectangle(&bgBr, r.x, r.y, r.dx, r.dy);
    }

    // in onhover state, background is a red-ish circle
    if (args.isHover) {
        c.SetFromCOLORREF(args.colHoverBg);
        Gdiplus::SolidBrush b(c);
        g.FillEllipse(&b, r.x, r.y, r.dx - 2, r.dy - 2);
    }

    // draw 'x' — pad scales with the control so large tab-close buttons
    // (taller UI fonts / touch, issue #5220) still look balanced
    c.SetFromCOLORREF(args.isHover ? args.colXHover : args.colX);
    g.TranslateTransform((float)r.x, (float)r.y);
    int pad = std::max(3, r.dx / 4);
    int padFar = std::max(pad + 1, r.dx - pad - (r.dx > 16 ? 1 : 2));
    float penW = r.dx >= 22 ? 2.5f : 2.f;
    Gdiplus::Pen p(c, penW);
    if (isHover) {
        g.DrawLine(&p, Gdiplus::Point(pad, pad), Gdiplus::Point(padFar, padFar));
        g.DrawLine(&p, Gdiplus::Point(padFar, pad), Gdiplus::Point(pad, padFar));
    } else {
        int yOff = r.dx >= 20 ? 0 : 1;
        g.DrawLine(&p, Gdiplus::Point(pad, pad + yOff), Gdiplus::Point(padFar, padFar - yOff));
        g.DrawLine(&p, Gdiplus::Point(padFar, pad + yOff), Gdiplus::Point(pad, padFar - yOff));
    }
}

void DrawCloseButton2(const DrawCloseButtonArgs& args) {
    // bool isHover = args.isHover;
    HDC hdc = args.hdc;
    const Rect& r = args.r;
    COLORREF lineCol = args.colX;
    if (args.isHover) {
        lineCol = args.colXHover;
        int p = 3;
        HWND hwnd = WindowFromDC(hdc);
        DpiScale(hwnd, p);
        AutoDeleteBrush brush(CreateSolidBrush(args.colHoverBg));
        ScopedSelectBrush br(hdc, brush);
        RECT r2 = ToRECT(r);
        r2.left -= p;
        r2.right += p;
        r2.top -= p;
        r2.bottom += p;
        HdcFillRect(hdc, ToRect(r2), brush);
        // Ellipse(hdc, r2.left, r2.top, r2.right, r2.bottom);
    }
    AutoDeletePen pen(CreatePen(PS_SOLID, 2, lineCol));
    ScopedSelectPen p(hdc, pen);
    MoveToEx(hdc, r.x, r.y, nullptr);
    LineTo(hdc, r.x + r.dx, r.y + r.dy);

    MoveToEx(hdc, r.x + r.dx, r.y, nullptr);
    LineTo(hdc, r.x, r.y + r.dy);
}
