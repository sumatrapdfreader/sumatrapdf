/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"

int gDpiOverride = 0;

static int gDpi = 96;

void DpiSetWineOverride(int dpi) {
    if (dpi >= 72) {
        gDpi = dpi;
    }
}

int DpiGetForHwnd(HWND hwnd) {
    // see the note in Dpi.h: the override doesn't apply to the desktop
    if (gDpiOverride > 0 && hwnd) {
        return (96 * gDpiOverride) / 100;
    }
    return gDpi;
}

int DpiGet(HWND hwnd) {
    return DpiGetForHwnd(hwnd);
}

int DpiScale(HWND hwnd, int x) {
    return (int)(((i64)x * DpiGet(hwnd)) / 96);
}

void DpiScale(HWND hwnd, int& x1, int& x2) {
    x1 = DpiScale(hwnd, x1);
    x2 = DpiScale(hwnd, x2);
}

int DpiScale(HDC /*hdc*/, int x) {
    return DpiScale((HWND) nullptr, x);
}

int DpiGetSystemMetrics(int /*index*/, int /*dpi*/) {
    return 0;
}

int DpiGetSystemMetrics(HWND /*hwnd*/, int /*index*/) {
    return 0;
}
