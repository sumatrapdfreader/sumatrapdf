/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StressTesting_h
#define StressTesting_h

bool IsValidPageRange(const WCHAR *ranges);
bool IsBenchPagesInfo(const WCHAR *s);
void BenchFileOrDir(WStrVec& pathsToBench);
bool IsStressTesting();

class WindowInfo;
class RenderCache;
class CommandLineInfo;

void StartStressTest(CommandLineInfo *, WindowInfo *, RenderCache *);

void OnStressTestTimer(WindowInfo *win, int timerId);
void FinishStressTest(WindowInfo *win);

#endif
