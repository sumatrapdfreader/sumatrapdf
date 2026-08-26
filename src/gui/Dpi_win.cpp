/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/ScopedWin.h"
#include "gui/Dpi.h"

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

int gDpiOverride = 0;
int dpiX = 96;
int dpiY = 96;

static int gWineDpiOverride = 0;
static bool gDpiOverrideLegacy = false;

static void DpiMaybeReadEnvOverride() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    if (gDpiOverride > 0) {
        return;
    }
    char buf[16]{};
    DWORD n = GetEnvironmentVariableA("SUMATRA_DPI_OVERRIDE", buf, (DWORD)sizeof(buf));
    if (n == 0 || n >= (DWORD)sizeof(buf)) {
        return;
    }
    char* pctText = buf;
    bool legacy = str::StartsWith(Str(buf, (int)n), StrL("legacy:"));
    if (legacy) {
        pctText += 7;
    }
    int pct = atoi(pctText);
    if (pct >= 50 && pct <= 500) {
        gDpiOverride = pct;
        gDpiOverrideLegacy = legacy;
    }
}

void DpiSetWineOverride(int dpi) {
    gWineDpiOverride = dpi;
}

static int DpiApplyWineOverride(int dpi) {
    if (gWineDpiOverride > dpi) {
        return gWineDpiOverride;
    }
    return dpi;
}

static bool DpiIsDesktopHwnd(HWND hwnd) {
    return !hwnd || hwnd == HWND_DESKTOP || hwnd == GetDesktopWindow();
}

static bool DpiFromMonitor(HMONITOR h, int* outX, int* outY) {
    if (!h || !DynGetDpiForMonitor || gDpiOverrideLegacy) {
        return false;
    }
    uint monX = 96, monY = 96;
    HRESULT hr = DynGetDpiForMonitor(h, kMdtEffectiveDpi, &monX, &monY);
    if (hr != S_OK || monX < 72) {
        return false;
    }
    *outX = DpiApplyWineOverride((int)monX);
    *outY = DpiApplyWineOverride((int)(monY >= 72 ? monY : monX));
    return true;
}

// The monitor that contains (x,y), for seeding layout DPI before a window
// exists. GetForegroundWindow / HWND_DESKTOP report the *primary* monitor.
int DpiGetForPoint(int x, int y) {
    DpiMaybeReadEnvOverride();
    if (gDpiOverride > 0 && !gDpiOverrideLegacy) {
        return MulDiv(96, gDpiOverride, 100);
    }
    POINT pt{x, y};
    int dx = 96, dy = 96;
    if (DpiFromMonitor(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &dx, &dy)) {
        return dx;
    }
    // GetDpiForMonitor is unavailable on Windows 7. It only supports one
    // system DPI, which the desktop DC reports even though GetDpiForWindow is
    // also unavailable. Returning 96 here left the installer and first app
    // window unscaled while their system fonts were scaled.
    return DpiGetForHwnd(HWND_DESKTOP);
}

// Uncached per-window DPI. HWND_DESKTOP / null report the system (primary
// monitor) DPI and ignore gDpiOverride, so a multi-monitor setup and the
// override test path both see per-window vs system DPI disagree.
// Prefer the monitor the window sits on: GetDpiForWindow() on a newly created
// (still hidden) hwnd returns the process / primary DPI, which made the
// toolbar and tab bar 2.5× too big when launching on a 100% screen next to a
// 250% primary (discussion #4831).
static void DpiQueryForHwnd(HWND hwnd, int* outX, int* outY) {
    DpiMaybeReadEnvOverride();
    int x = 96;
    int y = 96;
    if (gDpiOverride > 0 && !DpiIsDesktopHwnd(hwnd)) {
        x = y = MulDiv(96, gDpiOverride, 100);
        *outX = x;
        *outY = y;
        return;
    }
    if (!DpiIsDesktopHwnd(hwnd)) {
        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (root && !IsWindowVisible(root)) {
            // A newly-created hidden popup with CW_USEDEFAULT is parked on the
            // primary monitor until its owner positions it. Keep the DPI the
            // caller seeded from the owner; querying the temporary position
            // here makes its layout use the primary monitor's scale.
            *outX = dpiX > 0 ? dpiX : 96;
            *outY = dpiY > 0 ? dpiY : *outX;
            return;
        }
        if (DpiFromMonitor(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &x, &y)) {
            *outX = x;
            *outY = y;
            return;
        }
        if (DynGetDpiForWindow) {
            uint dpiWin = DynGetDpiForWindow(hwnd);
            if (dpiWin >= 72) {
                x = y = DpiApplyWineOverride((int)dpiWin);
                *outX = x;
                *outY = y;
                return;
            }
        }
    }

    ScopedGetDC dc(hwnd);
    x = GetDeviceCaps(dc, LOGPIXELSX);
    y = GetDeviceCaps(dc, LOGPIXELSY);
    if (gDpiOverrideLegacy) {
        x = y = MulDiv(96, gDpiOverride, 100);
    }
    if (x < 72) {
        HDC screenDC = GetDC(nullptr);
        if (screenDC) {
            int screenX = GetDeviceCaps(screenDC, LOGPIXELSX);
            int screenY = GetDeviceCaps(screenDC, LOGPIXELSY);
            ReleaseDC(nullptr, screenDC);
            if (screenX >= 72) {
                x = screenX;
                y = screenY >= 72 ? screenY : screenX;
            }
        }
    }
    if (x < 72) {
        x = 96;
    }
    if (y < 72) {
        y = x;
    }
    *outX = DpiApplyWineOverride(x);
    *outY = DpiApplyWineOverride(y);
}

int DpiGetForHwnd(HWND hwnd) {
    int x = 96, y = 96;
    DpiQueryForHwnd(hwnd, &x, &y);
    return x;
}

int DpiGet() {
    return dpiX > 0 ? dpiX : 96;
}

void DpiSet(int x, int y) {
    if (x <= 0) {
        x = 96;
    }
    if (y <= 0) {
        y = x;
    }
    dpiX = RoundUp(x, 4);
    dpiY = RoundUp(y, 4);
}

void DpiSetFromHwnd(HWND hwnd) {
    // a child hwnd (tooltip, toolbar host, …) can report the process /
    // primary DPI while the parent frame is still hidden. Always take the
    // top-level window so CreateToolbar / GetAppFont see the frame's monitor.
    if (hwnd && !DpiIsDesktopHwnd(hwnd)) {
        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (root) {
            hwnd = root;
        }
    }
    int x = 96, y = 96;
    DpiQueryForHwnd(hwnd, &x, &y);
    DpiSet(x, y);
}

int DpiScaleByDpi(int dpi, int n) {
    if (dpi <= 0) {
        dpi = 96;
    }
    return MulDiv(n, dpi, 96);
}

int DpiScale(int x) {
    return DpiScaleByDpi(DpiGet(), x);
}

void DpiScale(int& x, int& y) {
    x = DpiScaleByDpi(dpiX > 0 ? dpiX : 96, x);
    y = DpiScaleByDpi(dpiY > 0 ? dpiY : 96, y);
}

// GetSystemMetrics() for the dpi of a specific monitor/window. Plain
// GetSystemMetrics() always answers for the *system* dpi (the primary
// monitor's), so under PerMonitorV2 scrollbar widths, caption heights, border
// sizes etc. come out wrong on any monitor scaled differently from the primary.
// Only pass indices that actually depend on dpi (SM_CXVSCROLL, SM_CYCAPTION,
// SM_CXEDGE, ...); screen sizes are in physical pixels and don't.
int DpiGetSystemMetrics(int index, int dpi) {
    if (dpi <= 0) {
        dpi = 96;
    }
    if (DynGetSystemMetricsForDpi) {
        // returns 0 on failure, which is never a real answer for a dpi-dependent
        // metric, so treating it as "fall back" is safe
        int res = DynGetSystemMetricsForDpi(index, (uint)dpi);
        if (res > 0) {
            return res;
        }
    }
    // pre-Win10 1607: scale the system-dpi answer ourselves
    int res = GetSystemMetrics(index);
    int sysDpi = DpiGetForHwnd(HWND_DESKTOP);
    if (sysDpi > 0 && sysDpi != dpi) {
        res = MulDiv(res, dpi, sysDpi);
    }
    return res;
}

int DpiGetSystemMetrics(int index) {
    return DpiGetSystemMetrics(index, DpiGet());
}
