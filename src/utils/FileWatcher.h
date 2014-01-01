/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FileWatcher_h
#define FileWatcher_h

class FileChangeObserver {
public:
    virtual ~FileChangeObserver() { }
    virtual void OnFileChanged() = 0;
};

struct WatchedFile;

WatchedFile *FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer);
void         FileWatcherUnsubscribe(WatchedFile *wf);

#endif
