/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;
struct MainWindow;
struct ReadAloudPlaybackBar;

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab);
void ReadAloudPlaybackBarHide(MainWindow* win);
void ReadAloudPlaybackBarRelayout(HWND hwndCanvas);

void ReadAloudPlaybackPauseOrResume();
void ReadAloudPlaybackStop();
void ReadAloudPlaybackCycleSpeed(int dir);
void ReadAloudPlaybackBarDestroy(MainWindow* win);