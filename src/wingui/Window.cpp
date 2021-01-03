

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"
#include "utils/VecSegmented.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

// TODO: call RemoveWindowSubclass in WM_NCDESTROY as per
// https://devblogs.microsoft.com/oldnewthing/20031111-00/?p=41883

#define DEFAULT_WIN_CLASS L"WC_WIN32_WINDOW"

static UINT_PTR g_subclassId = 0;

UINT_PTR NextSubclassId() {
    g_subclassId++;
    return g_subclassId;
}

// initial value which should be save
static int g_currCtrlID = 100;

int GetNextCtrlID() {
    ++g_currCtrlID;
    return g_currCtrlID;
}

// a way to register for messages for a given hwnd / msg combo

struct HwndMsgHandler {
    HWND hwnd;
    UINT msg;
    void* user = nullptr;
    void (*handler)(void* user, WndEvent* ev);
};

VecSegmented<HwndMsgHandler> gHwndMsgHandlers;

void WindowCleanup() {
    gHwndMsgHandlers.allocator.FreeAll();
}

static void ClearHwndMsgHandler(HwndMsgHandler* h) {
    CrashIf(!h);
    if (!h) {
        return;
    }
    h->hwnd = nullptr;
    h->msg = 0;
    h->user = nullptr;
    h->handler = nullptr;
}

static HwndMsgHandler* FindHandlerForHwndAndMsg(HWND hwnd, UINT msg, bool create) {
    CrashIf(hwnd == nullptr);
    for (auto h : gHwndMsgHandlers) {
        if (h->hwnd == hwnd && h->msg == msg) {
            return h;
        }
    }
    if (!create) {
        return nullptr;
    }
    // we might have free slot
    HwndMsgHandler* res = nullptr;
    for (auto h : gHwndMsgHandlers) {
        if (h->hwnd == nullptr) {
            res = h;
            break;
        }
    }
    if (!res) {
        res = gHwndMsgHandlers.AllocAtEnd();
    }
    ClearHwndMsgHandler(res);
    res->hwnd = hwnd;
    res->msg = msg;
    return res;
}

void RegisterHandlerForMessage(HWND hwnd, UINT msg, void (*handler)(void* user, WndEvent*), void* user) {
    auto h = FindHandlerForHwndAndMsg(hwnd, msg, true);
    h->handler = handler;
    h->user = user;
}

void UnregisterHandlerForMessage(HWND hwnd, UINT msg) {
    auto h = FindHandlerForHwndAndMsg(hwnd, msg, false);
    ClearHwndMsgHandler(h);
}

static void UnregisterHandlersForHwnd(HWND hwnd) {
    for (auto h : gHwndMsgHandlers) {
        if (h->hwnd == hwnd) {
            ClearHwndMsgHandler(h);
        }
    }
}

// TODO: potentially more messages
// https://docs.microsoft.com/en-us/cpp/mfc/reflected-window-message-ids?view=vs-2019
static HWND GetChildHWNDForMessage(UINT msg, WPARAM wp, LPARAM lp) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorbtn
    if (WM_CTLCOLORBTN == msg) {
        return (HWND)lp;
    }
    if (WM_CTLCOLORSTATIC == msg) {
        HDC hdc = (HDC)wp;
        return WindowFromDC(hdc);
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-notify
    if (WM_NOTIFY == msg) {
        NMHDR* hdr = (NMHDR*)lp;
        return hdr->hwndFrom;
    }
    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-command
    if (WM_COMMAND == msg) {
        return (HWND)lp;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-drawitem
    if (WM_DRAWITEM == msg) {
        DRAWITEMSTRUCT* s = (DRAWITEMSTRUCT*)lp;
        return s->hwndItem;
    }
    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
    if (WM_CONTEXTMENU == msg) {
        return (HWND)wp;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
    if (WM_VSCROLL == msg) {
        return (HWND)lp;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-hscroll--trackbar-
    if (WM_HSCROLL == msg) {
        return (HWND)lp;
    }

    // TODO: there's no HWND so have to do it differently e.g. allocate
    // unique CtlID, store it in WindowBase and compare that
#if 0
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-measureitem
    if (WM_MEASUREITEM == msg) {
        MEASUREITEMSTRUCT* s = (MEASUREITEMSTRUCT*)lp;
        return s->CtlID;
    }
#endif
    return nullptr;
}

bool HandleRegisteredMessages(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    HWND hwndLookup = hwnd;
    HWND hwndMaybe = GetChildHWNDForMessage(msg, wp, lp);
    if (hwndMaybe != nullptr) {
        hwndLookup = hwndMaybe;
    }
    auto h = FindHandlerForHwndAndMsg(hwndLookup, msg, false);
    if (!h) {
        return false;
    }
    WndEvent ev;
    SetWndEventSimple(ev);
    h->handler(h->user, &ev);
    res = ev.result;
    return ev.didHandle;
}

// to ensure we never overflow control ids
// we reset the counter in Window::Window(),
// because ids only need to be unique within window
// this works as long as we don't interleave creation
// of windows and controls in those windows
void ResetCtrlID() {
    g_currCtrlID = 100;
}

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

CopyWndEvent::CopyWndEvent(WndEvent* dst, WndEvent* src) {
    this->dst = dst;
    this->src = src;
    dst->hwnd = src->hwnd;
    dst->msg = src->msg;
    dst->lp = src->lp;
    dst->wp = src->wp;
    dst->w = src->w;
}

CopyWndEvent::~CopyWndEvent() {
    src->didHandle = dst->didHandle;
    src->result = dst->result;
}

Kind kindWindowBase = "windowBase";

static LRESULT wndBaseProcDispatch(WindowBase* w, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    CrashIf(hwnd != w->hwnd);

    // or maybe get rid of WindowBase::WndProc and use msgFilterInternal
    // when per-control custom processing is needed
    if (w->msgFilter) {
        WndEvent ev{};
        SetWndEvent(ev);
        w->msgFilter(&ev);
        if (ev.didHandle) {
            didHandle = true;
            return ev.result;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorbtn
    if (WM_CTLCOLORBTN == msg) {
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            didHandle = true;
            return (LRESULT)bgBrush;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorstatic
    if (WM_CTLCOLORSTATIC == msg) {
        HDC hdc = (HDC)wp;
        if (w->textColor != ColorUnset) {
            SetTextColor(hdc, w->textColor);
            // SetTextColor(hdc, RGB(255, 255, 255));
            didHandle = true;
        }
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            SetBkMode(hdc, TRANSPARENT);
            didHandle = true;
            return (LRESULT)bgBrush;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-size
    if (WM_SIZE == msg) {
        if (!w->onSize) {
            return 0;
        }
        SizeEvent ev;
        SetWndEvent(ev);
        ev.dx = LOWORD(lp);
        ev.dy = HIWORD(lp);
        w->onSize(&ev);
        if (ev.didHandle) {
            didHandle = true;
            return 0;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-command
    if (WM_COMMAND == msg) {
        if (!w->onWmCommand) {
            return 0;
        }
        WmCommandEvent ev{};
        SetWndEvent(ev);
        ev.id = LOWORD(wp);
        ev.ev = HIWORD(wp);
        w->onWmCommand(&ev);
        if (ev.didHandle) {
            didHandle = true;
            return ev.result;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-keydown
    // https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-keyup
    if ((WM_KEYUP == msg) || (WM_KEYDOWN == msg)) {
        if (!w->onKeyDownUp) {
            return 0;
        }
        KeyEvent ev{};
        SetWndEvent(ev);
        ev.isDown = (WM_KEYDOWN == msg);
        ev.keyVirtCode = (int)wp;
        w->onKeyDownUp(&ev);
        if (ev.didHandle) {
            didHandle = true;
            // 0 means: did handle
            return 0;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-char
    if (WM_CHAR == msg) {
        if (!w->onChar) {
            return 0;
        }
        CharEvent ev{};
        SetWndEvent(ev);
        ev.keyCode = (int)wp;
        w->onChar(&ev);
        if (ev.didHandle) {
            didHandle = true;
            // 0 means: did handle
            return 0;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        if (!w->onMouseWheel) {
            return 0;
        }
        MouseWheelEvent ev{};
        SetWndEvent(ev);
        ev.isVertical = (msg == WM_MOUSEWHEEL);
        ev.delta = GET_WHEEL_DELTA_WPARAM(wp);
        ev.keys = GET_KEYSTATE_WPARAM(wp);
        ev.x = GET_X_LPARAM(lp);
        ev.y = GET_Y_LPARAM(lp);
        w->onMouseWheel(&ev);
        if (ev.didHandle) {
            didHandle = true;
            return 0;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/shell/wm-dropfiles
    if (msg == WM_DROPFILES) {
        if (!w->onDropFiles) {
            return 0;
        }

        DropFilesEvent ev{};
        SetWndEvent(ev);
        ev.hdrop = (HDROP)wp;
        // TODO: docs say it's always zero but sumatra code elsewhere
        // treats 0 and 1 differently
        CrashIf(lp != 0);
        w->onDropFiles(&ev);
        if (ev.didHandle) {
            didHandle = true;
            return 0; // 0 means: did handle
        }
    }

    // handle the rest in WndProc
    WndEvent ev{};
    SetWndEvent(ev);
    w->WndProc(&ev);
    didHandle = ev.didHandle;
    return ev.result;
}

static LRESULT CALLBACK wndProcCustom(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // auto msgName = GetWinMessageName(msg);
    // dbglogf("hwnd: 0x%6p, msg: 0x%03x (%s), wp: 0x%x\n", hwnd, msg, msgName, wp);

    if (WM_NCCREATE == msg) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        Window* w = (Window*)cs->lpCreateParams;
        w->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    LRESULT res = 0;
    if (HandleRegisteredMessages(hwnd, msg, wp, lp, res)) {
        return res;
    }

    // TODDO: a hack, a Window might be deleted when we get here
    // happens e.g. when we call CloseWindow() inside
    // wndproc. Maybe instead of calling DestroyWindow()
    // we should delete WindowInfo, for proper shutdown sequence
    if (WM_DESTROY == msg) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Window* w = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!w) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // this is the last message ever received by hwnd
    // TODO: move it to wndBaseProcDispatch? Maybe they don't
    // need WM_*DESTROY notifications?
    if (WM_NCDESTROY == msg) {
        if (w->onDestroy) {
            WindowDestroyEvent ev{};
            SetWndEvent(ev);
            ev.window = w;
            w->onDestroy(&ev);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // TODO: should this go into WindowBase?
    if (WM_CLOSE == msg) {
        if (w->onClose) {
            WindowCloseEvent ev{};
            SetWndEvent(ev);
            w->onClose(&ev);
            if (ev.cancel) {
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (w->isDialog) {
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

    if (WM_PAINT == msg) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            FillRect(ps.hdc, &ps.rcPaint, bgBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    bool didHandle = false;
    res = wndBaseProcDispatch(w, hwnd, msg, wp, lp, didHandle);
    if (didHandle) {
        return res;
    }
    res = DefWindowProcW(hwnd, msg, wp, lp);
    // auto msgName = GetWinMessageName(msg);
    // dbglogf("hwnd: 0x%6p, msg: 0x%03x (%s), wp: 0x%x, res: 0x%x\n", hwnd, msg, msgName, wp, res);
    return res;
}

static LRESULT CALLBACK wndProcSubclassed(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                          DWORD_PTR dwRefData) {
    CrashIf(dwRefData == 0);
    WindowBase* w = (WindowBase*)dwRefData;

    if (uIdSubclass != w->subclassId) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    LRESULT res = 0;
    if (HandleRegisteredMessages(hwnd, msg, wp, lp, res)) {
        return res;
    }

    bool didHandle = false;
    res = wndBaseProcDispatch(w, hwnd, msg, wp, lp, didHandle);
    if (didHandle) {
        return res;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// TODO: do I need WM_CTLCOLORSTATIC?
#if 0
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorstatic
    if (WM_CTLCOLORSTATIC == msg) {
        HDC hdc = (HDC)wp;
        if (w->textColor != ColorUnset) {
            SetTextColor(hdc, w->textColor);
        }
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            // SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)bgBrush;
        }
    }
#endif

void WindowBase::Subclass() {
    CrashIf(!hwnd);
    WindowBase* wb = this;
    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, wndProcSubclassed, subclassId, (DWORD_PTR)wb);
    CrashIf(!ok);
    if (!ok) {
        subclassId = 0;
    }
}

void WindowBase::Unsubclass() {
    if (subclassId) {
        RemoveWindowSubclass(hwnd, wndProcSubclassed, subclassId);
        subclassId = 0;
    }
}

WindowBase::WindowBase(HWND p) {
    kind = kindWindowBase;
    parent = p;
    ctrlID = GetNextCtrlID();
}

// generally not needed for child controls as they are destroyed when
// a parent is destroyed
void WindowBase::Destroy() {
    auto tmp = hwnd;
    if (IsWindow(tmp)) {
        DestroyWindow(tmp);
        tmp = nullptr;
    }
    hwnd = nullptr;
}

WindowBase::~WindowBase() {
    Unsubclass();
    if (backgroundColorBrush != nullptr) {
        DeleteObject(backgroundColorBrush);
    }
    Destroy();
    UnregisterHandlersForHwnd(hwnd);
}

void WindowBase::WndProc(WndEvent* ev) {
    ev->didHandle = false;
}

Size WindowBase::GetIdealSize() {
    return {};
}

void Handle_WM_CONTEXTMENU(WindowBase* w, WndEvent* ev) {
    CrashIf(ev->msg != WM_CONTEXTMENU);
    CrashIf(!w->onContextMenu);
    // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
    ContextMenuEvent cmev;
    CopyWndEvent cpev(&cmev, ev);
    cmev.w = w;
    cmev.mouseGlobal.x = GET_X_LPARAM(ev->lp);
    cmev.mouseGlobal.y = GET_Y_LPARAM(ev->lp);
    POINT pt{cmev.mouseGlobal.x, cmev.mouseGlobal.y};
    if (pt.x != -1) {
        MapWindowPoints(HWND_DESKTOP, w->hwnd, &pt, 1);
    }
    cmev.mouseWindow.x = pt.x;
    cmev.mouseWindow.y = pt.y;
    w->onContextMenu(&cmev);
    ev->didHandle = true;
}

static void Dispatch_WM_CONTEXTMENU(void* user, WndEvent* ev) {
    WindowBase* w = (WindowBase*)user;
    Handle_WM_CONTEXTMENU(w, ev);
}

bool WindowBase::Create() {
    auto h = GetModuleHandle(nullptr);
    int x = CW_USEDEFAULT;
    if (initialPos.x != -1) {
        x = initialPos.x;
    }
    int y = CW_USEDEFAULT;
    if (initialPos.y != -1) {
        y = initialPos.y;
    }

    int dx = CW_USEDEFAULT;
    if (initialSize.dx > 0) {
        dx = initialSize.dx;
    }
    int dy = CW_USEDEFAULT;
    if (initialSize.dy > 0) {
        dy = initialSize.dy;
    }
    HMENU m = (HMENU)(UINT_PTR)ctrlID;
    hwnd = CreateWindowExW(dwExStyle, winClass, L"", dwStyle, x, y, dx, dy, parent, m, h, nullptr);
    CrashIf(!hwnd);

    if (hwnd == nullptr) {
        return false;
    }

    if (onDropFiles != nullptr) {
        DragAcceptFiles(hwnd, TRUE);
    }

    // TODO: maybe always register so that we can set onContextMenu
    // after creation
    if (onContextMenu) {
        void* user = this;
        RegisterHandlerForMessage(hwnd, WM_CONTEXTMENU, Dispatch_WM_CONTEXTMENU, user);
    }

    if (hfont == nullptr) {
        hfont = GetDefaultGuiFont();
    }
    SetFont(hfont);
    HwndSetText(hwnd, text.AsView());
    return true;
}

void WindowBase::SuspendRedraw() {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
}

void WindowBase::ResumeRedraw() {
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
}

void WindowBase::SetFocus() {
    ::SetFocus(hwnd);
}

bool WindowBase::IsFocused() {
    BOOL isFocused = ::IsFocused(hwnd);
    return tobool(isFocused);
}

void WindowBase::SetIsEnabled(bool isEnabled) {
    // TODO: make it work even if not yet created?
    CrashIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool WindowBase::IsEnabled() {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

Kind WindowBase::GetKind() {
    return kind;
}

void WindowBase::SetVisibility(Visibility newVisibility) {
    // TODO: make it work before Create()?
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

Visibility WindowBase::GetVisibility() {
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

// convenience function
void WindowBase::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool WindowBase::IsVisible() const {
    return visibility == Visibility::Visible;
}

void WindowBase::SetPos(RECT* r) {
    ::MoveWindow(hwnd, r);
}

void WindowBase::SetBounds(const RECT& r) {
    SetPos((RECT*)&r);
}

void WindowBase::SetFont(HFONT f) {
    hfont = f;
    HwndSetFont(hwnd, f);
}

HFONT WindowBase::GetFont() const {
    HFONT res = hfont;
    if (!res) {
        res = HwndGetFont(hwnd);
    }
    if (!res) {
        res = GetDefaultGuiFont();
    }
    return res;
}

void WindowBase::SetIcon(HICON iconIn) {
    hIcon = iconIn;
    HwndSetIcon(hwnd, hIcon);
}

HICON WindowBase::GetIcon() const {
    return hIcon;
}

void WindowBase::SetText(const WCHAR* s) {
    AutoFree str = strconv::WstrToUtf8(s);
    SetText(str.AsView());
}

void WindowBase::SetText(std::string_view sv) {
    text.Set(sv);
    // can be set before we create the window
    HwndSetText(hwnd, text.AsView());
    HwndInvalidate(hwnd);
}

std::string_view WindowBase::GetText() {
    text = win::GetTextUtf8(hwnd);
    return text.AsView();
}

void WindowBase::SetTextColor(COLORREF col) {
    if (ColorNoChange == col) {
        return;
    }
    textColor = col;
    // can be set before we create the window
    if (!hwnd) {
        return;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void WindowBase::SetBackgroundColor(COLORREF col) {
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

void WindowBase::SetColors(COLORREF bg, COLORREF txt) {
    SetBackgroundColor(bg);
    SetTextColor(txt);
}

void WindowBase::SetRtl(bool isRtl) {
    SetWindowExStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRtl);
}

Kind kindWindow = "window";

struct winClassWithAtom {
    const WCHAR* winClass = nullptr;
    ATOM atom = 0;
};

Vec<winClassWithAtom> gRegisteredClasses;

static void RegisterWindowClass(Window* w) {
    // check if already registered
    for (auto&& ca : gRegisteredClasses) {
        if (str::Eq(ca.winClass, w->winClass)) {
            if (ca.atom != 0) {
                return;
            }
        }
    }
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.hIcon = w->hIcon;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hIconSm = w->hIconSm;
    wcex.lpfnWndProc = wndProcCustom;
    wcex.lpszClassName = w->winClass;
    wcex.lpszMenuName = w->lpszMenuName;
    ATOM atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
    winClassWithAtom ca = {w->winClass, atom};
    gRegisteredClasses.Append(ca);
}

Window::Window() {
    ResetCtrlID();
    kind = kindWindow;
    dwExStyle = 0;
    dwStyle = WS_OVERLAPPEDWINDOW;
    if (parent == nullptr) {
        dwStyle |= WS_CLIPCHILDREN;
    } else {
        dwStyle |= WS_CHILD;
    }
}

bool Window::Create() {
    if (winClass == nullptr) {
        winClass = DEFAULT_WIN_CLASS;
    }
    RegisterWindowClass(this);

    int x = CW_USEDEFAULT;
    if (initialPos.x != -1) {
        x = initialPos.x;
    }
    int y = CW_USEDEFAULT;
    if (initialPos.y != -1) {
        y = initialPos.y;
    }

    int dx = CW_USEDEFAULT;
    if (initialSize.dx > 0) {
        dx = initialSize.dx;
    }
    int dy = CW_USEDEFAULT;
    if (initialSize.dy > 0) {
        dy = initialSize.dy;
    }
    AutoFreeWstr title = strconv::Utf8ToWstr(this->text.AsView());
    HINSTANCE hinst = GetInstance();
    hwnd = CreateWindowExW(dwExStyle, winClass, title, dwStyle, x, y, dx, dy, parent, nullptr, hinst, (void*)this);
    CrashIf(!hwnd);
    if (!hwnd) {
        return false;
    }
    if (hfont == nullptr) {
        hfont = GetDefaultGuiFont();
    }
    // trigger creating a backgroundBrush
    SetBackgroundColor(backgroundColor);
    SetFont(hfont);
    SetIcon(hIcon);
    HwndSetText(hwnd, text.AsView());
    return true;
}

Window::~Window() {
}

void Window::SetTitle(std::string_view title) {
    SetText(title);
}

void Window::Close() {
    ::SendMessageW(hwnd, WM_CLOSE, 0, 0);
}

// if only top given, set them all to top
// if top, right given, set bottom to top and left to right
void WindowBase::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(hwnd, top, right, bottom, left);
}

Size WindowBase::Layout(const Constraints bc) {
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

int WindowBase::MinIntrinsicHeight(int) {
#if 0
    auto vinset = insets.top + insets.bottom;
    Size s = GetIdealSize();
    return s.dy + vinset;
#else
    Size s = GetIdealSize();
    return s.dy;
#endif
}

int WindowBase::MinIntrinsicWidth(int) {
#if 0
    auto hinset = insets.left + insets.right;
    Size s = GetIdealSize();
    return s.dx + hinset;
#else
    Size s = GetIdealSize();
    return s.dx;
#endif
}

void WindowBase::SetBounds(Rect bounds) {
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

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog) {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
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

// sets initial position of w within hwnd. Assumes w->initialSize is set.
void PositionCloseTo(WindowBase* w, HWND hwnd) {
    CrashIf(!hwnd);
    Size is = w->initialSize;
    CrashIf(is.IsEmpty());
    RECT r{};
    BOOL ok = GetWindowRect(hwnd, &r);
    CrashIf(!ok);

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
