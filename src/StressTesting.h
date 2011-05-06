/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StressTesting_h
#define StressTesting_h

#include "Vec.h"

bool IsBenchPagesInfo(const TCHAR *s);
void Bench(StrVec& filesToBench);

#define DIR_STRESS_TIMER_ID 101

class WindowInfo;
class RenderCache;
class DisplayModel;
class StressTest;

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles=false);
void StartDirStressTest(WindowInfo *win, const TCHAR *dir, RenderCache *renderCache);
void StartFileStressTest(WindowInfo *win, const TCHAR *dir, RenderCache *renderCache, int repCount);
void RandomIsOverGlyph(DisplayModel *dm, int pageNo);
char *GetStressTestInfo(StressTest *);

#endif
