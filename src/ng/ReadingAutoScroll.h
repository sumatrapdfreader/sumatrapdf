/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct ReadingAutoScrollBar;

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

void ReadingAutoScrollToggle(MainWindow*);
void ReadingAutoScrollStop(MainWindow*);
void ReadingAutoScrollPause(MainWindow*);
void ReadingAutoScrollFaster(MainWindow*);
void ReadingAutoScrollSlower(MainWindow*);
void ReadingAutoScrollReverse(MainWindow*);
// ng: orig runs a 10 ms WM_TIMER on the canvas; the shell's tick drives this
void ReadingAutoScrollTick(MainWindow*, int elapsedMs);
bool ReadingAutoScrollOnKey(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool ReadingAutoScrollIsOn(MainWindow*);
void ReadingAutoScrollHideBar(MainWindow*);
void ReadingAutoScrollSyncToTab(WindowTab*);
void ReadingAutoScrollForgetTab(WindowTab*);
void ReadingAutoScrollDestroy(MainWindow*);
// ng: orig's bar is a WS_POPUP window it positions itself; here it is an
// element the canvas puts at the bottom of its own rect
gpui::El* ReadingAutoScrollBarBuild(MainWindow*, gpui::Ctx*);
TempStr ReadingAutoScrollBarStateTemp(int* exitCodeOut);
