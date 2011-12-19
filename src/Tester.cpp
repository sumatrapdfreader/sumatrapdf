/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include <stdio.h>
#include <stdlib.h>

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
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

DirIter::DirIter(bool recur) :
    recursive(recur), baseDir(NULL), currDir(NULL), currPath(NULL),
    currFindHandle(0), stillIterating(false)
{
}

DirIter::~DirIter()
{
    free(baseDir);
    free(currDir);
    free(currPath);
    FreeVecMembers<TCHAR*>(dirsToVisit);
}

// Start directory traversal in a given dir (if NULL, it's baseDir)
bool DirIter::StartDirIter(TCHAR *dir)
{
    str::ReplacePtr(&currDir, dir);
    ScopedMem<TCHAR> pattern(path::Join(currDir, _T("*")));
    currFindHandle = FindFirstFile(pattern, &currFindData);
    if (INVALID_HANDLE_VALUE == currFindHandle)
        return false;
    return true;
}

void DirIter::TryNextDir()
{
    while (dirsToVisit.Count() > 0) {
        TCHAR *nextDir = dirsToVisit.Pop();
        // it's ok if we fail, this might be an auth problem,
        // we keep going
        bool ok = StartDirIter(nextDir);
        if (ok)
            return;
    }
    stillIterating = false;
}

// Start iteration in a given dir. Returns false if error.
bool DirIter::Start(TCHAR *dir)
{
    str::ReplacePtr(&baseDir, dir);
    stillIterating = StartDirIter(baseDir);
    return stillIterating;
}

// try to filter out things that are not files
static bool IsRegularFile(DWORD fileAttr)
{
    if (fileAttr & FILE_ATTRIBUTE_ARCHIVE)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_DEVICE)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_ENCRYPTED)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_HIDDEN)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_SYSTEM)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY)
        return false;
    if (fileAttr & FILE_ATTRIBUTE_VIRTUAL)
        return false;
    return true;
}

// "." and ".." are special
static bool IsSpecialDir(TCHAR *s)
{
    if ('.' == *s++) {
        if (*s == 0)
            return true;
        if (('.' == *s++)  && (0 == *s))
            return true;
    }
    return false;
}

// Returns a path (relative to basePath) of the next file.
// Returns NULL if finished iteration.
// Returned value is valid only until we call Next() again.
TCHAR *DirIter::Next()
{
    // when we enter here, currFindData has info for an entry
    // we haven't processed yet (filled by StartDirIter() or
    // ourselves at the tend)
    bool again = true;
    while (again) {
        if (!stillIterating) // can be changed in TryNextDir()
            return NULL;
        TCHAR *f = currFindData.cFileName;
        if (currFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive && !IsSpecialDir(f)) {
                TCHAR *d = path::Join(currDir, f);
                dirsToVisit.Append(d);
            }
            again = true;
        } else if (IsRegularFile(currFindData.dwFileAttributes)) {
            TCHAR *p = path::Join(currDir, f);
            free(currPath);
            currPath = p;
            again = false;
        } else {
            again = true;
        }
        BOOL hasMore = FindNextFile(currFindHandle, &currFindData);
        if (!hasMore)
            TryNextDir();
    }
    return currPath;
}

static int usage()
{
    printf("st.exe\n");
    printf("  -mobi dir : run mobi tests in a given directory\n");
    return 1;
}

static void testmobifile(TCHAR *path)
{
    _tprintf(_T("Testing file '%s'\n"), path);
}

static void mobitest(char *dir)
{
    printf("Testing mobi files in '%s'\n", dir);
    DirIter di(true);
    ScopedMem<TCHAR> tmp(str::conv::FromAnsi(dir));
    if (!di.Start(tmp)) {
        printf("Error: invalid directory '%s'\n", dir);
        return;
    }

    for (;;) {
        TCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (str::EndsWithI(p, _T(".mobi")))
            testmobifile(p);
    }
}

extern "C"
int main(int argc, char **argv)
{
    int i = 1;
    int left = argc - 1;

    if (left < 2)
        return usage();

    if (str::Eq(argv[i], "-mobi")) {
        ++i; --left;
        if (left != 1)
            return usage();
        mobitest(argv[i]);
        return 0;
    }

    return usage();
}
