/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define INVALID_TOKEN -1

struct WatchedFile {
    WatchedFile *       next;
    const WCHAR *       filePath;
    FileChangeObserver *observer;
    FileWatcherToken    token;
};

struct WatchedDir {
    WatchedDir * next;
    const WCHAR *dirPath;
    HANDLE       hDir;
    OVERLAPPED   overlapped;
    // a double buffer where the Windows API ReadDirectory will store the list
    // of files that have been modified.
    FILE_NOTIFY_INFORMATION buffer[2][512];
    int          currBuffer; // current buffer used (alternate between 0 and 1)
};

static int g_currentToken = 0;
static HANDLE g_threadControlHandle = 0;
static WatchedDir *firstDir = NULL;
static WatchedFile *firstFile = NULL;

static WatchedFile *NewWatchedFile(const WCHAR *filePath, FileChangeObserver *observer)
{
    WatchedFile *wf = AllocStruct<WatchedFile>();
    wf->filePath = str::Dup(filePath);
    wf->observer = observer;
    wf->token = g_currentToken++;
    return wf;
}

static WatchedDir *FindExistingWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *curr = firstDir;
    while (curr) {
        // TODO: str::EqI ?
        if (str::Eq(dirPath, curr->dirPath))
            return curr;
        curr = curr->next;
    }
}

static void FreeWatchedDir(WatchedDir *wd)
{
    free(wd->dirPath);
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

    return wd;
Failed:
    FreeWatchedDir(wd);
    return NULL;
}

static WatchedDir *GetOrFindDirForFile(const WCHAR *filePath)
{
    ScopedMem<WCHAR> dirPath(path::GetDir(filePath));
    WatchedDir *wd = FindExistingWatchedDir(dirPath);
    if (!wd)
        wd = NewWatchedDir(dirPath);
    return wd;
}

/* Subscribe for notifications about file changes. When a file changes, we'll
call observer->OnFileChanged(). We take ownership of observer object.

Returns a cancellation token that can be used in FileWatcherUnsubscribe(). That
way we can support multiple callers subscribing for the same file.
*/
FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer)
{
    if (!file::Exists(path)) {
        delete observer;
        return INVALID_TOKEN;
    }
    // TODO: if the file is on a network drive we should periodically check
    // it ourselves, because ReadDirectoryChangesW()
    // doesn't work in that case
    WatchedFile *wf = NewWatchedFile(path, observer);

    return wf->token;
}

void FileWatcherUnsubscribe(FileWatcherToken token)
{
    if (INVALID_TOKEN == token)
        return;
    
}

