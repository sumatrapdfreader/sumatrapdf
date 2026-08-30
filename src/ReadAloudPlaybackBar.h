/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;
struct MainWindow;
struct ReadAloudPlaybackBar;

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab);
void ReadAloudPlaybackBarHide(MainWindow* win);
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab);
void ReadAloudPlaybackBarRelayout(HWND hwndCanvas);
void ReadAloudPlaybackBarTick(MainWindow* win);

void ReadAloudPlaybackPauseOrResume();
void ReadAloudPlaybackStop();
void ReadAloudPlaybackCycleSpeed(int dir);
void ReadAloudPlaybackBarDestroy(MainWindow* win);
TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut);