/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/DirIter.h"

static i64 GetWinFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

// try to filter out things that are not files
// or not meant to be used by other applications
static bool IsRegularFileAttr(DWORD fileAttr) {
    if (fileAttr & FILE_ATTRIBUTE_DEVICE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        return false;
    }
    return true;
}

static bool IsDirectoryAttr(DWORD fileAttr) {
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool IsSpecialDir(Str s) {
    return str::Eq(s, StrL(".")) || str::Eq(s, StrL(".."));
}

static void SetDirIterData(DirIter::iterator* it, TempStr name, TempStr path, bool isFile, bool isDir) {
    it->data.fd = &it->fd;
    it->data.name = name;
    it->data.filePath = path;
    it->data.size = GetWinFileSize(&it->fd);
    it->data.accessTime = it->fd.ftLastAccessTime;
    it->data.modificationTime = it->fd.ftLastWriteTime;
    it->data.isFile = isFile;
    it->data.isDir = isDir;
}

void CloseDirIter(DirIter::iterator* it) {
    wstr::FreePtr(&it->pattern);
    SafeFindClose(&it->h);
}

void AdvanceDirIter(DirIter::iterator* it, int n) {
    ReportIf(n != 1);
    if (it->didFinish) {
        return;
    }
    if (it->data.stopTraversal) {
        // could have been set by user accessing prev traversal
        it->didFinish = true;
        return;
    }

    bool includeFiles = it->di->includeFiles;
    bool includeDirs = it->di->includeDirs;
    bool recur = it->di->recurse;

    bool ok;
    bool isFile;
    bool isDir;
    TempStr name;
    TempStr path;

NextDir:
    if (!it->pattern) {
        int nDirs = len(it->dirsToVisit);
        if (nDirs == 0) {
            goto DidFinish;
        }
        it->currDir = it->dirsToVisit.RemoveAt(nDirs - 1);
        TempWStr ws = ToWStrTemp(it->currDir);
        it->pattern = path::Join(ws, WStrL(L"*"));
        it->h = FindFirstFileW(it->pattern.s, &it->fd);
        if (!IsValidHandle(it->h)) {
            goto DidFinish;
        }
    } else {
        ok = FindNextFileW(it->h, &it->fd);
        if (!ok) {
            CloseDirIter(it);
            goto NextDir;
        }
    }
    while (true) {
        isFile = IsRegularFileAttr(it->fd.dwFileAttributes);
        isDir = IsDirectoryAttr(it->fd.dwFileAttributes);
        name = ToUtf8Temp(it->fd.cFileName);
        path = path::JoinTemp(it->currDir, name);
        SetDirIterData(it, name, path, isFile, isDir);
        if (isFile && includeFiles) {
            return;
        }
        if (isDir && !IsSpecialDir(name)) {
            if (recur) {
                it->dirsToVisit.Append(path);
            }
            if (includeDirs) {
                return;
            }
        }
        ok = FindNextFileW(it->h, &it->fd);
        if (!ok) {
            CloseDirIter(it);
            goto NextDir;
        }
    };
DidFinish:
    CloseDirIter(it);
    it->didFinish = true;
}
