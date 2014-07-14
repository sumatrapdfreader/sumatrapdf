/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SplitterWnd.h"

#include "WinCursors.h"
#include "WinUtil.h"

// the technique for drawing the splitter for non-live resize is described
// at http://www.catch22.net/tuts/splitter-windows

#define SPLITTER_CLASS_NAME          L"SplitterWndClass"

// TODO: always use checkered as it looks nicer than solid?
static const bool useCheckeredBar = true;

// TODO: should call DeleteObject() on them on exit
static HBITMAP splitterBmp = NULL;
static HBRUSH splitterBrush = NULL;

struct SplitterWnd {
    // none of this data needs to be freed by us
    HWND                hwnd;
    void *              ctx;
    SplitterType        type;
    SplitterCallback    cb;
    COLORREF            bgCol;
    int                 prevResizeLinePos;
};

static bool IsLive(SplitterWnd *w)
{
    return (SplitterHorizLive == w->type) || (SplitterVertLive == w->type);
}

static void OnPaint(SplitterWnd *w)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w->hwnd, &ps);
    HBRUSH br = CreateSolidBrush(w->bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    DeleteObject(br);
    EndPaint(w->hwnd, &ps);
}

static void DrawLineHAtY(HDC hdc, RECT& rc, int y)
{
    MoveToEx(hdc, rc.left, y, NULL);
    LineTo(hdc, rc.right, y);
}

static void DrawLineVAtX(HDC hdc, RECT& rc, int x)
{
    MoveToEx(hdc, x, rc.top, NULL);
    LineTo(hdc, x, rc.bottom);
}

static void DrawXorBar(HDC hdc, int x1, int y1, int width, int height)
{
    SetBrushOrgEx(hdc, x1, y1, 0);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, splitterBrush);
    PatBlt(hdc, x1, y1, width, height, PATINVERT);
    SelectObject(hdc, hbrushOld);
}

static HDC InitDraw(SplitterWnd *w, RECT& rc)
{
    rc = ChildPosWithinParent(w->hwnd);
    HDC hdc = GetDC(GetParent(w->hwnd));
    SetROP2(hdc, R2_NOTXORPEN);
    return hdc;
}

static void DrawResizeLineV(SplitterWnd *w, int x)
{
    RECT rc;
    HDC hdc = InitDraw(w, rc);
    if (useCheckeredBar) {
        DrawXorBar(hdc, x, rc.top, 4, RectDy(rc));
    } else {
        for (int i=0; i<4; i++) {
            DrawLineVAtX(hdc, rc, x+i);
        }
    }
    ReleaseDC(GetParent(w->hwnd), hdc);
}

static void DrawResizeLineH(SplitterWnd *w, int y)
{
    RECT rc;
    HDC hdc = InitDraw(w, rc);

    if (useCheckeredBar) {
        DrawXorBar(hdc, rc.left, y, RectDx(rc), 4);
    } else {
        for (int i=0; i<4; i++) {
            DrawLineHAtY(hdc, rc, y+i);
        }
    }
    ReleaseDC(GetParent(w->hwnd), hdc);
}

static int GetCursorPos(SplitterWnd *w, bool& vert)
{
    POINT cp;
    GetCursorPos(&cp);
    ScreenToClient(GetParent(w->hwnd), &cp);

    if (w->type == SplitterHoriz) {
        vert = false;
        return cp.y;
    }
    vert = true;
    return cp.x;
}

static void DrawResizeLine(SplitterWnd *w, bool isVert, int pos)
{
    if (isVert)
        DrawResizeLineV(w, pos);
    else
        DrawResizeLineH(w, pos);
}

static void DrawResizeLine(SplitterWnd *w, bool erasePrev, bool drawCurr)
{
    if (IsLive(w)) {
        return;
    }
    bool isVert;
    int pos = GetCursorPos(w, isVert);

    if (erasePrev) {
        DrawResizeLine(w, isVert, w->prevResizeLinePos);
    }

    if (drawCurr) {
        DrawResizeLine(w, isVert, pos);
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
        DrawResizeLine(w, false, true);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        DrawResizeLine(w, true, false);
        ReleaseCapture();
        w->cb(w->ctx, true);
        ScheduleRepaint(w->hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        LPWSTR curId = IDC_SIZENS;
        if ((SplitterVert == w->type) || (SplitterVertLive == w->type)) {
            curId = IDC_SIZEWE;
        }
        if (hwnd == GetCapture()) {
            bool resizingAllowed = w->cb(w->ctx, false);
            if (!resizingAllowed) {
                curId = IDC_NO;
            } else {
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

// to delete, free()
SplitterWnd *CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb)
{
    SplitterWnd *w = AllocStruct<SplitterWnd>();
    w->ctx = ctx;
    w->cb = cb;
    w->type = type;
    w->bgCol = GetSysColor(COLOR_BTNFACE);
    // sets w->hwnd during WM_NCCREATE
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
