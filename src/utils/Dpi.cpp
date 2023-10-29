/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"
#include "utils/ScopedWin.h"

/* Info from https://code.msdn.microsoft.com/DPI-Tutorial-sample-64134744

DPI Unaware: virtualized to 96 DPI and scaled by the system for the DPI of the monitor where shown

System DPI Aware:
 These apps render themselves according to the DPI of the display where they
 are launched, and they expect that scaling to remain constant for all displays on the system.
 These apps are scaled up or down when moved to a display with a different DPI from the system DPI.

Per-Monitor DPI Aware:
 These apps render themselves for any DPI, and re-render when the DPI changes
 (as indicated by the WM_DPICHANGED window message).
*/

/*
If you have HWND, call DpiScaleX(HWND, x) or DpiScaleY(HWND, y).
If we don't have dpi information for this HWND, we'll create it.

On WM_DPICHANGED call DpiUpdate(HWND) so that we can update
dpi information for that window.

On WM_DESTROY call DpiRemove(HWND) so that we remove it.

For even faster access you can cache struct Dpi somewhere.
*/

#include <shellscalingapi.h>
#pragma comment(lib, "Shcore")

// get uncached dpi
int DpiGetForHwnd(HWND hwnd) {
    // GetDpiForWindow() returns defult 96 DPI for desktop window
    // (most likely desktop has DPI_AWARENESS set to UNAWARE)
    if (!hwnd || (hwnd == HWND_DESKTOP) || (hwnd == GetDesktopWindow())) {
        goto GetGlobalDpi;
    }

    if (DynGetDpiForWindow) {
        uint dpi = DynGetDpiForWindow(hwnd);
        // returns 0 for HWND_DESKTOP
        if (dpi > 0) {
            CrashIf(dpi < 72);
            return (int)dpi;
        }
    }

#if 0
    // TODO: only available in 8.1
    uint dpiX = 96, dpiY = 96;
    HMONITOR h = MonitorFromWindow(hwnd, 0);
    if (h != nullptr) {
        HRESULT hr = GetDpiForMonitor(h, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        if (hr == S_OK) {
            return (int)dpiX;
        }
    }
#endif
GetGlobalDpi:
    ScopedGetDC dc(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    return dpi;
}

int DpiGet(HWND hwnd) {
    int dpi = DpiGetForHwnd(hwnd);
    dpi = RoundUp(dpi, 4);
    return dpi;
}

int DpiScale(HWND hwnd, int x) {
    int dpi = DpiGet(hwnd);
    int res = MulDiv(x, dpi, 96);
    return res;
}

void DpiScale(HWND hwnd, int& x1, int& x2) {
    int dpi = DpiGet(hwnd);
    int nx1 = MulDiv(x1, dpi, 96);
    int nx2 = MulDiv(x2, dpi, 96);
    x1 = nx1;
    x2 = nx2;
}

int DpiScale(HDC hdc, int x) {
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    int res = MulDiv(x, dpi, 96);
    return res;
}
