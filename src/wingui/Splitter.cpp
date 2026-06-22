/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

//- Splitter

Kind kindSplitter = "splitter";

static const WCHAR* kResizeOverlayClass = L"SplitterResizeOverlayWnd";

static void OnSplitterPaint(HWND hwnd, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    AutoDeleteBrush br = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

static void RegisterResizeOverlayClass();

static void HideResizeOverlay(Splitter* splitter) {
    if (!splitter || !splitter->resizeOverlayHwnd) {
        return;
    }
    ShowWindow(splitter->resizeOverlayHwnd, SW_HIDE);
}

static void EnsureResizeOverlay(Splitter* splitter) {
    if (!splitter || splitter->resizeOverlayHwnd) {
        return;
    }
    RegisterResizeOverlayClass();
    HWND parent = GetParent(splitter->hwnd);
    HWND owner = GetAncestor(parent, GA_ROOT);
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;
    HWND hwnd = CreateWindowExW(exStyle, kResizeOverlayClass, nullptr, style, 0, 0, 0, 0, owner, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    ReportIf(!hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)splitter);
    splitter->resizeOverlayHwnd = hwnd;
}

static void UpdateResizeOverlay(Splitter* splitter, Point pos) {
    if (!splitter) {
        return;
    }
    EnsureResizeOverlay(splitter);
    if (!splitter->resizeOverlayHwnd) {
        return;
    }

    HWND parent = GetParent(splitter->hwnd);
    POINT origin = {0, 0};
    ClientToScreen(parent, &origin);
    Rect splitterRc = WindowRect(splitter->hwnd);

    int x = 0, y = 0, dx = 0, dy = 0;
    bool isVert = splitter->type != SplitterType::Horiz;
    if (isVert) {
        x = origin.x + pos.x - 2;
        y = splitterRc.y;
        dx = 4;
        dy = splitterRc.dy;
    } else {
        x = splitterRc.x;
        y = origin.y + pos.y - 2;
        dx = splitterRc.dx;
        dy = 4;
    }

    SetWindowPos(splitter->resizeOverlayHwnd, HWND_TOP, x, y, dx, dy, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(splitter->resizeOverlayHwnd, nullptr, TRUE);
}

static LRESULT CALLBACK ResizeOverlayWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_ERASEBKGND) {
        return TRUE;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        auto* splitter = (Splitter*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (splitter && splitter->brush) {
            SetBrushOrgEx(hdc, 0, 0, nullptr);
            FillRect(hdc, &ps.rcPaint, splitter->brush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void RegisterResizeOverlayClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wcex{};
    FillWndClassEx(wcex, kResizeOverlayClass, ResizeOverlayWndProc);
    RegisterClassExW(&wcex);
    registered = true;
}

Splitter::Splitter() {
    kind = kindSplitter;
}

Splitter::~Splitter() {
    if (resizeOverlayHwnd) {
        DestroyWindow(resizeOverlayHwnd);
        resizeOverlayHwnd = nullptr;
    }
    DeleteObject(brush);
    DeleteObject(bmp);
}

HWND Splitter::Create(const CreateArgs& args) {
    ReportIf(!args.parent);

    isLive = args.isLive;
    type = args.type;
    auto bgCol = args.backgroundColor;
    if (bgCol == kColorUnset) {
        bgCol = GetSysColor(COLOR_BTNFACE);
    }
    SetColors(kColorUnset, bgCol);

    bmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    ReportIf(!bmp);
    brush = CreatePatternBrush(bmp);
    ReportIf(!brush);

    if (!isLive) {
        RegisterResizeOverlayClass();
    }

    CreateCustomArgs cargs;
    // cargs.className = L"SplitterWndClass";
    cargs.parent = args.parent;
    cargs.style = WS_CHILDWINDOW;
    cargs.exStyle = 0;
    CreateCustom(cargs);

    return hwnd;
}

LRESULT Splitter::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (WM_ERASEBKGND == msg) {
        // TODO: should this be FALSE?
        return TRUE;
    }

    if (WM_LBUTTONDOWN == msg) {
        SetCapture(hwnd);
        if (!isLive) {
            Point pos = HwndGetCursorPos(GetParent(hwnd));
            UpdateResizeOverlay(this, pos);
        }
        return 1;
    }

    if (WM_LBUTTONUP == msg) {
        if (!isLive) {
            HideResizeOverlay(this);
        }
        ReleaseCapture();
        Splitter::MoveEvent arg;
        arg.w = this;
        arg.finishedDragging = true;
        onMove.Call(&arg);
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_CAPTURECHANGED == msg) {
        if ((HWND)lparam != hwnd && !isLive) {
            HideResizeOverlay(this);
        }
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterType::Vert == type) {
            curId = IDC_SIZEWE;
        }
        if (!mouseTracking) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            mouseTracking = true;
        }
        if (!isMouseOver) {
            isMouseOver = true;
            HwndScheduleRepaint(hwnd);
        }
        if (hwnd == GetCapture()) {
            Splitter::MoveEvent arg;
            arg.w = this;
            arg.finishedDragging = false;
            onMove.Call(&arg);
            if (!arg.resizeAllowed) {
                curId = IDC_NO;
            } else if (!isLive) {
                Point pos = HwndGetCursorPos(GetParent(hwnd));
                UpdateResizeOverlay(this, pos);
            }
        }
        SetCursorCached(curId);
        return 0;
    }

    if (WM_MOUSELEAVE == msg) {
        mouseTracking = false;
        if (isMouseOver) {
            isMouseOver = false;
            HwndScheduleRepaint(hwnd);
        }
        return 0;
    }

    if (WM_PAINT == msg) {
        OnSplitterPaint(hwnd, AccentColor(bgColor, 30));
        return 0;
    }

    return WndProcDefault(hwnd, msg, wparam, lparam);
}