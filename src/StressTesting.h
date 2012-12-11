/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StressTesting_h
#define StressTesting_h

bool IsValidPageRange(const WCHAR *ranges);
bool IsBenchPagesInfo(const WCHAR *s);
void BenchFileOrDir(WStrVec& pathsToBench);
bool IsStressTesting();

#define DIR_STRESS_TIMER_ID 101

class WindowInfo;
class RenderCache;

void StartStressTest(WindowInfo *win, const WCHAR *path, const WCHAR *filter,
                     const WCHAR *ranges, int cycles, RenderCache *renderCache);
void OnStressTestTimer(WindowInfo *win);
void FinishStressTest(WindowInfo *win);

#endif
