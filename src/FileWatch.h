/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileWatch_h
#define FileWatch_h

//#define USE_NEW_FW

#ifndef USE_NEW_FW
class FileChangeObserver {
public:
    virtual ~FileChangeObserver() { }
    virtual void OnFileChanged() = 0;
};
#endif

#ifdef USE_NEW_FW
#include "FileWatcher.h"
#else
namespace oldfw {

typedef void *FileWatcherToken;

FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *);
void             FileWatcherUnsubscribe(FileWatcherToken *);
}

using namespace oldfw;

#endif  // !USE_NEW_FW

#endif
