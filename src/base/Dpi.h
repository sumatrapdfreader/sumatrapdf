/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Debug: scaling percentage forced on every window, ignoring the real monitor
// DPI. 0 (the default) means "use the system DPI"; 125 / 150 etc. pretend the
// whole app is on a 125% / 150% monitor. Lets DPI-change handling be tested
// without a second monitor.
extern int gDpiOverride;

int DpiGetForHwnd(HWND);
int DpiGet(HWND);
void DpiSetWineOverride(int dpi);
int DpiScale(HWND, int);
void DpiScale(HWND, int&, int&);

int DpiScale(HDC, int x);
