/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void BenchFileOrDir(StrVec& pathsToBench);
bool IsStressTesting();

struct WindowInfo;
void StartStressTest(Flags* i, WindowInfo* win);

void OnStressTestTimer(WindowInfo* win, int timerId);
void FinishStressTest(WindowInfo* win);
