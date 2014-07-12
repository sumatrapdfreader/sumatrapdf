/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Splitter.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "WinUtil.h"

/*
TODO:
 - allow setting background color of splitter window (to better blend with
   ebook window)
 - have only one window/class that calls a callback on WM_MOUSEMOVE
 - implement splitter like in Visual Studio, where during move we don't
   re-layout everything, only show how the splitter would move and only
   on WM_LBUTTONUP we would trigger re-layout. This is probably done
   with over-laid top-level window which moves with the cursor
 - move to src/utils
*/

#define FAV_SPLITTER_CLASS_NAME      L"FavSplitter"
#define SPLITTER_CLASS_NAME          L"SumatraSplitter"

// TODO: temporary
// in SumatraPDF.cpp
extern void ResizeFav(WindowInfo *win);

static LRESULT CALLBACK WndProcFavSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg)
    {
        case WM_MOUSEMOVE:
            if (hwnd == GetCapture()) {
                ResizeFav(win);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

struct Splitter {
    // we don't own this data
    HWND                hwnd;
    void *              ctx;
    SplitterType        type;
    SplitterCallback    cb;
};

static LRESULT CALLBACK WndProcSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    /*
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }*/

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
        goto Exit;
    }

    if (WM_LBUTTONUP == msg) {
        ReleaseCapture();
        splitter->cb(splitter->ctx, true);
        SetCursor(gCursorArrow);
        goto Exit;
    }

    if (WM_MOUSELEAVE == msg) {
        SetCursor(gCursorArrow);
        goto Exit;
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

Exit:
    return DefWindowProc(hwnd, msg, wp, lp);
}

void RegisterSplitterWndClass()
{
    WNDCLASSEX wcex;

    FillWndClassEx(wcex, FAV_SPLITTER_CLASS_NAME, WndProcFavSplitter);
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZENS);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassEx(&wcex);

    FillWndClassEx(wcex, SPLITTER_CLASS_NAME, WndProcSplitter);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassEx(&wcex);
}

HWND CreateHSplitter(HWND parent)
{
    return CreateWindow(FAV_SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                        0, 0, 0, 0, parent, (HMENU)0,
                        GetModuleHandle(NULL), NULL);
}

// to delete, free()
Splitter *CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb)
{
    Splitter *s = AllocStruct<Splitter>();
    s->ctx = ctx;
    s->cb = cb;
    s->type = type;
    s->hwnd = CreateWindow(SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                           0, 0, 0, 0, parent, (HMENU)0,
                           GetModuleHandle(NULL), s);
    return s;
}

HWND GetSplitterHwnd(Splitter *s)
{
    return s->hwnd;
}
