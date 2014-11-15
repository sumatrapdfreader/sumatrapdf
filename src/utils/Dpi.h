/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Dpi {
    HWND hwnd;
    int dpiX;
    int dpiY;
};

Dpi *DpiGet(HWND);
int DpiGetPreciseX(HWND hwnd);
int DpiGetPreciseY(HWND hwnd);
inline int DpiScaleX(HWND hwnd, int x) { return MulDiv(x, DpiGet(hwnd)->dpiX, 96); }
inline int DpiScaleY(HWND hwnd, int y) { return MulDiv(y, DpiGet(hwnd)->dpiY, 96); }
void DpiUpdate(Dpi *);
inline void DpiUpdate(HWND hwnd) { return DpiUpdate(DpiGet(hwnd)); }
void DpiRemove(HWND);
void DpiRemoveAll();
