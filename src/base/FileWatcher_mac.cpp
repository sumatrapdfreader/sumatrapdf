/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: macOS file watching using portable file metadata polling. This keeps the
// same callback and shutdown behavior as the inotify implementation without
// requiring a CoreFoundation run loop for FSEvents.

#include "base/Base.h"
#include "base/File.h"

#include <sys/stat.h>

#include "base/FileWatcher.h"

static bool StatFileMeta(Str path, FILETIME* modOut, i64* sizeOut) {
    if (len(path) == 0) {
        return false;
    }
    struct stat st{};
    if (stat(CStrTemp(path), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    i64 secs = (i64)st.st_mtimespec.tv_sec;
    i64 nsecs = (i64)st.st_mtimespec.tv_nsec;
    u64 ticks = kFileTimeUnixEpoch + (u64)secs * kFileTimeTicksPerSec + (u64)(nsecs / 100);
    *modOut = FileTimeFromU64(ticks);
    *sizeOut = (i64)st.st_size;
    return true;
}

struct WatchedFile {
    WatchedFile* next = nullptr;
    Str path;
    Func0 onFileChanged;
    FILETIME modified{};
    i64 size = 0;
    bool exists = false;
    bool ignore = false;
    // tells a new file apart from a freed one at the same address
    u64 id = 0;
};

static Mutex gWatcherMutex;
static WatchedFile* gWatchedFiles = nullptr;
static Str gSkipPath;
static ThreadHandle gWatcherThread = nullptr;
static AtomicBool gShouldExit = 0;
static u64 gNextWatchedFileId = 0;

static void FreeWatchedFile(WatchedFile* file) {
    str::Free(file->path);
    delete file;
}

static bool WatchedFileStillActive(WatchedFile* file, u64 id) {
    for (WatchedFile* f = gWatchedFiles; f; f = f->next) {
        if (f == file && f->id == id) {
            return true;
        }
    }
    return false;
}

// stat runs without gWatcherMutex: on a stalled network share it can block
// for minutes, and Subscribe / Unsubscribe take the mutex on the UI thread
static void FileWatcherThread() {
    struct Check {
        WatchedFile* file = nullptr;
        u64 id = 0;
        Str path;
        FILETIME modified{};
        i64 size = 0;
        bool exists = false;
    };
    while (!AtomicBoolGet(&gShouldExit)) {
        SleepInMs(100);
        Vec<Check> checks;
        {
            ScopedMutex lock(&gWatcherMutex);
            for (WatchedFile* file = gWatchedFiles; file; file = file->next) {
                Check c;
                c.file = file;
                c.id = file->id;
                c.path = str::Dup(file->path);
                VecAppend(checks, c);
            }
        }
        for (Check& c : checks) {
            c.exists = StatFileMeta(c.path, &c.modified, &c.size);
            if (!c.exists) {
                c.modified = {};
                c.size = 0;
            }
            str::Free(c.path);
        }

        Vec<Func0> toNotify;
        {
            ScopedMutex lock(&gWatcherMutex);
            for (Check& c : checks) {
                if (!WatchedFileStillActive(c.file, c.id)) {
                    continue;
                }
                WatchedFile* file = c.file;
                bool changed =
                    c.exists && (!file->exists || !FileTimeEq(c.modified, file->modified) || c.size != file->size);
                file->exists = c.exists;
                file->modified = c.modified;
                file->size = c.size;
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
    str::ReplacePtr(&gSkipPath, str::Dup(path));
}

void FileWatcherInit(void) {}

WatchedFile* FileWatcherSubscribe(Str path, const Func0& onFileChangedCb, bool) {
    FILETIME modified{};
    i64 size = 0;
    if (!StatFileMeta(path, &modified, &size)) {
        return nullptr;
    }
    ScopedMutex lock(&gWatcherMutex);
    if (path::IsSame(gSkipPath, path) || !StartFileWatcher()) {
        return nullptr;
    }
    auto* file = new WatchedFile();
    file->path = str::Dup(path);
    file->onFileChanged = onFileChangedCb;
    file->modified = modified;
    file->size = size;
    file->exists = true;
    file->id = ++gNextWatchedFileId;
    ListInsertFront(&gWatchedFiles, file);
    logf("FileWatcherSubscribe: watching '%s'\n", path);
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
    SetIgnoreLocked(file, &gWatcherMutex, ignore);
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
