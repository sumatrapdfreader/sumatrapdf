/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Dpi {
    HWND hwnd;
    int dpiX;
    int dpiY;
};

Dpi *DpiGet(HWND);
int DpiGetPreciseX(HWND);
int DpiGetPreciseY(HWND);
int DpiScaleX(HDC, int&);
int DpiScaleX(HWND, int);
void DpiScaleX2(HWND, int&, int&);
void DpiScaleY2(HWND, int&, int&);
int DpiScaleY(HWND, int y);
void DpiUpdate(Dpi *);
void DpiUpdate(HWND);
void DpiRemove(HWND);
void DpiRemoveAll();
