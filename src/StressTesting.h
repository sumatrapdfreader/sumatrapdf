/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void BenchFileOrDir(StrVec& pathsToBench);
bool IsStressTesting();
void StartStressTest(Flags* i, MainWindow* win);
void OnStressTestTimer(MainWindow* win, int timerId);
void FinishStressTest(MainWindow* win);
