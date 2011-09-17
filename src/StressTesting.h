/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StressTesting_h
#define StressTesting_h

#include "Vec.h"

bool IsValidPageRange(const TCHAR *ranges);
bool IsBenchPagesInfo(const TCHAR *s);
void Bench(StrVec& filesToBench);

#define DIR_STRESS_TIMER_ID 101

class WindowInfo;
class RenderCache;
class DisplayModel;

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles=false);
void StartStressTest(WindowInfo *win, const TCHAR *path, const TCHAR *filter,
                     const TCHAR *ranges, int cycles, RenderCache *renderCache);
char *GetStressTestInfo(CallbackFunc *dst);

#endif
