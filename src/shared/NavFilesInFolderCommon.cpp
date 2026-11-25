/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Timer.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "NavFilesInFolder.h"
#include "NavFilesInFolderCommon.h"

// entry display name without a trailing path separator (dirs use "name\\")
Str NavEntryBaseName(const NavFileEntry& e) {
    Str name = e.name;
    if (!e.isDir || len(name) < 2) {
        return name;
    }
    // a directory's name ends with "\\", which is not a separator off Windows
    char last = name.s[len(name) - 1];
    if (last == '\\' || path::IsSep(last)) {
        return Str(name.s, len(name) - 1);
    }
    return name;
}

void FreeNavEntry(NavFileEntry& e) {
    str::Free(e.name);
    str::Free(e.path);
}

// skip GuessFileTypeFromName()'s IsDirectory() probe: name is relative to the
// listed dir, and callers already skipped directories.
bool CanOpenFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true);
}

// dirs first, then files, each sorted naturally by name.
// qsort: the old insertion sort was O(n^2) and froze on huge folders.
static int CmpNavEntry(const NavFileEntry* a, const NavFileEntry* b) {
    if (a->isDir != b->isDir) {
        return a->isDir ? -1 : 1;
    }
    return str::CmpNatural(a->name, b->name);
}

static void SortNavEntries(Vec<NavFileEntry>& entries, int firstIdx) {
    int n = len(entries) - firstIdx;
    if (n <= 1) {
        return;
    }
    auto cmp = (int (*)(const void*, const void*))CmpNavEntry;
    qsort(entries.els + firstIdx, (size_t)n, sizeof(NavFileEntry), cmp);
}

void FreeNavEntries(Vec<NavFileEntry>& entries) {
    for (NavFileEntry& e : entries) {
        FreeNavEntry(e);
    }
    VecReset(entries);
}

void StealNavEntries(Vec<NavFileEntry>& dst, Vec<NavFileEntry>& src) {
    FreeNavEntries(dst);
    dst.els = src.els;
    dst.len = src.len;
    dst.cap = src.cap;
    src.els = nullptr;
    src.len = 0;
    src.cap = 0;
}

bool SameNavEntries(const Vec<NavFileEntry>& a, const Vec<NavFileEntry>& b) {
    if (len(a) != len(b)) {
        return false;
    }
    for (int i = 0; i < len(a); i++) {
        const NavFileEntry& ea = a[i];
        const NavFileEntry& eb = b[i];
        if (ea.isDir != eb.isDir || ea.size != eb.size || !str::Eq(ea.name, eb.name)) {
            return false;
        }
    }
    return true;
}

bool NavDirHasParent(Str dir) {
    if (len(dir) == 0) {
        return false; // home view
    }
    TempStr parent = path::GetDirTemp(dir);
    return !path::IsSame(parent, dir);
}

void AppendNavParentEntry(Vec<NavFileEntry>& entries) {
    NavFileEntry e;
    e.name = str::Dup(StrL(".."));
    e.isDir = true;
    VecAppend(entries, e);
}

// leaf name for display: find-data names are usually basenames, but some network
// providers put a relative or full path in cFileName — always show the leaf only.
static TempStr NavLeafNameTemp(DirIterEntry* de) {
    TempStr leaf = path::GetBaseNameTemp(de->name);
    if (len(leaf) == 0) {
        leaf = path::GetBaseNameTemp(de->filePath);
    }
    return leaf;
}

// Explorer's Quick access, fetched once per process because the shell resolves
// every entry. F5 in the home view drops the cache.
static Mutex gQuickAccessMutex;

static bool gQuickAccessCached = false;

void ResetQuickAccessCache() {
    gQuickAccessMutex.Lock();
    gQuickAccessCached = false;
    gQuickAccessMutex.Unlock();
}

// home entries show the full path: Quick access has many same-named folders
static void AppendHomeDirEntry(Vec<NavFileEntry>& out, Str path) {
    if (len(path) == 0) {
        return;
    }
    NavFileEntry e;
    e.isDir = true;
    e.path = str::Dup(path);
    bool hasSep = path::IsSep(path.s[len(path) - 1]);
    e.name = hasSep ? str::Dup(path) : str::Join(path, StrL("\\"));
    VecAppend(out, e);
}

static StrVec gQuickAccessDirs;

static StrVec gQuickAccessFiles;

static void GetQuickAccessCached(StrVec& dirsOut, StrVec& filesOut) {
    gQuickAccessMutex.Lock();
    if (!gQuickAccessCached) {
        gQuickAccessDirs.Reset();
        gQuickAccessFiles.Reset();
        auto t = TimeGet();
        ListShellQuickAccess(gQuickAccessDirs, gQuickAccessFiles);
        logf("NavDirScan: quick access %d dirs, %d files in %.1fms\n", len(gQuickAccessDirs), len(gQuickAccessFiles),
             TimeSinceInMs(t));
        gQuickAccessCached = true;
    }
    dirsOut = gQuickAccessDirs;
    filesOut = gQuickAccessFiles;
    gQuickAccessMutex.Unlock();
}

static void AppendHomeFileEntry(Vec<NavFileEntry>& out, Str path) {
    if (!CanOpenFile(path)) {
        return;
    }
    // Quick access keeps listing files after they are deleted
#if OS_WIN
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!path::GetCachedAttributesEx(path, &fad)) {
        return;
    }
    i64 size = ((i64)fad.nFileSizeHigh << 32) | (i64)fad.nFileSizeLow;
#else
    if (!file::Exists(path)) {
        return;
    }
    i64 size = file::GetSize(path);
#endif
    NavFileEntry e;
    e.path = str::Dup(path);
    e.name = str::Dup(path);
    e.size = size;
    VecAppend(out, e);
}

// Home view: drive roots, then Explorer's Quick access folders and files
static void CollectHomeEntries(Vec<NavFileEntry>& out) {
    StrVec drives;
    ListDriveRoots(drives);
    for (int i = 0; i < len(drives); i++) {
        AppendHomeDirEntry(out, drives[i]);
    }

    StrVec dirs;
    StrVec files;
    GetQuickAccessCached(dirs, files);
    for (int i = 0; i < len(dirs); i++) {
        AppendHomeDirEntry(out, dirs[i]);
    }
    for (int i = 0; i < len(files); i++) {
        AppendHomeFileEntry(out, files[i]);
    }
}

// Built on a worker thread: listing + filtering + sorting a folder of tens of
// thousands of files must not freeze the UI (discussion #6014).
void CollectNavEntriesForDir(Str dir, Vec<NavFileEntry>& out) {
    if (len(dir) == 0) {
        CollectHomeEntries(out);
        return;
    }
    // ".." also in a drive root, where it leads to the home view
    AppendNavParentEntry(out);
    int firstIdx = 1; // keep ".." at the top when sorting

    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = true;
    for (DirIterEntry* de : di) {
        TempStr leaf = NavLeafNameTemp(de);
        if (len(leaf) == 0) {
            continue;
        }
#if OS_WIN
        DWORD attrs = de->fd->dwFileAttributes;
        if (attrs & FILE_ATTRIBUTE_HIDDEN) {
            continue;
        }
#else
        if (leaf.s[0] == '.') {
            continue;
        }
#endif
        // own path/name before any further temp allocations
        Str fullPath = str::Dup(de->filePath);

        NavFileEntry e;
        e.path = fullPath;
        if (IsDirectory(de)) {
            e.isDir = true;
            e.name = str::Join(leaf, StrL("\\"));
        } else {
            if (!CanOpenFile(leaf)) {
                str::Free(fullPath);
                continue;
            }
            e.name = str::Dup(leaf);
            e.size = GetFileSize(de);
            // FindFirstFile size is sometimes 0 on network/cloud providers even
            // when the file has content; attributes are cached for network paths.
#if OS_WIN
            if (e.size == 0) {
                WIN32_FILE_ATTRIBUTE_DATA fad{};
                if (path::GetCachedAttributesEx(fullPath, &fad) && !(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    e.size = ((i64)fad.nFileSizeHigh << 32) | (i64)fad.nFileSizeLow;
                }
            }
#endif
        }
        VecAppend(out, e);
    }

    SortNavEntries(out, firstIdx);
}

// dirs visited ("" for home), kept for the whole session so a re-opened
// window can still go Back to where the previous one was
StrVec gNavHistory;
int gNavHistIdx = -1;

// record dir in the session history, dropping the forward entries
void NavHistoryPush(Str dir) {
    bool same = gNavHistIdx >= 0 && str::EqI(gNavHistory[gNavHistIdx], dir);
    if (same) {
        return;
    }
    while (len(gNavHistory) > gNavHistIdx + 1) {
        gNavHistory.RemoveAt(len(gNavHistory) - 1);
    }
    if (len(gNavHistory) >= kNavHistoryMax) {
        gNavHistory.RemoveAt(0);
    }
    gNavHistory.Append(dir);
    gNavHistIdx = len(gNavHistory) - 1;
}
