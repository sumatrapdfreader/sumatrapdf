/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by ReadingAutoScrollCommon.cpp and each app's ReadingAutoScroll.cpp ---

// Arrow keys step these. Finer than Acrobat's 0-9 so speed is tunable.
static const float kSpeeds[] = {8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 80, 96, 120, 160, 200, 260, 320};
int SpeedCount();
float CurrentSpeed();
int ClosestSpeedIdx(float s);
void SetSpeed(float s);
void StepSpeed(int dir);
TempStr StatusTextTemp(WindowTab* tab);
DisplayModel* ScrollModel(WindowTab* tab);
WindowTab* CurrentDocTab(MainWindow* win);
WindowTab* ActiveTab(MainWindow* win);
WindowTab* SessionTab(MainWindow* win);
bool AtScrollLimit(DisplayModel* dm, int dir);
void ClearTabScroll(WindowTab* tab);

TempStr SpeedLabelTemp(WindowTab* tab);
void ApplyArrowSpeed(MainWindow* win, WindowTab* tab, int keyDir);
void ApplyDigitSpeed(MainWindow* win, int digit);

// implemented by each app
void BarUpdate(MainWindow* win, bool forceLayout = false);
