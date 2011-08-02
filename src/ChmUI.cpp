/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmUI.h"
#include "SumatraPDF.h"
#include "resource.h"
#include "WinUtil.h"
#include "ChmEngine.h"
#include "AppTools.h"
#include "Menu.h"

#define FRAME_CHM_CLASS_NAME        _T("SUMATRA_CHM_FRAME")
#define HTML_CHM_CLASS_NAME         _T("SUMATRA_CHM_HTML")

       Vec<ChmWindowInfo*>             gChmWindows;

static LRESULT CALLBACK WndProcChmFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcChmHtml(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

ChmWindowInfo::ChmWindowInfo(HWND hwndFrame) :
    loadedFilePath(NULL), chmEngine(NULL), hwndFrame(NULL),
    hwndHtml(NULL), hwndToolbar(NULL), hwndReBar(NULL), menu(NULL)
{
}

ChmWindowInfo::~ChmWindowInfo()
{
    delete chmEngine;
    free(loadedFilePath);
}

ChmWindowInfo *CreateChmWindowInfo()
{
    RectI windowPos = gGlobalPrefs.windowPos;
    if (!windowPos.IsEmpty())
        EnsureAreaVisibility(windowPos);
    else
        windowPos = GetDefaultWindowPos();

    HWND hwndFrame = CreateWindow(
            FRAME_CHM_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;
    ChmWindowInfo *win = new ChmWindowInfo(hwndFrame);
    win->hwndHtml = CreateWindowEx(
            WS_EX_STATICEDGE, 
            HTML_CHM_CLASS_NAME, NULL,
            WS_CHILD | WS_HSCROLL | WS_VSCROLL,
            0, 0, 0, 0, /* position and size determined in OnSize */
            hwndFrame, NULL,
            ghinst, NULL);
    if (!win->hwndHtml) {
        delete win;
        return NULL;
    }

    win->menu = BuildChmMenu(win);

    ShowWindow(win->hwndHtml, SW_SHOW);
    UpdateWindow(win->hwndHtml);

    // TODO:
    //CreateToolbar(win);
    //CreateSidebar(win);

    DragAcceptFiles(win->hwndHtml, TRUE);
    gChmWindows.Append(win);

    // TODO:
    //UpdateWindowRtlLayout(win);
    return win;
}

bool RegisterChmWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcChmFrame;
    wcex.lpszClassName  = FRAME_CHM_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hIconSm        = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SMALL));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = WndProcChmHtml;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = HTML_CHM_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    return true;
}
