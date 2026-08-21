/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Debug: scaling percentage forced on every window, ignoring the real monitor
// DPI. 0 (the default) means "use the system DPI"; 125 / 150 etc. pretend the
// whole app is on a 125% / 150% monitor. Lets DPI-change handling be tested
// without a second monitor. HWND_DESKTOP keeps reporting the real system DPI,
// so code that mixes per-window and system DPI misbehaves here the same way it
// does on a real multi-monitor setup.
extern int gDpiOverride;

// Current layout DPI. DpiScale() and DpiGet() read these. Message dispatch
// (WindowBase / ControlBase / the frame, canvas and VirtHost wndprocs) sets
// them per window through DpiScope; code that runs outside a wndproc (window
// creation, startup) sets them explicitly with DpiSetFromHwnd / DpiSet.
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

// Sets the layout dpi from hwnd for the current scope and restores the
// previous value on exit. Used at the top of message dispatch, so a nested
// dispatch (a modal dialog, a SendMessage to a window on another monitor,
// DestroyWindow tearing down children) can't leave its dpi behind in the
// handler that resumes after it.
struct DpiScope {
    int prevX;
    int prevY;

    explicit DpiScope(HWND hwnd) : prevX(dpiX), prevY(dpiY) { DpiSetFromHwnd(hwnd); }
    DpiScope(const DpiScope&) = delete;
    DpiScope& operator=(const DpiScope&) = delete;
    ~DpiScope() {
        // raw restore: the saved values already went through DpiSet()
        dpiX = prevX;
        dpiY = prevY;
    }
};

int DpiGetSystemMetrics(int index);
int DpiGetSystemMetrics(int index, int dpi);
