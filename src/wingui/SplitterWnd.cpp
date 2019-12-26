/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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

static HBITMAP splitterBmp = nullptr;
static HBRUSH splitterBrush = nullptr;

class SplitterWnd {
  public:
    SplitterWnd(const SplitterWndCb& cb) : cb(cb) {
    }
    ~SplitterWnd() {
    }

    // none of this data needs to be freed by us
    HWND hwnd;
    SplitterType type;
    SplitterWndCb cb;
    COLORREF bgCol;
    bool isLive;
    PointI prevResizeLinePos;
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool parentClipsChildren;
};

static void OnPaint(HWND hwnd, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    ScopedBrush br = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static void DrawXorBar(HDC hdc, int x1, int y1, int width, int height) {
    SetBrushOrgEx(hdc, x1, y1, 0);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, splitterBrush);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(HWND hwnd, RectI& rc) {
    rc = ChildPosWithinParent(hwnd);
    HDC hdc = GetDC(GetParent(hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(HWND hwnd, int x) {
    RectI rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, x, rc.y, 4, rc.dy);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineH(HWND hwnd, int y) {
    RectI rc;
    HDC hdc = InitDraw(hwnd, rc);
    DrawXorBar(hdc, rc.x, y, rc.dx, 4);
    ReleaseDC(GetParent(hwnd), hdc);
}

static void DrawResizeLineVH(HWND hwnd, bool isVert, PointI pos) {
    if (isVert) {
        DrawResizeLineV(hwnd, pos.x);
    } else {
        DrawResizeLineH(hwnd, pos.y);
    }
}

static void DrawResizeLine(HWND hwnd, SplitterType stype, bool erasePrev, bool drawCurr, PointI& prevResizeLinePos) {
    PointI pos;
    GetCursorPosInHwnd(GetParent(hwnd), pos);
    bool isVert = stype != SplitterType::Horiz;

    if (erasePrev) {
        DrawResizeLineVH(hwnd, isVert, prevResizeLinePos);
    }
    if (drawCurr) {
        DrawResizeLineVH(hwnd, isVert, pos);
    }
    prevResizeLinePos = pos;
}

static LRESULT CALLBACK WndProcSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

    SplitterWnd* w = nullptr;
    if (WM_NCCREATE == msg) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lp);
        w = reinterpret_cast<SplitterWnd*>(lpcs->lpCreateParams);
        w->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(w));
        goto Exit;
    } else {
        w = reinterpret_cast<SplitterWnd*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!w) {
        goto Exit;
    }

    if (WM_LBUTTONDOWN == msg) {
        SetCapture(hwnd);
        if (!w->isLive) {
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, false);
            }
            DrawResizeLine(w->hwnd, w->type, false, true, w->prevResizeLinePos);
        }
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        if (!w->isLive) {
            DrawResizeLine(w->hwnd, w->type, true, false, w->prevResizeLinePos);
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        w->cb(true);
        ScheduleRepaint(w->hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterType::Vert == w->type) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            bool resizingAllowed = w->cb(false);
            if (!resizingAllowed) {
                curId = IDC_NO;
            } else if (!w->isLive) {
                DrawResizeLine(w->hwnd, w->type, true, true, w->prevResizeLinePos);
            }
        }
        SetCursor(curId);
        return 0;
    }

    if (WM_PAINT == msg) {
        OnPaint(w->hwnd, w->bgCol);
        return 0;
    }

Exit:
    return DefWindowProc(hwnd, msg, wp, lp);
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

// call only once at the beginning of program
static void RegisterSplitterWndClass() {
    static ATOM atom = 0;

    if (atom != 0) {
        // already registered
        return;
    }

    splitterBmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    splitterBrush = CreatePatternBrush(splitterBmp);

    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, SPLITTER_CLASS_NAME, WndProcSplitter);
    atom = RegisterClassEx(&wcex);
}

// caller needs to free() the result
SplitterWnd* CreateSplitter(HWND parent, SplitterType type, const SplitterWndCb& cb) {
    RegisterSplitterWndClass();
    SplitterWnd* w = new SplitterWnd(cb);
    w->type = type;
    w->bgCol = GetSysColor(COLOR_BTNFACE);
    w->isLive = true;
    DWORD style = GetWindowLong(parent, GWL_STYLE);
    w->parentClipsChildren = bit::IsMaskSet<DWORD>(style, WS_CLIPCHILDREN);
    // w->hwnd is set during WM_NCCREATE
    CreateWindow(SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW, 0, 0, 0, 0, parent, (HMENU)0, GetModuleHandle(nullptr), w);
    CrashIf(!w->hwnd);
    return w;
}

HWND GetHwnd(SplitterWnd* s) {
    return s->hwnd;
}

void SetBgCol(SplitterWnd* w, COLORREF c) {
    w->bgCol = c;
    ScheduleRepaint(w->hwnd);
}

void SetSplitterLive(SplitterWnd* w, bool live) {
    w->isLive = live;
}

void DeleteSplitterBrush() {
    DeleteObject(splitterBrush);
    splitterBrush = nullptr;
    DeleteObject(splitterBmp);
    splitterBmp = nullptr;
}

Kind kindSplitter = "splitter";

SplitterCtrl::SplitterCtrl(HWND parent) {
    kind = kindSplitter;
    winClass = SPLITTER_CLASS_NAME;
    parent = parent;
}

SplitterCtrl::~SplitterCtrl() {
    DeleteObject(brush);
    DeleteObject(bmp);
}

static void SplitterWndProc(WndProcArgs* args) {
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
            DrawResizeLine(w->hwnd, w->type, false, true, w->prevResizeLinePos);
        }
        args->didHandle = true;
        return;
    }

    if (WM_LBUTTONUP == msg) {
        if (!w->isLive) {
            DrawResizeLine(w->hwnd, w->type, true, false, w->prevResizeLinePos);
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        SplitterMoveArgs arg;
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
            arg.done = false;
            w->onSplitterMove(&arg);            
            if (!arg.resizeAllowed) {
                curId = IDC_NO;
            } else if (!w->isLive) {
                DrawResizeLine(w->hwnd, w->type, true, true, w->prevResizeLinePos);
            }
        }
        SetCursor(curId);
        args->didHandle = true;
        return;
    }

    if (WM_PAINT == msg) {
        OnPaint(w->hwnd, w->backgroundColor);
        args->didHandle = true;
        return;
    }
}

bool SplitterCtrl::Create() {
    bmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    CrashIf(!bmp);
    brush = CreatePatternBrush(bmp);
    CrashIf(!brush);

    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    msgFilter = SplitterWndProc;
    Subclass();
    return true;
}
