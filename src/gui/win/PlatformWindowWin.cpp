/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/PlatformWindow.h"

static constexpr WCHAR kPlatformWindowClass[] = L"SUMATRA_PLATFORM_WINDOW";

static LRESULT CALLBACK PlatformWindowProc(HWND, UINT, WPARAM, LPARAM);

static void RegisterPlatformWindowClass() {
    static bool didRegister = false;
    if (didRegister) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = PlatformWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    wc.lpszClassName = kPlatformWindowClass;
    didRegister = RegisterClassExW(&wc) != 0;
}

static void PaintPlatformWindow(PlatformWindow* window, HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    Rect client = HwndClientRect(hwnd);
    DoubleBuffer buffer(hwnd, client);
    Gfx* gfx = GfxCreate(buffer.GetDC());
    PlatformWindowPaintEvent ev{window, gfx, client};
    window->onPaint.Call(&ev);
    delete gfx;
    buffer.Flush(hdc);
    EndPaint(hwnd, &ps);
}

static PlatformPointerEvent MakePointerEvent(PlatformWindow* window, PlatformPointerEventType type, WPARAM wp,
                                             LPARAM lp) {
    PlatformPointerEvent ev;
    ev.window = window;
    ev.type = type;
    ev.pos = {(short)LOWORD(lp), (short)HIWORD(lp)};
    ev.isCtrl = (wp & MK_CONTROL) != 0;
    ev.isShift = (wp & MK_SHIFT) != 0;
    ev.isAlt = GetKeyState(VK_MENU) < 0;
    return ev;
}

static LRESULT CALLBACK PlatformWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PlatformWindow* window = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        window = (PlatformWindow*)cs->lpCreateParams;
        window->native = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    window = (PlatformWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!window) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            PaintPlatformWindow(window, hwnd);
            return 0;
        case WM_CLOSE:
            window->onCloseRequest.Call();
            return 0;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            auto ev = MakePointerEvent(window, PlatformPointerEventType::Move, wp, lp);
            window->onPointer.Call(&ev);
            return ev.didHandle ? 0 : DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_MOUSELEAVE: {
            PlatformPointerEvent ev;
            ev.window = window;
            ev.type = PlatformPointerEventType::Leave;
            window->onPointer.Call(&ev);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            PlatformPointerEventType type =
                msg == WM_LBUTTONDOWN ? PlatformPointerEventType::Down : PlatformPointerEventType::Up;
            auto ev = MakePointerEvent(window, type, wp, lp);
            ev.button = 1;
            window->onPointer.Call(&ev);
            return ev.didHandle ? 0 : DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_CHAR: {
            PlatformKeyEvent ev;
            ev.window = window;
            ev.codepoint = (int)wp;
            ev.isCtrl = GetKeyState(VK_CONTROL) < 0;
            ev.isShift = GetKeyState(VK_SHIFT) < 0;
            ev.isAlt = GetKeyState(VK_MENU) < 0;
            window->onKey.Call(&ev);
            return ev.didHandle ? 0 : DefWindowProcW(hwnd, msg, wp, lp);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

PlatformWindow* PlatformWindow::Create(const CreateArgs& args) {
    RegisterPlatformWindowClass();
    auto* window = new PlatformWindow();
    window->userData = args.userData;

    DWORD style = WS_POPUP;
    if (args.resizable) {
        style |= WS_THICKFRAME;
    }
    if (!args.frameless) {
        style |= WS_CAPTION | WS_SYSMENU;
    }
    DWORD exStyle = WS_EX_TOOLWINDOW;
    TempWStr title = ToWStrTemp(args.title);
    HWND hwnd = CreateWindowExW(exStyle, kPlatformWindowClass, title.s, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                args.initialSize.dx, args.initialSize.dy, args.parent, nullptr,
                                GetModuleHandleW(nullptr), window);
    if (!hwnd) {
        delete window;
        return nullptr;
    }
    if (args.visible) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    return window;
}

PlatformWindow::~PlatformWindow() {
    HWND hwnd = native;
    if (hwnd) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    }
    native = nullptr;
    HwndDestroyWindowSafe(&hwnd);
}

Rect PlatformWindow::ClientRect() const {
    return HwndClientRect(native);
}

Rect PlatformWindow::ScreenRect() const {
    return HwndWindowRect(native);
}

void PlatformWindow::SetBounds(Rect r) {
    SetWindowPos(native, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOOWNERZORDER);
}

void PlatformWindow::Show(bool show) {
    ShowWindow(native, show ? SW_SHOWNORMAL : SW_HIDE);
}

void PlatformWindow::Focus() {
    HwndSetFocus(native);
}

void PlatformWindow::Invalidate() {
    HwndInvalidate(native, false);
}

static LPWSTR CursorName(CursorId id) {
    switch (id) {
        case CursorId::Arrow:
            return IDC_ARROW;
        case CursorId::IBeam:
            return IDC_IBEAM;
        case CursorId::Hand:
            return IDC_HAND;
        case CursorId::Cross:
            return IDC_CROSS;
        case CursorId::Move:
            return IDC_SIZEALL;
        case CursorId::SizeNS:
            return IDC_SIZENS;
        case CursorId::SizeWE:
            return IDC_SIZEWE;
        case CursorId::No:
            return IDC_NO;
        case CursorId::None:
            return nullptr;
    }
    return nullptr;
}

void PlatformWindow::SetCursor(CursorId id) {
    LPWSTR name = CursorName(id);
    if (name) {
        SetCursorCached(name);
    }
}

void PlatformWindow::BeginMove(const PlatformPointerEvent&) {
    ReleaseCapture();
    SendMessageW(native, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

Rect PlatformWindowRect(NativeWnd native) {
    return HwndWindowRect(native);
}

Rect PlatformWindowWorkArea(NativeWnd native) {
    Rect r = HwndWindowRect(native);
    return GetWorkAreaRect(r, native);
}

bool PlatformWindowIsMaximized(NativeWnd native) {
    return IsZoomed(native);
}

void PlatformWindowActivateIfForeground(NativeWnd native) {
    if (!native) {
        return;
    }
    HWND fg = GetForegroundWindow();
    if (!fg || fg == native) {
        SetActiveWindow(native);
    }
}

void PlatformPostTask(const Func0& fn) {
    uitask::Post(fn, "PlatformWindowTask");
}
