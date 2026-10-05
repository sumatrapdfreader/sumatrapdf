/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#include "base/FileWatcher.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

#if OS_WASM
void FileWatcherCheckNowForTests();
#endif

static void NoteFileWatcherChange(AtomicInt* count) {
    AtomicIntInc(count);
}

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
}
