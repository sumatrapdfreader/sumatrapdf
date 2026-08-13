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
        if (c->onWndProc.IsValid()) {
            ControlBase::WndProcEvent ev;
            ev.w = c;
            ev.hwnd = hwnd;
            ev.msg = msg;
            ev.wparam = wparam;
            ev.lparam = lparam;
            c->onWndProc.Call(&ev);
            if (ev.didHandle) {
                return ev.result;
            }
        }
        DpiSetFromHwnd(hwnd);
        return c->WndProcDefault(hwnd, msg, wparam, lparam);
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

bool ControlBase::DispatchCommand(WPARAM wparam, LPARAM lparam) {
    if (!onCommand.IsValid()) {
        return false;
    }
    CommandEvent ev;
    ev.w = this;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onCommand.Call(&ev);
    return ev.didHandle;
}

LRESULT ControlBase::DispatchMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (!onMessageReflect.IsValid()) {
        return 0;
    }
    MessageReflectEvent ev;
    ev.w = this;
    ev.msg = msg;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onMessageReflect.Call(&ev);
    return ev.result;
}

LRESULT ControlBase::DispatchNotifyReflect(WPARAM wparam, LPARAM lparam) {
    if (!onNotifyReflect.IsValid()) {
        return 0;
    }
    NotifyReflectEvent ev;
    ev.w = this;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onNotifyReflect.Call(&ev);
    return ev.result;
}

static void ControlBaseDefaultPaint(ControlBase* w, HDC hdc, PAINTSTRUCT* ps) {
    auto* br = w->BackgroundBrush();
    if (br != nullptr) {
        HdcFillRect(hdc, ToRect(ps->rcPaint), br);
    }
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
                didHandle = pWnd->DispatchCommand(wparam, lparam);
            }

            // Handle user commands.
            if (!didHandle) {
                didHandle = DispatchCommand(wparam, lparam);
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

        case WM_DPICHANGED: {
            DpiSet((int)LOWORD(wparam), (int)HIWORD(wparam));
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
            // false: claim handled so DefWindowProc / DefSubclassProc does not
            // fill (controls that paint the full client set shouldEraseBackground)
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
                    PaintEvent pev;
                    pev.w = this;
                    pev.hdc = hdc;
                    pev.ps = &ps;
                    onPaint.Call(&pev);
                } else {
                    ControlBaseDefaultPaint(this, hdc, &ps);
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

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            if (onSize.IsValid()) {
                SizeEvent sev;
                sev.w = this;
                sev.msg = msg;
                onSize.Call(&sev);
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
                MouseEvent mev;
                mev.w = this;
                mev.msg = msg;
                mev.wparam = wparam;
                mev.lparam = lparam;
                onMouseEvent.Call(&mev);
                if (mev.result != -1) {
                    return mev.result;
                }
            }
            break;
        }
        case WM_CONTEXTMENU: {
            if (onContextMenu.IsValid()) {
                Point ptScreen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
                ContextMenuEvent cev;
                cev.w = this;
                cev.mouseScreen = ptScreen;
                Point ptW = ptScreen;
                if (ptScreen.x != -1) {
                    ptW = HwndMapWindowPoint(HWND_DESKTOP, hwnd, ptW);
                }
                cev.mouseWindow = ptW;
                onContextMenu.Call(&cev);
            }
            break;
        }

        case WM_SIZE: {
            DpiSetFromHwnd(hwnd);
            if (onSize.IsValid()) {
                SizeEvent sev;
                sev.w = this;
                sev.msg = msg;
                sev.type = static_cast<UINT>(wparam);
                sev.size = {LOWORD(lparam), HIWORD(lparam)};
                onSize.Call(&sev);
            }
            break;
        }
        case WM_TIMER: {
            if (onTimer.IsValid()) {
                TimerEvent tev;
                tev.w = this;
                tev.timerId = static_cast<UINT_PTR>(wparam);
                onTimer.Call(&tev);
            }
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
    DpiSetFromHwnd(hwnd);
    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }
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
    wc.hCursor = GetCachedCursor(IDC_ARROW);
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
    DpiSetFromHwnd(hwnd);

    // TODO: validate that
    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }

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

    DpiSetFromHwnd(hwnd);
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
    insets = DpiScaledInsets(uniform);
}

void ControlBase::SetInsetsPt(int topBottom, int leftRight) {
    insets = DpiScaledInsets(topBottom, leftRight);
}

void ControlBase::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(top, right, bottom, left);
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
