/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

Note: maybe I don't need keep a per-HWND cache and instead
always call GetDpiXY() ?
*/

static HWND gLastHwndParent = nullptr;
static HWND gLastHwnd = nullptr;
static UINT gLastDpi = 0;

void DpiReset() {
    gLastHwndParent = nullptr;
    gLastHwnd = nullptr;
    gLastDpi = 0;
}

// Uncached getting of dpi
int DpiGetForHwnd(HWND hwnd) {
    if (DynGetDpiForWindow) {
        // HWND_DESKTOP is 0 and not really HWND
        // GetDpiForWindow(HWND_DESKTOP) returns 0
        if (hwnd == HWND_DESKTOP) {
            hwnd = GetDesktopWindow();
        }
        UINT dpi = DynGetDpiForWindow(hwnd);
        // returns 0 for HWND_DESKTOP,
        if (dpi > 0) {
            CrashIf(dpi < 72);
            return (int)dpi;
        }
    }

#if 0
    // TODO: only available in 8.1
    UINT dpiX = 96, dpiY = 96;
    HMONITOR h = MonitorFromWindow(hwnd, 0);
    if (h != nullptr ) {
        HRESULT hr = GetDpiForMonitor(h, 0 /* MDT_Effective_DPI */, &dpiX, &dpiY);
        if (hr == S_OK) {
            scaleX = (int)dpiX;
            scaleY = (int)dpiY;
            return;
        }
#endif

    ScopedGetDC dc(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    return dpi;
}

int DpiGet(HWND hwnd) {
    CrashIf(!hwnd);
    if (hwnd == gLastHwnd) {
        CrashIf(gLastDpi == 0);
        return gLastDpi;
    }

    HWND hwndParent = hwnd;
    // TODO: not sure if it's worth, perf-wise, to go up level
    while (true) {
        HWND p = GetParent(hwndParent);
        if (p == nullptr) {
            break;
        }
        hwndParent = p;
    }
    if (hwndParent == gLastHwndParent) {
        CrashIf(gLastDpi == 0);
        return gLastDpi;
    }

    int dpi = DpiGetForHwnd(hwnd);
    dpi = RoundUp(dpi, 4);
    gLastDpi = dpi;
    gLastHwnd = hwnd;
    gLastHwndParent = hwndParent;
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

int DpiScale(int x) {
    int dpi = gLastDpi;
    if (dpi == 0) {
        HWND hwnd = GetDesktopWindow();
        dpi = DpiGetForHwnd(hwnd);
    }
    return MulDiv(x, dpi, 96);
}
