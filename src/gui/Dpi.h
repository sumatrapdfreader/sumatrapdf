/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Debug: scaling percentage forced on every window, ignoring the real monitor
// DPI. 0 (the default) means "use the system DPI"; 125 / 150 etc. pretend the
// whole app is on a 125% / 150% monitor. Lets DPI-change handling be tested
// without a second monitor. HWND_DESKTOP keeps reporting the real system DPI,
// so code that mixes per-window and system DPI misbehaves here the same way it
// does on a real multi-monitor setup.
extern int gDpiOverride;

// Current layout DPI. Set at the start of layout / paint / size / DPI-change
// (DpiSetFromHwnd / DpiSet). DpiScale() and DpiGet() read these.
extern int dpiX;
extern int dpiY;

int DpiGetForHwnd(HWND);
int DpiGetForPoint(int x, int y);
int DpiGet();
void DpiSetWineOverride(int dpi);
void DpiSetFromHwnd(HWND);
void DpiSet(int x, int y);
int DpiScaleByDpi(int dpi, int n);
int DpiScale(int x);
void DpiScale(int& x, int& y);

int DpiGetSystemMetrics(int index);
int DpiGetSystemMetrics(int index, int dpi);
