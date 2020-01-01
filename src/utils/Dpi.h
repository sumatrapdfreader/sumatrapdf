/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

int DpiGetForHwnd(HWND);
int DpiGet(HWND);
int DpiScale(HWND, int);
int DpiScale(int);
void DpiScale(HWND, int&, int&);
void DpiReset();
