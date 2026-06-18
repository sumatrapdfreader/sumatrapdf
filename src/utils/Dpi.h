/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

int DpiGetForHwnd(HWND);
int DpiGet(HWND);
void DpiSetWineOverride(int dpi);
int DpiScale(HWND, int);
void DpiScale(HWND, int&, int&);

int DpiScale(HDC, int x);
