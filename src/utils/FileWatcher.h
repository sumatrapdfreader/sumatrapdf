/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FileWatcher_h
#define FileWatcher_h

/* Experimental: a different take on file watching API */

class FileChangeObserver {
public:
    virtual ~FileChangeObserver() { }
    virtual void OnFileChanged() = 0;
};

struct WatchedFile;
typedef WatchedFile* FileWatcherToken;

FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer);
void             FileWatcherUnsubscribe(FileWatcherToken token);

#endif
