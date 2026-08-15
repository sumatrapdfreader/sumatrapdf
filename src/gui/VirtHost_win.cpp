/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/PlatformFont.h"
#include "gui/VirtHost.h"

// the class names we have already registered. They are string literals that
// live for the whole run, so keeping the pointers is fine
static Vec<WStr> gRegisteredClasses;

static LRESULT CALLBACK WndProcVirtHost(HWND, UINT, WPARAM, LPARAM);

static void RegisterHostClass(WStr className) {
    for (WStr s : gRegisteredClasses) {
        if (wstr::Eq(s, className)) {
            return;
        }
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    // virtual controls do their own hover/press drawing and want double clicks
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProcVirtHost;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    wc.lpszClassName = className.s;
    RegisterClassExW(&wc);
    gRegisteredClasses.Append(className);
}

static void PaintHost(VirtHost* host, HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    Rect rc = HwndClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rc);
    HDC memDC = buffer.GetDC();
    SetBkMode(memDC, TRANSPARENT);
    // scoped: GfxDirect2D reaches the dc only when destroyed, so the gfx must
    // die before the buffer is flushed
    {
        Gfx* gfx = GfxCreate(memDC);
        VirtHostPaintEvent ev;
        ev.host = host;
        ev.gfx = gfx;
        ev.clientRect = rc;
        if (host->onPaintBackground.IsValid()) {
            host->onPaintBackground.Call(&ev);
        } else {
            gfx->FillRect(rc, host->bgColor);
        }
        if (host->vroot) {
            host->vroot->Paint(gfx, rc);
        }
        if (host->onPaint.IsValid()) {
            host->onPaint.Call(&ev);
        }
        delete gfx;
    }
    buffer.Flush(hdc);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcVirtHost(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    VirtHost* host;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        host = (VirtHost*)cs->lpCreateParams;
        host->native = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)host);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    host = (VirtHost*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!host) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // the host previously relied on whatever dpi the last window left behind
    DpiScope dpiScope(hwnd);

    switch (msg) {
        case WM_MOUSEACTIVATE:
            if (host->noActivate) {
                if (host->isPopup) {
                    return MA_NOACTIVATE;
                }
                // a child only refuses activation when the frame is already the
                // foreground window: clicking an inactive window should still
                // activate it
                HWND frame = GetAncestor(hwnd, GA_ROOT);
                if (frame && GetForegroundWindow() == frame) {
                    return MA_NOACTIVATE;
                }
            }
            break;
        case WM_SIZE:
            host->Relayout();
            return 0;
        case WM_ERASEBKGND:
            // WM_PAINT draws the whole client area into a back buffer
            return 1;
        case WM_PAINT:
            PaintHost(host, hwnd);
            return 0;
        case WM_TIMER:
            if (host->onTimer.IsValid()) {
                host->onTimer.Call((int)wp);
                return 0;
            }
            break;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            host->onMouseMove.Call();
            break;
        }
        case WM_MOUSELEAVE:
            host->onMouseLeave.Call();
            break;
    }

    if (host->onNativeMsg.IsValid()) {
        VirtHostNativeMsg ev;
        ev.host = host;
        ev.msg = msg;
        ev.wp = wp;
        ev.lp = lp;
        host->onNativeMsg.Call(&ev);
        if (ev.didHandle) {
            return ev.res;
        }
    }

    if (host->vroot) {
        LRESULT res = 0;
        if (VirtTreeOnMessage(hwnd, host->vroot, msg, wp, lp, res)) {
            return res;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

VirtHost* VirtHost::Create(const CreateArgs& args) {
    RegisterHostClass(args.className);

    DWORD style = args.isPopup ? WS_POPUP : (WS_CHILD | WS_CLIPCHILDREN);
    if (args.visible) {
        style |= WS_VISIBLE;
    }
    if (args.clipSiblings) {
        style |= WS_CLIPSIBLINGS;
    }
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (args.isPopup && args.noActivate) {
        // a popup can keep the focus off itself with a style; a child can't
        exStyle |= WS_EX_NOACTIVATE;
    }
    if (args.isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    auto* host = new VirtHost();
    host->bgColor = args.bgColor;
    host->noActivate = args.noActivate;
    host->isPopup = args.isPopup;
    host->userData = args.userData;

    Size sz = args.initialSize;
    // WM_NCCREATE sets host->native
    HWND hwnd = CreateWindowExW(exStyle, args.className.s, nullptr, style, 0, 0, sz.dx, sz.dy, args.parent, nullptr,
                                GetModuleHandle(nullptr), host);
    if (!hwnd) {
        delete host;
        return nullptr;
    }
    return host;
}

VirtHost::~VirtHost() {
    HWND hwnd = native;
    // the window procedure runs while DestroyWindow() unwinds; make it fall
    // through to DefWindowProc instead of using a half-destroyed host
    if (hwnd) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    }
    native = nullptr;
    delete layout;
    layout = nullptr;
    delete vroot;
    vroot = nullptr;
    HwndDestroyWindowSafe(&hwnd);
}

void VirtHost::SetLayout(ILayout* l) {
    if (layout != l) {
        delete layout;
        layout = l;
    }
    Relayout();
}

void VirtHost::Relayout() {
    if (!layout || !native) {
        return;
    }
    Rect rc = ClientRect();
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }
    LayoutTreeToSize(native, layout, rc.Size(), &vroot);
}

Size VirtHost::SetLayoutSizedToContent(ILayout* l) {
    if (layout != l) {
        // deleting the old tree takes its controls out of vroot->tops
        delete layout;
        layout = l;
    }
    if (!layout || !native) {
        return {};
    }
    Size sz = layout->Layout(ExpandInf());
    Rect bounds{0, 0, sz.dx, sz.dy};
    layout->SetBounds(bounds);
    RefreshVirtTops(native, layout, bounds, &vroot);
    return sz;
}

Rect VirtHost::ClientRect() const {
    return HwndClientRect(native);
}

Rect VirtHost::ScreenRect() const {
    return HwndWindowRect(native);
}

Point VirtHost::FromScreen(Point pt) const {
    return HwndScreenToClient(native, pt);
}

Rect VirtHost::ToScreen(Rect r) const {
    return HwndMapRectToWindow(r, native, HWND_DESKTOP);
}

void VirtHost::SetPos(Rect r, bool visible) {
    UINT flags = SWP_NOACTIVATE;
    flags |= visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    SetWindowPos(native, HWND_TOP, r.x, r.y, r.dx, r.dy, flags);
}

void VirtHost::SetBounds(Rect r) {
    UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
    SetWindowPos(native, nullptr, r.x, r.y, r.dx, r.dy, flags);
}

void VirtHost::Show(bool show) {
    ShowWindow(native, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

bool VirtHost::IsVisible() const {
    return HwndIsVisible(native);
}

void VirtHost::ClipToRoundedRect(int radius, Size sz) {
    int dx = std::max(sz.dx, 1);
    int dy = std::max(sz.dy, 1);
    int r = DpiScale(radius);
    HRGN rgn = CreateRoundRectRgn(0, 0, dx + 1, dy + 1, r, r);
    if (!SetWindowRgn(native, rgn, TRUE)) {
        DeleteObject(rgn);
    }
}

void VirtHost::Invalidate(bool erase) {
    HwndInvalidate(native, erase);
}

void VirtHost::Repaint() {
    HwndInvalidate(native, true);
    UpdateWindow(native);
}

bool VirtHost::HasFocus() const {
    HWND focus = GetFocus();
    if (!focus || !native) {
        return false;
    }
    return focus == native || IsChild(native, focus);
}

bool VirtHost::ContainsScreenPoint(Point pt) const {
    HWND under = HwndWindowFromPoint(pt);
    if (!under || !native) {
        return false;
    }
    return under == native || IsChild(native, under);
}

void VirtHost::SetTimer(int id, int delayMs) {
    ::SetTimer(native, (UINT_PTR)id, (UINT)delayMs, nullptr);
}

void VirtHost::KillTimer(int id) {
    ::KillTimer(native, (UINT_PTR)id);
}

void VirtHost::SetFont(PlatformFont* f) {
    if (f) {
        HwndSetFont(native, f->GetHFont());
    }
}
