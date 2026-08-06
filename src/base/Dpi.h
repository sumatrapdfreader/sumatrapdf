/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Debug: scaling percentage forced on every window, ignoring the real monitor
// DPI. 0 (the default) means "use the system DPI"; 125 / 150 etc. pretend the
// whole app is on a 125% / 150% monitor. Lets DPI-change handling be tested
// without a second monitor. HWND_DESKTOP keeps reporting the real system DPI,
// so code that mixes per-window and system DPI misbehaves here the same way it
// does on a real multi-monitor setup.
extern int gDpiOverride;

int DpiGetForHwnd(HWND);
int DpiGet(HWND);
void DpiSetWineOverride(int dpi);
int DpiScale(HWND, int);
void DpiScale(HWND, int&, int&);

int DpiScale(HDC, int x);

// GetSystemMetrics() for the dpi of a specific monitor/window. Plain
// GetSystemMetrics() always answers for the *system* dpi (the primary
// monitor's), so under PerMonitorV2 scrollbar widths, caption heights, border
// sizes etc. come out wrong on any monitor scaled differently from the primary.
// Only pass indices that actually depend on dpi (SM_CXVSCROLL, SM_CYCAPTION,
// SM_CXEDGE, ...); screen sizes are in physical pixels and don't.
int DpiGetSystemMetrics(int index, int dpi);
int DpiGetSystemMetrics(HWND hwnd, int index);
