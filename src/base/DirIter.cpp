/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/StrQueue.h"
#include "base/DirIter.h"

void AdvanceDirIter(DirIter::iterator* it, int n);
void CloseDirIter(DirIter::iterator* it);

DirIter::iterator::iterator(const DirIter* di, bool didFinish) {
    this->di = di;
    this->dirsToVisit.Append(di->dir);
    this->didFinish = didFinish;
#if OS_WIN
    this->data.fd = &this->fd;
#endif
    AdvanceDirIter(this, 1);
}

DirIter::iterator::iterator(const iterator& that) {
    *this = that;
}

DirIter::iterator& DirIter::iterator::operator=(const iterator& that) {
    if (this == &that) {
        return *this;
    }
    CloseDirIter(this);
    this->di = that.di;
    this->didFinish = that.didFinish;
    this->dirsToVisit = that.dirsToVisit;
    this->currDir = that.currDir;
    this->data = that.data;
#if OS_WIN
    this->fd = that.fd;
    this->data.fd = &this->fd;
#endif
    return *this;
}

DirIter::iterator::~iterator() {
    CloseDirIter(this);
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

i64 GetFileSize(DirIterEntry* de) {
    return de ? de->size : 0;
}

bool IsDirectory(DirIterEntry* de) {
    return de && de->isDir;
}

bool IsRegularFile(DirIterEntry* de) {
    return de && de->isFile;
}

struct DirTraverseThreadData {
    StrQueue* queue = nullptr; // we don't own it
    Str dir;
    bool recurse = false;
    ~DirTraverseThreadData() { str::Free(dir); }
};

static void DirTraverseThread(DirTraverseThreadData* td) {
    DirIter di(td->dir);
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = td->recurse;
    for (DirIterEntry* de : di) {
        td->queue->append(de->filePath);
    }
    td->queue->MarkFinished();
    delete td;
}

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse) {
    auto td = new DirTraverseThreadData{queue, str::Dup(dir), recurse};
    auto fn = MkFunc0(DirTraverseThread, td);
    RunAsync(fn, "DirTraverseThread");
}
