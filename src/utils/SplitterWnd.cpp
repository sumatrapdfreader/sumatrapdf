/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SplitterWnd.h"

#include "BitManip.h"
#include "WinCursors.h"
#include "WinUtil.h"

// the technique for drawing the splitter for non-live resize is described
// at http://www.catch22.net/tuts/splitter-windows

#define SPLITTER_CLASS_NAME          L"SplitterWndClass"

static HBITMAP splitterBmp = NULL;
static HBRUSH  splitterBrush = NULL;

struct SplitterWnd {
    // none of this data needs to be freed by us
    HWND                hwnd;
    void *              ctx;
    SplitterType        type;
    SplitterCallback    cb;
    COLORREF            bgCol;
    bool                isLive;
    PointI              prevResizeLinePos;
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool                parentClipsChildren;
};

static void OnPaint(SplitterWnd *w)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w->hwnd, &ps);
    HBRUSH br = CreateSolidBrush(w->bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    DeleteObject(br);
    EndPaint(w->hwnd, &ps);
}

static void DrawXorBar(HDC hdc, int x1, int y1, int width, int height)
{
    SetBrushOrgEx(hdc, x1, y1, 0);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, splitterBrush);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(SplitterWnd *w, RectI& rc)
{
    rc = ChildPosWithinParent(w->hwnd);
    HDC hdc = GetDC(GetParent(w->hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(SplitterWnd *w, int x)
{
    RectI rc;
    HDC hdc = InitDraw(w, rc);
    DrawXorBar(hdc, x, rc.y, 4, rc.dy);
    ReleaseDC(GetParent(w->hwnd), hdc);
}

static void DrawResizeLineH(SplitterWnd *w, int y)
{
    RectI rc;
    HDC hdc = InitDraw(w, rc);
    DrawXorBar(hdc, rc.x, y, rc.dx, 4);
    ReleaseDC(GetParent(w->hwnd), hdc);
}

static void DrawResizeLineVH(SplitterWnd *w, bool isVert, PointI pos)
{
    if (isVert)
        DrawResizeLineV(w, pos.x);
    else
        DrawResizeLineH(w, pos.y);
}

static void DrawResizeLine(SplitterWnd *w, bool erasePrev, bool drawCurr)
{
    PointI pos;
    GetCursorPosInHwnd(GetParent(w->hwnd), pos);
    bool isVert = w->type != SplitterHoriz;

    if (erasePrev) {
        DrawResizeLineVH(w, isVert, w->prevResizeLinePos);
    }
    if (drawCurr) {
        DrawResizeLineVH(w, isVert, pos);
    }
    w->prevResizeLinePos = pos;
}

static LRESULT CALLBACK WndProcSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

    SplitterWnd *w = NULL;
    if (WM_NCCREATE == msg) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lp);
        w = reinterpret_cast<SplitterWnd *>(lpcs->lpCreateParams);
        w->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(w));
        goto Exit;
    } else {
        w = reinterpret_cast<SplitterWnd *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
            DrawResizeLine(w, false, true);
        }
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        if (!w->isLive) {
            DrawResizeLine(w, true, false);
            if (w->parentClipsChildren) {
                ToggleWindowStyle(GetParent(hwnd), WS_CLIPCHILDREN, true);
            }
        }
        ReleaseCapture();
        w->cb(w->ctx, true);
        ScheduleRepaint(w->hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if (SplitterVert == w->type) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            bool resizingAllowed = w->cb(w->ctx, false);
            if (!resizingAllowed) {
                curId = IDC_NO;
            } else if (!w->isLive) {
                DrawResizeLine(w, true, true);
            }
        }
        SetCursor(curId);
        return 0;
    }

    if (WM_PAINT == msg) {
        OnPaint(w);
        return 0;
    }

Exit:
    return DefWindowProc(hwnd, msg, wp, lp);
}

// call only once at the beginning of program
void RegisterSplitterWndClass()
{
    if (splitterBmp)
        return;

    static WORD dotPatternBmp[8] = 
    { 
        0x00aa, 0x0055, 0x00aa, 0x0055, 
        0x00aa, 0x0055, 0x00aa, 0x0055
    };

    splitterBmp = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    splitterBrush = CreatePatternBrush(splitterBmp);

    WNDCLASSEX wcex;
    FillWndClassEx(wcex, SPLITTER_CLASS_NAME, WndProcSplitter);
    RegisterClassEx(&wcex);
}

// caller needs to free() the result
SplitterWnd *CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb)
{
    SplitterWnd *w = AllocStruct<SplitterWnd>();
    w->ctx = ctx;
    w->cb = cb;
    w->type = type;
    w->bgCol = GetSysColor(COLOR_BTNFACE);
    w->isLive = true;
    DWORD style = GetWindowLong(parent, GWL_STYLE);
    w->parentClipsChildren = bit::IsMaskSet<DWORD>(style, WS_CLIPCHILDREN);
    // w->hwnd is set during WM_NCCREATE
    CreateWindow(SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                           0, 0, 0, 0, parent, (HMENU)0,
                           GetModuleHandle(NULL), w);
    CrashIf(!w->hwnd);
    return w;
}

HWND GetHwnd(SplitterWnd *s)
{
    return s->hwnd;
}

void SetBgCol(SplitterWnd *w, COLORREF c)
{
    w->bgCol = c;
    ScheduleRepaint(w->hwnd);
}

void SetSplitterLive(SplitterWnd *w, bool live)
{
    w->isLive = live;
}

void DeleteSplitterBrush()
{
    DeleteObject(splitterBrush);
    splitterBrush = NULL;
    DeleteObject(splitterBmp);
    splitterBmp = NULL;
}
