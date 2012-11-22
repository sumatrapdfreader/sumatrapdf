#include "BaseUtil.h"
#include "SumatraAbout2.h"

#include "resource.h"
#include "SumatraPDF.h"
#include "WinUtil.h"

/* This is an experiment to re-implement About window using a generic
layout logic */

#define WND_CLASS_ABOUT2        L"WND_CLASS_SUMATRA_ABOUT2"
#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

static ATOM gAboutWndAtom = 0;
static HWND gHwndAbout2 = NULL;

LRESULT CALLBACK WndProcAbout2(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

}

void OnMenuAbout2()
{
    WNDCLASSEX  wcex;

    if (gHwndAbout2) {
        SetActiveWindow(gHwndAbout2);
        return;
    }

    if (!gAboutWndAtom) {
        FillWndClassEx(wcex, ghinst, WND_CLASS_ABOUT2, WndProcAbout2);
        wcex.hIcon = LoadIcon(ghinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
        gAboutWndAtom = RegisterClassEx(&wcex);
        CrashIf(!gAboutWndAtom)
    }
    gHwndAbout2 = CreateWindow(
            WND_CLASS_ABOUT2, ABOUT_WIN_TITLE,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout2)
        return;

    ShowWindow(gHwndAbout2, SW_SHOW);

}

