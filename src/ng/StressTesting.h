/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Flags;
struct MainWindow;

void BenchFileOrDir(StrVec& pathsToBench);
bool IsStressTesting();
void StartStressTest(Flags* i, MainWindow* win);
// ng: orig drives one step per WM_TIMER it re-arms itself; the port's shell
// already ticks every window, so the tick calls this instead
void OnStressTestTimer(MainWindow* win);
void FinishStressTest(MainWindow* win);
void GetStressTestInfo(str::Builder& b);
