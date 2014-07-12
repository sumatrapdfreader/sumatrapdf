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
 - test if RegisterClassEx() is safe for dups and if yes, remove
   gSplitterClassRegistered
 - implement splitter like in Visual Studio, where during move we don't
   re-layout everything, only show how the splitter would move and only
   on WM_LBUTTONUP we would trigger re-layout. This is probably done
   with over-laid top-level window which moves with the cursor
*/

#define SIDEBAR_SPLITTER_CLASS_NAME  L"SidebarSplitter"
#define FAV_SPLITTER_CLASS_NAME      L"FavSplitter"
#define SPLITTER_CLASS_NAME          L"SumatraSplitter"

// TODO: temporary
// in SumatraPDF.cpp
extern void ResizeSidebar(WindowInfo *win);
extern void ResizeFav(WindowInfo *win);

static bool gSplitterClassRegistered = false;

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

static LRESULT CALLBACK WndProcSidebarSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg)
    {
        case WM_MOUSEMOVE:
            if (hwnd == GetCapture()) {
                ResizeSidebar(win);
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

static LRESULT CALLBACK WndProcSplitter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_MOUSEMOVE:
            if (hwnd == GetCapture()) {
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

static void RegisterSplitterClass()
{
    if (gSplitterClassRegistered) {
        return;
    }
    WNDCLASSEX wcex;

    FillWndClassEx(wcex, SIDEBAR_SPLITTER_CLASS_NAME, WndProcSidebarSplitter);
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    ATOM atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, FAV_SPLITTER_CLASS_NAME, WndProcFavSplitter);
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZENS);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, SPLITTER_CLASS_NAME, WndProcSplitter);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    gSplitterClassRegistered = true;
}

HWND CreateHSplitter(HWND parent)
{
    RegisterSplitterClass();
    return CreateWindow(FAV_SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                        0, 0, 0, 0, parent, (HMENU)0,
                        GetModuleHandle(NULL), NULL);
}

HWND CreateVSplitter(HWND parent)
{
    RegisterSplitterClass();
    return CreateWindow(SIDEBAR_SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                        0, 0, 0, 0, parent, (HMENU)0,
                        GetModuleHandle(NULL), NULL);
}

struct Splitter {
    Splitter() {};
    ~Splitter() {};

    HWND        hwnd;
    // we don't own this data
    void *      ctx;
    onMouseMove cb;
};

Splitter *CreateSpliter(HWND parent, SplitterType type, void *ctx, onMouseMove cb)
{
    RegisterSplitterClass();
    Splitter *s = new Splitter();
    s->ctx = ctx;
    s->cb = cb;
    s->hwnd = CreateWindow(SPLITTER_CLASS_NAME, L"", WS_CHILDWINDOW,
                           0, 0, 0, 0, parent, (HMENU)0,
                           GetModuleHandle(NULL), s);

    /*
    if (SplitterVert == type) {
        SetCursor(s->hwnd, LoadCursor(NULL, IDC_SIZENS);
    } else {
        SetCursor(s->hwnd, LoadCursor(NULL, IDC_SIZEWE);
    }
    */
    return s;
}

void DeleteSplitter(Splitter *s)
{
    // Splitter::hwnd will be destroyed when parent is destroyed
    delete s;
}

