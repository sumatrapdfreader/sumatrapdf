/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: macOS file watching using portable file metadata polling. This keeps the
// same callback and shutdown behavior as the inotify implementation without
// requiring a CoreFoundation run loop for FSEvents.

#include "base/Base.h"
#include "base/File.h"

#include "base/FileWatcher.h"

struct WatchedFile {
    WatchedFile* next = nullptr;
    Str path;
    Func0 onFileChanged;
    FILETIME modified{};
    i64 size = 0;
    bool exists = false;
    bool ignore = false;
};

static Mutex gWatcherMutex;
static WatchedFile* gWatchedFiles = nullptr;
static Str gSkipPath;
static ThreadHandle gWatcherThread = nullptr;
static AtomicBool gShouldExit = 0;

static void FreeWatchedFile(WatchedFile* file) {
    str::Free(file->path);
    delete file;
}

static void FileWatcherThread() {
    while (!AtomicBoolGet(&gShouldExit)) {
        SleepInMs(100);
        Vec<Func0> toNotify;
        {
            ScopedMutex lock(&gWatcherMutex);
            for (WatchedFile* file = gWatchedFiles; file; file = file->next) {
                bool exists = file::Exists(file->path);
                FILETIME modified = exists ? file::GetModificationTime(file->path) : FILETIME{};
                i64 size = exists ? file::GetSize(file->path) : 0;
                bool changed = exists && (!file->exists || !FileTimeEq(modified, file->modified) || size != file->size);
                file->exists = exists;
                file->modified = modified;
                file->size = size;
                if (changed && !file->ignore) {
                    VecAppend(toNotify, file->onFileChanged);
                }
            }
        }
        for (const Func0& cb : toNotify) {
            cb.Call();
        }
        ResetTempArena();
    }
}

// Callers hold gWatcherMutex.
static bool StartFileWatcher() {
    if (gWatcherThread) {
        return true;
    }
    AtomicBoolSet(&gShouldExit, false);
    gWatcherThread = StartThread(MkFunc0Void(FileWatcherThread), StrL("FileWatcherThread"));
    return gWatcherThread != nullptr;
}

void FileWatcherSetSkipPath(Str path) {
    ScopedMutex lock(&gWatcherMutex);
    str::Free(gSkipPath);
    gSkipPath = str::Dup(path);
}

void FileWatcherInit(void) {
    ScopedMutex lock(&gWatcherMutex);
    StartFileWatcher();
}

WatchedFile* FileWatcherSubscribe(Str path, const Func0& onFileChangedCb, bool) {
    if (!file::Exists(path)) {
        return nullptr;
    }
    ScopedMutex lock(&gWatcherMutex);
    if (path::IsSame(gSkipPath, path) || !StartFileWatcher()) {
        return nullptr;
    }
    auto* file = new WatchedFile();
    file->path = str::Dup(path);
    file->onFileChanged = onFileChangedCb;
    file->modified = file::GetModificationTime(path);
    file->size = file::GetSize(path);
    file->exists = true;
    ListInsertFront(&gWatchedFiles, file);
    return file;
}

void FileWatcherUnsubscribe(WatchedFile* file) {
    if (!file) {
        return;
    }
    ScopedMutex lock(&gWatcherMutex);
    bool removed = ListRemove(&gWatchedFiles, file);
    ReportIf(!removed);
    FreeWatchedFile(file);
}

void WatchedFileSetIgnore(WatchedFile* file, bool ignore) {
    if (!file) {
        return;
    }
    ScopedMutex lock(&gWatcherMutex);
    file->ignore = ignore;
}

void FileWatcherWaitForShutdown(void) {
    ThreadHandle thread = nullptr;
    {
        ScopedMutex lock(&gWatcherMutex);
        if (!gWatcherThread) {
            return;
        }
        ReportIf(gWatchedFiles != nullptr);
        AtomicBoolSet(&gShouldExit, true);
        thread = gWatcherThread;
        gWatcherThread = nullptr;
    }
    JoinThread(&thread, -1);

    ScopedMutex lock(&gWatcherMutex);
    while (gWatchedFiles) {
        WatchedFile* file = gWatchedFiles;
        gWatchedFiles = file->next;
        FreeWatchedFile(file);
    }
}
