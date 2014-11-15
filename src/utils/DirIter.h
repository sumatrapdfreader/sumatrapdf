/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* How to use:

DirIter di(dir, recursive);
for (const WCHAR *filePath = di.First(); filePath; filePath = di.Next()) {
    // process filePath
}

*/
class DirIter
{
    bool                recursive;

    WStrVec             dirsToVisit;
    ScopedMem<WCHAR>    startDir;
    ScopedMem<WCHAR>    currDir;
    ScopedMem<WCHAR>    currPath;
    HANDLE              currFindHandle;
    WIN32_FIND_DATA     currFindData;
    bool                foundNext;

    bool StartDirIter(const WCHAR *dir);
    bool TryNextDir();

public:
    DirIter(const WCHAR *dir, bool recursive=false) : foundNext(false), currFindHandle(NULL), recursive(recursive) {
        startDir.Set(str::Dup(dir));
    }
    ~DirIter() { FindClose(currFindHandle); }

    const WCHAR *First();
    const WCHAR *Next();
};

bool CollectPathsFromDirectory(const WCHAR *pattern, WStrVec& paths, bool dirsInsteadOfFiles=false);

