/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "base/File.h"

#include "base/DirScan.h"

struct TempEntryVec {
    DirEntry* els;
    int len;
    int cap;
};

static FILETIME FileTimeFromTimespec(time_t sec, long nsec) {
    u64 ft = ((u64)sec * 1000000000ULL) + (u64)nsec;
    FILETIME res;
    res.dwLowDateTime = (DWORD)ft;
    res.dwHighDateTime = (DWORD)(ft >> 32);
    return res;
}

#if OS_DARWIN
static FILETIME StatModTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec);
}

static FILETIME StatAccessTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_atimespec.tv_sec, st.st_atimespec.tv_nsec);
}

static FILETIME StatCreateTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_birthtimespec.tv_sec, st.st_birthtimespec.tv_nsec);
}
#else
static FILETIME StatModTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
}

static FILETIME StatAccessTime(const struct stat& st) {
    return FileTimeFromTimespec(st.st_atim.tv_sec, st.st_atim.tv_nsec);
}

static FILETIME StatCreateTime(const struct stat& st) {
    return StatModTime(st);
}
#endif

static DIR* DirHandle(DirIter::iterator* it) {
    return (DIR*)it->dirHandle;
}

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
    it->data.modificationTime = StatModTime(st);
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

void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit) {
    if (shouldExit && AtomicBoolGet(shouldExit)) {
        return;
    }

    TempEntryVec temp = {};

    DirEntry dotdot = {};
    dotdot.name = StrL("..");
    dotdot.size = 0;
    dotdot.dv = kStillScanningDir;
    VecPush(GetTempArena(), temp, dotdot);

    char* dirPath = CStrTemp(dv->fullDir);
    DIR* dir = opendir(dirPath);
    if (!dir) {
        dv->err = str::Dup(arena, Str(strerror(errno)));
        return;
    }

    for (;;) {
        if (shouldExit && AtomicBoolGet(shouldExit)) {
            closedir(dir);
            return;
        }

        errno = 0;
        dirent* de = readdir(dir);
        if (!de) {
            if (errno != 0) {
                dv->err = str::Dup(arena, Str(strerror(errno)));
            }
            break;
        }

        Str name(de->d_name);
        if (str::Eq(name, StrL(".")) || str::Eq(name, StrL(".."))) {
            continue;
        }

        TempStr fullPath = path::JoinTemp(dv->fullDir, name);
        struct stat st;
        if (lstat(CStrTemp(fullPath), &st) != 0) {
            continue;
        }

        DirEntry e = {};
        e.name = name;
        e.createTime = StatCreateTime(st);
        e.modTime = StatModTime(st);
        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            e.size = 0;
            e.dv = kStillScanningDir;
        } else {
            e.size = (u64)st.st_size;
            e.dv = nullptr;
        }
        VecPush(GetTempArena(), temp, e);
    }

    closedir(dir);

    DirEntry* els = (DirEntry*)Alloc(arena, temp.len * sizeof(DirEntry));
    for (int i = 0; i < temp.len; i++) {
        els[i].name = str::Dup(arena, temp.els[i].name);
        els[i].size = temp.els[i].size;
        els[i].dv = temp.els[i].dv;
        els[i].createTime = temp.els[i].createTime;
        els[i].modTime = temp.els[i].modTime;
    }
    dv->els = els;
    __sync_synchronize();
    dv->len = temp.len;
}
