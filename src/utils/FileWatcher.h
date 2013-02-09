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

typedef int FileWatcherToken;

FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *);
void             FileWatcherUnsubscribe(FileWatcherToken *);

#endif

