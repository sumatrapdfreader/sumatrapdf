/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "base/DirScan.h"

static i64 GetWinFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

// try to filter out things that are not files
// or not meant to be used by other applications
//
// Takes the whole find data because the reparse tag is in dwReserved0, and
// only there when the entry is a reparse point.
static bool IsRegularFile(const WIN32_FIND_DATAW& fd) {
    DWORD fileAttr = fd.dwFileAttributes;
    if (fileAttr & FILE_ATTRIBUTE_DEVICE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        // A symlink stands for a file somewhere else rather than being one. A
        // cloud provider's placeholder is the file: OneDrive puts a reparse
        // point on everything it syncs, and reading one fetches the contents.
        return !IsReparseTagNameSurrogate(fd.dwReserved0);
    }
    // Offline with no reparse point is the older kind of archived storage,
    // where reading can block for a very long time. A cloud placeholder is
    // marked offline as well, but it was already let through above.
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE) {
        return false;
    }
    return true;
}

static bool IsDirectoryAttr(DWORD fileAttr) {
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Whether the entry stands for something elsewhere rather than being it.
// Symlinks and junctions are such name surrogates, and descending into one can
// loop or count the same files twice: a profile has
// AppData\Local\"Application Data" pointing back at AppData\Local. A cloud
// provider's reparse point is not a surrogate: OneDrive marks every folder it
// syncs with one, and those are ordinary directories that only look like
// something else. Treating them as surrogates hides all of OneDrive.
//
// dwReserved0 holds the reparse tag, but only when the entry is a reparse
// point, which is why this takes the whole find data. FindExInfoBasic still
// fills it in; it only leaves out cAlternateFileName.
static bool IsNameSurrogate(const WIN32_FIND_DATAW& fd) {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return false;
    }
    return IsReparseTagNameSurrogate(fd.dwReserved0);
}

// Hidden and system together is what Windows calls a protected operating system
// file. Explorer keeps these out of sight even when it's been told to show
// hidden files, behind a second setting of its own, and it's a good rule: what
// it covers is pagefile.sys, System Volume Information, $Recycle.Bin, a
// profile's registry hives, and the legacy junctions ("Moje dokumenty",
// PrintHood, "Ustawienia lokalne") that exist only so pre-Vista programs keep
// working. None of it is a person's own files.
//
// Hidden on its own still shows: AppData, ProgramData and NTUSER.DAT are things
// someone browsing might actually be after.
static bool IsProtectedOSFile(DWORD fileAttr) {
    DWORD both = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (fileAttr & both) == both;
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
        isFile = IsRegularFile(it->fd);
        isDir = IsDirectoryAttr(it->fd.dwFileAttributes);
        name = ToUtf8Temp(it->fd.cFileName);
        path = path::JoinTemp(it->currDir, name);
        SetDirIterData(it, name, path, isFile, isDir);
        if (isFile && includeFiles) {
            return;
        }
        if (isDir && !IsSpecialDir(name)) {
            if (recur && !IsNameSurrogate(it->fd)) {
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

struct TempEntryVec {
    DirEntry* els;
    int len;
    int cap;
};

static const WStr wdot = WStrL(L".");
static const WStr wdotdot = WStrL(L"..");

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

    WStr widePath = ToWStrTemp(dv->fullDir);

    wchar_t searchPath[MAX_PATH + 2];
    int wideLen = 0;
    while (wideLen < widePath.len && wideLen < MAX_PATH - 2) {
        searchPath[wideLen] = widePath.s[wideLen];
        wideLen++;
    }
    if (wideLen > 0 && searchPath[wideLen - 1] != L'\\') {
        searchPath[wideLen++] = L'\\';
    }
    searchPath[wideLen++] = L'*';
    searchPath[wideLen] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE hFind =
        FindFirstFileExW(searchPath, FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE) {
        dv->err = GetLastErrorAsStr(arena);
        return;
    }
    do {
        if (shouldExit && AtomicBoolGet(shouldExit)) {
            FindClose(hFind);
            return;
        }

        if (wstr::Eq(WStr(fd.cFileName), wdot) || wstr::Eq(WStr(fd.cFileName), wdotdot)) {
            continue;
        }

        if (IsProtectedOSFile(fd.dwFileAttributes)) {
            continue;
        }

        Str utf8Name = ToUtf8Temp(WStr(fd.cFileName));

        DirEntry e = {};
        e.name = utf8Name;
        e.createTime = fd.ftCreationTime;
        e.modTime = fd.ftLastWriteTime;
        e.isLink = IsNameSurrogate(fd);
        // A junction is still a directory to show and to let the user step
        // into. It's only the walking that stops here, which the caller does by
        // looking at isLink.
        if (IsDirectoryAttr(fd.dwFileAttributes)) {
            e.size = 0;
            e.dv = kStillScanningDir;
        } else {
            e.size = ((u64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            e.dv = nullptr;
        }
        VecPush(GetTempArena(), temp, e);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    DirEntry* els = (DirEntry*)Alloc(arena, temp.len * sizeof(DirEntry));
    for (int i = 0; i < temp.len; i++) {
        els[i].name = str::Dup(arena, temp.els[i].name);
        els[i].size = temp.els[i].size;
        els[i].dv = temp.els[i].dv;
        els[i].createTime = temp.els[i].createTime;
        els[i].modTime = temp.els[i].modTime;
        els[i].isLink = temp.els[i].isLink;
    }
    dv->els = els;
    MemoryBarrier();
    dv->len = temp.len;
}
