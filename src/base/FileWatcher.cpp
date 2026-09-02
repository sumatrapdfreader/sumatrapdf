/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/FileWatcher.h"

// Log file path we must not reload-on-change (set by SumatraLog).
static Str gFileWatcherSkipPath;

void FileWatcherSetSkipPath(Str path) {
    gFileWatcherSkipPath = path;
}

static Str FileWatcherGetSkipPath() {
    return gFileWatcherSkipPath;
}

/*
This code is tricky, so here's a high-level overview. More info at:
http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html

Also, we did have a bug caused by incorrect use of CancelIo(). Here's a good
description of its intricacies: http://blogs.msdn.com/b/oldnewthing/archive/2011/02/02/10123392.aspx

We use ReadDirectoryChangesW() with overlapped i/o and i/o completion
callback function.

Callback function is called in the context of the thread that called
ReadDirectoryChangesW() but only if it's in alertable state.
Our ui thread isn't so we create our own thread and run code that
calls ReadDirectoryChangesW() on that thread via QueueUserAPC().

gWatchedDirs and gWsatchedFiles are shared between the main thread and
worker thread so must be protected via gFileWatcherMutex.

ReadDirectChangesW() doesn't always work for files on network drives,
so for those files, we do manual checks, by using a timeout to
periodically wake up thread.
*/

/*
TODO:
  - should I end the thread when there are no files to watch?

  - a single file copy can generate multiple notifications for the same
    file. add some delay mechanism so that subsequent change notifications
    cancel a previous, delayed one ? E.g. a copy f2.pdf f.pdf generates 3
    notifications if f2.pdf is 2 MB.

  - try to handle short file names as well: http://blogs.msdn.com/b/ericgu/archive/2005/10/07/478396.aspx
    but how to test it?

- I could try to remove the need for gFileWatcherMutex by queing all code
    that touches gWatchedDirs/gWatchedFiles onto a thread via APC, but that's
    probably an overkill
*/

// there's a balance between responsiveness to changes and efficiency
constexpr int kFileWatchDelayInMs = 1000;

// Some people use overlapped.hEvent to store data but I'm playing it safe.
struct OverlappedEx {
    OVERLAPPED overlapped{};
    void* data = nullptr;
};

// info needed to detect that a file has changed
struct FileWatcherState {
    FILETIME time{};
    i64 size = 0;
};

struct WatchedDir {
    WatchedDir* next = nullptr;
    Str dirPath;
    HANDLE hDir = nullptr;
    bool startMonitoring = true;
    // when removal was queued (GetTickCount64()), for shutdown diagnostics
    u64 removalQueuedAt = 0;
    // a ReadDirectoryChangesW() is in flight, so its completion routine is
    // still going to run for this dir
    bool ioPending = false;
    // StartMonitoringDirForChangesAPC() calls queued but not run yet
    int startApcQueued = 0;
    // StopMonitoringDirAPC() ran: the dir is on its way out
    bool stopped = false;
    OverlappedEx overlapped;
    char buf[16 * 1024]{};
};

struct WatchedFile {
    WatchedFile* next = nullptr;
    WatchedDir* watchedDir = nullptr;
    Str filePath;
    Func0 onFileChangedCb;

    // if true, the file is not on a fixed drive and we have
    // to check if it changed manually, by periodically checking
    // file state for changes
    bool isManualCheck = false;
    FileWatcherState fileState;

    bool ignore = false;
};

void WatchedFileSetIgnore(WatchedFile* wf, bool ignore) {
    if (wf) {
        wf->ignore = ignore;
    }
}

static ThreadHandle gThreadHandle = nullptr;

static HANDLE gThreadControlHandle = nullptr;
static AtomicBool gShouldExit = 0;

// protects data structures shared between ui thread and file
// watcher thread i.e. gWatchedDirs, gWatchedFiles
static Mutex gFileWatcherMutex;

static WatchedDir* gWatchedDirs = nullptr;
static WatchedFile* gWatchedFiles = nullptr;

static AtomicInt gRemovalsPending = 0;

// dirs unlinked from gWatchedDirs whose StopMonitoringDirAPC / cancel-io round
// trip hasn't completed yet. Only used to report what shutdown is waiting for.
// Protected by gFileWatcherMutex; reuses WatchedDir::next (it's off gWatchedDirs).
static WatchedDir* gRemovalsPendingDirs = nullptr;

static void StartMonitoringDirForChanges(WatchedDir* wd);

static void AwakeWatcherThread() {
    SetEvent(gThreadControlHandle);
}

static void GetFileState(Str path, FileWatcherState* fs) {
    // Note: in my testing on network drive that is mac volume mounted
    // via parallels, lastWriteTime is not updated. lastAccessTime is,
    // but it's also updated when the file is being read from (e.g.
    // copy f.pdf f2.pdf will change lastAccessTime of f.pdf)
    // So I'm sticking with lastWriteTime
    //
    // Use GetFileAttributesExW instead of opening the file with CreateFileW.
    // Opening a file (even read-only) on a network drive can trigger
    // Windows Defender to re-scan it, which is slow and generates unwanted
    // network traffic. GetFileAttributesExW queries filesystem metadata
    // without opening the file content, avoiding the scan.
    WCHAR* pathW = CWStrTemp(path);
    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if (GetFileAttributesExW(pathW, GetFileExInfoStandard, &attrs)) {
        fs->time = attrs.ftLastWriteTime;
        fs->size = (i64)attrs.nFileSizeHigh << 32 | attrs.nFileSizeLow;
    }
}

static bool FileStateEq(FileWatcherState* fs1, FileWatcherState* fs2) {
    if (0 != CompareFileTime(&fs1->time, &fs2->time)) {
        return false;
    }
    if (fs1->size != fs2->size) {
        return false;
    }
    return true;
}

static bool FileStateChanged(Str filePath, FileWatcherState* fs) {
    FileWatcherState fsTmp;

    GetFileState(filePath, &fsTmp);
    if (FileStateEq(fs, &fsTmp)) {
        return false;
    }

    memcpy(fs, &fsTmp, sizeof(*fs));
    return true;
}

// TODO: per internet, fileName could be short, 8.3 dos-style name
// and we don't handle that. On the other hand, I've only seen references
// to it wrt. to rename/delete operation, which we don't get notified about
//

static void NotifyAboutFile(WatchedDir* d, Str fileName) {
    int i = 0;

    for (WatchedFile* wf = gWatchedFiles; wf; wf = wf->next) {
        if (wf->ignore) {
            continue;
        }
        if (wf->watchedDir != d) {
            continue;
        }
        TempStr path = path::GetBaseNameTemp(wf->filePath);

        if (!str::EqI(fileName, path)) {
            continue;
        }
        logf("NotifyAboutFile(): i=%d '%s' '%s'\n", i, wf->filePath, fileName);
        i++;

        // NOTE: It is not recommended to check whether the timestamp has changed
        // because the time granularity is so big that this can cause genuine
        // file notifications to be ignored. (This happens for instance for
        // PDF files produced by pdftex from small.tex document)
        wf->onFileChangedCb.Call();
    }
}

static void DeleteWatchedDir(WatchedDir* wd) {
    logf("DeleteWatchedDir() %s\n", wd->dirPath);
    str::Free(wd->dirPath);
    free(wd);
}

// A dir queued for removal can only be freed once nothing can still reach it:
// no ReadDirectoryChangesW() completion is in flight and no re-arm APC is
// queued. Shutdown waits for gRemovalsPending to drain, so every path that
// could be the last one out has to come through here.
// Callers hold gFileWatcherMutex.
static void CompleteRemovalIfDone(WatchedDir* wd) {
    if (!wd->stopped || wd->ioPending || wd->startApcQueued > 0) {
        return;
    }
    ListRemove(&gRemovalsPendingDirs, wd);
    DeleteWatchedDir(wd);
    AtomicIntDec(&gRemovalsPending);
}

// clang-format off
SeqStrings gFileActionNames =
    "FILE_ACTION_ADDED\0" \
    "FILE_ACTION_REMOVED\0" \
    "FILE_ACTION_MODIFIED\0" \
    "FILE_ACTION_RENAMED_OLD_NAME\0" \
    "FILE_ACTION_RENAMED_NEW_NAME\0";
// clang-format on

// only used by the commented-out log in ReadDirectoryChangesNotification()
__unused static TempStr GetFileActionNameTemp(int actionId) {
    if (actionId < 1 || actionId > 5) {
        return StrL("(unknown)");
    }
    int n = actionId - 1;
    return SeqStrByIndex(gFileActionNames, n);
}

static void CALLBACK ReadDirectoryChangesNotification(DWORD errCode, DWORD bytesTransfered, LPOVERLAPPED overlapped) {
    ScopedMutex cs(&gFileWatcherMutex);

    OverlappedEx* over = (OverlappedEx*)overlapped;
    WatchedDir* wd = (WatchedDir*)over->data;

    // logf("ReadDirectoryChangesNotification() dir: %s, numBytes: %d\n", wd->dirPath, (int)bytesTransfered);

    ReportIf(wd != wd->overlapped.data);

    // whatever the outcome, this read is done
    wd->ioPending = false;

    if (errCode == ERROR_OPERATION_ABORTED) {
        // logf("ReadDirectoryChangesNotification: ERROR_OPERATION_ABORTED\n");
        CompleteRemovalIfDone(wd);
        return;
    }
    if (wd->stopped) {
        // removed while this completion was already queued: don't re-arm a
        // handle that StopMonitoringDirAPC() closed
        CompleteRemovalIfDone(wd);
        return;
    }

    wd->startMonitoring = false;

    // This might mean overflow? Not sure.
    if (!bytesTransfered) {
        StartMonitoringDirForChanges(wd);
        return;
    }

    FILE_NOTIFY_INFORMATION* notify = (FILE_NOTIFY_INFORMATION*)wd->buf;

    // collect files that changed, removing duplicates
    StrVec changedFiles;
    for (;;) {
        size_t fnLen = notify->FileNameLength / sizeof(WCHAR);
        TempStr fileName = ToUtf8Temp(WStr(notify->FileName, (int)fnLen));
        // files can get updated either by writing to them directly or
        // by writing to a .tmp file first and then moving that file in place
        // (the latter only yields a RENAMED action with the expected file name)
        // logf("ReadDirectoryChangesNotification: %s '%s'\n", GetFileActionNameTemp(notify->Action), fileName);
        if (notify->Action == FILE_ACTION_ADDED || notify->Action == FILE_ACTION_MODIFIED ||
            notify->Action == FILE_ACTION_RENAMED_NEW_NAME) {
            AppendIfNotExists(&changedFiles, fileName);
        }

        // step to the next entry if there is one
        DWORD nextOff = notify->NextEntryOffset;
        if (!nextOff) {
            break;
        }
        notify = (FILE_NOTIFY_INFORMATION*)((char*)notify + nextOff);
    }

    StartMonitoringDirForChanges(wd);

    for (Str f : changedFiles) {
        NotifyAboutFile(wd, f);
    }
}

static void CALLBACK StartMonitoringDirForChangesAPC(ULONG_PTR arg) {
    WatchedDir* wd = (WatchedDir*)arg;
    ScopedMutex cs(&gFileWatcherMutex);
    wd->startApcQueued--;
    if (wd->stopped) {
        // removed while this was queued: the handle is closed, and this may be
        // the last thing that was keeping the dir alive
        CompleteRemovalIfDone(wd);
        return;
    }
    ZeroMemory(&wd->overlapped, sizeof(wd->overlapped));

    OVERLAPPED* overlapped = (OVERLAPPED*)&(wd->overlapped);
    wd->overlapped.data = (HANDLE)wd;

    // this is called after reading change notification and we're only
    // interested in logging the first time a dir is registered for monitoring
    if (wd->startMonitoring) {
        logf("StartMonitoringDirForChangesAPC() %s\n", wd->dirPath);
    }

    DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME;
    BOOL ok = ReadDirectoryChangesW(wd->hDir,
                                    wd->buf,                           // read results buffer
                                    sizeof(wd->buf),                   // length of buffer
                                    FALSE,                             // bWatchSubtree
                                    dwNotifyFilter,                    // filter conditions
                                    nullptr,                           // bytes returned
                                    overlapped,                        // overlapped buffer
                                    ReadDirectoryChangesNotification); // completion routine
    // only a read that actually started will call back, and removal waits for
    // that callback - so this has to be tracked, not assumed
    wd->ioPending = (ok != FALSE);
    if (!ok) {
        LogLastError();
        logf("StartMonitoringDirForChangesAPC: ReadDirectoryChangesW() failed for '%s'\n", wd->dirPath);
    }
}

// callers hold gFileWatcherMutex
static void StartMonitoringDirForChanges(WatchedDir* wd) {
    wd->startApcQueued++;
    if (QueueUserAPC(StartMonitoringDirForChangesAPC, gThreadHandle, (ULONG_PTR)wd)) {
        return;
    }
    LogLastError();
    logf("StartMonitoringDirForChanges: QueueUserAPC failed for '%s'\n", wd->dirPath);
    wd->startApcQueued--;
    CompleteRemovalIfDone(wd);
}

static DWORD GetTimeoutInMs() {
    ScopedMutex cs(&gFileWatcherMutex);
    for (WatchedFile* wf = gWatchedFiles; wf; wf = wf->next) {
        if (wf->isManualCheck) {
            return kFileWatchDelayInMs;
        }
    }
    return INFINITE;
}

static bool WatchedFileStillActive(WatchedFile* wf) {
    for (WatchedFile* p = gWatchedFiles; p; p = p->next) {
        if (p == wf) {
            return true;
        }
    }
    return false;
}

// Manual checks use GetFileAttributesEx (via FileStateChanged), which is slow
// on network drives. Hold gFileWatcherMutex only while collecting / applying;
// never around the filesystem call.
static void RunManualChecks() {
    struct ManualCheckItem {
        WatchedFile* wf = nullptr;
        Str path;
        FileWatcherState state{};
        bool changed = false;
    };
    Vec<ManualCheckItem> items;
    {
        ScopedMutex cs(&gFileWatcherMutex);
        for (WatchedFile* wf = gWatchedFiles; wf; wf = wf->next) {
            if (!wf->isManualCheck) {
                continue;
            }
            ManualCheckItem it;
            it.wf = wf;
            // Dup so we can use path after releasing the lock (wf may be freed).
            it.path = str::Dup(wf->filePath);
            it.state = wf->fileState;
            VecAppend(items, it);
        }
    }

    for (ManualCheckItem& it : items) {
        // slow path: no lock held
        it.changed = FileStateChanged(it.path, &it.state);
        str::Free(it.path);
        it.path = {};
    }

    ScopedMutex cs(&gFileWatcherMutex);
    for (ManualCheckItem& it : items) {
        if (!it.changed) {
            continue;
        }
        // Unwatch may have removed/freed wf while we were querying attributes
        if (!WatchedFileStillActive(it.wf)) {
            continue;
        }
        it.wf->fileState = it.state;
        // logf("RunManualCheck() %s changed\n", it.wf->filePath);
        it.wf->onFileChangedCb.Call();
    }
}

// manual watcher thread if we can't rely on notifications
static void FileWatcherThread() {
    HANDLE handles[1];
    // must be alertable to receive ReadDirectoryChangesW() callbacks and APCs
    BOOL alertable = TRUE;

    for (;;) {
        ResetTempArena();
        if (AtomicBoolGet(&gShouldExit)) {
            break;
        }
        handles[0] = gThreadControlHandle;
        DWORD timeout = GetTimeoutInMs();
        DWORD obj = WaitForMultipleObjectsEx(1, handles, FALSE, timeout, alertable);
        if (WAIT_TIMEOUT == obj) {
            RunManualChecks();
            continue;
        }

        if (WAIT_IO_COMPLETION == obj) {
            // APC complete. Nothing to do
            // logf("FileWatcherThread(): APC complete\n");
            continue;
        }

        int n = (int)(obj - WAIT_OBJECT_0);

        if (n == 0) {
            // a thread was explicitly awaken
            ResetEvent(gThreadControlHandle);
            // logf("FileWatcherThread(): gThreadControlHandle signalled\n");
        } else {
            logf("FileWatcherThread(): n=%d\n", n);
            ReportIf(true);
        }
    }
    logf("FileWatcherThread: exiting\n");
    DestroyTempArena();
}

static WatchedDir* FindExistingWatchedDir(Str dirPath) {
    for (WatchedDir* wd = gWatchedDirs; wd; wd = wd->next) {
        // TODO: normalize dirPath?
        if (str::EqI(dirPath, wd->dirPath)) {
            return wd;
        }
    }
    return nullptr;
}

static void CALLBACK StopMonitoringDirAPC(ULONG_PTR arg) {
    WatchedDir* wd = (WatchedDir*)arg;
    ScopedMutex cs(&gFileWatcherMutex);
    // logf("StopMonitoringDirAPC() wd=0x%p\n", wd);
    wd->stopped = true;

    // with a read in flight this makes ReadDirectoryChangesNotification() run
    // with errCode = ERROR_OPERATION_ABORTED, which finishes the removal.
    // With none in flight (the last read completed and its re-arm APC hasn't
    // run yet, or the read never started) nothing calls back at all, and
    // waiting for a callback that isn't coming stalled shutdown for 15s
    if (wd->hDir) {
        BOOL ok = CancelIo(wd->hDir);
        if (!ok) {
            LogLastError();
        }
        SafeCloseHandle(&wd->hDir);
    }
    CompleteRemovalIfDone(wd);
}

static WatchedDir* NewWatchedDir(Str dirPath) {
    WCHAR* dirW = CWStrTemp(dirPath);
    DWORD access = FILE_LIST_DIRECTORY;
    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE;
    DWORD disp = OPEN_EXISTING;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED;
    HANDLE hDir = CreateFileW(dirW, access, shareMode, nullptr, disp, flags, nullptr);
    if (INVALID_HANDLE_VALUE == hDir) {
        return nullptr;
    }

    WatchedDir* wd = AllocStruct<WatchedDir>();
    wd->hDir = hDir;
    wd->dirPath = str::Dup(dirPath);

    ListInsertFront(&gWatchedDirs, wd);
    return wd;
}

static WatchedDir* FindOrCreateWatchedDir(Str dirPath, bool* newDir) {
    WatchedDir* wd = FindExistingWatchedDir(dirPath);
    if (wd) {
        return wd;
    }
    wd = NewWatchedDir(dirPath);
    if (!wd) {
        return nullptr;
    }
    wd->startMonitoring = true;
    *newDir = true;
    return wd;
}

static WatchedFile* NewWatchedFile(Str filePath, const Func0& onFileChangedCb, bool enableManualCheckOnNetworkDrives) {
    bool isManualCheck = !path::SupportsChangeNotifications(filePath);
    bool isNetworkDrive = path::IsOnNetworkDrive(filePath);
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5297#issuecomment-3810653582
    // On network drives, avoid manual checks and their network traffic unless TeX
    // support explicitly enables them for auto-reload.
    if (isManualCheck && isNetworkDrive && !enableManualCheckOnNetworkDrives) {
        return nullptr;
    }
    TempStr dirPath = path::GetDirTemp(filePath);
    WatchedDir* wd = nullptr;
    bool newDir = false;
    if (!isManualCheck) {
        wd = FindOrCreateWatchedDir(dirPath, &newDir);
        if (!wd) {
            return nullptr;
        }
    }

    WatchedFile* wf = AllocStruct<WatchedFile>();
    wf->filePath = str::Dup(filePath);
    wf->onFileChangedCb = onFileChangedCb;
    wf->watchedDir = wd;
    wf->isManualCheck = isManualCheck;

    ListInsertFront(&gWatchedFiles, wf);

    if (wf->isManualCheck) {
        GetFileState(filePath, &wf->fileState);
        AwakeWatcherThread();
    } else if (newDir) {
        StartMonitoringDirForChanges(wf->watchedDir);
    }

    return wf;
}

static void DeleteWatchedFile(WatchedFile* wf) {
    str::Free(wf->filePath);
    free(wf);
}

void FileWatcherInit(void) {}

/* Subscribe for notifications about file changes. When a file changes, we'll
call observer->OnFileChanged().

We take ownership of observer object.

Returns a cancellation token that can be used in FileWatcherUnsubscribe(). That
way we can support multiple callers subscribing to the same file.
*/
WatchedFile* FileWatcherSubscribe(Str path, const Func0& onFileChangedCb, bool enableManualCheck) {
    // logf("FileWatcherSubscribe() path: %s\n", path);

    if (!file::Exists(path)) {
        logf("FileWatcherSubscribe: '%s' doesn't exist\n", path);
        return nullptr;
    }

    if (path::IsSame(FileWatcherGetSkipPath(), path)) {
        logf("FileWatcherSubscribe: '%s' is our own log file\n", path);
        return nullptr;
    }
#if 0
    if (IsProcess32()) {
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4111
        logf("FileWatcherSubscribe: not starting a file watcher thread due to 32-bit miscompilation\n");
        return nullptr;
    }
#endif
    ScopedMutex cs(&gFileWatcherMutex);
    if (!gThreadHandle) {
        logf("FileWatcherSubscribe: starting a thread\n");
        AtomicBoolSet(&gShouldExit, false);
        gThreadControlHandle = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        auto fn = MkFunc0Void(FileWatcherThread);
        gThreadHandle = StartThread(fn, StrL("FileWatcherThread"));
    }

    return NewWatchedFile(path, onFileChangedCb, enableManualCheck);
}

static bool IsWatchedDirReferenced(WatchedDir* wd) {
    for (WatchedFile* wf = gWatchedFiles; wf; wf = wf->next) {
        if (wf->watchedDir == wd) {
            return true;
        }
    }
    return false;
}

static void RemoveWatchedDirIfNotReferenced(WatchedDir* wd) {
    if (IsWatchedDirReferenced(wd)) {
        return;
    }

    bool ok = ListRemove(&gWatchedDirs, wd);
    ReportIf(!ok);
    // memory will be eventually freed in ReadDirectoryChangesNotification()
    AtomicIntInc(&gRemovalsPending);
    wd->removalQueuedAt = GetTickCount64();
    ListInsertFront(&gRemovalsPendingDirs, wd);
    DWORD res = QueueUserAPC(StopMonitoringDirAPC, gThreadHandle, (ULONG_PTR)wd);
    if (!res) {
        LogLastError();
        logf("RemoveWatchedDirIfNotReferenced: QueueUserAPC failed for '%s'\n", wd->dirPath);
        // nothing is going to stop it, so retire it here rather than leave
        // shutdown waiting on a removal that can't finish. Closing the handle
        // completes any read in flight with ERROR_OPERATION_ABORTED.
        wd->stopped = true;
        SafeCloseHandle(&wd->hDir);
        CompleteRemovalIfDone(wd);
    }
}

// A pending removal is a WatchedDir that was unlinked from gWatchedDirs and handed
// to StopMonitoringDirAPC(); it lives until CancelIo() reports back through
// ReadDirectoryChangesNotification(ERROR_OPERATION_ABORTED). If that never happens
// (APC not delivered because the watcher thread isn't alertable, or the handle is
// on an unresponsive network share) shutdown spins here, so name the culprits.
// returns false (and logs nothing) if nothing is pending anymore
static bool LogPendingRemovals(Str when) {
    // hold the mutex while reading the count too: ReadDirectoryChangesNotification()
    // unlinks the dir and decrements under the same mutex, so an unlocked read of
    // gRemovalsPending can disagree with the list
    ScopedMutex cs(&gFileWatcherMutex);
    int nPending = AtomicIntGet(&gRemovalsPending);
    if (nPending <= 0) {
        return false;
    }
    u64 now = GetTickCount64();
    logf("FileWatcher: %s, %d removals pending\n", when, nPending);
    int n = 0;
    for (WatchedDir* wd = gRemovalsPendingDirs; wd; wd = wd->next) {
        n++;
        logf("  %d: dir '%s' hDir=%p startMonitoring=%d queued %d ms ago\n", n, wd->dirPath, (const void*)wd->hDir,
             (int)wd->startMonitoring, (int)(now - wd->removalQueuedAt));
    }
    if (n != nPending) {
        logf("  note: %d dirs on the pending list but gRemovalsPending is %d\n", n, nPending);
    }
    return true;
}

void FileWatcherWaitForShutdown(void) {
    if (!gThreadHandle) {
        return;
    }
    // this is meant to be called at the end so we shouldn't
    // have any file watching subscriptions pending
    ReportIf(gWatchedFiles != nullptr);
    ReportIf(gWatchedDirs != nullptr);

    u64 timeStart = GetTickCount64();
    bool loggedPending = false;
    while (AtomicIntGet(&gRemovalsPending) > 0 && (GetTickCount64() - timeStart) < 15000) {
        if (!loggedPending) {
            loggedPending = LogPendingRemovals(StrL("waiting for shutdown"));
        }
        SleepInMs(100);
    }
    if (loggedPending) {
        u64 waitedMs = GetTickCount64() - timeStart;
        TempStr when = fmt("gave up waiting after %d ms", (int)waitedMs);
        if (!LogPendingRemovals(when)) {
            logf("FileWatcher: all removals drained after %d ms\n", (int)waitedMs);
        }
    }

    // Signal from this thread and wake the watcher through the control event.
    // Relying on an APC to both set the flag and wake the thread is fragile
    // during shutdown, especially when previous directory-cancel APCs are still
    // pending or a debugger interrupted the drain above.
    AtomicBoolSet(&gShouldExit, true);
    AwakeWatcherThread();

    // wait for the thread to actually exit (up to 5 seconds)
    DWORD res = WaitForSingleObject(gThreadHandle, 5000);
    if (res == WAIT_TIMEOUT) {
        logf("FileWatcherWaitForShutdown: thread didn't exit in 5 seconds\n");
        return;
    }
    SafeCloseThreadHandle(&gThreadHandle);
    SafeCloseHandle(&gThreadControlHandle);
}

static void RemoveWatchedFile(WatchedFile* wf) {
    WatchedDir* wd = wf->watchedDir;
    bool ok = ListRemove(&gWatchedFiles, wf);
    ReportIf(!ok);

    bool needsAwakeThread = wf->isManualCheck;
    DeleteWatchedFile(wf);
    if (needsAwakeThread) {
        AwakeWatcherThread();
    } else {
        RemoveWatchedDirIfNotReferenced(wd);
    }
}

void FileWatcherUnsubscribe(WatchedFile* wf) {
    if (!wf) {
        return;
    }
    ReportIf(!gThreadHandle);

    ScopedMutex cs(&gFileWatcherMutex);

    RemoveWatchedFile(wf);
}
