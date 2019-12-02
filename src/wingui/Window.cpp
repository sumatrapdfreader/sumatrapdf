/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

#define WIN_CLASS L"WC_WIN32_WINDOW"

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
    WCHAR* ws = str::conv::Utf8ToWchar(s);
    win::SetText(hwnd, ws);
    free(ws);
}

Kind kindWindow = "window";

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        Window* w = (Window*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Window* w = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // don't allow intercepting those messages
    if (WM_DESTROY == msg) {
        PostQuitMessage(0);
        return 0;
    }

    if (WM_NCDESTROY == msg) {
        delete w;
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    if (!w) {
        goto Exit;
    }

    if (w->preFilter) {
        bool discardMsg = false;
        LRESULT res = w->preFilter(hwnd, msg, wp, lp, discardMsg);
        if (discardMsg)
            return res;
    }

    if ((WM_COMMAND == msg) && w->onCommand) {
        bool discardMsg = false;
        LRESULT res = w->onCommand(hwnd, LOWORD(wp), HIWORD(wp), lp, discardMsg);
        if (discardMsg)
            return res;
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if ((WM_SIZE == msg) && w->onSize) {
        int dx = LOWORD(lp);
        int dy = HIWORD(lp);
        w->onSize(hwnd, dx, dy, wp);
        return 0;
    }

Exit:
    if (WM_PAINT == msg) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static void RegisterClass() {
    static ATOM atom = 0;

    if (atom != 0) {
        // already registered
        return;
    }

    WNDCLASSEXW wcex = {};
    FillWndClassEx(wcex, WIN_CLASS, WndProc);
    atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
}

Window::Window(HWND parent, RECT* initialPosition) {
    this->parent = parent;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    }

    this->dwExStyle = 0;
    this->dwStyle = WS_OVERLAPPEDWINDOW;
    if (parent != nullptr) {
        this->dwStyle |= WS_CHILD;
    }
}

bool Window::Create(const WCHAR* title) {
    RegisterClass();

    RECT rc = this->initialPos;
    int x = rc.left;
    int y = rc.top;
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    if (dx == 0) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        dx = CW_USEDEFAULT;
        dy = CW_USEDEFAULT;
    }
    this->hwnd = CreateWindowExW(this->dwExStyle, WIN_CLASS, title, this->dwStyle, x, y, dx, dy, this->parent, nullptr,
                                 GetInstance(), (void*)this);

    return this->hwnd != nullptr;
}

Window::~Window() {
    DestroyWindow(hwnd);
}

Kind kindWindowBase = "windowBase";

static LRESULT CALLBACK wndProcDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                        DWORD_PTR dwRefData) {
    CrashIf(dwRefData == 0);
    LRESULT res = 0;
    bool didHandle = false;
    WindowBase* wb = (WindowBase*)dwRefData;
    if (uIdSubclass == wb->subclassId) {
        CrashIf(hwnd != wb->hwnd);
        res = wb->WndProc(hwnd, msg, wp, lp, didHandle);
    } else if (uIdSubclass == wb->subclassParentId) {
        CrashIf(hwnd != wb->parent);
        if (WM_COMMAND == msg) {
            // the same parent is sub-classed by many controls
            // we only want to dispatch the message to the control
            // that originated the message
            HWND hwndCtrl = (HWND)lp;
            if (hwndCtrl == wb->hwnd) {
                res = wb->WndProcParent(hwnd, msg, wp, lp, didHandle);
            }
        }
    } else {
        CrashMe();
    }
    if (didHandle) {
        return res;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void WindowBase::Subclass() {
    CrashIf(!hwnd);
    WindowBase* wb = this;
    UINT_PTR id = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, wndProcDispatch, id, (DWORD_PTR)wb);
    CrashIf(!ok);
    if (ok) {
        subclassId = id;
    }
}

void WindowBase::SubclassParent() {
    CrashIf(!parent);
    WindowBase* wb = this;
    UINT_PTR id = NextSubclassId();
    BOOL ok = SetWindowSubclass(parent, wndProcDispatch, id, (DWORD_PTR)wb);
    CrashIf(!ok);
    if (ok) {
        subclassParentId = id;
    }
}

void WindowBase::Unsubclass() {
    if (subclassId) {
        RemoveWindowSubclass(hwnd, wndProcDispatch, subclassId);
        subclassId = 0;
    }
    if (subclassParentId) {
        RemoveWindowSubclass(parent, wndProcDispatch, subclassParentId);
        subclassParentId = 0;
    }
}

WindowBase::WindowBase(HWND p) {
    parent = p;
}

WindowBase::~WindowBase() {
    Unsubclass();
    DestroyWindow(hwnd);
}

LRESULT WindowBase::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    UNUSED(hwnd);
    UNUSED(msg);
    UNUSED(wp);
    UNUSED(lp);
    didHandle = false;
    return 0;
}

LRESULT WindowBase::WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    UNUSED(hwnd);
    UNUSED(msg);
    UNUSED(wp);
    UNUSED(lp);
    didHandle = false;
    return 0;
}

SIZE WindowBase::GetIdealSize() {
    return {};
}

bool WindowBase::Create() {
    RECT rc = initialPos;
    auto h = GetModuleHandle(nullptr);
    int x = rc.left;
    int y = rc.top;
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    HMENU idMenu = (HMENU)(UINT_PTR)menuId;
    hwnd = CreateWindowExW(dwExStyle, winClass, L"", dwStyle, x, y, dx, dy, parent, idMenu, h, nullptr);

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

void WindowBase::SetFocus() {
    ::SetFocus(hwnd);
}

void WindowBase::SetIsEnabled(bool isEnabled) {
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool WindowBase::IsEnabled() {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return enabled ? true : false;
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
    auto d = str::conv::WcharToUtf8(s);
    SetText(d.AsView());
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
