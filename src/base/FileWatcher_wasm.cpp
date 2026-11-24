/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: browser file systems do not expose change notifications. Poll MEMFS
// metadata on the browser event loop so open documents still reload when
// JavaScript or a file copied in from OPFS changes underneath them.

#include "base/Base.h"
#include "base/File.h"

#include <emscripten/eventloop.h>
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
    i64 secs = (i64)st.st_mtim.tv_sec;
    i64 nsecs = (i64)st.st_mtim.tv_nsec;
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
};

static WatchedFile* gWatchedFiles = nullptr;
static Str gSkipPath;
static int gTimerId = 0;

static void FreeWatchedFile(WatchedFile* file) {
    str::Free(file->path);
    delete file;
}

static void CheckWatchedFiles(void*) {
    Vec<Func0> toNotify;
    for (WatchedFile* file = gWatchedFiles; file; file = file->next) {
        FILETIME modified{};
        i64 size = 0;
        bool exists = StatFileMeta(file->path, &modified, &size);
        bool changed = exists && (!file->exists || !FileTimeEq(modified, file->modified) || size != file->size);
        file->exists = exists;
        file->modified = modified;
        file->size = size;
        if (changed && !file->ignore) {
            VecAppend(toNotify, file->onFileChanged);
        }
    }
    for (const Func0& cb : toNotify) {
        cb.Call();
    }
    ResetTempArena();
}

void FileWatcherCheckNowForTests() {
    CheckWatchedFiles(nullptr);
}

static void StartFileWatcher() {
    if (!gTimerId) {
        gTimerId = emscripten_set_interval(CheckWatchedFiles, 250, nullptr);
    }
}

void FileWatcherSetSkipPath(Str path) {
    str::Free(gSkipPath);
    gSkipPath = str::Dup(path);
}

void FileWatcherInit(void) {}

WatchedFile* FileWatcherSubscribe(Str path, const Func0& onFileChangedCb, bool) {
    FILETIME modified{};
    i64 size = 0;
    if (!StatFileMeta(path, &modified, &size) || path::IsSame(gSkipPath, path)) {
        return nullptr;
    }
    auto* file = new WatchedFile();
    file->path = str::Dup(path);
    file->onFileChanged = onFileChangedCb;
    file->modified = modified;
    file->size = size;
    file->exists = true;
    ListInsertFront(&gWatchedFiles, file);
    StartFileWatcher();
    return file;
}

void FileWatcherUnsubscribe(WatchedFile* file) {
    if (!file) {
        return;
    }
    bool removed = ListRemove(&gWatchedFiles, file);
    ReportIf(!removed);
    FreeWatchedFile(file);
    if (!gWatchedFiles && gTimerId) {
        emscripten_clear_interval(gTimerId);
        gTimerId = 0;
    }
}

void WatchedFileSetIgnore(WatchedFile* file, bool ignore) {
    if (file) {
        file->ignore = ignore;
    }
}

void FileWatcherWaitForShutdown(void) {
    if (gTimerId) {
        emscripten_clear_interval(gTimerId);
        gTimerId = 0;
    }
    while (gWatchedFiles) {
        WatchedFile* file = gWatchedFiles;
        gWatchedFiles = file->next;
        FreeWatchedFile(file);
    }
}
