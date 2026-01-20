/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//- Splitter

// the technique for drawing the splitter for non-live resize is described
// at http://www.catch22.net/tuts/splitter-windows

Kind kindSplitter = "splitter";

static void OnSplitterPaint(HWND hwnd, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    AutoDeleteBrush br = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static void DrawXorBar(HDC hdc, HBRUSH br, int x1, int y1, int width, int height) {
    SetBrushOrgEx(hdc, x1, y1, nullptr);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, br);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(HWND hwnd, Rect& rc) {
    rc = ChildPosWithinParent(hwnd);
    HDC hdc = GetDC(GetParent(hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(HWND hwnd, HBRUSH br, int x) {
    Rect rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, x, rc.y, 4, rc.dy);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineH(HWND hwnd, HBRUSH br, int y) {
    Rect rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, rc.x, y, rc.dx, 4);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineVH(HWND hwnd, HBRUSH br, bool isVert, Point pos) {
    if (isVert) {
        DrawResizeLineV(hwnd, br, pos.x);
    } else {
        DrawResizeLineH(hwnd, br, pos.y);
    }
}

static void DrawResizeLine(HWND hwnd, HBRUSH br, SplitterType stype, bool erasePrev, bool drawCurr,
                           Point& prevResizeLinePos) {
    Point pos = HwndGetCursorPos(GetParent(hwnd));
    bool isVert = stype != SplitterType::Horiz;

    if (erasePrev) {
        DrawResizeLineVH(hwnd, br, isVert, prevResizeLinePos);
    }
    if (drawCurr) {
        DrawResizeLineVH(hwnd, br, isVert, pos);
    }
    prevResizeLinePos = pos;
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

Splitter::Splitter() {
    kind = kindSplitter;
}

Splitter::~Splitter() {
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

    DWORD style = GetWindowLong(args.parent, GWL_STYLE);
    parentClipsChildren = bit::IsMaskSet<DWORD>(style, WS_CLIPCHILDREN);

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
            if (parentClipsChildren) {
                SetWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, false);
            }
            DrawResizeLine(hwnd, brush, type, false, true, prevResizeLinePos);
        }
        return 1;
    }

    if (WM_LBUTTONUP == msg) {
        if (!isLive) {
            DrawResizeLine(hwnd, brush, type, true, false, prevResizeLinePos);
            if (parentClipsChildren) {
                SetWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        Splitter::MoveEvent arg;
        arg.w = this;
        arg.finishedDragging = true;
        onMove.Call(&arg);
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterType::Vert == type) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            Splitter::MoveEvent arg;
            arg.w = this;
            arg.finishedDragging = false;
            onMove.Call(&arg);
            if (!arg.resizeAllowed) {
                curId = IDC_NO;
            } else if (!isLive) {
                DrawResizeLine(hwnd, brush, type, true, true, prevResizeLinePos);
            }
        }
        SetCursorCached(curId);
        return 0;
    }

    if (WM_PAINT == msg) {
        OnSplitterPaint(hwnd, bgColor);
        return 0;
    }

    return WndProcDefault(hwnd, msg, wparam, lparam);
}
