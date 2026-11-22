/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by ReadingAutoScrollCommon.cpp and each app's ReadingAutoScroll.cpp ---

int ReadingAutoScrollSpeedCount();
float ReadingAutoScrollSpeedAt(int idx);
float ReadingAutoScrollCurSpeed(WindowTab* tab);
int ReadingAutoScrollSpeedIdx(float s);
void ReadingAutoScrollSetSpeed(WindowTab* tab, float s);
void ReadingAutoScrollStepSpeed(WindowTab* tab, int dir);
TempStr ReadingAutoScrollStatusTemp(WindowTab* tab);
DisplayModel* ReadingAutoScrollModel(WindowTab* tab);
WindowTab* ReadingAutoScrollActiveTab(MainWindow* win);
WindowTab* ReadingAutoScrollSessionTab(MainWindow* win);
bool ReadingAutoScrollAtLimit(DisplayModel* dm, int dir);
void ReadingAutoScrollClearTab(WindowTab* tab);

TempStr ReadingAutoScrollLabelTemp(WindowTab* tab);
void ReadingAutoScrollArrowSpeed(MainWindow* win, WindowTab* tab, int keyDir);
void ReadingAutoScrollDigitSpeed(MainWindow* win, int digit);

void ReadingAutoScrollBarSetTab(MainWindow* win, WindowTab* tab);

// implemented by each app
void ReadingAutoScrollBarUpdate(MainWindow* win, bool forceLayout = false);
void ReadingAutoScrollBarHide(MainWindow* win);
void ArmReadingTimer(MainWindow* win, WindowTab* tab);
void KillReadingTimer(MainWindow* win);
void StopMiddleClickScroll(MainWindow* win);
float ReadingTickSeconds(WindowTab* tab, int elapsedMs);
ReadingAutoScrollBar* ReadingAutoScrollBarEnsure(MainWindow* win);
