/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileWatch_h
#define FileWatch_h

class FileChangeObserver {
public:
    virtual ~FileChangeObserver() { }
    virtual void OnFileChanged() = 0;
};

typedef struct WatchedFile* FileWatcherToken;

namespace oldfw {

FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer);
void             FileWatcherUnsubscribe(FileWatcherToken token);
}

using namespace oldfw;

#endif
