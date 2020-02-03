/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsValidPageRange(const WCHAR* ranges);
bool IsBenchPagesInfo(const WCHAR* s);
void BenchFileOrDir(WStrVec& pathsToBench);
bool IsStressTesting();
void BenchEbookLayout(WCHAR* filePath);

class Flags;
class WindowInfo;

void StartStressTest(Flags* i, WindowInfo* win);

void OnStressTestTimer(WindowInfo* win, int timerId);
void FinishStressTest(WindowInfo* win);
