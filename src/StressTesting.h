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
class StressTest;

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles=false);
void StartStressTest(WindowInfo *win, const TCHAR *path, const TCHAR *ranges,
    int cycles, RenderCache *renderCache, bool disableDjvu, bool disablePdf,
    bool disableCbx);
char *GetStressTestInfo(StressTest *);

#endif
