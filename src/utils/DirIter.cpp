/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/WinUtil.h"
#include "utils/StrQueue.h"
#include "utils/DirIter.h"

// try to filter out things that are not files
// or not meant to be used by other applications
bool IsRegularFile(DWORD fileAttr) {
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

bool IsDirectory(DWORD fileAttr) {
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool IsSpecialDir(const char* s) {
    return str::Eq(s, ".") || str::Eq(s, "..");
}

// if cb returns false, we stop further traversal
bool VisitDir(const char* dir, u32 flg, const VisitDirCb& cb) {
    ReportIf(flg == 0);
    bool includeFiles = flg & kVisitDirIncudeFiles;
    bool includeDirs = flg & kVisitDirIncludeDirs;
    bool recur = flg & kVisitDirRecurse;

    auto dirW = ToWStrTemp(dir);
    WCHAR* pattern = path::JoinTemp(dirW, L"*");

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (!IsValidHandle(h)) {
        return false;
    }

    bool isFile;
    bool isDir;
    bool cont = true;
    char* name;
    char* path;
    VisitDirData d;
    d.fd = &fd;
    do {
        isFile = IsRegularFile(fd.dwFileAttributes);
        isDir = IsDirectory(fd.dwFileAttributes);
        name = ToUtf8Temp(fd.cFileName);
        path = path::JoinTemp(dir, name);
        if (isFile && includeFiles) {
            d.filePath = path;
            cb.Call(&d);
            cont = !d.stopTraversal;
        }
        if (isDir && !IsSpecialDir(name)) {
            if (includeDirs) {
                d.filePath = path;
                cb.Call(&d);
                cont = !d.stopTraversal;
            }
            if (cont && recur) {
                cont = VisitDir(path, flg, cb);
            }
        }
    } while (cont && FindNextFileW(h, &fd));
    FindClose(h);
    return true;
}

// if cb returns false, we stop further traversal
bool DirTraverse(const char* dir, bool recurse, const VisitDirCb& cb) {
    u32 flg = kVisitDirIncudeFiles;
    if (recurse) {
        flg |= kVisitDirRecurse;
    }
    bool ok = VisitDir(dir, flg, cb);
    return ok;
}

bool CollectPathsFromDirectory(const char* pattern, StrVec& paths) {
    TempStr dir = path::GetDirTemp(pattern);

    WIN32_FIND_DATAW fdata{};
    WCHAR* patternW = ToWStr(pattern);
    HANDLE hfind = FindFirstFileW(patternW, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    do {
        char* name = ToUtf8Temp(fdata.cFileName);
        DWORD attrs = fdata.dwFileAttributes;
        if (IsRegularFile(attrs)) {
            TempStr path = path::JoinTemp(dir, name);
            paths.Append(path);
        }
    } while (FindNextFileW(hfind, &fdata));
    FindClose(hfind);
    return paths.Size() > 0;
}

struct CollectFilesData {
    StrVec* files = nullptr;
    const VisitDirCb* fileMatches;
};

static void CollectFilesCb(CollectFilesData* d, VisitDirData* vd) {
    d->fileMatches->Call(vd);
    if (!vd->stopTraversal) {
        d->files->Append(vd->filePath);
    }
}

bool CollectFilesFromDirectory(const char* dir, StrVec& files, const VisitDirCb& fileMatches) {
    u32 flg = kVisitDirIncudeFiles;
    auto data = new CollectFilesData;
    data->files = &files;
    data->fileMatches = &fileMatches;
    auto fn = MkFunc1(CollectFilesCb, data);
    bool ok = VisitDir(dir, flg, fn);
    delete data;
    return ok;
}

i64 GetFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

struct DirTraverseThreadData {
    StrQueue* queue = nullptr;
    const char* dir = nullptr;
    bool recurse = false;
};

static void DirTraverseThreadCb(DirTraverseThreadData* td, VisitDirData* d) {
    td->queue->Append(d->filePath);
}

static void WINAPI DirTraverseThread(DirTraverseThreadData* td) {
    auto fn = MkFunc1(DirTraverseThreadCb, td);
    DirTraverse(td->dir, td->recurse, fn);
    td->queue->MarkFinished();
    delete td;
}

void StartDirTraverseAsync(StrQueue* queue, const char* dir, bool recurse) {
    auto td = new DirTraverseThreadData{queue, dir, recurse};
    auto fn = MkFunc0(DirTraverseThread, td);
    RunAsync(fn, "DirTraverseThread");
}
