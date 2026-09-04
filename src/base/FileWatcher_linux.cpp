/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#if IS_DEBUG
#include "base/UtAssert.h"
#endif

#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "base/FileWatcher.h"

struct WatchedDir {
    WatchedDir* next = nullptr;
    Str path;
    int descriptor = -1;
};

struct WatchedFile {
    WatchedFile* next = nullptr;
    WatchedDir* dir = nullptr;
    Str path;
    Str name;
    Func0 onFileChanged;
    bool ignore = false;
};

static Mutex gWatcherMutex;
static WatchedDir* gWatchedDirs = nullptr;
static WatchedFile* gWatchedFiles = nullptr;
static Str gSkipPath;
static int gInotifyFd = -1;
static int gWakeFd = -1;
static pthread_t gWatcherThread{};
static bool gThreadRunning = false;
static AtomicBool gShouldExit = 0;

static void FreeWatchedDir(WatchedDir* dir) {
    str::Free(dir->path);
    delete dir;
}

static void FreeWatchedFile(WatchedFile* file) {
    str::Free(file->path);
    str::Free(file->name);
    delete file;
}

static WatchedDir* FindWatchedDirByPath(Str path) {
    for (WatchedDir* dir = gWatchedDirs; dir; dir = dir->next) {
        if (str::Eq(dir->path, path)) {
            return dir;
        }
    }
    return nullptr;
}

static WatchedDir* FindWatchedDirByDescriptor(int descriptor) {
    for (WatchedDir* dir = gWatchedDirs; dir; dir = dir->next) {
        if (dir->descriptor == descriptor) {
            return dir;
        }
    }
    return nullptr;
}

static bool IsWatchedDirReferenced(WatchedDir* dir) {
    for (WatchedFile* file = gWatchedFiles; file; file = file->next) {
        if (file->dir == dir) {
            return true;
        }
    }
    return false;
}

static void NotifyChangedFile(int descriptor, Str name) {
    ScopedMutex lock(&gWatcherMutex);
    WatchedDir* dir = FindWatchedDirByDescriptor(descriptor);
    if (!dir) {
        return;
    }
    for (WatchedFile* file = gWatchedFiles; file; file = file->next) {
        if (file->dir == dir && !file->ignore && str::Eq(file->name, name)) {
            file->onFileChanged.Call();
        }
    }
}

static void DrainInotifyEvents() {
    alignas(inotify_event) char buf[16 * 1024];
    for (;;) {
        ssize_t count = read(gInotifyFd, buf, sizeof(buf));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (count == 0) {
            return;
        }
        char* current = buf;
        char* end = buf + count;
        while (current < end) {
            auto* event = (inotify_event*)current;
            constexpr u32 changeMask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_ATTRIB;
            if ((event->mask & changeMask) && event->len > 0) {
                NotifyChangedFile(event->wd, Str(event->name));
            }
            current += sizeof(inotify_event) + event->len;
        }
    }
}

static void* FileWatcherThread(void*) {
    SetThreadName(StrL("FileWatcherThread"));
    pollfd fds[] = {{gInotifyFd, POLLIN, 0}, {gWakeFd, POLLIN, 0}};
    while (!AtomicBoolGet(&gShouldExit)) {
        int result = poll(fds, dimofi(fds), -1);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (fds[1].revents & POLLIN) {
            eventfd_t value = 0;
            eventfd_read(gWakeFd, &value);
        }
        if (fds[0].revents & POLLIN) {
            DrainInotifyEvents();
        }
        ResetTempArena();
    }
    DestroyTempArena();
    return nullptr;
}

// Callers hold gWatcherMutex.
static bool StartFileWatcher() {
    if (gThreadRunning) {
        return true;
    }
    gInotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    gWakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (gInotifyFd < 0 || gWakeFd < 0) {
        if (gInotifyFd >= 0) {
            close(gInotifyFd);
        }
        if (gWakeFd >= 0) {
            close(gWakeFd);
        }
        gInotifyFd = -1;
        gWakeFd = -1;
        return false;
    }
    AtomicBoolSet(&gShouldExit, false);
    int error = pthread_create(&gWatcherThread, nullptr, FileWatcherThread, nullptr);
    if (error != 0) {
        close(gInotifyFd);
        close(gWakeFd);
        gInotifyFd = -1;
        gWakeFd = -1;
        return false;
    }
    gThreadRunning = true;
    return true;
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
    TempStr dirPath = path::GetDirTemp(path);
    if (len(dirPath) == 0) {
        dirPath = StrL(".");
    }
    TempStr name = path::GetBaseNameTemp(path);
    WatchedDir* dir = FindWatchedDirByPath(dirPath);
    if (!dir) {
        constexpr u32 mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_ATTRIB;
        int descriptor = inotify_add_watch(gInotifyFd, CStrTemp(dirPath), mask);
        if (descriptor < 0) {
            return nullptr;
        }
        dir = new WatchedDir();
        dir->path = str::Dup(dirPath);
        dir->descriptor = descriptor;
        ListInsertFront(&gWatchedDirs, dir);
    }

    auto* file = new WatchedFile();
    file->dir = dir;
    file->path = str::Dup(path);
    file->name = str::Dup(name);
    file->onFileChanged = onFileChangedCb;
    ListInsertFront(&gWatchedFiles, file);
    return file;
}

void FileWatcherUnsubscribe(WatchedFile* file) {
    if (!file) {
        return;
    }
    ScopedMutex lock(&gWatcherMutex);
    WatchedDir* dir = file->dir;
    bool removed = ListRemove(&gWatchedFiles, file);
    ReportIf(!removed);
    FreeWatchedFile(file);
    if (dir && !IsWatchedDirReferenced(dir)) {
        ListRemove(&gWatchedDirs, dir);
        inotify_rm_watch(gInotifyFd, dir->descriptor);
        FreeWatchedDir(dir);
    }
}

void WatchedFileSetIgnore(WatchedFile* file, bool ignore) {
    if (!file) {
        return;
    }
    ScopedMutex lock(&gWatcherMutex);
    file->ignore = ignore;
}

void FileWatcherWaitForShutdown(void) {
    pthread_t thread{};
    {
        ScopedMutex lock(&gWatcherMutex);
        if (!gThreadRunning) {
            return;
        }
        ReportIf(gWatchedFiles != nullptr);
        AtomicBoolSet(&gShouldExit, true);
        eventfd_write(gWakeFd, 1);
        thread = gWatcherThread;
    }
    pthread_join(thread, nullptr);

    ScopedMutex lock(&gWatcherMutex);
    while (gWatchedFiles) {
        WatchedFile* file = gWatchedFiles;
        gWatchedFiles = file->next;
        FreeWatchedFile(file);
    }
    while (gWatchedDirs) {
        WatchedDir* dir = gWatchedDirs;
        gWatchedDirs = dir->next;
        FreeWatchedDir(dir);
    }
    close(gInotifyFd);
    close(gWakeFd);
    gInotifyFd = -1;
    gWakeFd = -1;
    gThreadRunning = false;
}

#if IS_DEBUG

static void NoteFileWatcherChange(AtomicInt* count) {
    AtomicIntInc(count);
}

void FileWatcher_UnitTests() {
    TempStr tempPath = GetTempFilePathTemp(StrL("sumatra-watcher-"));
    utassert(!!tempPath);
    AtomicInt changeCount = 0;
    WatchedFile* watched = FileWatcherSubscribe(tempPath, MkFunc0(NoteFileWatcherChange, &changeCount));
    utassert(watched != nullptr);
    utassert(file::WriteFile(tempPath, StrL("changed")));
    for (int i = 0; i < 300 && AtomicIntGet(&changeCount) == 0; i++) {
        SleepInMs(10);
    }
    utassert(AtomicIntGet(&changeCount) > 0);
    FileWatcherUnsubscribe(watched);
    FileWatcherWaitForShutdown();
    utassert(file::Delete(tempPath));
}

#endif
