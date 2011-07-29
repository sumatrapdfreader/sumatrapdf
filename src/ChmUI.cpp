/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmUI.h"
#include "SumatraPDF.h"
#include "resource.h"
#include "WinUtil.h"
#include "ChmEngine.h"

#define FRAME_CHM_CLASS_NAME        _T("SUMATRA_CHM_FRAME")
#define CANVAS_CHM_CLASS_NAME       _T("SUMATRA_CHM_CANVAS")

static LRESULT CALLBACK WndProcChmFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcChmCanvas(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

ChmWindowInfo::ChmWindowInfo(HWND hwndFrame) :
    loadedFilePath(NULL), chmEngine(NULL), hwndFrame(NULL),
    hwndCanvas(NULL), hwndToolbar(NULL), hwndReBar(NULL)
{
}

ChmWindowInfo::~ChmWindowInfo()
{
    delete chmEngine;
    free(loadedFilePath);
}

ChmWindowInfo *CreateChmWindowInfo()
{
    RectI windowPos;
    if (gGlobalPrefs.windowPos.IsEmpty()) {
        CenterAreaInPrimaryMonitor(windowPos);
    } else {
        windowPos = gGlobalPrefs.windowPos;
        EnsureAreaVisibility(windowPos);
    }

    HWND hwndFrame = CreateWindow(
            FRAME_CHM_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    // TODO: write me
    return NULL;
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
    wcex.lpfnWndProc    = WndProcChmCanvas;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = CANVAS_CHM_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    return true;
}
