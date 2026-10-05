/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the model half of orig's NavFilesInFolder.cpp: listing a folder's (or
// the home view's) openable entries, sorted. The window half - orig's
// NavFilesInFolderWnd, built of win32 virtual controls - is gpui and lives in
// gui/NavFilesUI.cpp.

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Timer.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "NavFilesInFolder.h"

#include "SumatraLog.h"

// A modeless directory browser listing sub-directories and files SumatraPDF
// can open (judged by extension). Enter / double-click replaces the document
// in the current tab or descends into a directory; Ctrl + Enter / Ctrl +
// double-click switches to the tab already showing the file or opens it in a
// new tab; ".." goes one directory up. Back / Forward walk the folders visited
// in this session, Up goes to the parent, Home lists the drives and Explorer's
// Quick access. The window stays open so you can open several files in
// succession; Esc or the close button dismisses it.

// ng: NavFileEntry moved to NavFilesInFolder.h

static void FreeNavEntry(NavFileEntry& e) {
    str::Free(e.name);
    str::Free(e.path);
}

// skip GuessFileTypeFromName()'s IsDirectory() probe: name is relative to the
// listed dir, and callers already skipped directories.
static bool CanOpenFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind);
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

// entry display name without a trailing path separator (dirs use "name\\")
Str NavEntryBaseName(const NavFileEntry& e) {
    Str name = e.name;
    if (!e.isDir || name.len < 2) {
        return name;
    }
    // ng: the "\\" a directory's name ends with is not a separator off Windows
    char last = name.s[name.len - 1];
    if (last == '\\' || path::IsSep(last)) {
        return Str(name.s, name.len - 1);
    }
    return name;
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

#if !OS_WIN
// ng: orig's are in base/Win.cpp. Off Windows the "drives" are the root and
// the user's home directory, and there is no Quick access.
static void ListDriveRoots(StrVec& out) {
    out.Append(StrL("/"));
    const char* home = getenv("HOME");
    Str homeDir = Str((char*)(home ? home : ""));
    if (len(homeDir) > 1 && dir::Exists(homeDir)) {
        out.Append(homeDir);
    }
}

static bool ListShellQuickAccess(StrVec&, StrVec&) {
    return false;
}
#endif

// Explorer's Quick access, fetched once per process because the shell resolves
// every entry. F5 in the home view drops the cache.
static Mutex gQuickAccessMutex;
static bool gQuickAccessCached = false;
static StrVec gQuickAccessDirs;
static StrVec gQuickAccessFiles;

void ResetQuickAccessCache() {
    gQuickAccessMutex.Lock();
    gQuickAccessCached = false;
    gQuickAccessMutex.Unlock();
}

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

// home entries show the full path: Quick access has many same-named folders
static void AppendHomeDirEntry(Vec<NavFileEntry>& out, Str path) {
    NavFileEntry e;
    e.isDir = true;
    e.path = str::Dup(path);
    bool hasSep = path::IsSep(path.s[len(path) - 1]);
    e.name = hasSep ? str::Dup(path) : str::Join(path, StrL("\\"));
    VecAppend(out, e);
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

bool NavFiles_UnitTestHidden() {
#if OS_WIN
    return true;
#else
    TempStr root = GetTempFilePathTemp(StrL("nav-hidden-ut"));
    file::Delete(root);
    if (!dir::Create(root)) {
        return false;
    }
    TempStr hidden = path::JoinTemp(root, StrL(".hidden.pdf"));
    TempStr visible = path::JoinTemp(root, StrL("visible.pdf"));
    bool ok = file::WriteFile(hidden, StrL("hidden")) && file::WriteFile(visible, StrL("visible"));

    Vec<NavFileEntry> entries;
    if (ok) {
        CollectNavEntriesForDir(root, entries);
        bool foundHidden = false;
        bool foundVisible = false;
        for (const NavFileEntry& e : entries) {
            foundHidden = foundHidden || str::Eq(e.name, StrL(".hidden.pdf"));
            foundVisible = foundVisible || str::Eq(e.name, StrL("visible.pdf"));
        }
        ok = !foundHidden && foundVisible;
    }
    FreeNavEntries(entries);
    ok = dir::RemoveAll(root) && ok;
    return ok;
#endif
}
