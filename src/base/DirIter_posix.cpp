/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <dirent.h>
#include <sys/stat.h>

#include "base/File.h"
#include "base/DirIter.h"

static DIR* DirHandle(DirIter::iterator* it) {
    return (DIR*)it->dirHandle;
}

static FILETIME FileTimeFromTimespec(time_t sec, long nsec) {
    u64 ft = (u64)sec * 1000000000ULL + (u64)nsec;
    FILETIME res;
    res.dwLowDateTime = (DWORD)ft;
    res.dwHighDateTime = (DWORD)(ft >> 32);
    return res;
}

#if OS_DARWIN
static FILETIME StatAccessTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_atimespec.tv_sec, st.st_atimespec.tv_nsec);
}

static FILETIME StatModificationTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec);
}
#else
static FILETIME StatAccessTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_atim.tv_sec, st.st_atim.tv_nsec);
}

static FILETIME StatModificationTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
}
#endif

static bool IsSpecialDir(Str s) {
    return str::Eq(s, StrL(".")) || str::Eq(s, StrL(".."));
}

static void SetDirIterData(DirIter::iterator* it, TempStr name, TempStr path, const struct stat& st) {
    bool isDir = S_ISDIR(st.st_mode);
    bool isFile = S_ISREG(st.st_mode);
    it->data.name = name;
    it->data.filePath = path;
    it->data.size = isFile ? (i64)st.st_size : 0;
    it->data.accessTime = StatAccessTime(st);
    it->data.modificationTime = StatModificationTime(st);
    it->data.isFile = isFile;
    it->data.isDir = isDir;
}

void CloseDirIter(DirIter::iterator* it) {
    DIR* dir = DirHandle(it);
    if (dir) {
        closedir(dir);
        it->dirHandle = nullptr;
    }
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

NextDir:
    if (!DirHandle(it)) {
        int nDirs = len(it->dirsToVisit);
        if (nDirs == 0) {
            goto DidFinish;
        }
        it->currDir = it->dirsToVisit.RemoveAt(nDirs - 1);
        it->dirHandle = opendir(CStrTemp(it->currDir));
        if (!DirHandle(it)) {
            goto NextDir;
        }
    }

    while (true) {
        dirent* de = readdir(DirHandle(it));
        if (!de) {
            CloseDirIter(it);
            goto NextDir;
        }

        TempStr name = str::DupTemp(de->d_name);
        if (IsSpecialDir(name)) {
            continue;
        }

        TempStr path = path::JoinTemp(it->currDir, name);
        struct stat st;
        if (lstat(CStrTemp(path), &st) != 0) {
            continue;
        }

        SetDirIterData(it, name, path, st);
        if (it->data.isFile && includeFiles) {
            return;
        }
        if (it->data.isDir) {
            if (recur) {
                it->dirsToVisit.Append(path);
            }
            if (includeDirs) {
                return;
            }
        }
    }

DidFinish:
    CloseDirIter(it);
    it->didFinish = true;
}
