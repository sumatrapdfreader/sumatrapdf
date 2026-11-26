/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

int gDpiOverride = 0;
static thread_local int dpiX = 96;
static thread_local int dpiY = 96;

int DpiGetForHwnd(HWND hwnd) {
    // see the note in Dpi.h: the override doesn't apply to the desktop
    if (gDpiOverride > 0 && hwnd) {
        return (96 * gDpiOverride) / 100;
    }
    return DpiGet();
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

DpiScope::DpiScope(HWND hwnd) : prevX(dpiX), prevY(dpiY) {
    DpiSetFromHwnd(hwnd);
}

DpiScope::DpiScope(int x, int y) : prevX(dpiX), prevY(dpiY) {
    DpiSet(x, y);
}

DpiScope::~DpiScope() {
    dpiX = prevX;
    dpiY = prevY;
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

int DpiGetSystemMetrics(int index, int dpi) {
    constexpr int kScrollbarSize = 17;
    if (index == SM_CXVSCROLL || index == SM_CYHSCROLL) {
        return DpiScaleByDpi(dpi, kScrollbarSize);
    }
    return 0;
}

int DpiGetSystemMetrics(int index) {
    return DpiGetSystemMetrics(index, DpiGet());
}

bool DpiPosix_UnitTests() {
    return DpiGetSystemMetrics(SM_CXVSCROLL, 96) == 17 && DpiGetSystemMetrics(SM_CYHSCROLL, 192) == 34 &&
           DpiGetSystemMetrics(-1, 96) == 0;
}
