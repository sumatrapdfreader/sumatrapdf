/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileWatcher.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/WinUtil.h"

#include "utils/Log.h"

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

g_watchedDirs and g_watchedFiles are shared between the main thread and
worker thread so must be protected via g_threadCritSec.

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

  - I could try to remove the need for g_threadCritSec by queing all code
    that touches g_watchedDirs/g_watchedFiles onto a thread via APC, but that's
    probably an overkill
*/

// there's a balance between responsiveness to changes and efficiency
#define FILEWATCH_DELAY_IN_MS 1000

// Some people use overlapped.hEvent to store data but I'm playing it safe.
struct OverlappedEx {
    OVERLAPPED overlapped{};
    void* data = nullptr;
};

// info needed to detect that a file has changed
struct FileWatcherState {
    FILETIME time = {0};
    i64 size = 0;
};

struct WatchedDir {
    WatchedDir* next = nullptr;
    const char* dirPath = nullptr;
    HANDLE hDir = nullptr;
    bool startMonitoring = true;
    OverlappedEx overlapped;
    char buf[8 * 1024]{};
};

struct WatchedFile {
    WatchedFile* next = nullptr;
    WatchedDir* watchedDir = nullptr;
    const char* filePath = nullptr;
    std::function<void()> onFileChangedCb;

    // if true, the file is on a network drive and we have
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

static HANDLE g_threadHandle = nullptr;
static DWORD g_threadId = 0;

static HANDLE g_threadControlHandle = nullptr;

// protects data structures shared between ui thread and file
// watcher thread i.e. g_watchedDirs, g_watchedFiles
static CRITICAL_SECTION g_threadCritSec;

static WatchedDir* g_watchedDirs = nullptr;
static WatchedFile* g_watchedFiles = nullptr;

static LONG gRemovalsPending = 0;

static void StartMonitoringDirForChanges(WatchedDir* wd);

static void AwakeWatcherThread() {
    SetEvent(g_threadControlHandle);
}

static void GetFileState(const char* path, FileWatcherState* fs) {
    // Note: in my testing on network drive that is mac volume mounted
    // via parallels, lastWriteTime is not updated. lastAccessTime is,
    // but it's also updated when the file is being read from (e.g.
    // copy f.pdf f2.pdf will change lastAccessTime of f.pdf)
    // So I'm sticking with lastWriteTime
    fs->time = file::GetModificationTime(path);
    fs->size = file::GetSize(path);
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

static bool FileStateChanged(const char* filePath, FileWatcherState* fs) {
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
// TODO: to collapse multiple notifications for the same file, could put it on a
// queue, restart the thread with a timeout, restart the process if we
// get notified again before timeout expires, call OnFileChanges() when
// timeout expires
static void NotifyAboutFile(WatchedDir* d, const char* fileName) {
    int i = 0;

    for (WatchedFile* wf = g_watchedFiles; wf; wf = wf->next) {
        if (wf->ignore) {
            logf("NotifyAboutFile: ignoring '%s'\n", wf->filePath);
            continue;
        }
        if (wf->watchedDir != d) {
            continue;
        }
        const char* path = path::GetBaseNameTemp(wf->filePath);

        if (!str::EqI(fileName, path)) {
            continue;
        }
        logf("NotifyAboutFile(): i=%d '%s' '%s'\n", i, wf->filePath, fileName);
        i++;

        // NOTE: It is not recommended to check whether the timestamp has changed
        // because the time granularity is so big that this can cause genuine
        // file notifications to be ignored. (This happens for instance for
        // PDF files produced by pdftex from small.tex document)
        wf->onFileChangedCb();
    }
}

static void DeleteWatchedDir(WatchedDir* wd) {
    str::Free(wd->dirPath);
    free(wd);
}

// clang-format off
SeqStrings gFileActionNames =
    "FILE_ACTION_ADDED\0" \
    "FILE_ACTION_REMOVED\0" \
    "FILE_ACTION_MODIFIED\0" \
    "FILE_ACTION_RENAMED_OLD_NAME\0" \
    "FILE_ACTION_RENAMED_NEW_NAME\0";
// clang-format on

const char* GetFileActionName(int actionId) {
    if (actionId < 1 || actionId > 5) {
        return "(unknown)";
    }
    int n = actionId - 1;
    return seqstrings::IdxToStr(gFileActionNames, n);
}

static void CALLBACK ReadDirectoryChangesNotification(DWORD errCode, DWORD bytesTransfered, LPOVERLAPPED overlapped) {
    ScopedCritSec cs(&g_threadCritSec);

    OverlappedEx* over = (OverlappedEx*)overlapped;
    WatchedDir* wd = (WatchedDir*)over->data;

    // logf("ReadDirectoryChangesNotification() dir: %s, numBytes: %d\n", wd->dirPath, (int)bytesTransfered);

    CrashIf(wd != wd->overlapped.data);

    if (errCode == ERROR_OPERATION_ABORTED) {
        // logf("ReadDirectoryChangesNotification: ERROR_OPERATION_ABORTED\n");
        DeleteWatchedDir(wd);
        InterlockedDecrement(&gRemovalsPending);
        return;
    }

    // This might mean overflow? Not sure.
    if (!bytesTransfered) {
        return;
    }

    FILE_NOTIFY_INFORMATION* notify = (FILE_NOTIFY_INFORMATION*)wd->buf;

    // collect files that changed, removing duplicates
    StrVec changedFiles;
    for (;;) {
        size_t fnLen = notify->FileNameLength / sizeof(WCHAR);
        char* fileName = ToUtf8Temp(notify->FileName, fnLen);
        // files can get updated either by writing to them directly or
        // by writing to a .tmp file first and then moving that file in place
        // (the latter only yields a RENAMED action with the expected file name)
        const char* actionName = GetFileActionName(notify->Action);
        logf("ReadDirectoryChangesNotification: %s '%s'\n", actionName, fileName);
        if (notify->Action == FILE_ACTION_MODIFIED || notify->Action == FILE_ACTION_RENAMED_NEW_NAME) {
            changedFiles.AppendIfNotExists(fileName);
        }

        // step to the next entry if there is one
        DWORD nextOff = notify->NextEntryOffset;
        if (!nextOff) {
            break;
        }
        notify = (FILE_NOTIFY_INFORMATION*)((char*)notify + nextOff);
    }

    wd->startMonitoring = false;
    StartMonitoringDirForChanges(wd);

    for (char* f : changedFiles) {
        NotifyAboutFile(wd, f);
    }
}

static void CALLBACK StartMonitoringDirForChangesAPC(ULONG_PTR arg) {
    WatchedDir* wd = (WatchedDir*)arg;
    ZeroMemory(&wd->overlapped, sizeof(wd->overlapped));

    OVERLAPPED* overlapped = (OVERLAPPED*)&(wd->overlapped);
    wd->overlapped.data = (HANDLE)wd;

    // this is called after reading change notification and we're only
    // interested in logging the first time a dir is registered for monitoring
    if (wd->startMonitoring) {
        logf("StartMonitoringDirForChangesAPC() %s\n", wd->dirPath);
    }

    CrashIf(g_threadId != GetCurrentThreadId());

    DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME;
    ReadDirectoryChangesW(wd->hDir,
                          wd->buf,                           // read results buffer
                          sizeof(wd->buf),                   // length of buffer
                          FALSE,                             // bWatchSubtree
                          dwNotifyFilter,                    // filter conditions
                          nullptr,                           // bytes returned
                          overlapped,                        // overlapped buffer
                          ReadDirectoryChangesNotification); // completion routine
}

static void StartMonitoringDirForChanges(WatchedDir* wd) {
    QueueUserAPC(StartMonitoringDirForChangesAPC, g_threadHandle, (ULONG_PTR)wd);
}

static DWORD GetTimeoutInMs() {
    ScopedCritSec cs(&g_threadCritSec);
    for (WatchedFile* wf = g_watchedFiles; wf; wf = wf->next) {
        if (wf->isManualCheck) {
            return FILEWATCH_DELAY_IN_MS;
        }
    }
    return INFINITE;
}

static void RunManualChecks() {
    ScopedCritSec cs(&g_threadCritSec);

    for (WatchedFile* wf = g_watchedFiles; wf; wf = wf->next) {
        if (!wf->isManualCheck) {
            continue;
        }
        if (FileStateChanged(wf->filePath, &wf->fileState)) {
            // logf("RunManualCheck() %s changed\n", wf->filePath);
            wf->onFileChangedCb();
        }
    }
}

static DWORD WINAPI FileWatcherThread(void*) {
    HANDLE handles[1];
    // must be alertable to receive ReadDirectoryChangesW() callbacks and APCs
    BOOL alertable = TRUE;

    for (;;) {
        ResetTempAllocator();
        handles[0] = g_threadControlHandle;
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
            ResetEvent(g_threadControlHandle);
            // logf("FileWatcherThread(): g_threadControlHandle signalled\n");
        } else {
            logf("FileWatcherThread(): n=%d\n", n);
            CrashIf(true);
        }
    }
    DestroyTempAllocator();
}

static void StartThreadIfNecessary() {
    if (g_threadHandle) {
        return;
    }

    InitializeCriticalSection(&g_threadCritSec);
    g_threadControlHandle = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    g_threadHandle = CreateThread(nullptr, 0, FileWatcherThread, nullptr, 0, &g_threadId);
    SetThreadName("FileWatcherThread", g_threadId);
}

static WatchedDir* FindExistingWatchedDir(const char* dirPath) {
    for (WatchedDir* wd = g_watchedDirs; wd; wd = wd->next) {
        // TODO: normalize dirPath?
        if (str::EqI(dirPath, wd->dirPath)) {
            return wd;
        }
    }
    return nullptr;
}

static void CALLBACK StopMonitoringDirAPC(ULONG_PTR arg) {
    WatchedDir* wd = (WatchedDir*)arg;
    // logf("StopMonitoringDirAPC() wd=0x%p\n", wd);

    // this will cause ReadDirectoryChangesNotification() to be called
    // with errCode = ERROR_OPERATION_ABORTED
    BOOL ok = CancelIo(wd->hDir);
    if (!ok) {
        LogLastError();
    }
    SafeCloseHandle(&wd->hDir);
}

static void CALLBACK ExitMonitoringThread(ULONG_PTR arg) {
    log("ExitMonitoringThraed\n");
    ExitThread(0);
}

static WatchedDir* NewWatchedDir(const char* dirPath) {
    WCHAR* dirW = ToWStrTemp(dirPath);
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

    ListInsert(&g_watchedDirs, wd);
    return wd;
}

static WatchedFile* NewWatchedFile(const char* filePath, const std::function<void()>& onFileChangedCb) {
    WCHAR* pathW = ToWStrTemp(filePath);
    bool isManualCheck = PathIsNetworkPathW(pathW);
    char* dirPath = path::GetDirTemp(filePath);
    WatchedDir* wd = nullptr;
    bool newDir = false;
    if (!isManualCheck) {
        wd = FindExistingWatchedDir(dirPath);
        if (!wd) {
            wd = NewWatchedDir(dirPath);
            if (!wd) {
                return nullptr;
            }
            wd->startMonitoring = true;
            newDir = true;
        }
    }

    WatchedFile* wf = AllocStruct<WatchedFile>();
    wf->filePath = str::Dup(filePath);
    wf->onFileChangedCb = onFileChangedCb;
    wf->watchedDir = wd;
    wf->isManualCheck = isManualCheck;

    ListInsert(&g_watchedFiles, wf);

    if (wf->isManualCheck) {
        GetFileState(filePath, &wf->fileState);
        AwakeWatcherThread();
    } else {
        if (newDir) {
            StartMonitoringDirForChanges(wf->watchedDir);
        }
    }

    return wf;
}

static void DeleteWatchedFile(WatchedFile* wf) {
    str::Free(wf->filePath);
    free(wf);
}

/* Subscribe for notifications about file changes. When a file changes, we'll
call observer->OnFileChanged().

We take ownership of observer object.

Returns a cancellation token that can be used in FileWatcherUnsubscribe(). That
way we can support multiple callers subscribing to the same file.
*/
WatchedFile* FileWatcherSubscribe(const char* path, const std::function<void()>& onFileChangedCb) {
    // logf("FileWatcherSubscribe() path: %s\n", path);

    if (!file::Exists(path)) {
        return nullptr;
    }

    StartThreadIfNecessary();

    ScopedCritSec cs(&g_threadCritSec);
    return NewWatchedFile(path, onFileChangedCb);
}

static bool IsWatchedDirReferenced(WatchedDir* wd) {
    for (WatchedFile* wf = g_watchedFiles; wf; wf = wf->next) {
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

    bool ok = ListRemove(&g_watchedDirs, wd);
    CrashIf(!ok);
    // memory will be eventually freed in ReadDirectoryChangesNotification()
    InterlockedIncrement(&gRemovalsPending);
    QueueUserAPC(StopMonitoringDirAPC, g_threadHandle, (ULONG_PTR)wd);
}

void FileWatcherWaitForShutdown() {
    // this is meant to be called at the end so we shouldn't
    // have any file watching subscriptions pending
    CrashIf(g_watchedFiles != nullptr);
    CrashIf(g_watchedDirs != nullptr);
    QueueUserAPC(ExitMonitoringThread, g_threadHandle, (ULONG_PTR)0);

    // wait for ReadDirectoryChangesNotification() process actions triggered
    // in RemoveWatchedDirIfNotReferenced
    LONG v;
    int maxWait = 100; // 1 sec
    for (;;) {
        v = InterlockedCompareExchange(&gRemovalsPending, 0, 0);
        if (v == 0) {
            return;
        }
        Sleep(10);
        if (--maxWait < 0) {
            return;
        }
    }
}

static void RemoveWatchedFile(WatchedFile* wf) {
    WatchedDir* wd = wf->watchedDir;
    bool ok = ListRemove(&g_watchedFiles, wf);
    CrashIf(!ok);

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
    CrashIf(!g_threadHandle);

    ScopedCritSec cs(&g_threadCritSec);

    RemoveWatchedFile(wf);
}
