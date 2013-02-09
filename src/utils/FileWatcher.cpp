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
TODO:

  - I'm ocasionally getting the 'overlapped modified after being freed'

  - handle files on non-fixed drives (network drives, usb) by using
    a timeout in the thread

  - should I end the thread when there are no files to watch?

  - a single file copy can generate multiple notifications for the same
    file. add some delay mechanism so that subsequent change notifications
    cancel a previous, delayed one ? E.g. a copy f2.pdf f.pdf generates 3
    notifications if f2.pdf is 2 MB.

  - implement it the way http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html
    suggests?
*/

// TODO: a hack for VS 2011 compilation. 1600 is VS 2010
#if _MSC_VER > 1600
extern "C" {
WINBASEAPI BOOL WINAPI
GetOverlappedResult(_In_ HANDLE hFile, _In_ LPOVERLAPPED lpOverlapped, _Out_ LPDWORD lpNumberOfBytesTransferred, _In_ BOOL bWait);
}
#endif

#define INVALID_TOKEN 0

#define FILEWATCH_DELAY_IN_MS       1000

struct WatchedDir {
    WatchedDir *    next;
    const WCHAR *   dirPath;
    HANDLE          hDir;
    OVERLAPPED      overlapped;
    // a double buffer where the Windows API ReadDirectoryChanges will store the list
    // of files that have been modified.
    FILE_NOTIFY_INFORMATION     buf1[512];
    FILE_NOTIFY_INFORMATION     buf2[512];
    FILE_NOTIFY_INFORMATION *   currBuf;
};

struct WatchedFile {
    WatchedFile *           next;
    WatchedDir *            watchedDir;
    const WCHAR *           fileName;
    FileChangeObserver *    observer;
};

static int              g_currentToken = 1;
static HANDLE           g_threadHandle = 0;
// used to wake-up file wather thread to notify about
// added/removed files to be watched
static HANDLE           g_threadControlHandle = 0;

// protects data structures shared between ui thread and file
// watcher thread i.e. g_firstDir, g_firstFile
static CRITICAL_SECTION g_threadCritSec;

// 1 taken by g_threadControlHandle
static int              g_maxFilesToWatch = MAXIMUM_WAIT_OBJECTS - 1;

static WatchedDir *     g_firstDir = NULL;
static WatchedFile *    g_firstFile = NULL;

static HANDLE g_fileWatcherHandles[MAXIMUM_WAIT_OBJECTS];

static void WakeUpWatcherThread()
{
    SetEvent(g_threadControlHandle);
}

static void NotifyAboutFile(WatchedDir *d, const WCHAR *fileName)
{
    lf(L"NotifyAboutFile() %s", fileName);

    WatchedFile *curr = g_firstFile;
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

// start a new asynchronous call to ReadDirectory in the alternate buffer
static void StartMonitoringDirForChanges(WatchedDir *wd)
{
    if (wd->currBuf == wd->buf1) {
        wd->currBuf = wd->buf2;
        lf(L"StartMonitoringDirForChanges() %s, buf2", wd->dirPath);
    } else {
        wd->currBuf = wd->buf1;
        lf(L"StartMonitoringDirForChanges() %s, buf1", wd->dirPath);
    }

    ReadDirectoryChangesW(
         wd->hDir,
         wd->currBuf, /* read results buffer */
         sizeof(wd->buf1), /* length of buffer */
         FALSE, /* monitoring option */
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &wd->overlapped, /* overlapped buffer */
         NULL); /* completion routine */
}

static void NotifyAboutDirChanged(WatchedDir *wd)
{
    // Read the asynchronous result of the previous call to ReadDirectory
    DWORD numBytes;
    BOOL ok = GetOverlappedResult(wd->hDir, &wd->overlapped, &numBytes, FALSE);
    if (!ok)
        LogLastError();

    lf(L"NotifyAboutDirChanged() dir: %s, ok=%d, numBytes=%d", wd->dirPath, (int)ok, (int)numBytes);
    FILE_NOTIFY_INFORMATION *notify = wd->currBuf;

    // a single notification can have multiple notifications for the same file, so we filter duplicates
    WStrVec seenFiles;

    for (;;) {
        WCHAR *fileName = str::DupN(notify->FileName, notify->FileNameLength / sizeof(WCHAR));
        if (seenFiles.Contains(fileName)) {
            lf(L"NotifyAboutDirChanged() eliminating duplicate notification for '%s'", fileName);
            free(fileName);
        } else {
            if (notify->Action == FILE_ACTION_MODIFIED) {
                lf(L"NotifyAboutDirChanged() FILE_ACTION_MODIFIED, for '%s'", fileName);
                NotifyAboutFile(wd, fileName);
            } else {
                lf(L"NotifyAboutDirChanged() action=%d, for '%s'", (int)notify->Action, fileName);
            }
            seenFiles.Append(fileName);
        }

        // step to the next entry if there is one
        DWORD nextOff = notify->NextEntryOffset;
        if (!nextOff)
            break;
        notify = (FILE_NOTIFY_INFORMATION *)((char*)notify + nextOff);
    }

    StartMonitoringDirForChanges(wd);
}

static void NotifyAboutFileChanges(HANDLE h)
{
    ScopedCritSec cs(&g_threadCritSec);

    WatchedDir *curr = g_firstDir;
    while (curr) {
        if (h == curr->overlapped.hEvent)
            break;
        curr = curr->next;
    }
    if (!curr) {
        // I believe this can happen i.e. a dir can be
        // removed from the list after we received a notification
        // but before we got here
        return;
    }
    NotifyAboutDirChanged(curr);
}

static int CollectHandlesToWaitOn()
{
    int n = 0;
    ScopedCritSec cs(&g_threadCritSec);
    g_fileWatcherHandles[n++] = g_threadControlHandle;
    WatchedDir *curr = g_firstDir;
    while (curr) {
        g_fileWatcherHandles[n++] = curr->overlapped.hEvent;
        if (n >= MAXIMUM_WAIT_OBJECTS)
            break;
        curr = curr->next;
    }
    return n;
}

static DWORD WINAPI FileWatcherThread(void *param)
{
    for (;;) {
        int nHandles = CollectHandlesToWaitOn();
        DWORD obj = WaitForMultipleObjects(nHandles, g_fileWatcherHandles, FALSE, INFINITE);
        int n = (int)(obj - WAIT_OBJECT_0);
        CrashIf(n < 0 || n >= nHandles);
        if (n > 0) {
            NotifyAboutFileChanges(g_fileWatcherHandles[n]);
        } else {
            // dirs have been added/deleted and we need to
            // rebuild g_fileWatcherHandles
            ResetEvent(g_threadControlHandle);
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

    DWORD threadId;
    g_threadHandle = CreateThread(NULL, 0, FileWatcherThread, 0, 0, &threadId);
    SetThreadName(threadId, "FileWatcherThread");
}

static WatchedDir *FindExistingWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *curr = g_firstDir;
    while (curr) {
        // TODO: normalize dirPath?
        if (str::EqI(dirPath, curr->dirPath))
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// TODO: this is not right
static void DeleteWatchedDir(WatchedDir *wd)
{
    lf("DeleteWatchedDir() wd=0x%p", wd);

    // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2039
    // we must wait for all outstanding overlapped i/o on hDir has stopped.
    // otherwise CloseHandle(hDir) might end up modyfing it's associated
    // overlapped asynchronously, after we have already freed its memory, causing
    // memory corruption
    BOOL ok = CancelIo(wd->hDir);
    if (!ok)
        LogLastError();

    // TODO: I get ERROR_IO_INCOMPLETE here
    // http://stackoverflow.com/questions/4273594/overlapped-io-and-error-io-incomplete suggests
    // to wait forever in GetOverlappedResult(), but that just blocks forever
    DWORD numBytesTransferred;
    ok = GetOverlappedResult(wd->hDir, &wd->overlapped, &numBytesTransferred, FALSE);
    if (!ok) {
        DWORD err = GetLastError();
        if (ERROR_OPERATION_ABORTED != err)
            LogLastError();
    }

    SafeCloseHandle(&wd->overlapped.hEvent);
    SafeCloseHandle(&wd->hDir);

    free((void*)wd->dirPath);
    free(wd);
}

static WatchedDir *NewWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *wd = AllocStruct<WatchedDir>();
    wd->currBuf = wd->buf1;
    wd->dirPath = str::Dup(dirPath);
    wd->hDir = CreateFile(
        dirPath, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED, NULL);
    if (!wd->hDir)
        goto Failed;

    wd->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wd->overlapped.hEvent)
        goto Failed;

    wd->next = g_firstDir;
    g_firstDir = wd;

    StartMonitoringDirForChanges(wd);

    return wd;
Failed:
    DeleteWatchedDir(wd);
    return NULL;
}

static WatchedFile *NewWatchedFile(const WCHAR *filePath, FileChangeObserver *observer)
{
    ScopedMem<WCHAR> dirPath(path::GetDir(filePath));
    WatchedDir *wd = FindExistingWatchedDir(dirPath);
    if (!wd)
        wd = NewWatchedDir(dirPath);

    WatchedFile *wf = AllocStruct<WatchedFile>();
    wf->fileName = str::Dup(path::GetBaseName(filePath));
    wf->watchedDir = wd;
    wf->observer = observer;
    wf->next = g_firstFile;
    g_firstFile = wf;
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
way we can support multiple callers subscribing for the same file.
*/
FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer)
{
    CrashIf(!observer);

    if (!file::Exists(path)) {
        delete observer;
        return NULL;
    }

    StartThreadIfNecessary();

    ScopedCritSec cs(&g_threadCritSec);

    // TODO: if the file is on a network drive we should periodically check
    // it ourselves, because ReadDirectoryChangesW()
    // doesn't work in that case
    WatchedFile *wf = NewWatchedFile(path, observer);
    WakeUpWatcherThread();
    return wf;
}

static bool IsWatchedDirReferenced(WatchedDir *wd)
{
    for (WatchedFile *wf = g_firstFile; wf; wf->next) {
        if (wf->watchedDir == wd)
            return true;
    }
    return false;
}

static void RemoveWatchedDirIfNotReferenced(WatchedDir *wd)
{
    if (IsWatchedDirReferenced(wd))
        return;
    WatchedDir **currPtr = &g_firstDir;
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
    DeleteWatchedDir(toRemove);
}

static void RemoveWatchedFile(WatchedFile *wf)
{
    WatchedDir *wd = wf->watchedDir;

    WatchedFile **currPtr = &g_firstFile;
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
    WakeUpWatcherThread();
}

void FileWatcherUnsubscribe(FileWatcherToken token)
{
    if (!token)
        return;
    CrashIf(!g_threadHandle);

    ScopedCritSec cs(&g_threadCritSec);

    RemoveWatchedFile(token);
}
