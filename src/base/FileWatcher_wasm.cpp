/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: browser file systems do not expose change notifications. Poll MEMFS
// metadata on the browser event loop so open documents still reload when
// JavaScript or a file copied in from OPFS changes underneath them.

#include "base/Base.h"
#include "base/File.h"

#include <emscripten/eventloop.h>

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
    if (!file::Exists(path) || path::IsSame(gSkipPath, path)) {
        return nullptr;
    }
    auto* file = new WatchedFile();
    file->path = str::Dup(path);
    file->onFileChanged = onFileChangedCb;
    file->modified = file::GetModificationTime(path);
    file->size = file::GetSize(path);
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
