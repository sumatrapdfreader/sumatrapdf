/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileWatcher.h"

#include "FileUtil.h"
#include "ThreadUtil.h"
#include "WinUtil.h"

// TODO: a hack for VS 2011 compilation. 1600 is VS 2010
#if _MSC_VER > 1600
extern "C" {
WINBASEAPI BOOL WINAPI
GetOverlappedResult(_In_ HANDLE hFile, _In_ LPOVERLAPPED lpOverlapped, _Out_ LPDWORD lpNumberOfBytesTransferred, _In_ BOOL bWait);
}
#endif

#define INVALID_TOKEN -1

struct WatchedDir {
    WatchedDir * next;
    const WCHAR *dirPath;
    HANDLE       hDir;
    OVERLAPPED   overlapped;
    // a double buffer where the Windows API ReadDirectoryChanges will store the list
    // of files that have been modified.
    FILE_NOTIFY_INFORMATION buffer[2][512];
    int          currBuffer; // current buffer used (alternate between 0 and 1)
};

struct WatchedFile {
    WatchedFile *           next;
    WatchedDir *            watchedDir;
    const WCHAR *           filePath;
    FileChangeObserver *    observer;
    FileWatcherToken        token;
};

static int              g_currentToken = 0;
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
    WatchedFile *curr = g_firstFile;
    while (curr) {
        if (curr->watchedDir == d) {
            const WCHAR *fileName2 = path::GetBaseName(curr->filePath);

            // is it the file that is being watched?
            if (str::EqI(fileName, fileName2)) {
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

static void NotifyAboutDirChanged(WatchedDir *d)
{
    // Read the asynchronous result of the previous call to ReadDirectory
    DWORD dwNumberbytes;
    GetOverlappedResult(d->hDir, &d->overlapped, &dwNumberbytes, FALSE);

    FILE_NOTIFY_INFORMATION *notify = d->buffer[d->currBuffer];
    // Switch the 2 buffers
    d->currBuffer = (d->currBuffer + 1) % dimof(d->buffer);

    // start a new asynchronous call to ReadDirectory in the alternate buffer
    ReadDirectoryChangesW(
         d->hDir, /* handle to directory */
         &d->buffer[d->currBuffer], /* read results buffer */
         sizeof(d->buffer[d->currBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         //FILE_NOTIFY_CHANGE_CREATION|
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &d->overlapped, /* overlapped buffer */
         NULL); /* completion routine */

    // Note: the ReadDirectoryChangesW API fills the buffer with WCHAR strings.
    for (;;) {
        if (notify->Action == FILE_ACTION_MODIFIED) {
            ScopedMem<WCHAR> fileName(str::DupN(notify->FileName, notify->FileNameLength / sizeof(WCHAR)));
            NotifyAboutFile(d, fileName);
        }

        // step to the next entry if there is one
        DWORD nextOff = notify->NextEntryOffset;
        if (!nextOff)
            break;
        notify = (FILE_NOTIFY_INFORMATION *)((PBYTE)notify + nextOff);
    }
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
    ScopedCritSec cs(&g_threadCritSec);
    g_fileWatcherHandles[0] = g_threadControlHandle;
    int n = 1;
    WatchedDir *curr = g_firstDir;
    while (curr) {
        g_fileWatcherHandles[n] = curr->overlapped.hEvent;
        n++;
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
        }
        // otherwise this is g_threadControlHandle which means
        // that dirs have been added/deleted and we need to
        // rebuild g_fileWatcherHandles
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

static WatchedFile *FindByToken(FileWatcherToken token)
{
    WatchedFile *curr = g_firstFile;
    while (curr) {
        if (curr->token == token)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static void DeleteWatchedDir(WatchedDir *wd)
{
    free((void*)wd->dirPath);
    SafeCloseHandle(wd->hDir);
    free(wd);
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

    wd->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wd->overlapped.hEvent)
        goto Failed;

    // start watching the directory
    wd->currBuffer = 0;
    ReadDirectoryChangesW(
         wd->hDir,
         &wd->buffer[wd->currBuffer], /* read results buffer */
         sizeof(wd->buffer[wd->currBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &wd->overlapped, /* overlapped buffer */
         NULL); /* completion routine */

    wd->next = g_firstDir;
    g_firstDir = wd;

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
    wf->filePath = str::Dup(filePath);
    wf->watchedDir = wd;
    wf->observer = observer;
    wf->token = g_currentToken++;
    wf->next = g_firstFile;
    g_firstFile = wf;
    return wf;
}

static void DeleteWatchedFile(WatchedFile *wf)
{
    free((void*)wf->filePath);
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
        return INVALID_TOKEN;
    }

    StartThreadIfNecessary();

    ScopedCritSec cs(&g_threadCritSec);

    // TODO: if the file is on a network drive we should periodically check
    // it ourselves, because ReadDirectoryChangesW()
    // doesn't work in that case
    WatchedFile *wf = NewWatchedFile(path, observer);
    WakeUpWatcherThread();
    return wf->token;
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
    if (INVALID_TOKEN == token)
        return;
    CrashIf(!g_threadHandle);

    ScopedCritSec cs(&g_threadCritSec);

    WatchedFile *wf = FindByToken(token);
    CrashIf(!wf);
    RemoveWatchedFile(wf);
}

