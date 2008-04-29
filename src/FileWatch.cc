// By william blum, 2008
#include "SumatraPDF.h"
#include "FileWatch.h"
#include "file_util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <crtdbg.h>
#include <time.h>

// Get the directory name from a full file path and copy it to pszDir
bool GetDirectory (PCTSTR pszFile, PTSTR pszDir, size_t wMaxSize)
{
    PCTSTR pszBaseName = FilePath_GetBaseName(pszFile);

    if (EINVAL == _tcsncpy_s(pszDir, wMaxSize, pszFile, pszBaseName-pszFile))
        return false;

    // Is the file located in the root directory?
    if (pszDir[pszBaseName-pszFile-2] == ':') {
        // add the backslash at the end
        pszDir[pszBaseName-pszFile-1] = '\\';
        pszDir[pszBaseName-pszFile] = '\0';
    }
    return true;
}

// Abort simultaneously all the watching thread and wait until they are all stopped.
void SimultaneousSyncronousAbort(int nfw, FileWatcher **fw){
    // Preparing to exit the program: ask the children thread to terminate
    HANDLE *hp = new HANDLE[nfw];
    int k = 0;
    for(int i=0; i<nfw;i++) {
        if (fw[i]->hWatchingThread) {
            // send a message to the stop the watching thread
            SetEvent(fw[i]->hEvtStopWatching);
            hp[k++] = fw[i]->hWatchingThread;
        }
    }
    // wait for the two threads to end
    WaitForMultipleObjects(k, hp, TRUE, INFINITE);
    for(int i=0; i<nfw;i++) {
        if (fw[i]->hWatchingThread) {
            CloseHandle(fw[i]->hWatchingThread);
            fw[i]->hWatchingThread = NULL;
        }
    }
    delete hp;
}

bool FileWatcher::IsThreadRunning()
{
    return hWatchingThread && (WaitForSingleObject(hWatchingThread, 0) == WAIT_TIMEOUT);
}

// Ask for the thread to stop and waith until it ends
void FileWatcher::SyncronousAbort()
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
        SyncronousAbort();

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

void FileWatcher::Init(PCTSTR filefullpath)
{
    // if the thread already exists then stop it
    if (IsThreadRunning())
        SyncronousAbort();

    _tcscpy_s(szFilepath, _countof(szFilepath), filefullpath);
    pszFilename = FilePath_GetBaseName(szFilepath);
    GetDirectory(filefullpath, szDir, _countof(szDir));
    
    int res = _tstat(filefullpath, &timestamp);

    callbackparam = NULL;
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
    overl.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
    curBuffer = 0;

    // watch the directory
    DWORD BytesReturned;
    ReadDirectoryChangesW(
         hDir, /* handle to directory */
         &buffer[curBuffer], /* read results buffer */
         sizeof(buffer[curBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         FILE_NOTIFY_CHANGE_CREATION|
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         &BytesReturned, /* bytes returned */
         &overl, /* overlapped buffer */
         NULL); /* completion routine */

}

// Start watching a file for changes
void FileWatcher::StartWatchThread(PCTSTR filefullpath, WATCHCALLBACK cb, LPARAM param)
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
        DWORD dwRet = 0;
        DWORD dwObj = WaitForMultipleObjects(_countof(hp), hp, FALSE, INFINITE ) - WAIT_OBJECT_0;
        _ASSERT( dwObj >= 0 && dwObj <= _countof(hp) );
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
    // Read the asyncronous result of the previous call to ReadDirectory
    DWORD dwNumberbytes;
    GetOverlappedResult(hDir, &overl, &dwNumberbytes, FALSE);

    // Switch the 2 buffers
    curBuffer = 1 - curBuffer;

    // start a new asynchronous call to ReadDirectory in the alternate buffer
    DWORD BytesReturned;
    ReadDirectoryChangesW(
         hDir, /* handle to directory */
         &buffer[curBuffer], /* read results buffer */
         sizeof(buffer[curBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         FILE_NOTIFY_CHANGE_CREATION|
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         &BytesReturned, /* bytes returned */
         &overl, /* overlapped buffer */
         NULL); /* completion routine */

    // Browse the list of FILE_NOTIFY_INFORMATION entries
    FILE_NOTIFY_INFORMATION *pFileNotify;
    pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer[1-curBuffer];
    while (pFileNotify) {
        pFileNotify->FileName[min(pFileNotify->FileNameLength/sizeof(WCHAR), _MAX_FNAME-1)] = 0;

        PTSTR pFilename;
        #ifdef _UNICODE
        pFilename = pFileNotify->FileName;
        #else
        // Convert the filename from unicode string to oem string
        TCHAR oemfilename[_MAX_FNAME];
        wcstombs( oemfilename, pFileNotify->FileName, _MAX_FNAME );
        pFilename = oemfilename;
        #endif

        // is it the file that is being watched?
        if (stricmp(pFilename, pszFilename) == 0) {
            if (pFileNotify->Action != FILE_ACTION_MODIFIED) {
                // file touched but not modified
            }
            else {
                // Check that the timestamp has change (this is necessary because the ReadDirectory API 
                // reports duplicates)
                struct _stat newstamp;

                // compare the old and new time stamps
                if (_tstat(szFilepath, &newstamp) == 0
                    && difftime(newstamp.st_mtime, timestamp.st_mtime) > 0) {
                    timestamp = newstamp;

                    return true; // the file has changed!
                }
                else {
                    // false positive: this file notification entry is a duplicate.
                    ;
                }
            }
        }

        // step to the next entry if there is one
        if (pFileNotify->NextEntryOffset)
            pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset) ;
        else
            pFileNotify = NULL;
    }
    return false;
}