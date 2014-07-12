/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Splitter.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "WinUtil.h"

/*
TODO:
 - implement splitter like in Visual Studio, where during move we don't
   re-layout everything, only show how the splitter would move and only
   on WM_LBUTTONUP we would trigger re-layout. This is probably done
   with over-laid top-level window which moves with the cursor
 - move to src/utils
*/

#define SPLITTER_CLASS_NAME          L"SumatraSplitter"

struct Splitter {
    // we don't own this data
    HWND                hwnd;
    void *              ctx;
    SplitterType        type;
    SplitterCallback    cb;
    COLORREF            bgCol;
};

static void OnPaint(Splitter *splitter, HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HBRUSH br = CreateSolidBrush(splitter->bgCol);
    FillRect(hdc, &ps.rcPaint, br);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

    Splitter *splitter = NULL;
    if (WM_NCCREATE == msg) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lp);
        splitter = reinterpret_cast<Splitter *>(lpcs->lpCreateParams);
        splitter->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(splitter));
        goto Exit;
    } else {
        splitter = reinterpret_cast<Splitter *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!splitter) {
        goto Exit;
    }

    if (WM_LBUTTONDOWN == msg) {
        SetCapture(hwnd);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        ReleaseCapture();
        splitter->cb(splitter->ctx, true);
        SetCursor(gCursorArrow);
        return 0;
    }

    if (WM_MOUSELEAVE == msg) {
        SetCursor(gCursorArrow);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        if (SplitterVert == splitter->type) {
            SetCursor(gCursorSizeWE);
        } else {
            SetCursor(gCursorSizeNS);
        }
        if (hwnd == GetCapture()) {
            bool resizingAllowed = splitter->cb(splitter->ctx, false);
            if (!resizingAllowed) {
                SetCursor(gCursorNo);
            }
        }
        return 0;
    }

    if (WM_PAINT == msg) {
        OnPaint(splitter, hwnd);
        return 0;
    }

Exit:
    return DefWindowProc(hwnd, msg, wp, lp);
}

void RegisterSplitterWndClass()
{
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, SPLITTER_CLASS_NAME, WndProcSplitter);
    RegisterClassEx(&wcex);
}

// to delete, free()
Splitter *CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb)
{
    Splitter *s = AllocStruct<Splitter>();
    s->ctx = ctx;
    s->cb = cb;
    s->type = type;
    s->bgCol = GetSysColor(COLOR_BTNFACE);
    s->hwnd = CreateWindow(SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                           0, 0, 0, 0, parent, (HMENU)0,
                           GetModuleHandle(NULL), s);
    return s;
}

HWND GetSplitterHwnd(Splitter *s)
{
    return s->hwnd;
}

void SetSplitterBgCol(Splitter *s, COLORREF c)
{
    s->bgCol = c;
    InvalidateRect(s->hwnd, NULL, false);
}
