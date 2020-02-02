/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

#include "SplitterWnd.h"

// the technique for drawing the splitter for non-live resize is described
// at http://www.catch22.net/tuts/splitter-windows

#define SPLITTER_CLASS_NAME L"SplitterWndClass"

static void OnSplitterCtrlPaint(HWND hwnd, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    AutoDeleteBrush br = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static void DrawXorBar(HDC hdc, HBRUSH br, int x1, int y1, int width, int height) {
    SetBrushOrgEx(hdc, x1, y1, 0);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, br);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(HWND hwnd, RectI& rc) {
    rc = ChildPosWithinParent(hwnd);
    HDC hdc = GetDC(GetParent(hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(HWND hwnd, HBRUSH br, int x) {
    RectI rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, x, rc.y, 4, rc.dy);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineH(HWND hwnd, HBRUSH br, int y) {
    RectI rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, br, rc.x, y, rc.dx, 4);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineVH(HWND hwnd, HBRUSH br, bool isVert, PointI pos) {
    if (isVert) {
        DrawResizeLineV(hwnd, br, pos.x);
    } else {
        DrawResizeLineH(hwnd, br, pos.y);
    }
}

static void DrawResizeLine(HWND hwnd, HBRUSH br, SplitterType stype, bool erasePrev, bool drawCurr,
                           PointI& prevResizeLinePos) {
    PointI pos;
    GetCursorPosInHwnd(GetParent(hwnd), pos);
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

Kind kindSplitter = "splitter";

SplitterCtrl::SplitterCtrl(HWND p) {
    kind = kindSplitter;
    // winClass = SPLITTER_CLASS_NAME;
    parent = p;
    backgroundColor = GetSysColor(COLOR_BTNFACE);
    dwStyle = WS_CHILDWINDOW;
    dwExStyle = 0;
}

SplitterCtrl::~SplitterCtrl() {
    DeleteObject(brush);
    DeleteObject(bmp);
}

static void SplitterCtrlWndProc(WndProcArgs* args) {
    UINT msg = args->msg;
    if (WM_ERASEBKGND == msg) {
        args->didHandle = true;
        // TODO: should this be FALSE?
        args->result = TRUE;
        return;
    }

    HWND hwnd = args->hwnd;
    SplitterCtrl* w = (SplitterCtrl*)args->w;
    CrashIf(!w);
    if (!w) {
        return;
    }

    if (WM_LBUTTONDOWN == msg) {
        SetCapture(hwnd);
        if (!w->isLive) {
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, false);
            }
            DrawResizeLine(w->hwnd, w->brush, w->type, false, true, w->prevResizeLinePos);
        }
        args->didHandle = true;
        return;
    }

    if (WM_LBUTTONUP == msg) {
        if (!w->isLive) {
            DrawResizeLine(w->hwnd, w->brush, w->type, true, false, w->prevResizeLinePos);
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        SplitterMoveArgs arg;
        arg.w = (SplitterCtrl*)args->w;
        arg.done = true;
        w->onSplitterMove(&arg);
        ScheduleRepaint(w->hwnd);
        args->didHandle = true;
        return;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterType::Vert == w->type) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            SplitterMoveArgs arg;
            arg.w = (SplitterCtrl*)args->w;
            arg.done = false;
            w->onSplitterMove(&arg);
            if (!arg.resizeAllowed) {
                curId = IDC_NO;
            } else if (!w->isLive) {
                DrawResizeLine(w->hwnd, w->brush, w->type, true, true, w->prevResizeLinePos);
            }
        }
        SetCursor(curId);
        args->didHandle = true;
        return;
    }

    if (WM_PAINT == msg) {
        OnSplitterCtrlPaint(w->hwnd, w->backgroundColor);
        args->didHandle = true;
        return;
    }
}

bool SplitterCtrl::Create() {
    bmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    CrashIf(!bmp);
    brush = CreatePatternBrush(bmp);
    CrashIf(!brush);

    DWORD style = GetWindowLong(parent, GWL_STYLE);
    parentClipsChildren = bit::IsMaskSet<DWORD>(style, WS_CLIPCHILDREN);

    bool ok = Window::Create();
    if (!ok) {
        return false;
    }
    msgFilter = SplitterCtrlWndProc;
    // Subclass();
    return true;
}
