/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;
struct ReadingAutoScrollBar;

constexpr UINT_PTR kReadingAutoScrollTimerID = 16;

void ReadingAutoScrollToggle(MainWindow*);
void ReadingAutoScrollStop(MainWindow*);
void ReadingAutoScrollPause(MainWindow*);
void ReadingAutoScrollFaster(MainWindow*);
void ReadingAutoScrollSlower(MainWindow*);
void ReadingAutoScrollReverse(MainWindow*);
void ReadingAutoScrollTick(MainWindow*);
void ReadingAutoScrollTick(MainWindow*, int elapsedMs);
bool ReadingAutoScrollOnKey(MainWindow*, WPARAM key);
bool ReadingAutoScrollOnKey(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool ReadingAutoScrollIsOn(MainWindow*);
void ReadingAutoScrollHideBar(MainWindow*);
void ReadingAutoScrollSyncToTab(WindowTab*);
void ReadingAutoScrollForgetTab(WindowTab*);
void ReadingAutoScrollRelayout(HWND hwndCanvas);
void ReadingAutoScrollDestroy(MainWindow*);
gpui::El* ReadingAutoScrollBarBuild(MainWindow*, gpui::Ctx*);
TempStr ReadingAutoScrollBarStateTemp(int* exitCodeOut);
