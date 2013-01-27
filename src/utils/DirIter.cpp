/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "DirIter.h"

#include "FileUtil.h"

// Start directory traversal in a given dir
bool DirIter::StartDirIter(const WCHAR *dir)
{
    currDir.Set(str::Dup(dir));
    ScopedMem<WCHAR> pattern(path::Join(currDir, L"*"));
    currFindHandle = FindFirstFile(pattern, &currFindData);
    if (INVALID_HANDLE_VALUE == currFindHandle)
        return false;
    return true;
}

bool DirIter::TryNextDir()
{
    while (dirsToVisit.Count() > 0) {
        ScopedMem<WCHAR> nextDir(dirsToVisit.Pop());
        // it's ok if we fail, this might be an auth problem,
        // we keep going
        bool ok = StartDirIter(nextDir);
        if (ok)
            return true;
    }
    return false;
}

// Start iteration in a given dir. Returns false if error.
bool DirIter::Start(const WCHAR *dir, bool recursive)
{
    this->recursive = recursive;
    foundNext = StartDirIter(dir);
    return foundNext;
}

// try to filter out things that are not files
// or not meant to be used by other applications
static bool IsRegularFile(DWORD fileAttr)
{
    if (fileAttr & FILE_ATTRIBUTE_DEVICE)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT)
        return false;
    return true;
}

// "." and ".." are special
static bool IsSpecialDir(const WCHAR *s)
{
    if ('.' == *s++) {
        if (*s == 0)
            return true;
        if (('.' == *s++) && (0 == *s))
            return true;
    }
    return false;
}

// Returns a path of the next file (relative to the path passed to Start()).
// Returns NULL if finished iteration.
// Returned value is valid only until we call Next() again.
const WCHAR *DirIter::Next()
{
    // when we enter here, currFindData has info for an entry
    // we haven't processed yet (filled by StartDirIter() or
    // ourselves at the end) unless foundNext is false
    currPath.Set(NULL);
    while (foundNext && !currPath) {
        WCHAR *f = currFindData.cFileName;
        if ((currFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (recursive && !IsSpecialDir(f)) {
                WCHAR *d = path::Join(currDir, f);
                dirsToVisit.Append(d);
            }
        } else if (IsRegularFile(currFindData.dwFileAttributes)) {
            WCHAR *p = path::Join(currDir, f);
            currPath.Set(p);
        }
        BOOL hasMore = FindNextFile(currFindHandle, &currFindData);
        if (!hasMore)
            foundNext = TryNextDir();
    }
    return currPath;
}

bool CollectPathsFromDirectory(const WCHAR *pattern, WStrVec& paths, bool dirsInsteadOfFiles)
{
    ScopedMem<WCHAR> dirPath(path::GetDir(pattern));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        bool append = !dirsInsteadOfFiles;
        if ((fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            append = dirsInsteadOfFiles && !IsSpecialDir(fdata.cFileName);
        if (append)
            paths.Append(path::Join(dirPath, fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    return paths.Count() > 0;
}
