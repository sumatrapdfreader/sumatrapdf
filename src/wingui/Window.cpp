

/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

// TODO: call RemoveWindowSubclass in WM_NCDESTROY as per
// https://devblogs.microsoft.com/oldnewthing/20031111-00/?p=41883

#define DEFAULT_WIN_CLASS L"WC_WIN32_WINDOW"

UINT_PTR g_subclassId = 0;

UINT_PTR NextSubclassId() {
    g_subclassId++;
    return g_subclassId;
}

CopyWndProcArgs::CopyWndProcArgs(WndProcArgs* dst, WndProcArgs* src) {
    this->dst = dst;
    this->src = src;
    dst->hwnd = src->hwnd;
    dst->msg = src->msg;
    dst->lparam = src->lparam;
    dst->wparam = src->wparam;
    dst->w = src->w;
}

CopyWndProcArgs::~CopyWndProcArgs() {
    src->didHandle = dst->didHandle;
    src->result = dst->result;
}

void HwndSetText(HWND hwnd, std::string_view s) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (s.empty()) {
        return;
    }
    AutoFreeWstr ws = strconv::Utf8ToWstr(s);
    win::SetText(hwnd, ws);
}

Kind kindWindowBase = "windowBase";

static LRESULT wndBaseProcDispatch(WindowBase* w, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    CrashIf(hwnd != w->hwnd);

    // TODO: maybe more this to WindowBase::WndProc?
    // or maybe get rid of WindowBase::WndProc and use msgFilterInternal
    // when per-control custom processing is needed
    if (w->msgFilter) {
        WndProcArgs args{};
        SetWndProcArgs(args);
        w->msgFilter(&args);
        if (args.didHandle) {
            didHandle = true;
            return args.result;
        }
    }

    if (WM_CTLCOLORBTN == msg) {
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            didHandle = true;
            return (LRESULT)bgBrush;
        }
    }

    if (WM_CTLCOLORSTATIC == msg) {
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            HDC hdc = (HDC)wp;
            // SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            didHandle = true;
            return (LRESULT)bgBrush;
        }
    }

    if (WM_SIZE == msg) {
        if (!w->onSize) {
            return 0;
        }
        SizeArgs args;
        SetWndProcArgs(args);
        args.dx = LOWORD(lp);
        args.dy = HIWORD(lp);
        w->onSize(&args);
        if (args.didHandle) {
            didHandle = true;
            return 0;
        }
    }

    if (WM_COMMAND == msg) {
        if (!w->onWmCommand) {
            return 0;
        }
        WmCommandArgs args{};
        SetWndProcArgs(args);
        args.id = LOWORD(wp);
        args.ev = HIWORD(wp);
        w->onWmCommand(&args);
        if (args.didHandle) {
            didHandle = true;
            return args.result;
        }
    }

    if (WM_CHAR == msg) {
        if (!w->onChar) {
            return 0;
        }
        CharArgs args{};
        SetWndProcArgs(args);
        args.keyCode = (int)wp;
        w->onChar(&args);
        if (args.didHandle) {
            didHandle = true;
            return args.result;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        if (!w->onMouseWheel) {
            return 0;
        }
        MouseWheelArgs args{};
        SetWndProcArgs(args);
        args.isVertical = (msg == WM_MOUSEWHEEL);
        args.delta = GET_WHEEL_DELTA_WPARAM(wp);
        args.keys = GET_KEYSTATE_WPARAM(wp);
        args.x = GET_X_LPARAM(lp);
        args.y = GET_Y_LPARAM(lp);
        w->onMouseWheel(&args);
        if (args.didHandle) {
            didHandle = true;
            return 0;
        }
    }

    // handle the rest in WndProc
    WndProcArgs args{};
    SetWndProcArgs(args);
    w->WndProc(&args);
    didHandle = args.didHandle;
    return args.result;
}

static LRESULT CALLBACK wndProcCustom(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_NCCREATE == msg) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        Window* w = (Window*)cs->lpCreateParams;
        w->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Window* w = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // this is the last message ever received by hwnd
    // TODO: move it to wndBaseProcDispatch? Maybe they don't
    // need WM_*DESTROY notifications?
    if (WM_NCDESTROY == msg) {
        if (w->onDestroy) {
            WindowDestroyArgs args{};
            SetWndProcArgs(args);
            args.window = w;
            w->onDestroy(&args);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // TODO: should this go into WindowBase?
    if (WM_CLOSE == msg) {
        if (w->onClose) {
            WindowCloseArgs args{};
            SetWndProcArgs(args);
            w->onClose(&args);
            if (args.cancel) {
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // TODDO: a hack, a Window might be deleted when we get here
    // happens e.g. when we call CloseWindow() inside
    // wndproc. Maybe instead of calling DestroyWindow()
    // we should delete WindowInfo, for proper shutdown sequence
    if (WM_DESTROY == msg) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (!w) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (WM_PAINT == msg) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        auto bgBrush = w->backgroundColorBrush;
        if (bgBrush != nullptr) {
            RECT rc = GetClientRect(hwnd);
            FillRect(ps.hdc, &ps.rcPaint, bgBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    bool didHandle = false;
    LRESULT res = wndBaseProcDispatch(w, hwnd, msg, wp, lp, didHandle);
    if (didHandle) {
        return res;
    }
    res = DefWindowProcW(hwnd, msg, wp, lp);
    // char* msgName = getWinMessageName(msg);
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

    bool didHandle = false;
    LRESULT res = wndBaseProcDispatch(w, hwnd, msg, wp, lp, didHandle);
    if (didHandle) {
        return res;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// TODO: potentially more messages
// https://docs.microsoft.com/en-us/cpp/mfc/reflected-window-message-ids?view=vs-2019
static HWND getChildHWNDForMessage(UINT msg, WPARAM wp, LPARAM lp) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorbtn
    if (WM_CTLCOLORBTN == msg) {
        return (HWND)lp;
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

// TODO: maybe just always subclass main window and reflect those messages
// back to their children instead of subclassing in each child
// Another option: always create a reflector HWND window, like walk does
static LRESULT CALLBACK wndProcParentDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                              DWORD_PTR dwRefData) {
    WindowBase* w = (WindowBase*)dwRefData;
    // char* msgName = getWinMessageName(msg);
    // dbglogf("hwnd: 0x%6p, msg: 0x%03x (%s), wp: 0x%x, id: %d, w: 0x%p\n", hwnd, msg, msgName, wp, uIdSubclass, w);
    if (uIdSubclass != w->subclassParentId) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    // needed for drag&drop in TreeCtrl
    // TODO: not quite happy with this
    if (WM_LBUTTONUP == msg || WM_MOUSEMOVE == msg) {
        WndProcArgs args{};
        SetWndProcArgs(args);
        w->WndProcParent(&args);
        if (args.didHandle) {
            return args.result;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    HWND hwndCtrl = getChildHWNDForMessage(msg, wp, lp);
    if (!hwndCtrl) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    CrashIf(hwnd != w->parent);
    if (hwndCtrl != w->hwnd) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    if (msg == WM_CONTEXTMENU && w->onContextMenu) {
        ContextMenuArgs args;
        SetWndProcArgs(args);
        args.w = w;
        args.mouseGlobal.x = GET_X_LPARAM(lp);
        args.mouseGlobal.y = GET_Y_LPARAM(lp);
        POINT pt{args.mouseGlobal.x, args.mouseGlobal.y};
        if (pt.x != -1) {
            MapWindowPoints(HWND_DESKTOP, w->hwnd, &pt, 1);
        }
        args.mouseWindow.x = pt.x;
        args.mouseWindow.y = pt.y;
        w->onContextMenu(&args);
        return 0;
    }

    WndProcArgs args{};
    SetWndProcArgs(args);
    w->WndProcParent(&args);
    if (args.didHandle) {
        return args.result;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

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

void WindowBase::SubclassParent() {
    CrashIf(!parent);
    WindowBase* wb = this;
    subclassParentId = NextSubclassId();
    BOOL ok = SetWindowSubclass(parent, wndProcParentDispatch, subclassParentId, (DWORD_PTR)wb);
    CrashIf(!ok);
    if (!ok) {
        subclassParentId = 0;
    }
}

void WindowBase::Unsubclass() {
    if (subclassId) {
        RemoveWindowSubclass(hwnd, wndProcSubclassed, subclassId);
        subclassId = 0;
    }
    if (subclassParentId) {
        RemoveWindowSubclass(parent, wndProcParentDispatch, subclassParentId);
        subclassParentId = 0;
    }
}

WindowBase::WindowBase(HWND p) {
    kind = kindWindowBase;
    parent = p;
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
}

void WindowBase::WndProc(WndProcArgs* args) {
    args->didHandle = false;
}

void WindowBase::WndProcParent(WndProcArgs* args) {
    args->didHandle = false;
}

SIZE WindowBase::GetIdealSize() {
    return {};
}

bool WindowBase::Create() {
    auto h = GetModuleHandle(nullptr);
    int x = CW_USEDEFAULT;
    if (initialPos.X != -1) {
        x = initialPos.X;
    }
    int y = CW_USEDEFAULT;
    if (initialPos.Y != -1) {
        y = initialPos.Y;
    }

    int dx = CW_USEDEFAULT;
    if (initialSize.Width > 0) {
        dx = initialSize.Width;
    }
    int dy = CW_USEDEFAULT;
    if (initialSize.Height > 0) {
        dy = initialSize.Height;
    }
    HMENU m = (HMENU)(UINT_PTR)menuId;
    hwnd = CreateWindowExW(dwExStyle, winClass, L"", dwStyle, x, y, dx, dy, parent, m, h, nullptr);

    if (hwnd == nullptr) {
        return false;
    }

    if (hfont == nullptr) {
        hfont = GetDefaultGuiFont();
    }
    SetFont(hfont);
    HwndSetText(hwnd, text.AsView());
    // SubclassParent();
    return true;
}

void WindowBase::SuspendRedraw() {
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
}

void WindowBase::ResumeRedraw() {
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
}

void WindowBase::SetFocus() {
    ::SetFocus(hwnd);
}

bool WindowBase::IsFocused() {
    BOOL isFocused = ::IsFocused(hwnd);
    return tobool(isFocused);
}

void WindowBase::SetIsEnabled(bool isEnabled) {
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool WindowBase::IsEnabled() {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void WindowBase::SetIsVisible(bool isVisible) {
    ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
}

bool WindowBase::IsVisible() {
    return ::IsWindowVisible(hwnd);
}

void WindowBase::SetPos(RECT* r) {
    ::MoveWindow(hwnd, r);
}

void WindowBase::SetBounds(const RECT& r) {
    SetPos((RECT*)&r);
}

void WindowBase::SetFont(HFONT f) {
    hfont = f;
    SetWindowFont(hwnd, f, TRUE);
}

void WindowBase::SetText(const WCHAR* s) {
    AutoFree str = strconv::WstrToUtf8(s);
    SetText(str.as_view());
}

void WindowBase::SetText(std::string_view sv) {
    text.Set(sv);
    HwndSetText(hwnd, text.AsView());
    InvalidateRect(hwnd, nullptr, FALSE);
}

std::string_view WindowBase::GetText() {
    text = win::GetTextUtf8(hwnd);
    return text.as_view();
}

void WindowBase::SetTextColor(COLORREF col) {
    if (ColorNoChange == col) {
        return;
    }
    textColor = col;
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
    InvalidateRect(hwnd, nullptr, FALSE);
}

void WindowBase::SetColors(COLORREF bg, COLORREF txt) {
    SetBackgroundColor(bg);
    SetTextColor(txt);
}

void WindowBase::SetRtl(bool isRtl) {
    ToggleWindowExStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRtl);
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
    if (initialPos.X != -1) {
        x = initialPos.X;
    }
    int y = CW_USEDEFAULT;
    if (initialPos.Y != -1) {
        y = initialPos.Y;
    }

    int dx = CW_USEDEFAULT;
    if (initialSize.Width > 0) {
        dx = initialSize.Width;
    }
    int dy = CW_USEDEFAULT;
    if (initialSize.Height > 0) {
        dy = initialSize.Height;
    }
    AutoFreeWstr title = strconv::Utf8ToWstr(this->text.as_view());
    HINSTANCE hinst = GetInstance();
    hwnd = CreateWindowExW(dwExStyle, winClass, title, dwStyle, x, y, dx, dy, parent, nullptr, hinst, (void*)this);
    if (!hwnd) {
        return false;
    }
    if (hfont == nullptr) {
        hfont = GetDefaultGuiFont();
    }
    // trigger creating a backgroundBrush
    SetBackgroundColor(backgroundColor);
    SetFont(hfont);
    HwndSetText(hwnd, text.AsView());

    return true;
}

Window::~Window() {
}

void Window::SetTitle(std::string_view title) {
    SetText(title);
}

void Window::Close() {
    ::SendMessage(hwnd, WM_CLOSE, 0, 0);
}

WindowBaseLayout::WindowBaseLayout(WindowBase* b, Kind k) {
    wb = b;
    kind = k;
}

WindowBaseLayout::~WindowBaseLayout() {
    delete wb;
}

Size WindowBaseLayout::Layout(const Constraints bc) {
    i32 width = MinIntrinsicWidth(0);
    i32 height = MinIntrinsicHeight(0);
    return bc.Constrain(Size{width, height});
}

i32 WindowBaseLayout::MinIntrinsicHeight(i32) {
    SIZE s = wb->GetIdealSize();
    return (i32)s.cy;
}

i32 WindowBaseLayout::MinIntrinsicWidth(i32) {
    SIZE s = wb->GetIdealSize();
    return (i32)s.cx;
}

void WindowBaseLayout::SetBounds(const Rect bounds) {
    auto r = RectToRECT(bounds);
    ::MoveWindow(wb->hwnd, &r);
}

int RunMessageLoop(HACCEL accelTable) {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, accelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

// sets initial position of w within hwnd. Assumes w->initialSize is set.
void PositionCloseTo(WindowBase* w, HWND hwnd) {
    CrashIf(!hwnd);
    Size is = w->initialSize;
    CrashIf(is.empty());
    RECT r{};
    BOOL ok = GetWindowRect(hwnd, &r);
    CrashIf(!ok);

    // position w in the the center of hwnd
    // if window is bigger than hwnd, let the system position
    // we don't want to hide it
    int offX = (RectDx(r) - is.Width) / 2;
    if (offX < 0) {
        return;
    }
    int offY = (RectDy(r) - is.Height) / 2;
    if (offY < 0) {
        return;
    }
    Point& ip = w->initialPos;
    ip.X = (Length)r.left + (Length)offX;
    ip.Y = (Length)r.top + (Length)offY;
}
