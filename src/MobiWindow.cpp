#include "MobiWindow.h"
#include "Resource.h"
#include "WinUtil.h"

#define MOBI_FRAME_CLASS_NAME    _T("SUMATRA_MOBI_FRAME")

// TODO: write me
void RestartLayoutTimer()
{
}

static LRESULT CALLBACK MobiWndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}


bool RegisterMobikWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;

    FillWndClassEx(wcex, hinst);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = MOBI_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

    wcex.lpfnWndProc    = MobiWndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}
