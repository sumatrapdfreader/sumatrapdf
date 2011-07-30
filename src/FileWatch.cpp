/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FileWatch.h"
#include "StrUtil.h"
#include "FileUtil.h"

bool FileWatcher::IsThreadRunning()
{
    return hWatchingThread && (WaitForSingleObject(hWatchingThread, 0) == WAIT_TIMEOUT);
}

// Ask for the thread to stop and waith until it ends
void FileWatcher::SynchronousAbort()
{
    SetEvent(hEvtStopWatching);
    if (hWatchingThread) {
        WaitForSingleObject(hWatchingThread, INFINITE);
        CloseHandle(hWatchingThread);
        hWatchingThread = NULL;
    }

    CloseHandle(overl.hEvent); 
    overl.hEvent = NULL;
    CloseHandle(hDir);
    hDir = NULL;
}

// Start watching a file for changes
void FileWatcher::StartWatchThread()
{
    // if the thread already exists then stop it
    if (IsThreadRunning())
        SynchronousAbort();

    assert(hDir);
    if (!hDir)
        return;

    // reset the hEvtStopWatching event so that it can be set if
    // some thread requires the watching thread to stop
    ResetEvent(hEvtStopWatching);

    DWORD watchingthreadID;
    hWatchingThread = CreateThread(NULL, 0, WatchingThread, this, 0, &watchingthreadID);
}

void FileWatcher::Init(LPCTSTR filefullpath)
{
    // if the thread already exists then stop it
    if (IsThreadRunning())
        SynchronousAbort();

    free(szFilepath);
    szFilepath = str::Dup(filefullpath);
    TCHAR *dirPath = path::GetDir(szFilepath);

    hDir = CreateFile(
        dirPath, // pointer to the directory containing the tex files
        FILE_LIST_DIRECTORY,                // access (read-write) mode
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  // share mode
        NULL, // security descriptor
        OPEN_EXISTING, // how to create
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED , // file attributes
        NULL); // file with attributes to copy 
    free(dirPath);

    ZeroMemory(&overl, sizeof(overl));
    ZeroMemory(buffer, sizeof(buffer));
    overl.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // watch the directory
    ReadDirectoryChangesW(
         hDir, /* handle to directory */
         &buffer[curBuffer], /* read results buffer */
         sizeof(buffer[curBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         //FILE_NOTIFY_CHANGE_CREATION|
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &overl, /* overlapped buffer */
         NULL); /* completion routine */
}

// Thread responsible of watching the directory containg the file to be watched for modifications
DWORD WINAPI FileWatcher::WatchingThread(void *param)
{
    FileWatcher *fw = (FileWatcher *)param;

    HANDLE hp[2] = { fw->hEvtStopWatching, fw->overl.hEvent };
    for (;;) {
        DWORD dwObj = WaitForMultipleObjects(dimof(hp), hp, FALSE, INFINITE);
        if (dwObj == WAIT_OBJECT_0) // the user asked to quit the program
            break;
        if (dwObj != WAIT_OBJECT_0 + 1) {
            // BUG!
            assert(0);
            break;
        }
        fw->NotifyChange();
    }

    return 0;
}

// Call ReadDirectoryChangesW to check if the file has changed since the last call.
bool FileWatcher::CheckForChanges(DWORD waittime)
{
    if (!overl.hEvent)
        return false;

    DWORD dwObj = WaitForSingleObject(overl.hEvent, waittime);
    if (dwObj != WAIT_OBJECT_0)
        return false;

    return NotifyChange();
}

// Call the ReadDirectory API and determine if the file being watched has been modified since the last call. 
// Returns true if it is the case.
bool FileWatcher::NotifyChange()
{
    // Read the asynchronous result of the previous call to ReadDirectory
    DWORD dwNumberbytes;
    GetOverlappedResult(hDir, &overl, &dwNumberbytes, FALSE);

    // Browse the list of FILE_NOTIFY_INFORMATION entries
    FILE_NOTIFY_INFORMATION *pFileNotify = buffer[curBuffer];
    // Switch the 2 buffers
    curBuffer = (curBuffer + 1) % dimof(buffer);

    // start a new asynchronous call to ReadDirectory in the alternate buffer
    ReadDirectoryChangesW(
         hDir, /* handle to directory */
         &buffer[curBuffer], /* read results buffer */
         sizeof(buffer[curBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         //FILE_NOTIFY_CHANGE_CREATION|
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &overl, /* overlapped buffer */
         NULL); /* completion routine */

    // Note: the ReadDirectoryChangesW API fills the buffer with WCHAR strings.
    for (;;) {
        ScopedMem<WCHAR> filenameW(str::DupN(pFileNotify->FileName, pFileNotify->FileNameLength / sizeof(WCHAR)));
        ScopedMem<TCHAR> ptNotifyFilename(str::conv::FromWStr(filenameW));
        bool isWatchedFile = str::EqI(ptNotifyFilename, path::GetBaseName(szFilepath));

        // is it the file that is being watched?
        if (isWatchedFile && pFileNotify->Action == FILE_ACTION_MODIFIED) {
            // NOTE: It is not recommended to check whether the timestamp has changed
            // because the time granularity is so big that this can cause genuine
            // file notifications to be ignored. (This happens for instance for
            // PDF files produced by pdftex from small.tex document)
            DBG_OUT("FileWatch: change detected in %s\n", szFilepath);
            if (pCallback)
                pCallback->Callback();
            return true;
        }

        // step to the next entry if there is one
        if (!pFileNotify->NextEntryOffset)
            return false;
        pFileNotify = (FILE_NOTIFY_INFORMATION *)((PBYTE)pFileNotify + pFileNotify->NextEntryOffset);
    }
}
