/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Dpi_h
#define Dpi_h

struct Dpi {
    HWND hwnd;
    // for efficiency we store them as dpi * 100 / 96
    int scaleX;
    int scaleY;
};

Dpi *DpiGet(HWND);

inline int DpiScaleX(HWND hwnd, int x) { return MulDiv(x, DpiGet(hwnd)->scaleX, 100); }
inline int DpiScaleY(HWND hwnd, int y) { return MulDiv(y, DpiGet(hwnd)->scaleY, 100); }

void DpiUpdate(Dpi *);
inline void DpiUpdate(HWND hwnd) { return DpiUpdate(DpiGet(hwnd)); }

void DpiRemove(HWND);

#endif
