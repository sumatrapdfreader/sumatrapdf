// By william blum, 2008
#include "SumatraPDF.h"
#include "FileWatch.h"
#include "tstr_util.h"
#include "file_util.h"

// Get the directory name from a full file path and copy it to pszDir
bool GetDirectory(LPCTSTR pszFile, PTSTR pszDir, size_t cchDir)
{
    LPCTSTR pszBaseName = FilePath_GetBaseName(pszFile);

    if (0 == pszDir || 0 == pszFile) {
        return false;
    }
    if (!tstr_copyn(pszDir, cchDir, pszFile, pszBaseName-pszFile)) {
        return false;
    }

    // Is the file located in the root directory?
    if (pszDir[pszBaseName-pszFile-2] == ':') {
        // add the backslash at the end
        pszDir[pszBaseName-pszFile-1] = '\\';
        pszDir[pszBaseName-pszFile] = '\0';
    }
    return true;
}

// Abort simultaneously all the watching thread and wait until they are all stopped.
void SimultaneousSynchronousAbort(int nfw, FileWatcher **fw){
    // Preparing to exit the program: ask the children thread to terminate
    HANDLE *hp = new HANDLE[nfw];
    int k = 0;
    for (int i=0; i<nfw;i++) {
        if (fw[i]->hWatchingThread) {
            // send a message to the stop the watching thread
            SetEvent(fw[i]->hEvtStopWatching);
            hp[k++] = fw[i]->hWatchingThread;
        }
    }
    // wait for the two threads to end
    WaitForMultipleObjects(k, hp, TRUE, INFINITE);
    for (int i=0; i<nfw;i++) {
        if (fw[i]->hWatchingThread) {
            CloseHandle(fw[i]->hWatchingThread);
            fw[i]->hWatchingThread = NULL;
        }
    }
    delete [] hp;
}

bool FileWatcher::IsThreadRunning()
{
    return hWatchingThread && (WaitForSingleObject(hWatchingThread, 0) == WAIT_TIMEOUT);
}

// Ask for the thread to stop and waith until it ends
void FileWatcher::SynchronousAbort()
{
    SetEvent(hEvtStopWatching);
    if (hWatchingThread)
    {
        WaitForSingleObject(hWatchingThread, INFINITE);
        CloseHandle(hWatchingThread);
        hWatchingThread = NULL;
    }
}

void FileWatcher::RestartThread()
{
    // if the thread already exists then stop it
    if (IsThreadRunning())
        SynchronousAbort();

    DWORD watchingthreadID;

    // reset the hEvtStopWatching event so that it can be set if
    // some thread requires the watching thread to stop
    ResetEvent(hEvtStopWatching);

    hWatchingThread = CreateThread( NULL, 0,
        (LPTHREAD_START_ROUTINE) WatchingThread,
        this,
        0,
        &watchingthreadID);
}

void FileWatcher::Clean()
{
    if (overl.hEvent) {
        CloseHandle(overl.hEvent); 
        overl.hEvent = NULL;
    }
    if (hDir) {
        CloseHandle(hDir);
        hDir = NULL;
    }
}

void FileWatcher::Init(LPCTSTR filefullpath)
{
    // if the thread already exists then stop it
    if (IsThreadRunning())
        SynchronousAbort();

    tstr_copy(szFilepath, dimof(szFilepath), filefullpath);
    pszFilename = FilePath_GetBaseName(szFilepath);
    GetDirectory(filefullpath, szDir, dimof(szDir));
    
    _tstat(filefullpath, &timestamp);

    callbackparam = 0;
    pCallback = NULL;

    hDir = CreateFile(
        szDir, // pointer to the directory containing the tex files
        FILE_LIST_DIRECTORY,                // access (read-write) mode
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  // share mode
        NULL, // security descriptor
        OPEN_EXISTING, // how to create
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED , // file attributes
        NULL // file with attributes to copy 
      );

    memset(&overl, 0, sizeof(overl));
    memset(buffer, 0, sizeof(buffer));
    overl.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
    curBuffer = 0;

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

// Start watching a file for changes
void FileWatcher::StartWatchThread(LPCTSTR filefullpath, WATCHCALLBACK cb, LPARAM param)
{
    Init(filefullpath);
   
    callbackparam = param;
    pCallback = cb;

    RestartThread();
}

// Thread responsible of watching the directory containg the file to be watched for modifications
void WINAPI FileWatcher::WatchingThread( void *param )
{
    FileWatcher *fw = (FileWatcher *)param;

    if (!fw || fw->hDir == NULL) // if no directory to watch then leave
        return;
 
    // Main loop
    HANDLE hp[2] = { fw->hEvtStopWatching, fw->overl.hEvent};
    while (1) {
        DWORD dwObj = WaitForMultipleObjects(dimof(hp), hp, FALSE, INFINITE ) - WAIT_OBJECT_0;
        assert( dwObj >= 0 && dwObj <= dimof(hp) );
        if (dwObj == 0) { // the user asked to quit the program
            break;
        } else if (dwObj == 1) {
        } else {
            // BUG!
            break;
        }
        if (fw->ReadDir() && fw->pCallback)
            fw->pCallback(fw->szFilepath, fw->callbackparam);
    }

    fw->Clean();
}

// Call ReadDirectoryChangesW to check if the file has changed since the last call.
bool FileWatcher::HasChanged(DWORD waittime)
{
    if (overl.hEvent == NULL)
        return false;

    DWORD dwObj = WaitForSingleObject(overl.hEvent, waittime);
    if (dwObj == WAIT_OBJECT_0) {
        return ReadDir();
    }
    return false;
}

// Call the ReadDirectory API and determine if the file being watched has been modified since the last call. 
// Returns true if it is the case.
bool FileWatcher::ReadDir()
{
    // Read the asynchronous result of the previous call to ReadDirectory
    DWORD dwNumberbytes;
    GetOverlappedResult(hDir, &overl, &dwNumberbytes, FALSE);

    // Switch the 2 buffers
    curBuffer = 1 - curBuffer;

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

    // Browse the list of FILE_NOTIFY_INFORMATION entries
    FILE_NOTIFY_INFORMATION *pFileNotify;
    pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer[1-curBuffer];
    while (pFileNotify) {
        // Note: the ReadDirectoryChangesW API fills the buffer with WCHAR strings.
        pFileNotify->FileName[min(pFileNotify->FileNameLength/sizeof(WCHAR), _MAX_FNAME-1)] = 0;
        TCHAR *ptNotifyFilename = wstr_to_tstr(pFileNotify->FileName);
        BOOL isWatchedFile = tstr_ieq(ptNotifyFilename, pszFilename);
        free(ptNotifyFilename);

        // is it the file that is being watched?
        if (isWatchedFile) {
            // file modified?
            if (pFileNotify->Action == FILE_ACTION_MODIFIED) {

// NOTE: It is not recommanded to check that the timestamp has changed because the time granularity 
// is so big that this can cause genuine file notifications to be ignored.
// (This happens for instance for PDF files produced by pdftex from small.tex document.)
//
#ifdef   CHECK_TIMESTAMP_ON_FILENOTIFICATION
                // Check if the timestamp has changed to prevent spurious notifications to be reported.

                // compare the old and new time stamps
                struct _stat newstamp;
                if (_tstat(szFilepath, &newstamp) == 0
                    && difftime(newstamp.st_mtime, timestamp.st_mtime) > 0
                    ) {
                    DBG_OUT_T("FileWatch:change notification in %s\n", pszFilename);

                    // Check that the file has not already been reopened for writing.
                    // We try to open the file with write access and no write-sharing 
                    // so that if the file is opened for writing then the call fails.
                    // WARNING: the following code locks the file for a short moment. This may cause 
                    // problems if another process tries to write to the file at the same time.
                    HANDLE f = CreateFile(filepath(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    DWORD dw = GetLastError();
                    if (f == INVALID_HANDLE_VALUE && (dw==ERROR_SHARING_VIOLATION || dw==ERROR_LOCK_VIOLATION))
                        return false;

                    if (f != INVALID_HANDLE_VALUE)
                        CloseHandle(f);
                    
                    // reread the time stamp
                    //_tstat(szFilepath, &timestamp);
                    timestamp = newstamp;

                    return true; // the file has changed!
                }
                else {
                    // false positive: the time stamp has not changed
                    DBG_OUT_T("FileWatch:spurious change notification in %s\n", pszFilename);
                }
#else 
                ////
                // We do not check for timestamp change: all notifications are reported.
                //
                DBG_OUT_T("FileWatch:change detected in %s\n", pszFilename);
                return true; // report the notification.
#endif
            }
            //else {} // file touched but not modified.
            
        }

        // step to the next entry if there is one
        if (pFileNotify->NextEntryOffset)
            pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset) ;
        else
            pFileNotify = NULL;
    }
    return false;
}
