/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SplitterWnd.h"

#include "WinUtil.h"

/*
TODO:
 - implement splitter like in Visual Studio, where during move we don't
   re-layout everything, only show how the splitter would move and only
   on WM_LBUTTONUP we would trigger re-layout. This is probably done
   with over-laid top-level window which moves with the cursor
*/

#define SPLITTER_CLASS_NAME          L"SplitterWndClass"

static HCURSOR cursorSizeWE;
static HCURSOR cursorSizeNS;
static HCURSOR cursorNo;

struct SplitterWnd {
    // we don't own this data
    HWND                hwnd;
    void *              ctx;
    SplitterType        type;
    SplitterCallback    cb;
    COLORREF            bgCol;
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
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        ReleaseCapture();
        w->cb(w->ctx, true);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        if (SplitterVert == w->type) {
            SetCursor(cursorSizeWE);
        } else {
            SetCursor(cursorSizeNS);
        }
        if (hwnd == GetCapture()) {
            bool resizingAllowed = w->cb(w->ctx, false);
            if (!resizingAllowed) {
                SetCursor(cursorNo);
            }
        }
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
    if (cursorNo) {
        return;
    }
    cursorNo     = LoadCursor(NULL, IDC_NO);
    cursorSizeWE = LoadCursor(NULL, IDC_SIZEWE);
    cursorSizeNS = LoadCursor(NULL, IDC_SIZENS);

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
