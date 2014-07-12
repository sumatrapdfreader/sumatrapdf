/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Splitter.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "WinUtil.h"

/*
TODO:
 - allow setting background color of splitter window
 - have only one window/class that calls a callback on WM_MOUSEMOVE
 - test if RegisterClassEx() is safe for dups and if yes, remove
   gSplitterClassRegistered
*/

#define SIDEBAR_SPLITTER_CLASS_NAME  L"SidebarSplitter"
#define FAV_SPLITTER_CLASS_NAME      L"FavSplitter"

// TODO: temporary
// in SumatraPDF.cpp
extern void ResizeSidebar(WindowInfo *win);
extern void ResizeFav(WindowInfo *win);

static bool gSplitterClassRegistered = false;

static LRESULT CALLBACK WndProcFavSplitter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
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
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK WndProcSidebarSplitter(HWND hwnd, UINT message,
                                               WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
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
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void RegisterSplitterClass() {
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

