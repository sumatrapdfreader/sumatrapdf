/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#include "base/FileWatcher.h"

#if OS_POSIX && !OS_WASM
#include <unistd.h>
#endif

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

#if OS_WASM
void FileWatcherCheckNowForTests();
#endif

static void NoteFileWatcherChange(AtomicInt* count) {
    AtomicIntInc(count);
}

#if OS_POSIX && !OS_WASM
static bool WaitForChange(AtomicInt* count, int n) {
    for (int i = 0; i < 300 && AtomicIntGet(count) < n; i++) {
        SleepInMs(10);
    }
    return AtomicIntGet(count) >= n;
}

// two paths to one dir (through a symlink) share an inotify watch: both files
// must report changes, and dropping one must not stop the other
static void FileWatcherSymlinkDirTest() {
    TempStr root = GetTempFilePathTemp(StrL("sumatra-watcher-dir-"));
    file::Delete(root);
    TempStr realDir = path::JoinTemp(root, StrL("real"));
    TempStr linkDir = path::JoinTemp(root, StrL("link"));
    utassert(dir::CreateAll(realDir));
    utassert(symlink(CStrTemp(realDir), CStrTemp(linkDir)) == 0);
    TempStr viaLink = path::JoinTemp(linkDir, StrL("a.txt"));
    TempStr viaReal = path::JoinTemp(realDir, StrL("b.txt"));
    utassert(file::WriteFile(viaLink, StrL("a")));
    utassert(file::WriteFile(viaReal, StrL("b")));

    AtomicInt countLink = 0;
    AtomicInt countReal = 0;
    WatchedFile* wfLink = FileWatcherSubscribe(viaLink, MkFunc0(NoteFileWatcherChange, &countLink));
    WatchedFile* wfReal = FileWatcherSubscribe(viaReal, MkFunc0(NoteFileWatcherChange, &countReal));
    utassert(wfLink && wfReal);

    utassert(file::WriteFile(viaLink, StrL("a2")));
    utassert(WaitForChange(&countLink, 1));
    FileWatcherUnsubscribe(wfLink);
    int n = AtomicIntGet(&countReal);
    utassert(file::WriteFile(viaReal, StrL("b2")));
    utassert(WaitForChange(&countReal, n + 1));

    FileWatcherUnsubscribe(wfReal);
    FileWatcherWaitForShutdown();
    file::Delete(linkDir);
    dir::RemoveAll(root);
}
#endif

void FileWatcher_UnitTests() {
    TempStr tempPath = GetTempFilePathTemp(StrL("sumatra-watcher-"));
    utassert(len(tempPath) > 0);
    AtomicInt changeCount = 0;
    WatchedFile* watched = FileWatcherSubscribe(tempPath, MkFunc0(NoteFileWatcherChange, &changeCount));
    utassert(watched != nullptr);
    utassert(file::WriteFile(tempPath, StrL("changed")));
#if OS_WASM
    FileWatcherCheckNowForTests();
#else
    for (int i = 0; i < 300 && AtomicIntGet(&changeCount) == 0; i++) {
        SleepInMs(10);
    }
#endif
    utassert(AtomicIntGet(&changeCount) > 0);
    FileWatcherUnsubscribe(watched);
    FileWatcherWaitForShutdown();
    utassert(file::Delete(tempPath));
#if OS_POSIX && !OS_WASM
    FileWatcherSymlinkDirTest();
#endif
}
