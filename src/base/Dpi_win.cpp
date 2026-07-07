/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/WinDynCalls.h"
#include "base/ScopedWin.h"

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

constexpr int kMdtEffectiveDpi = 0;

static int gWineDpiOverride = 0;

void DpiSetWineOverride(int dpi) {
    gWineDpiOverride = dpi;
}

static int DpiApplyWineOverride(int dpi) {
    if (gWineDpiOverride > dpi) {
        return gWineDpiOverride;
    }
    return dpi;
}

// get uncached dpi
int DpiGetForHwnd(HWND hwnd) {
    // GetDpiForWindow() returns defult 96 DPI for desktop window
    // (most likely desktop has DPI_AWARENESS set to UNAWARE)
    if (hwnd && hwnd != HWND_DESKTOP && hwnd != GetDesktopWindow()) {
        if (DynGetDpiForWindow) {
            uint dpiWin = DynGetDpiForWindow(hwnd);
            // returns 0 for HWND_DESKTOP
            if (dpiWin >= 72) {
                return DpiApplyWineOverride((int)dpiWin);
            }
        }

        if (DynGetDpiForMonitor) {
            uint dpiX = 96, dpiY = 96;
            HMONITOR h = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (h != nullptr) {
                HRESULT hr = DynGetDpiForMonitor(h, kMdtEffectiveDpi, &dpiX, &dpiY);
                if (hr == S_OK && dpiX >= 72) {
                    return DpiApplyWineOverride((int)dpiX);
                }
            }
        }
    }

    ScopedGetDC dc(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    if (dpi < 72) {
        HDC screenDC = GetDC(nullptr);
        if (screenDC) {
            int screenDpi = GetDeviceCaps(screenDC, LOGPIXELSX);
            ReleaseDC(nullptr, screenDC);
            if (screenDpi >= 72) {
                dpi = screenDpi;
            }
        }
    }
    if (dpi < 72) {
        dpi = 96;
    }
    return DpiApplyWineOverride(dpi);
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
