/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Debug: scaling percentage forced on every window, ignoring the real monitor
// DPI. 0 (the default) means "use the system DPI"; 125 / 150 etc. pretend the
// whole app is on a 125% / 150% monitor. Lets DPI-change handling be tested
// without a second monitor. HWND_DESKTOP keeps reporting the real system DPI,
// so code that mixes per-window and system DPI misbehaves here the same way it
// does on a real multi-monitor setup. SUMATRA_DPI_OVERRIDE=<percent> sets this
// at process start (the installer has no debug command to toggle it). The
// legacy:<percent> form also disables the newer monitor DPI query to emulate
// Windows 7's system-DPI-only path.
int GetDpiOverride();
void SetDpiOverride(int pct);

#if !OS_WIN
// win32's MulDiv: a * b / c, rounded to nearest
inline int MulDiv(int a, int b, int c) {
    if (c == 0) {
        return -1;
    }
    long long n = (long long)a * (long long)b;
    long long half = (long long)(c < 0 ? -c : c) / 2;
    n = n >= 0 ? n + half : n - half;
    return (int)(n / c);
}
#endif

int DpiGetForHwnd(HWND);
int DpiGetForPoint(int x, int y);
int DpiGet();
void DpiSetFromHwnd(HWND);
void DpiSet(int x, int y);
int DpiScaleByDpi(int dpi, int n);
int DpiScale(int x);
void DpiScale(int& x, int& y);

// Sets the thread's layout DPI for the current scope and restores the
// previous value on exit. Used at the top of message dispatch, so a nested
// dispatch (a modal dialog, a SendMessage to a window on another monitor,
// DestroyWindow tearing down children) can't leave its dpi behind in the
// handler that resumes after it.
class DpiScope {
    int prevX;
    int prevY;

  public:
    explicit DpiScope(HWND hwnd);
    DpiScope(int x, int y);
    DpiScope(const DpiScope&) = delete;
    DpiScope& operator=(const DpiScope&) = delete;
    ~DpiScope();
};

#if !OS_WIN
// ng: the win32 metric ids the portable model code asks for. Dpi_posix.cpp
// supplies the portable scrollbar dimensions for them.
#define SM_CXVSCROLL 2
#define SM_CYHSCROLL 3
#endif

int DpiGetSystemMetrics(int index);
int DpiGetSystemMetrics(int index, int dpi);
