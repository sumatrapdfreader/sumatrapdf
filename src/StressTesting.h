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
class DirStressTest;

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles=false);
void StartDirStressTest(WindowInfo *win, const TCHAR *dir, RenderCache *renderCache);
void RandomIsOverGlyph(DisplayModel *dm, int pageNo);
void AppendStressTestInfo(DirStressTest *, str::Str<char>&);
#endif
