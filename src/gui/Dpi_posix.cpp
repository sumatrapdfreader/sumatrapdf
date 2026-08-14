/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

int gDpiOverride = 0;
int dpiX = 96;
int dpiY = 96;

static int gDpi = 96;

void DpiSetWineOverride(int dpi) {
    if (dpi >= 72) {
        gDpi = dpi;
        dpiX = dpi;
        dpiY = dpi;
    }
}

int DpiGetForHwnd(HWND hwnd) {
    // see the note in Dpi.h: the override doesn't apply to the desktop
    if (gDpiOverride > 0 && hwnd) {
        return (96 * gDpiOverride) / 100;
    }
    return gDpi;
}

int DpiGetForPoint(int, int) {
    return DpiGet();
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
    dpiX = x;
    dpiY = y;
}

void DpiSetFromHwnd(HWND hwnd) {
    int d = DpiGetForHwnd(hwnd);
    DpiSet(d, d);
}

int DpiScaleByDpi(int dpi, int n) {
    if (dpi <= 0) {
        dpi = 96;
    }
    return (int)(((i64)n * dpi) / 96);
}

int DpiScale(int x) {
    return DpiScaleByDpi(DpiGet(), x);
}

void DpiScale(int& x, int& y) {
    x = DpiScaleByDpi(dpiX > 0 ? dpiX : 96, x);
    y = DpiScaleByDpi(dpiY > 0 ? dpiY : 96, y);
}

int DpiGetSystemMetrics(int /*index*/, int /*dpi*/) {
    return 0;
}

int DpiGetSystemMetrics(int index) {
    return DpiGetSystemMetrics(index, DpiGet());
}
