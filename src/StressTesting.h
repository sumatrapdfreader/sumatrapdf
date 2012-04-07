/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StressTesting_h
#define StressTesting_h

bool IsValidPageRange(const TCHAR *ranges);
bool IsBenchPagesInfo(const TCHAR *s);
void BenchFileOrDir(StrVec& pathsToBench);

#define DIR_STRESS_TIMER_ID 101

class WindowInfo;
class RenderCache;

class StressTestBase {
public:
    virtual ~StressTestBase() { }
    virtual void OnTimer() = 0;
    virtual void GetLogInfo(str::Str<char> *s) = 0;
};

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles=false);
void StartStressTest(WindowInfo *win, const TCHAR *path, const TCHAR *filter,
                     const TCHAR *ranges, int cycles, RenderCache *renderCache);

#endif
