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

// ControlBase is the base of the controls a layout positions. It is deliberately
// independent of WindowBase: the two share the shape of the win32 plumbing but
// little else, and having one inherit the other meant every control carried a
// top-level window's close / taskbar / drop-files machinery, and every window
// carried a control's layout

static Kind kindControl = "control";

// its own class, so a control's window proc is never installed on a window's
// class (they share the CreateCustom code but not the window list)
static const WStr kControlClassName = L"SumatraWgControlClass";

static Vec<ControlBase*> gControlList;

ControlBase* ControlFromHwnd(HWND hwnd) {
    for (auto& c : gControlList) {
        if (c->hwnd == hwnd) {
            if (HwndWasDestroyed(hwnd)) {
                return nullptr;
            }
            return c;
        }
    }
    return nullptr;
}

static bool ControlListRemove(ControlBase* c) {
    bool removed = false;
    while (gControlList.RemoveFast(c) >= 0) {
        removed = true;
    }
    return removed;
}

static void ControlListAdd(ControlBase* c) {
    bool report = ControlListRemove(c);
    ReportIfFast(report);
    gControlList.Append(c);
}

static LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // seen crashes that might come from messages arriving after the parent
    // window was destroyed
    if (!IsWindow(hwnd)) {
        return 0;
    }

    ControlBase* c = ControlFromHwnd(hwnd);

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)(lparam);
        ReportIf(c);
        c = (ControlBase*)(cs->lpCreateParams);
        c->hwnd = hwnd;
        ControlListAdd(c);
    }

    if (c) {
        return c->WndProc(hwnd, msg, wparam, lparam);
    }
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK ControlSubclassedWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                                    DWORD_PTR /*data*/) {
    return ControlWindowProc(hwnd, msg, wp, lp);
}

ControlBase::ControlBase() {
    kind = kindControl;
}

ControlBase::~ControlBase() {
    Destroy();
    // the tree first: a virtual control tells its root it's going away
    delete layout;
    delete vroot;
    DeleteBrushSafe(&bgBrush);
}

// ILayout
Kind ControlBase::GetKind() {
    return kind;
}

void ControlBase::SetText(Str s) {
    if (!s) {
        s = StrL("");
    }
    HwndSetText(hwnd, s);
    HwndRepaintNow(hwnd); // TODO: move inside HwndSetText()?
}

TempStr ControlBase::GetTextTemp() {
    return HwndGetTextTemp(hwnd);
}

void ControlBase::SetVisibility(Visibility newVisibility) {
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

Visibility ControlBase::GetVisibility() {
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

void ControlBase::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool ControlBase::IsVisible() const {
    return visibility == Visibility::Visible;
}

void ControlBase::Destroy() {
    // the order is important
    // stop dispatching messages to this WindowBase
    ControlListRemove(this);
    // unsubclass while hwnd is still valid
    UnSubclass();
    // finally destroy hwnd
    HwndDestroyWindowSafe(&hwnd);
}

// over-ride those to hook into message processing
LRESULT ControlBase::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

// This function is called when a window is attached to WindowBase.
// Override it to automatically perform tasks when the window is attached.
// Note:  Window controls are attached.
void ControlBase::OnAttach() {}

void ControlBase::OnFocus() {}

// Override this to handle WM_COMMAND messages
bool ControlBase::OnCommand(WPARAM /*wparam*/, LPARAM /*lparam*/) {
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
int ControlBase::OnCreate(CREATESTRUCT* /*cs*/) {
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

void ControlBase::OnContextMenu(Point ptScreen) {
    if (!onContextMenu.IsValid()) {
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
    ContextMenuEvent ev;
    ev.w = this;
    ev.mouseScreen = ptScreen;

    Point ptW = ptScreen;
    if (ptScreen.x != -1) {
        ptW = HwndMapWindowPoint(HWND_DESKTOP, hwnd, ptW);
    }
    ev.mouseWindow = ptW;
    onContextMenu.Call(&ev);
}

LRESULT ControlBase::OnMouseEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    return -1;
}

// Processes notification (WM_NOTIFY) messages from a child window.
LRESULT ControlBase::OnNotify(int /*controlId*/, NMHDR* /*nmh*/) {
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
LRESULT ControlBase::OnNotifyReflect(WPARAM /*wparam*/, LPARAM /*lparam*/) {
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

void ControlBase::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
    auto* br = BackgroundBrush();
    if (br != nullptr) {
        HdcFillRect(hdc, ToRect(ps->rcPaint), br);
    }
}

void ControlBase::OnSize(UINT msg, UINT type, Size size) {}

void ControlBase::OnTimer(UINT_PTR timerId) {}

// This function processes those special messages sent by some older controls,
// and reflects them back to the originating CWnd object.
// Override this function in your derived class to handle these special messages:
// WM_COMMAND, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORDLG, WM_CTLCOLORLISTBOX,
// WM_CTLCOLORSCROLLBAR, WM_CTLCOLORSTATIC, WM_CHARTOITEM,  WM_VKEYTOITEM,
// WM_HSCROLL, WM_VSCROLL, WM_DRAWITEM, WM_MEASUREITEM, WM_DELETEITEM,
// WM_COMPAREITEM, WM_PARENTNOTIFY.
LRESULT ControlBase::OnMessageReflect(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
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

Size ControlBase::GetIdealSize() {
    return {};
}

Size ControlBase::Layout(const Constraints bc) {
    dbglayout(fmt("WindowBase::Layout() %s ", Str(GetKind())));
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

int ControlBase::MinIntrinsicHeight(int /*width*/) {
#if 0
    auto vinset = insets.top + insets.bottom;
    Size s = GetIdealSize();
    return s.dy + vinset;
#else
    Size s = GetIdealSize();
    return s.dy;
#endif
}

int ControlBase::MinIntrinsicWidth(int /*height*/) {
#if 0
    auto hinset = insets.left + insets.right;
    Size s = GetIdealSize();
    return s.dx + hinset;
#else
    Size s = GetIdealSize();
    return s.dx;
#endif
}

void ControlBase::DoLayout(Size size) {
    LayoutTreeToSize(hwnd, layout, size, &vroot);
}

void ControlBase::SetPos(Rect* r) {
    HwndMoveWindow(hwnd, r);
}

ControlBase* ControlBase::AsControl() {
    return this;
}

void ControlBase::SetBounds(Rect bounds) {
    dbglayout(fmt("WindowBaseLayout:SetBounds() %s %d,%d - %d, %d\n", Str(GetKind()), bounds.x, bounds.y, bounds.dx,
                  bounds.dy));

    lastBounds = bounds;

    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);

    HwndMoveWindow(hwnd, &bounds);
    // TODO: optimize if doesn't change position
    HwndInvalidate(hwnd, true);
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT ControlBase::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
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

LRESULT ControlBase::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    WmEvent e{hwnd, msg, wparam, lparam, this->userData, this};

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
            if (result != 0) return result; // Message processed so return.
        } break;                            // Do default processing when message not already processed.

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            Size size{};
            OnSize(msg, 0, size);
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
        case WM_CONTEXTMENU: {
            Point ptScreen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            // Note: HWND in wparam might be a child window
            OnContextMenu(ptScreen);
            break;
        }

        case WM_SIZE: {
            Size size = {LOWORD(lparam), HIWORD(lparam)};
            OnSize(msg, static_cast<UINT>(wparam), size);
            break;
        }
        case WM_TIMER: {
            OnTimer(static_cast<UINT>(wparam));
            break;
        }
    }

    // Now hand all messages to the default procedure.
    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT ControlBase::FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (subclassId) {
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    } else {
        // TODO: also DefSubclassProc?
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

void ControlBase::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(ControlFromHwnd(hwnd));

    this->hwnd = hwnd;
    Subclass();
    OnAttach();
}

// Attaches a CWnd object to a dialog item.
void ControlBase::AttachDlgItem(UINT id, HWND parent) {
    ReportIf(!::IsWindow(parent));
    HWND wnd = ::GetDlgItem(parent, (int)id);
    Attach(wnd);
}

HWND ControlBase::Detach() {
    UnSubclass();

    HWND wnd = hwnd;
    ControlListRemove(this);
    hwnd = nullptr;
    return wnd;
}

static void ControlRegisterClass(WStr className) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetInstance();
    wc.style = CS_DBLCLKS;
    wc.lpszClassName = className.s;
    wc.lpfnWndProc = ControlWindowProc;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ::RegisterClassExW(&wc);
}

HWND ControlBase::CreateControl(const CreateControlArgs& args) {
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
    if (args.isRtl) {
        exStyle |= WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    }
    int x = args.pos.x;
    int y = args.pos.y;
    int dx = args.pos.dx;
    int dy = args.pos.dy;
    HWND parent = args.parent;
    HMENU id = args.ctrlId;
    HINSTANCE inst = GetInstance();
    void* createParams = this;
    hwnd = ::CreateWindowExW(exStyle, args.className.s, L"", style, x, y, dx, dy, parent, id, inst, createParams);
    ReportIf(!hwnd);
    if (!hwnd) {
        return nullptr;
    }
    HwndSetFont(hwnd, font);

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

HWND ControlBase::CreateCustom(const CreateCustomArgs& args) {
    font = args.font;

    WStr className = args.className ? args.className : kControlClassName;
    // TODO: validate className is not win32 control class
    ControlRegisterClass(className);
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
    ReportIf(this != ControlFromHwnd(hwndTmp));
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

void ControlBase::SetInsetsPt(int uniform) {
    insets = DpiScaledInsets(hwnd, uniform);
}

void ControlBase::SetInsetsPt(int topBottom, int leftRight) {
    insets = DpiScaledInsets(hwnd, topBottom, leftRight);
}

void ControlBase::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(hwnd, top, right, bottom, left);
}

void ControlBase::Subclass() {
    ReportIf(!IsWindow(hwnd));
    ReportIf(subclassId); // don't subclass multiple times
    if (subclassId) {
        return;
    }
    ControlListAdd(this);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, ControlSubclassedWindowProc, subclassId, (DWORD_PTR)this);
    if (!ok) {
        // can fail under low memory / desktop heap exhaustion (it allocates and
        // attaches a window property), so don't assert. Reset subclassId so that
        // `subclassId != 0` keeps meaning "is subclassed".
        logf("ControlBase::Subclass: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
        subclassId = 0;
    }
}

void ControlBase::UnSubclass() {
    if (!subclassId) {
        return;
    }
    RemoveWindowSubclass(hwnd, ControlSubclassedWindowProc, subclassId);
    subclassId = 0;
}

HFONT ControlBase::GetFont() {
    return font;
}

// HwndSetFont() sends WM_SETFONT, which our wndproc records in `font` and (for
// subclassed controls) forwards to the control itself, so this both remembers
// and applies the font. Without it SetFont() was a no-op on screen.
void ControlBase::SetFont(HFONT fontIn) {
    font = fontIn;
    if (!hwnd) {
        return;
    }
    HwndSetFont(hwnd, fontIn);
}

void ControlBase::SetIsEnabled(bool isEnabled) const {
    ReportIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool ControlBase::IsEnabled() const {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void ControlBase::SetColors(COLORREF textCol, COLORREF bgCol) {
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

HBRUSH ControlBase::BackgroundBrush() {
    if (bgBrush == nullptr) {
        if (bgColor != kColorUnset) {
            bgBrush = CreateSolidBrush(bgColor);
        }
    }
    return bgBrush;
}

void ControlBase::SuspendRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
}

void ControlBase::ResumeRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
}

// size a control to what it says it wants
void SizeToIdealSize(ControlBase* c) {
    if (!c || !c->hwnd) {
        return;
    }
    auto size = c->GetIdealSize();
    // TODO: don't change x,y, only dx/dy
    RECT r{0, 0, size.dx, size.dy};
    c->SetBounds(r);
}

