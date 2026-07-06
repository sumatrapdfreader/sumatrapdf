/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"

static int gDpi = 96;

void DpiSetWineOverride(int dpi) {
    if (dpi >= 72) {
        gDpi = dpi;
    }
}

int DpiGetForHwnd(HWND) {
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

int DpiScale(HDC, int x) {
    return DpiScale((HWND) nullptr, x);
}
