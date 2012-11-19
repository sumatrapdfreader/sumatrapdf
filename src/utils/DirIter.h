/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DirIter_h
#define DirIter_h

class DirIter
{
    bool            recursive;

    WStrVec         dirsToVisit;
    ScopedMem<WCHAR>currDir;
    ScopedMem<WCHAR>currPath;
    HANDLE          currFindHandle;
    WIN32_FIND_DATA currFindData;
    bool            foundNext;

    bool StartDirIter(const WCHAR *dir);
    bool TryNextDir();

public:
    DirIter() : foundNext(false), currFindHandle(NULL) { }
    ~DirIter() { FindClose(currFindHandle); }

    bool Start(const WCHAR *dir, bool recursive=false);
    const WCHAR *Next();
};

#endif
