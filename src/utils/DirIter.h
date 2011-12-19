/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DirIter_h
#define DirIter_h

#include "BaseUtil.h"
#include "Vec.h"

class DirIter
{
    bool            recursive;

    Vec<TCHAR*>     dirsToVisit;
    TCHAR *         baseDir;
    TCHAR *         currDir;
    TCHAR *         currPath;
    HANDLE          currFindHandle;
    WIN32_FIND_DATA currFindData;
    bool            stillIterating;

    bool StartDirIter(TCHAR *dir);
    void TryNextDir();
public:
    DirIter(bool recur);
    ~DirIter();

    bool Start(TCHAR *dir);
    TCHAR *Next();
};

#endif
