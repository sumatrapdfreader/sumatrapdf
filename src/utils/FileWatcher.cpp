/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileWatcher.h"

#include "FileUtil.h"
#include "ThreadUtil.h"
#include "WinUtil.h"

#define NOLOG 0
#include "DebugLog.h"

/*
This code is tricky, so here's a high-level overview. More info at:
http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html

We use ReadDirectoryChangesW() with overlapped i/o and i/o completion
callback function.

Callback function is called in the context of the thread that called
ReadDirectoryChangesW() but only if it's in alertable state.
Our ui thread isn't so we create our own thread and run code that
calls ReadDirectoryChangesW() on that thread via QueueUserAPC().

g_dirsList and g_filesList are shared between the main thread and
worker thread so must be protected via g_threadCritSec.
*/

/*
TODO:

  - handle files on non-fixed drives (network drives, usb) by using
    a timeout in the thread

  - should I end the thread when there are no files to watch?

  - a single file copy can generate multiple notifications for the same
    file. add some delay mechanism so that subsequent change notifications
    cancel a previous, delayed one ? E.g. a copy f2.pdf f.pdf generates 3
    notifications if f2.pdf is 2 MB.

  - try to handle short file names as well: http://blogs.msdn.com/b/ericgu/archive/2005/10/07/478396.aspx
    but how to test it?

  - I could try to remove the need for g_threadCritSec by queing all code
    that touches g_dirsList/g_filesList onto a thread via APC, but that's
    probably an overkill
*/

#define INVALID_TOKEN 0

#define FILEWATCH_DELAY_IN_MS       1000

// Some people use overlapped.hEvent to store data but I'm playing it safe.
struct OverlappedEx {
    OVERLAPPED      overlapped;
    void *          data;
};

struct WatchedDir {
    WatchedDir *    next;
    const WCHAR *   dirPath;
    HANDLE          hDir;
    OverlappedEx    overlapped;
    char            buf[8*1024];

    // if true, the file is on a network drive and we have
    // to check it manually, via timeout
    // TODO: not used yet
    bool            isManualCheck;
};

struct WatchedFile {
    WatchedFile *           next;
    WatchedDir *            watchedDir;
    const WCHAR *           fileName;
    FileChangeObserver *    observer;
};

static int              g_currentToken = 1;
static HANDLE           g_threadHandle = 0;
static DWORD            g_threadId = 0;

static HANDLE           g_threadControlHandle = 0;

// protects data structures shared between ui thread and file
// watcher thread i.e. g_dirsList, g_filesList
static CRITICAL_SECTION g_threadCritSec;

static WatchedDir *     g_dirsList = NULL;
static WatchedFile *    g_filesList = NULL;

static void StartMonitoringDirForChanges(WatchedDir *wd);

#if 0 // not used yet
static void WakeUpWatcherThread()
{
    SetEvent(g_threadControlHandle);
}
#endif

// TODO: per internet, fileName could be short, 8.3 dos-style name
// and we don't handle that. On the other hand, I've only seen references
// to it wrt. to rename/delete operation, which we don't get notified about
//
// TODO: to collapse multiple notifications for the same file, could put it on a
// queue, restart the thread with a timeout, restart the process if we
// get notified again before timeout expires, call OnFileChanges() when
// timeout expires
static void NotifyAboutFile(WatchedDir *d, const WCHAR *fileName)
{
    lf(L"NotifyAboutFile() %s", fileName);

    WatchedFile *curr = g_filesList;
    while (curr) {
        if (curr->watchedDir == d) {
            if (str::EqI(fileName, curr->fileName)) {
                // NOTE: It is not recommended to check whether the timestamp has changed
                // because the time granularity is so big that this can cause genuine
                // file notifications to be ignored. (This happens for instance for
                // PDF files produced by pdftex from small.tex document)
                curr->observer->OnFileChanged();
                return;
            }
        }
        curr = curr->next;
    }
}

static void DeleteWatchedDir(WatchedDir *wd)
{
    free((void*)wd->dirPath);
    free(wd);
}

static void CALLBACK ReadDirectoryChangesNotification(DWORD errCode, 
    DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    OverlappedEx *over = (OverlappedEx*)overlapped;
    WatchedDir* wd = (WatchedDir*)over->data;

    lf(L"ReadDirectoryChangesNotification() dir: %s, numBytes: %d", wd->dirPath, (int)bytesTransfered);

    CrashIf(wd != wd->overlapped.data);

    if (errCode == ERROR_OPERATION_ABORTED)
    {
        lf("   ERROR_OPERATION_ABORTED");
        DeleteWatchedDir(wd);
        return;
    }

    // This might mean overflow? Not sure.
    if (!bytesTransfered)
        return;

    FILE_NOTIFY_INFORMATION *notify = (FILE_NOTIFY_INFORMATION*)wd->buf;

    // collect files that changed, removing duplicates
    WStrVec changedFiles;
    for (;;) {
        WCHAR *fileName = str::DupN(notify->FileName, notify->FileNameLength / sizeof(WCHAR));
        if (notify->Action == FILE_ACTION_MODIFIED) {
            if (!changedFiles.Contains(fileName)) {
                changedFiles.Append(fileName);
                lf(L"ReadDirectoryChangesNotification() FILE_ACTION_MODIFIED, for '%s'", fileName);
            } else {
                lf(L"ReadDirectoryChangesNotification() eliminating duplicate notification for '%s'", fileName);
                free(fileName);
            }
        } else {
            lf(L"ReadDirectoryChangesNotification() action=%d, for '%s'", (int)notify->Action, fileName);
            free(fileName);
        }

        // step to the next entry if there is one
        DWORD nextOff = notify->NextEntryOffset;
        if (!nextOff)
            break;
        notify = (FILE_NOTIFY_INFORMATION *)((char*)notify + nextOff);
    }

    StartMonitoringDirForChanges(wd);

    ScopedCritSec cs(&g_threadCritSec);
    for (WCHAR **f = changedFiles.IterStart(); f; f = changedFiles.IterNext()) {
        NotifyAboutFile(wd, *f);
    }
}

static void CALLBACK StartMonitoringDirForChangesAPC(ULONG_PTR arg)
{
    WatchedDir *wd = (WatchedDir*)arg;
    ZeroMemory(&wd->overlapped, sizeof(wd->overlapped));

    OVERLAPPED *overlapped = (OVERLAPPED*)&(wd->overlapped);
    wd->overlapped.data = (HANDLE)wd;

    lf(L"StartMonitoringDirForChangesAPC() %s", wd->dirPath);

    CrashIf(g_threadId != GetCurrentThreadId());

    ReadDirectoryChangesW(
         wd->hDir,
         wd->buf,                           // read results buffer
         sizeof(wd->buf),                   // length of buffer
         FALSE,                             // bWatchSubtree
         FILE_NOTIFY_CHANGE_LAST_WRITE,     // filter conditions
         NULL,                              // bytes returned
         overlapped,                        // overlapped buffer
         ReadDirectoryChangesNotification); // completion routine
}

static void StartMonitoringDirForChanges(WatchedDir *wd)
{
    QueueUserAPC(StartMonitoringDirForChangesAPC, g_threadHandle, (ULONG_PTR)wd);
}

static DWORD WINAPI FileWatcherThread(void *param)
{
    HANDLE handles[1];
    for (;;) {
        handles[0] = g_threadControlHandle;
        DWORD obj = WaitForMultipleObjectsEx(1, handles, FALSE, INFINITE, TRUE /* must be alertable */);
        if (obj == WAIT_IO_COMPLETION) {
            // APC complete. Nothing to do
            lf("FileWatcherThread(): APC complete");
        } else {
            int n = (int)(obj - WAIT_OBJECT_0);
            CrashIf(n < 0 || n >= 1);

            if (n == 0) {
                // a thread was explicitly awaken
                ResetEvent(g_threadControlHandle);
                lf("FileWatcherThread(): g_threadControlHandle signalled");
            } else {
                lf("FileWatcherThread(): n=%d", n);
                CrashIf(true);
            }
        }
    }
    return 0;
}

static void StartThreadIfNecessary()
{
    if (g_threadHandle)
        return;

    InitializeCriticalSection(&g_threadCritSec);
    g_threadControlHandle = CreateEvent(NULL, TRUE, FALSE, NULL);

    g_threadHandle = CreateThread(NULL, 0, FileWatcherThread, 0, 0, &g_threadId);
    SetThreadName(g_threadId, "FileWatcherThread");
}

static WatchedDir *FindExistingWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *curr = g_dirsList;
    while (curr) {
        // TODO: normalize dirPath?
        if (str::EqI(dirPath, curr->dirPath))
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static void CALLBACK StopMonitoringDirAPC(ULONG_PTR arg)
{
    WatchedDir *wd = (WatchedDir*)arg;
    lf("StopMonitoringDirAPC() wd=0x%p", wd);

    // this will cause ReadDirectoryChangesNotification() to be called
    // with errCode = ERROR_OPERATION_ABORTED
    BOOL ok = CancelIo(wd->hDir);
    if (!ok)
        LogLastError();
    SafeCloseHandle(&wd->hDir);
}

static void StopMonitoringDir(WatchedDir *wd)
{
    QueueUserAPC(StopMonitoringDirAPC, g_threadHandle, (ULONG_PTR)wd);
}

static WatchedDir *NewWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *wd = AllocStruct<WatchedDir>();
    wd->dirPath = str::Dup(dirPath);
    wd->hDir = CreateFile(
        dirPath, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED, NULL);
    if (!wd->hDir)
        goto Failed;

    wd->next = g_dirsList;
    g_dirsList = wd;

    return wd;
Failed:
    DeleteWatchedDir(wd);
    return NULL;
}

static WatchedFile *NewWatchedFile(const WCHAR *filePath, FileChangeObserver *observer)
{
    ScopedMem<WCHAR> dirPath(path::GetDir(filePath));
    WatchedDir *wd = FindExistingWatchedDir(dirPath);
    bool newDir = false;
    if (!wd) {
        wd = NewWatchedDir(dirPath);
        newDir = true;
    }

    WatchedFile *wf = AllocStruct<WatchedFile>();
    wf->fileName = str::Dup(path::GetBaseName(filePath));
    wf->watchedDir = wd;
    wf->observer = observer;
    wf->next = g_filesList;
    g_filesList = wf;

    if (newDir)
        StartMonitoringDirForChanges(wd);

    return wf;
}

static void DeleteWatchedFile(WatchedFile *wf)
{
    free((void*)wf->fileName);
    delete wf->observer;
    free(wf);
}

/* Subscribe for notifications about file changes. When a file changes, we'll
call observer->OnFileChanged().

We take ownership of observer object.

Returns a cancellation token that can be used in FileWatcherUnsubscribe(). That
way we can support multiple callers subscribing to the same file.
*/
FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer)
{
    CrashIf(!observer);

    lf(L"FileWatcherSubscribe() path: %s", path);

    if (!file::Exists(path)) {
        delete observer;
        return NULL;
    }

    StartThreadIfNecessary();

    ScopedCritSec cs(&g_threadCritSec);

    // TODO: if the file is on a network drive we should periodically check
    // it ourselves, because ReadDirectoryChangesW()
    // doesn't always work in that case (per http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw_19.html
    // it depends on the server)
    WatchedFile *wf = NewWatchedFile(path, observer);
    return wf;
}

static bool IsWatchedDirReferenced(WatchedDir *wd)
{
    for (WatchedFile *wf = g_filesList; wf; wf->next) {
        if (wf->watchedDir == wd)
            return true;
    }
    return false;
}

static void RemoveWatchedDirIfNotReferenced(WatchedDir *wd)
{
    if (IsWatchedDirReferenced(wd))
        return;
    WatchedDir **currPtr = &g_dirsList;
    WatchedDir *curr;
    for (;;) {
        curr = *currPtr;
        CrashAlwaysIf(!curr);
        if (curr == wd)
            break;
        currPtr = &(curr->next);
    }
    WatchedDir *toRemove = curr;
    *currPtr = toRemove->next;

    StopMonitoringDir(toRemove);
}

static void RemoveWatchedFile(WatchedFile *wf)
{
    WatchedDir *wd = wf->watchedDir;

    WatchedFile **currPtr = &g_filesList;
    WatchedFile *curr;
    for (;;) {
        curr = *currPtr;
        CrashAlwaysIf(!curr);
        if (curr == wf)
            break;
        currPtr = &(curr->next);
    }
    WatchedFile *toRemove = curr;
    *currPtr = toRemove->next;
    DeleteWatchedFile(toRemove);

    RemoveWatchedDirIfNotReferenced(wd);
}

void FileWatcherUnsubscribe(FileWatcherToken token)
{
    if (!token)
        return;
    CrashIf(!g_threadHandle);

    ScopedCritSec cs(&g_threadCritSec);

    RemoveWatchedFile(token);
}
