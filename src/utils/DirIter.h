/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DirIter_h
#define DirIter_h

#include "BaseUtil.h"

class DirIter
{
    bool            recursive;

    StrVec          dirsToVisit;
    ScopedMem<TCHAR>currDir;
    ScopedMem<TCHAR>currPath;
    HANDLE          currFindHandle;
    WIN32_FIND_DATA currFindData;
    bool            foundNext;

    bool StartDirIter(const TCHAR *dir);
    bool TryNextDir();

public:
    DirIter() : foundNext(false), currFindHandle(NULL) { }
    ~DirIter() { FindClose(currFindHandle); }

    bool Start(const TCHAR *dir, bool recursive=false);
    const TCHAR *Next();
};

#endif
