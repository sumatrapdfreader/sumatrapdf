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

void SortNavEntries(Vec<NavFileEntry>& entries, int firstIdx) {
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
TempStr NavLeafNameTemp(DirIterEntry* de) {
    TempStr leaf = path::GetBaseNameTemp(de->name);
    if (len(leaf) == 0) {
        leaf = path::GetBaseNameTemp(de->filePath);
    }
    return leaf;
}

// Explorer's Quick access, fetched once per process because the shell resolves
// every entry. F5 in the home view drops the cache.
Mutex gQuickAccessMutex;

bool gQuickAccessCached = false;

void ResetQuickAccessCache() {
    gQuickAccessMutex.Lock();
    gQuickAccessCached = false;
    gQuickAccessMutex.Unlock();
}

// home entries show the full path: Quick access has many same-named folders
void AppendHomeDirEntry(Vec<NavFileEntry>& out, Str path) {
    NavFileEntry e;
    e.isDir = true;
    e.path = str::Dup(path);
    bool hasSep = path::IsSep(path.s[len(path) - 1]);
    e.name = hasSep ? str::Dup(path) : str::Join(path, StrL("\\"));
    VecAppend(out, e);
}
