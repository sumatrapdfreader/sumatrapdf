/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

static Kind kindWindow = "wnd";

TempStr WinMsgNameTemp(UINT msg) {
    return fmt("0x%x", (int)msg);
}

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
        if (wnd->onWndProc.IsValid()) {
            WindowBase::WndProcEvent ev;
            ev.w = wnd;
            ev.hwnd = hwnd;
            ev.msg = msg;
            ev.wparam = wparam;
            ev.lparam = lparam;
            wnd->onWndProc.Call(&ev);
            if (ev.didHandle) {
                return ev.result;
            }
        }
        DpiSetFromHwnd(hwnd);
        return wnd->WndProcDefault(hwnd, msg, wparam, lparam);
    }
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
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

// default paint when onPaint is not set: virtual tree, or solid background
static void WindowBaseDefaultPaint(WindowBase* w, HDC hdc, PAINTSTRUCT* ps) {
    if (w->vroot) {
        PaintVirtTree(w->vroot, hdc, ToRect(ps->rcPaint), w->bgColor);
        return;
    }
    auto* br = w->BackgroundBrush();
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

void WindowBase::SetFocusTo(VirtCtrl* w) {
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
    VirtCtrl* focusedVirt = vroot ? vroot->focused : nullptr;
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

void WindowBase::Close() {
    ReportIf(!::IsWindow(hwnd));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void WindowBase::SetPos(Rect* r) {
    HwndMoveWindow(hwnd, r);
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT WindowBase::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    HWND wnd = nullptr;
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
        return pWnd->DispatchMessageReflect(msg, wparam, lparam);
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
                didHandle = pWnd->DispatchCommand(wparam, lparam);
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
                return wndFrom->DispatchNotifyReflect(wparam, lparam);
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
                LRESULT result = pWnd->DispatchMessageReflect(msg, wparam, lparam);
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
                LRESULT result = pWnd->DispatchMessageReflect(msg, wparam, lparam);
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

    // WM_SETCURSOR: onSetCursor first so tooltips can update even when a virt
    // control later claims the cursor (FindBar / FindWindow button tips).
    if (msg == WM_SETCURSOR) {
        if (onSetCursor.IsValid()) {
            SetCursorEvent sev;
            sev.w = this;
            sev.hwndCursor = reinterpret_cast<HWND>(wparam);
            sev.hitTest = LOWORD(lparam);
            sev.mouseMsg = HIWORD(lparam);
            onSetCursor.Call(&sev);
            if (sev.didHandle) {
                return sev.result;
            }
        }
        if (vroot) {
            LRESULT res = 0;
            if (VirtTreeOnMessage(hwnd, vroot, msg, wparam, lparam, res)) {
                return res;
            }
        }
    } else if (vroot) {
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
            return (LRESULT)GetHFont();
        }

        case WM_SETFONT: {
            font = GetPlatformFont((HFONT)wparam);
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
                didHandle = pWnd->DispatchCommand(wparam, lparam);
            }

            // Handle user commands.
            if (!didHandle && onCommand.IsValid()) {
                CommandEvent ev;
                ev.w = this;
                ev.wparam = wparam;
                ev.lparam = lparam;
                onCommand.Call(&ev);
                didHandle = ev.didHandle;
            }

            if (didHandle) {
                return 0;
            }
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            if (onCreate.IsValid()) {
                CreateEvent ev;
                ev.w = this;
                ev.cs = (CREATESTRUCT*)lparam;
                onCreate.Call(&ev);
            }
            break;
        }

        case WM_SETFOCUS: {
            if (onFocus.IsValid()) {
                FocusEvent ev;
                ev.w = this;
                onFocus.Call(&ev);
            }
            break;
        }

        case WM_ACTIVATE: {
            if (onActivate.IsValid()) {
                ActivateEvent ev;
                ev.w = this;
                ev.state = LOWORD(wparam);
                ev.minimized = HIWORD(wparam) != 0;
                ev.other = reinterpret_cast<HWND>(lparam);
                onActivate.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
            break;
        }

        case WM_DPICHANGED: {
            DpiSet((int)LOWORD(wparam), (int)HIWORD(wparam));
            if (onDpiChanged.IsValid()) {
                DpiChangedEvent ev;
                ev.w = this;
                ev.dpiX = LOWORD(wparam);
                ev.dpiY = HIWORD(wparam);
                ev.suggested = reinterpret_cast<RECT*>(lparam);
                onDpiChanged.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
            break;
        }

        case WM_NCHITTEST: {
            if (onNcHitTest.IsValid()) {
                NcHitTestEvent ev;
                ev.w = this;
                ev.screenPos = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                onNcHitTest.Call(&ev);
                if (ev.didHandle) {
                    return ev.result;
                }
            }
            break;
        }

        case WM_SHOWWINDOW: {
            if (onShowWindow.IsValid()) {
                ShowWindowEvent ev;
                ev.w = this;
                ev.show = wparam != FALSE;
                ev.status = lparam;
                onShowWindow.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
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
                    result = wndFrom->DispatchNotifyReflect(wparam, lparam);
                }
            }

            // Handle user notifications
            if (result == 0 && onNotify.IsValid()) {
                NotifyEvent nev;
                nev.w = this;
                nev.controlId = (int)wparam;
                nev.nmh = (NMHDR*)lparam;
                onNotify.Call(&nev);
                result = nev.result;
            }
            if (result != 0) {
                return result;
            }
            break;
        }

        case WM_ERASEBKGND: {
            // false: claim handled so DefWindowProc does not fill with the class
            // brush (custom windows paint the full client in WM_PAINT)
            if (!shouldEraseBackground) {
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            DpiSetFromHwnd(hwnd);
            if (subclassId) {
                // Allow window controls to do their default drawing.
                return FinalWindowProc(msg, wparam, lparam);
            }

            {
                PAINTSTRUCT ps;
                HDC hdc = ::BeginPaint(hwnd, &ps);
                if (onPaint.IsValid()) {
                    PaintEvent ev;
                    ev.w = this;
                    ev.hdc = hdc;
                    ev.ps = &ps;
                    onPaint.Call(&ev);
                } else {
                    WindowBaseDefaultPaint(this, hdc, &ps);
                }
                ::EndPaint(hwnd, &ps);
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
            if (onDropFiles.IsValid()) {
                DropFilesEvent ev;
                ev.w = this;
                ev.dropInfo = reinterpret_cast<HDROP>(wparam);
                onDropFiles.Call(&ev);
            }
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            if (onSize.IsValid()) {
                SizeEvent ev;
                ev.w = this;
                ev.msg = msg;
                onSize.Call(&ev);
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            if (onGetMinMaxInfo.IsValid()) {
                GetMinMaxInfoEvent ev;
                ev.w = this;
                ev.mmi = reinterpret_cast<MINMAXINFO*>(lparam);
                onGetMinMaxInfo.Call(&ev);
            }
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
            if (onMouseEvent.IsValid()) {
                MouseEvent ev;
                ev.w = this;
                ev.msg = msg;
                ev.wparam = wparam;
                ev.lparam = lparam;
                onMouseEvent.Call(&ev);
                if (ev.result != -1) {
                    return ev.result;
                }
            }
            break;
        }
        case WM_MOVE: {
            if (onMove.IsValid()) {
                POINTS pts = MAKEPOINTS(lparam);
                MoveEvent ev;
                ev.w = this;
                ev.pts = &pts;
                onMove.Call(&ev);
            }
            break;
        }

        case WM_SIZE: {
            DpiSetFromHwnd(hwnd);
            Size size = {LOWORD(lparam), HIWORD(lparam)};
            // most WindowBase windows just want `layout` stretched to the new
            // client size. CreateCustom sends WM_SIZE before the tree exists,
            // so `layout` being null is the usual "not ready yet" guard.
            if (autoLayout && layout && (size.dx > 0) && (size.dy > 0)) {
                DoLayout(size);
                HwndInvalidate(hwnd);
            }
            if (onSize.IsValid()) {
                SizeEvent ev;
                ev.w = this;
                ev.msg = msg;
                ev.type = static_cast<UINT>(wparam);
                ev.size = size;
                onSize.Call(&ev);
            }
            break;
        }
        case WM_TIMER: {
            if (onTimer.IsValid()) {
                TimerEvent ev;
                ev.w = this;
                ev.timerId = static_cast<UINT_PTR>(wparam);
                onTimer.Call(&ev);
            }
            break;
        }
        case WM_WINDOWPOSCHANGING: {
            if (onWindowPosChanging.IsValid()) {
                WindowPosChangingEvent ev;
                ev.w = this;
                ev.windowPos = reinterpret_cast<LPWINDOWPOS>(lparam);
                onWindowPosChanging.Call(&ev);
            }
            break;
        }

        default: {
            if (msg == WM_TASKBARCREATED || msg == WM_TASKBARBUTTONCREATED || msg == WM_TASKBARCALLBACK) {
                if (onTaskbarCallback.IsValid()) {
                    TaskbarCallbackEvent ev;
                    ev.w = this;
                    ev.msg = msg;
                    ev.lparam = lparam;
                    onTaskbarCallback.Call(&ev);
                }
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
    }
    // TODO: also DefSubclassProc?
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

// PreTranslate: onPreTranslate first (WM_CHAR / KEYUP / etc.), then key-downs
// via onKeyDown (so dialog shortcuts work while focus is on a child HWND), then
// closeOnEsc / closeOnCtrlW, then default Tab among mixed HWND + virtual controls.
// WM_CHAR Escape is needed because an Edit can eat KEYDOWN Escape (IME / some
// locales); KEYDOWN is still handled so we close before TranslateMessage.
bool WindowBase::PreTranslateMessage(MSG& msg) {
    if (onPreTranslate.IsValid()) {
        PreTranslateEvent pev;
        pev.w = this;
        pev.msg = &msg;
        onPreTranslate.Call(&pev);
        if (pev.didHandle) {
            return true;
        }
    }
    if (closeOnEsc && msg.message == WM_CHAR && msg.wParam == VK_ESCAPE) {
        Close();
        return true;
    }
    if (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN) {
        return false;
    }
    KeyEvent ev;
    ev.hwnd = msg.hwnd;
    ev.vkey = (int)msg.wParam;
    ev.isCtrl = IsCtrlPressed();
    ev.isShift = IsShiftPressed();
    ev.isAlt = IsAltPressed();
    ev.isSysKey = (msg.message == WM_SYSKEYDOWN);
    if (onKeyDown.IsValid()) {
        onKeyDown.Call(&ev);
        if (ev.didHandle) {
            return true;
        }
    }
    if (closeOnEsc && ev.vkey == VK_ESCAPE) {
        Close();
        return true;
    }
    if (closeOnCtrlW && ev.vkey == 'W' && ev.isCtrl && !ev.isAlt) {
        Close();
        return true;
    }
    // default Tab among mixed HWND + virtual controls
    if (ev.vkey != VK_TAB || !layout || ev.isCtrl || ev.isAlt) {
        return false;
    }
    if (!vroot) {
        return false;
    }
    return TabNavigate(ev.isShift);
}

void WindowBase::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(WindowBaseFromHwnd(hwnd));

    this->hwnd = hwnd;
    DpiSetFromHwnd(hwnd);
    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }
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
    wc.hCursor = GetCachedCursor(IDC_ARROW);
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
    DpiSetFromHwnd(hwnd);
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

PlatformFont* WindowBase::GetFont() {
    return font;
}

HFONT WindowBase::GetHFont() const {
    return font ? font->GetHFont() : nullptr;
}

int WindowBase::GetDpi() const {
    return hwnd ? RoundUp(DpiGetForHwnd(hwnd), 4) : DpiGet();
}

void WindowBase::SetFont(PlatformFont* fontIn) {
    font = fontIn;
    if (!hwnd) {
        return;
    }
    HwndSetFont(hwnd, GetHFont());
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

void WindowBase::SetColors(Color textCol, Color bgCol) {
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
