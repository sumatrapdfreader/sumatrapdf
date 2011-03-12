/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileWatch_h
#define FileWatch_h

#include "BaseUtil.h"

// information concerning a directory being watched
class FileWatcher {
public:
    // Watching file modifications using a loop
    void Init(LPCTSTR filefullpath);
    bool CheckForChanges(DWORD waittime=0);

    // Watching file modification via a thread
    void StartWatchThread();
    bool IsThreadRunning();
    void SynchronousAbort();

    FileWatcher(CallbackFunc *callback) : hDir(NULL), curBuffer(0),
        szFilepath(NULL), hWatchingThread(NULL), pCallback(callback) {
        ZeroMemory(&this->overl, sizeof(this->overl));
        // create the event used to abort the "watching" thread
        hEvtStopWatching = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    ~FileWatcher() {
        SynchronousAbort();
        delete pCallback;
        free(szFilepath);
        CloseHandle(hEvtStopWatching);
    }

private:
    HANDLE  hDir; // handle of the directory to watch
    CallbackFunc *pCallback;// function called when a file change is detected
    TCHAR * szFilepath; // path to the file watched

    FILE_NOTIFY_INFORMATION buffer[2][512];
        // a double buffer where the Windows API ReadDirectory will store the list
        // of files that have been modified.
    int curBuffer; // current buffer used (alternate between 0 and 1)

    bool NotifyChange();
    static DWORD WINAPI WatchingThread(void *param);

public:
    // fields for use by the WathingThread
    OVERLAPPED overl; // object used for asynchronous API calls
    HANDLE hWatchingThread; // handle of the watching thread
    HANDLE hEvtStopWatching; // this event is fired when the watching thread needs to be aborted
};

#endif
