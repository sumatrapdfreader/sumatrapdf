/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

Kind kindWnd = "wnd";

#define WIN_MESSAGES(V)          \
    V(WM_CREATE)                 \
    V(WM_DESTROY)                \
    V(WM_MOVE)                   \
    V(WM_SIZE)                   \
    V(WM_ACTIVATE)               \
    V(WM_SETFOCUS)               \
    V(WM_KILLFOCUS)              \
    V(WM_ENABLE)                 \
    V(WM_SETREDRAW)              \
    V(WM_SETTEXT)                \
    V(WM_GETTEXT)                \
    V(WM_GETTEXTLENGTH)          \
    V(WM_PAINT)                  \
    V(WM_CLOSE)                  \
    V(WM_QUERYENDSESSION)        \
    V(WM_QUERYOPEN)              \
    V(WM_ENDSESSION)             \
    V(WM_QUIT)                   \
    V(WM_ERASEBKGND)             \
    V(WM_SYSCOLORCHANGE)         \
    V(WM_SHOWWINDOW)             \
    V(WM_WININICHANGE)           \
    V(WM_SETTINGCHANGE)          \
    V(WM_DEVMODECHANGE)          \
    V(WM_ACTIVATEAPP)            \
    V(WM_FONTCHANGE)             \
    V(WM_TIMECHANGE)             \
    V(WM_CANCELMODE)             \
    V(WM_SETCURSOR)              \
    V(WM_MOUSEACTIVATE)          \
    V(WM_CHILDACTIVATE)          \
    V(WM_QUEUESYNC)              \
    V(WM_GETMINMAXINFO)          \
    V(WM_PAINTICON)              \
    V(WM_ICONERASEBKGND)         \
    V(WM_NEXTDLGCTL)             \
    V(WM_SPOOLERSTATUS)          \
    V(WM_DRAWITEM)               \
    V(WM_MEASUREITEM)            \
    V(WM_DELETEITEM)             \
    V(WM_VKEYTOITEM)             \
    V(WM_CHARTOITEM)             \
    V(WM_SETFONT)                \
    V(WM_GETFONT)                \
    V(WM_SETHOTKEY)              \
    V(WM_GETHOTKEY)              \
    V(WM_QUERYDRAGICON)          \
    V(WM_COMPAREITEM)            \
    V(WM_GETOBJECT)              \
    V(WM_COMPACTING)             \
    V(WM_COMMNOTIFY)             \
    V(WM_WINDOWPOSCHANGING)      \
    V(WM_WINDOWPOSCHANGED)       \
    V(WM_POWER)                  \
    V(WM_COPYDATA)               \
    V(WM_CANCELJOURNAL)          \
    V(WM_NOTIFY)                 \
    V(WM_INPUTLANGCHANGEREQUEST) \
    V(WM_INPUTLANGCHANGE)        \
    V(WM_TCARD)                  \
    V(WM_HELP)                   \
    V(WM_USERCHANGED)            \
    V(WM_NOTIFYFORMAT)           \
    V(WM_CONTEXTMENU)            \
    V(WM_STYLECHANGING)          \
    V(WM_STYLECHANGED)           \
    V(WM_DISPLAYCHANGE)          \
    V(WM_GETICON)                \
    V(WM_SETICON)                \
    V(WM_NCCREATE)               \
    V(WM_NCDESTROY)              \
    V(WM_NCCALCSIZE)             \
    V(WM_NCHITTEST)              \
    V(WM_NCPAINT)                \
    V(WM_NCACTIVATE)             \
    V(WM_GETDLGCODE)             \
    V(WM_MOUSEMOVE)              \
    V(WM_LBUTTONDOWN)            \
    V(WM_LBUTTONUP)              \
    V(WM_LBUTTONDBLCLK)          \
    V(WM_RBUTTONDOWN)            \
    V(WM_RBUTTONUP)              \
    V(WM_RBUTTONDBLCLK)          \
    V(WM_MBUTTONDOWN)            \
    V(WM_MBUTTONUP)              \
    V(WM_MBUTTONDBLCLK)          \
    V(WM_MOUSEWHEEL)             \
    V(WM_XBUTTONDOWN)            \
    V(WM_XBUTTONUP)              \
    V(WM_XBUTTONDBLCLK)          \
    V(WM_MOUSEHWHEEL)            \
    V(WM_PARENTNOTIFY)           \
    V(WM_ENTERMENULOOP)          \
    V(WM_EXITMENULOOP)           \
    V(WM_NEXTMENU)               \
    V(WM_SIZING)                 \
    V(WM_CAPTURECHANGED)         \
    V(WM_MOVING)                 \
    V(TCM_GETITEMCOUNT)          \
    V(TCM_GETITEMW)              \
    V(TCM_SETITEMW)              \
    V(TCM_INSERTITEMW)           \
    V(TCM_DELETEITEM)            \
    V(TCM_DELETEALLITEMS)        \
    V(TCM_GETITEMRECT)           \
    V(TCM_GETCURSEL)             \
    V(TCM_SETCURSEL)             \
    V(TCM_HITTEST)               \
    V(TCM_SETITEMEXTRA)          \
    V(TCM_ADJUSTRECT)            \
    V(TCM_SETITEMSIZE)           \
    V(TCM_REMOVEIMAGE)           \
    V(TCM_SETPADDING)            \
    V(TCM_GETROWCOUNT)           \
    V(TCM_GETTOOLTIPS)           \
    V(TCM_SETTOOLTIPS)           \
    V(TCM_GETCURFOCUS)           \
    V(TCM_SETCURFOCUS)           \
    V(TCM_SETMINTABWIDTH)        \
    V(TCM_DESELECTALL)           \
    V(TCM_HIGHLIGHTITEM)

#define MSG_ID(id) id,
UINT gWinMessageIDs[] = {WIN_MESSAGES(MSG_ID)};
#undef MSG_ID

#define MSG_NAME(id) #id "\0"
SeqStrings gWinMessageNames = WIN_MESSAGES(MSG_NAME) "\0";
#undef MSG_NAME

TempStr WinMsgNameTemp(UINT msg) {
    int n = dimof(gWinMessageIDs);
    for (int i = 0; i < n; i++) {
        UINT m = gWinMessageIDs[i];
        if (m == msg) {
            return (TempStr)seqstrings::IdxToStr(gWinMessageNames, i);
        }
    }
    return str::FormatTemp("0x%x", (int)msg);
}

// TODO:
// - if layout is set, do layout on WM_SIZE using LayoutToSize

Vec<HWND> gHwndDestroyed;

void MarkHWNDDestroyed(HWND hwnd) {
    gHwndDestroyed.Append(hwnd);
}

Vec<Wnd*> gWndList;

Wnd* WndListFindByHwnd(HWND hwnd) {
    for (auto& wnd : gWndList) {
        if (wnd->hwnd == hwnd) {
            if (gHwndDestroyed.Find(hwnd) >= 0) {
                return nullptr;
            }
            return wnd;
        }
    }
    return nullptr;
}

static bool WndListRemove(Wnd* w) {
    bool removed = false;
    while (gWndList.RemoveFast(w) >= 0) {
        removed = true;
    }
    // logf("WndMapRemoveWnd: failed to remove w: 0x%p\n", w);
    return removed;
}

static void WndListAdd(Wnd* w) {
    bool report = WndListRemove(w);
    ReportIfQuick(report);
    gWndList.Append(w);
}

//- Taskbar.cpp

const DWORD WM_TASKBARCALLBACK = WM_APP + 0x15;
const DWORD WM_TASKBARCREATED = ::RegisterWindowMessage(L"TaskbarCreated");
const DWORD WM_TASKBARBUTTONCREATED = ::RegisterWindowMessage(L"TaskbarButtonCreated");

//- Window.h / Window.cpp

const WCHAR* kDefaultClassName = L"SumatraWgDefaultWinClass";

static LRESULT CALLBACK WndWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // seen crashes in TabCtrl::WndProc() which might be caused by handling drag&drop messages
    // after parent window was destroyed. maybe this will fix it
    if (!IsWindow(hwnd)) {
        return 0;
    }

    Wnd* wnd = WndListFindByHwnd(hwnd);

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)(lparam);
        ReportIf(wnd);
        wnd = (Wnd*)(cs->lpCreateParams);
        wnd->hwnd = hwnd;
        WndListAdd(wnd);
    }

    if (wnd) {
        return wnd->WndProc(hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

static LRESULT CALLBACK WndSubclassedWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                                DWORD_PTR data) {
    return WndWindowProc(hwnd, msg, wp, lp);
}

Wnd::Wnd() {
    // instance = GetModuleHandleW(nullptr);
    kind = kindWnd;
}

Wnd::~Wnd() {
    Destroy();
    delete layout;
    DeleteBrushSafe(&bgBrush);
}

Kind Wnd::GetKind() {
    return kind;
}

void Wnd::SetText(const char* s) {
    if (!s) {
        s = "";
    }
    HwndSetText(hwnd, s);
    HwndRepaintNow(hwnd); // TODO: move inside HwndSetText()?
}

TempStr Wnd::GetTextTemp() {
    char* s = HwndGetTextTemp(hwnd);
    return s;
}

void Wnd::SetVisibility(Visibility newVisibility) {
    ReportIf(!hwnd);
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
    // the order is important
    // stop dispatching messages to this Wnd
    WndListRemove(this);
    // unsubclass while hwnd is still valid
    UnSubclass();
    // finally destroy hwnd
    HwndDestroyWindowSafe(&hwnd);
}

LRESULT Wnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

// This function is called when a window is attached to Wnd.
// Override it to automatically perform tasks when the window is attached.
// Note:  Window controls are attached.
void Wnd::OnAttach() {
}

void Wnd::OnFocus() {
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
    font = ::CreateFontIndirectW(&logfont);
    ::SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    */

    return 0;
}

void Wnd::OnContextMenu(Point ptScreen) {
    if (!onContextMenu.IsValid()) {
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
    ContextMenuEvent ev;
    ev.w = this;
    ev.mouseScreen = ptScreen;

    POINT ptW = ToPOINT(ptScreen);
    if (ptScreen.x != -1) {
        MapWindowPoints(HWND_DESKTOP, hwnd, &ptW, 1);
    }
    ev.mouseWindow.x = ptW.x;
    ev.mouseWindow.y = ptW.y;
    onContextMenu.Call(&ev);
}

void Wnd::OnDropFiles(HDROP drop_info) {
}

void Wnd::OnGetMinMaxInfo(MINMAXINFO* mmi) {
}

LRESULT Wnd::OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam) {
    return -1;
}

void Wnd::OnMove(POINTS*) {
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
    auto br = BackgroundBrush();
    if (br != nullptr) {
        FillRect(hdc, &ps->rcPaint, br);
    }
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
    ReportIf(!::IsWindow(hwnd));
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

    auto r = ToRECT(bounds);
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

    Wnd* pWnd = WndListFindByHwnd(wnd);
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
            Wnd* pWnd = WndListFindByHwnd(reinterpret_cast<HWND>(lparam));
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
            Wnd* wndFrom = WndListFindByHwnd(from);
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
            Wnd* pWnd = WndListFindByHwnd(reinterpret_cast<HWND>(lparam));
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

    WmEvent e{hwnd, msg, wparam, lparam, this->userData, this};

    if (msg == WM_CLOSE) {
        if (onClose.IsValid()) {
            Wnd::CloseEvent ev;
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
            return 0;
        }

        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            Wnd* pWnd = WndListFindByHwnd(reinterpret_cast<HWND>(lparam));
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

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            Wnd* wndFrom = WndListFindByHwnd(from);

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
                // TODO: for now those are the same because LabelWithCloseWnd::OnPaint
                // assumes ps is provided (and maybe others)
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
            if (result != 0)
                return result; // Message processed so return.
        } break;               // Do default processing when message not already processed.

        case WM_DROPFILES: {
            OnDropFiles(reinterpret_cast<HDROP>(wparam));
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            SIZE size = {};
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
            Point ptScreen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            // Note: HWND in wparam might be a child window
            OnContextMenu(ptScreen);
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
    if (subclassId) {
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    } else {
        // TODO: also DefSubclassProc?
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

bool Wnd::PreTranslateMessage(MSG& msg) {
    return false;
}

void Wnd::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(WndListFindByHwnd(hwnd));

    this->hwnd = hwnd;
    Subclass();
    OnAttach();
}

// Attaches a CWnd object to a dialog item.
void Wnd::AttachDlgItem(UINT id, HWND parent) {
    ReportIf(!::IsWindow(parent));
    HWND wnd = ::GetDlgItem(parent, id);
    Attach(wnd);
}

HWND Wnd::Detach() {
    UnSubclass();

    HWND wnd = hwnd;
    WndListRemove(this);
    hwnd = nullptr;
    return wnd;
}

void Wnd::Cleanup() {
    WndListRemove(this);
    hwnd = nullptr;
    subclassId = 0;
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
    wc.lpfnWndProc = WndWindowProc;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ATOM atom = ::RegisterClassExW(&wc);
    ReportIf(!atom);
}

HWND Wnd::CreateControl(const CreateControlArgs& args) {
    ReportIf(!args.className);
    // TODO: validate that className is one of the known controls?

    font = args.font;
    if (!font) {
        // TODO: need this?
        font = GetDefaultGuiFont();
    }

    DWORD style = args.style;
    if (args.parent) {
        style |= WS_CHILD;
    }
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
    void* createParams = this;
    hwnd = ::CreateWindowExW(exStyle, className, L"", style, x, y, dx, dy, parent, id, inst, createParams);
    HwndSetFont(hwnd, font);
    ReportIf(!hwnd);

    // TODO: validate that
    Subclass();
    OnAttach();

    // prevWindowProc(hwnd, WM_SETFONT, (WPARAM)f, 0);
    // HwndSetFont(hwnd, f);
    if (args.text) {
        SetText(args.text);
    }
    return hwnd;
}

HWND Wnd::CreateCustom(const CreateCustomArgs& args) {
    font = args.font;

    const WCHAR* className = args.className;
    // TODO: validate className is not win32 control class
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
    ReportIf(args.menu && args.cmdId);
    HMENU m = args.menu;
    if (m == nullptr) {
        m = (HMENU)(INT_PTR)args.cmdId;
    }
    HINSTANCE inst = GetInstance();
    void* createParams = this;
    WCHAR* titleW = ToWStrTemp(args.title);

    HWND hwndTmp = ::CreateWindowExW(exStyle, className, titleW, style, x, y, dx, dy, parent, m, inst, createParams);

    ReportIf(!hwndTmp);
    // hwnd should be assigned in WM_CREATE
    ReportIf(hwndTmp != hwnd);
    ReportIf(this != WndListFindByHwnd(hwndTmp));
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

// if only top given, set them all to top
// if top, right given, set bottom to top and left to right
void Wnd::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(hwnd, top, right, bottom, left);
}

void Wnd::Subclass() {
    ReportIf(!IsWindow(hwnd));
    ReportIf(subclassId); // don't subclass multiple times
    if (subclassId) {
        return;
    }
    WndListAdd(this);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, WndSubclassedWindowProc, subclassId, (DWORD_PTR)this);
    ReportIf(!ok);
}

void Wnd::UnSubclass() {
    if (!subclassId) {
        return;
    }
    RemoveWindowSubclass(hwnd, WndSubclassedWindowProc, subclassId);
    subclassId = 0;
}

HFONT Wnd::GetFont() {
    return font;
}

void Wnd::SetFont(HFONT fontIn) {
    font = fontIn;
    // TODO: for controls, send WM_SETFONT message to original wndproc function
}

void Wnd::SetIsEnabled(bool isEnabled) const {
    ReportIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool Wnd::IsEnabled() const {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void Wnd::SetColors(COLORREF textCol, COLORREF bgCol) {
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

HBRUSH Wnd::BackgroundBrush() {
    if (bgBrush == nullptr) {
        if (bgColor != kColorUnset) {
            bgBrush = CreateSolidBrush(bgColor);
        }
    }
    return bgBrush;
}

void Wnd::SuspendRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
}

void Wnd::ResumeRedraw() const {
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
        auto wnd = WndListFindByHwnd(hwnd);
        if (wnd && wnd->PreTranslateMessage(msg)) {
            return true;
        }
    }
    return false;
}

void SizeToIdealSize(Wnd* wnd) {
    if (!wnd || !wnd->hwnd) {
        return;
    }
    auto size = wnd->GetIdealSize();
    // TODO: don't change x,y, only dx/dy
    RECT r{0, 0, size.dx, size.dy};
    wnd->SetBounds(r);
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
void PositionCloseTo(Wnd* w, HWND hwnd) {
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
HWND g_currentModelessDialog = nullptr;

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
    if (HwndIsRtl(hwnd)) {
        g.ScaleTransform(-1, 1);
        g.TranslateTransform((float)ClientRect(hwnd).dx, 0, Gdiplus::MatrixOrderAppend);
    }
    Gdiplus::Color c;

    // in onhover state, background is a red-ish circle
    if (args.isHover) {
        c.SetFromCOLORREF(args.colHoverBg);
        Gdiplus::SolidBrush b(c);
        g.FillEllipse(&b, r.x, r.y, r.dx - 2, r.dy - 2);
    }

    // draw 'x'
    c.SetFromCOLORREF(args.isHover ? args.colXHover : args.colX);
    g.TranslateTransform((float)r.x, (float)r.y);
    Gdiplus::Pen p(c, 2);
    if (isHover) {
        g.DrawLine(&p, Gdiplus::Point(4, 4), Gdiplus::Point(r.dx - 6, r.dy - 6));
        g.DrawLine(&p, Gdiplus::Point(r.dx - 6, 4), Gdiplus::Point(4, r.dy - 6));
    } else {
        g.DrawLine(&p, Gdiplus::Point(4, 5), Gdiplus::Point(r.dx - 6, r.dy - 5));
        g.DrawLine(&p, Gdiplus::Point(r.dx - 6, 5), Gdiplus::Point(4, r.dy - 5));
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
        FillRect(hdc, &r2, brush);
        // Ellipse(hdc, r2.left, r2.top, r2.right, r2.bottom);
    }
    AutoDeletePen pen(CreatePen(PS_SOLID, 2, lineCol));
    ScopedSelectPen p(hdc, pen);
    MoveToEx(hdc, r.x, r.y, nullptr);
    LineTo(hdc, r.x + r.dx, r.y + r.dy);

    MoveToEx(hdc, r.x + r.dx, r.y, nullptr);
    LineTo(hdc, r.x, r.y + r.dy);
}
