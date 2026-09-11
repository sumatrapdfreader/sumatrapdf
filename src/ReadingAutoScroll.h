/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct ReadingAutoScrollBar;

constexpr UINT_PTR kReadingAutoScrollTimerID = 16;

void ReadingAutoScrollToggle(MainWindow*);
void ReadingAutoScrollStop(MainWindow*);
void ReadingAutoScrollPause(MainWindow*);
void ReadingAutoScrollFaster(MainWindow*);
void ReadingAutoScrollSlower(MainWindow*);
void ReadingAutoScrollReverse(MainWindow*);
void ReadingAutoScrollTick(MainWindow*);
bool ReadingAutoScrollOnKey(MainWindow*, WPARAM key);
bool ReadingAutoScrollIsOn(MainWindow*);
void ReadingAutoScrollRelayout(HWND hwndCanvas);
void ReadingAutoScrollDestroy(MainWindow*);
TempStr ReadingAutoScrollBarStateTemp(int* exitCodeOut);
