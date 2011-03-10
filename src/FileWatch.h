/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileWatch_h
#define FileWatch_h

#include "BaseUtil.h"

typedef void (__cdecl *WATCHCALLBACK) (PCTSTR filename, LPARAM param);

// information concerning a directory being watched
class FileWatcher {
public:
    // Watching file modifications using a loop
    void Init(LPCTSTR filefullpath);
    bool HasChanged(DWORD waittime = 0);
    void Clean();

    // Watching file modification via a thread
    void StartWatchThread(LPCTSTR filefullpath, WATCHCALLBACK cb, LPARAM param);
    bool IsThreadRunning();
    void SynchronousAbort();

    LPCTSTR filepath() { return szFilepath; }

    FileWatcher() {
        hDir = NULL;
        ZeroMemory(&this->overl, sizeof(this->overl));
        curBuffer = 0;
        pszFilename = NULL;
        hWatchingThread = NULL;
        hEvtStopWatching = NULL;
        // create the event used to abort the "watching" thread
        hEvtStopWatching = CreateEvent(NULL,TRUE,FALSE,NULL);
        pCallback = NULL;
        callbackparam = 0;
        szFilepath[0]='0';
    }

    ~FileWatcher() {
        if (IsThreadRunning())
            SynchronousAbort();
        else
            Clean();

        CloseHandle(hEvtStopWatching);
    }

private:
    bool ReadDir();
    
    static void WINAPI WatchingThread( void *param );
    void RestartThread();

public:
    HANDLE  hDir; // handle of the directory to watch
    TCHAR   szFilepath[MAX_PATH]; // path to the file watched
    const   TCHAR * pszFilename; // pointer in szFilepath to the file part of the path
    TCHAR   szDir[MAX_PATH]; // path to the directory
    OVERLAPPED overl; // object used for asynchronous API calls
    BYTE buffer [2][512*sizeof(FILE_NOTIFY_INFORMATION )]; 
        // a double buffer where the Windows API ReadDirectory will store the list
        // of files that have been modified.
    int curBuffer; // current buffer used (alternate between 0 and 1)
    
    HANDLE hWatchingThread; // handle of the watching thread
    
    HANDLE hEvtStopWatching; // this event is fired when the watching thread needs to be aborted

    WATCHCALLBACK pCallback;// function called when a file change is detected
    LPARAM callbackparam;   // parameter to pass to the callback function

    struct _stat timestamp; // last modification time stamp of the file
};

#endif
