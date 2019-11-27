/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/Window.h"
#include "utils/FileUtil.h"

#define WIN_CLASS L"WC_WIN32_WINDOW"

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
    // we free w in WM_DESTROY
    DestroyWindow(this->hwnd);
}

Kind kindWindowBase = "windowBase";

const UINT_PTR idWndProc = 1;
const UINT_PTR idParentWndProc = 2;

static LRESULT CALLBACK wndProcDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                        DWORD_PTR dwRefData) {
    CrashIf(dwRefData == 0);
    WindowBase* wb = (WindowBase*)dwRefData;
    if (uIdSubclass == idWndProc) {
        wb->WndProc(hwnd, msg, wp, lp);
    } else if (uIdSubclass == idParentWndProc) {
        wb->WndProcParent(hwnd, msg, wp, lp);
    } else {
        CrashMe();
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void WindowBase::Subclass() {
    CrashIf(!hwnd);
    WindowBase* wb = this;
    BOOL ok = SetWindowSubclass(hwnd, wndProcDispatch, idWndProc, (DWORD_PTR)wb);
    CrashIf(!ok);
}

void WindowBase::SubclassParent() {
    CrashIf(!parent);
    WindowBase* wb = this;
    BOOL ok = SetWindowSubclass(parent, wndProcDispatch, idParentWndProc, (DWORD_PTR)wb);
    CrashIf(!ok);
}

static void Unsubclass(WindowBase*) {
    // TODO: implement me
}

WindowBase::WindowBase(HWND p) {
    parent = p;
}

WindowBase::~WindowBase() {
    Unsubclass(this);
}

LRESULT WindowBase::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefSubclassProc(hwnd, msg, wp, lp);
}

LRESULT WindowBase::WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefSubclassProc(hwnd, msg, wp, lp);
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

void WindowBase::SetText(std::string_view sv) {
    text.Set(sv.data());
    HwndSetText(hwnd, text.AsView());
}

void WindowBase::SetTextColor(COLORREF col) {
    textColor = col;
}

void WindowBase::SetBackgroundColor(COLORREF col) {
    backgroundColor = col;
}
