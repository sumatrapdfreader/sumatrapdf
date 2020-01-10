
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

void HwndSetText(HWND hwnd, std::string_view s) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (s.empty()) {
        return;
    }
    WCHAR* ws = strconv::Utf8ToWstr(s);
    win::SetText(hwnd, ws);
    free(ws);
}

Kind kindWindowBase = "windowBase";

static LRESULT CALLBACK wndProcDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                        DWORD_PTR dwRefData) {
    CrashIf(dwRefData == 0);

    WindowBase* w = (WindowBase*)dwRefData;
    if (uIdSubclass == w->subclassId) {
        CrashIf(hwnd != w->hwnd);
        WndProcArgs args{};
        SetWndProcArgs(args);
        w->WndProc(&args);
        if (args.didHandle) {
            return args.result;
        }
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
    HWND hwndCtrl = getChildHWNDForMessage(msg, wp, lp);
    if (!hwndCtrl) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    WindowBase* w = (WindowBase*)dwRefData;
    // char* msgName = getWinMessageName(msg);
    // dbglogf("hwnd: 0x%6p, msg: 0x%03x (%s), wp: 0x%x, id: %d, w: 0x%p\n", hwnd, msg, msgName, wp, uIdSubclass, w);
    if (uIdSubclass != w->subclassParentId) {
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
    BOOL ok = SetWindowSubclass(hwnd, wndProcDispatch, subclassId, (DWORD_PTR)wb);
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
        RemoveWindowSubclass(hwnd, wndProcDispatch, subclassId);
        subclassId = 0;
    }
    if (subclassParentId) {
        RemoveWindowSubclass(parent, wndProcParentDispatch, subclassParentId);
        subclassParentId = 0;
    }
}

WindowBase::WindowBase(HWND p) {
    parent = p;
}

void WindowBase::Destroy() {
    auto tmp = hwnd;
    hwnd = nullptr;
    if (IsWindow(tmp)) {
        DestroyWindow(tmp);
        tmp = nullptr;
    }
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

void WindowBase::SetTextColor(COLORREF col) {
    textColor = col;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void WindowBase::SetBackgroundColor(COLORREF col) {
    backgroundColor = col;
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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_NCCREATE == msg) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        Window* w = (Window*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Window* w = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // this is the last message ever received by hwnd
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

    if (WM_CLOSE == msg) {
        if (w->onClose) {
            WindowCloseArgs args{};
            SetWndProcArgs(args);
            w->onClose(&args);
            if (args.cancel) {
                return 0;
            }
        } else {
            w->Destroy();
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

    if (w->msgFilter) {
        WndProcArgs args{};
        SetWndProcArgs(args);
        w->msgFilter(&args);
        if (args.didHandle) {
            return args.result;
        }
    }

    if (WM_CTLCOLORBTN == msg) {
        if (ColorUnset != w->backgroundColor) {
            auto bgBrush = w->backgroundColorBrush;
            if (bgBrush != nullptr) {
                return (LRESULT)bgBrush;
            }
        }
    }

    if ((WM_COMMAND == msg) && w->onWmCommand) {
        WmCommandArgs args{};
        SetWndProcArgs(args);
        args.id = LOWORD(wp);
        args.ev = HIWORD(wp);
        w->onWmCommand(&args);
        if (args.didHandle) {
            return args.result;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if ((WM_SIZE == msg) && w->onSize) {
        SizeArgs args;
        SetWndProcArgs(args);
        args.dx = LOWORD(lp);
        args.dy = HIWORD(lp);
        w->onSize(&args);
        if (args.didHandle) {
            return 0;
        }
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

    return DefWindowProc(hwnd, msg, wp, lp);
}

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
    wcex.hIconSm = w->hIconSm;
    wcex.lpfnWndProc = WndProc;
    wcex.lpszClassName = w->winClass;
    wcex.lpszMenuName = w->lpszMenuName;
    ATOM atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
    winClassWithAtom ca = {w->winClass, atom};
    gRegisteredClasses.Append(ca);
}

Window::Window() {
    this->dwExStyle = 0;
    this->dwStyle = WS_OVERLAPPEDWINDOW;
    if (parent != nullptr) {
        this->dwStyle |= WS_CHILD;
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
    if (backgroundColor != ColorUnset) {
        backgroundColorBrush = CreateSolidBrush(backgroundColor);
    }
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
