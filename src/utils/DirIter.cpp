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

static void AdvanceDirIter(DirIter::iterator* it, int n) {
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
    char* name;
    char* path;

NextDir:
    if (!it->pattern) {
        int nDirs = it->dirsToVisit.Size();
        if (nDirs == 0) {
            goto DidFinish;
        }
        it->currDir = it->dirsToVisit.RemoveAt(nDirs - 1);
        TempWStr ws = ToWStrTemp(it->currDir);
        it->pattern = path::Join(ws, L"*");
        it->h = FindFirstFileW(it->pattern, &it->fd);
        if (!IsValidHandle(it->h)) {
            goto DidFinish;
        }
    } else {
        ok = FindNextFileW(it->h, &it->fd);
        if (!ok) {
            SafeCloseHandle(&it->h);
            str::FreePtr(&it->pattern);
            goto NextDir;
        }
    }
    while (true) {
        isFile = IsRegularFile(it->fd.dwFileAttributes);
        isDir = IsDirectory(it->fd.dwFileAttributes);
        name = ToUtf8Temp(it->fd.cFileName);
        path = path::JoinTemp(it->currDir, name);
        it->data.name = name;
        it->data.filePath = path;
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
            SafeCloseHandle(&it->h);
            str::FreePtr(&it->pattern);
            goto NextDir;
        }
    };
DidFinish:
    str::FreePtr(&it->pattern);
    SafeCloseHandle(&it->h);
    it->didFinish = true;
    return;
}

DirIter::iterator::iterator(const DirIter* di, bool didFinish) {
    this->di = di;
    this->dirsToVisit.Append(di->dir);
    this->didFinish = didFinish;
    this->data.fd = &this->fd;
    AdvanceDirIter(this, 1);
}

DirIter::iterator::~iterator() {
    str::Free(pattern);
}

DirIter::iterator DirIter::begin() const {
    return DirIter::iterator(this, false);
}

DirIter::iterator DirIter::end() const {
    return DirIter::iterator(this, true);
}

DirIterEntry* DirIter::iterator::operator*() {
    if (didFinish) {
        return nullptr;
    }
    return &data;
}

// postfix increment
DirIter::iterator DirIter::iterator::operator++(int) {
    auto res = *this;
    AdvanceDirIter(this, 1);
    return res;
}

DirIter::iterator& DirIter::iterator::operator++() {
    AdvanceDirIter(this, 1);
    return *this;
}

DirIter::iterator& DirIter::iterator::operator+(int n) {
    AdvanceDirIter(this, n);
    return *this;
}

bool operator==(const DirIter::iterator& a, const DirIter::iterator& b) {
    return (a.di == b.di) && (a.didFinish == b.didFinish);
};

bool operator!=(const DirIter::iterator& a, const DirIter::iterator& b) {
    return (a.di != b.di) || (a.didFinish != b.didFinish);
};

i64 GetFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

struct DirTraverseThreadData {
    StrQueue* queue = nullptr; // we don't own it
    const char* dir = nullptr;
    bool recurse = false;
    ~DirTraverseThreadData() {
        str::FreePtr(&dir);
    }
};

static void DirTraverseThread(DirTraverseThreadData* td) {
    DirIter di(td->dir);
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = td->recurse;
    for (DirIterEntry* de : di) {
        td->queue->Append(de->filePath);
    }
    td->queue->MarkFinished();
    delete td;
}

void StartDirTraverseAsync(StrQueue* queue, const char* dir, bool recurse) {
    auto td = new DirTraverseThreadData{queue, str::Dup(dir), recurse};
    auto fn = MkFunc0(DirTraverseThread, td);
    RunAsync(fn, "DirTraverseThread");
}
