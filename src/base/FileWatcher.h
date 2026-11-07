/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WatchedFile;

void FileWatcherSetSkipPath(Str path);
void FileWatcherInit(void);
WatchedFile* FileWatcherSubscribe(Str path, const Func0& onFileChangedCb, bool enableManualCheck = false);
void FileWatcherUnsubscribe(WatchedFile* wf);
void FileWatcherWaitForShutdown(void);
void WatchedFileSetIgnore(WatchedFile* wf, bool ignore);

// sets file->ignore under the posix watchers' mutex
template <class T>
void SetIgnoreLocked(T* file, Mutex* mutex, bool ignore) {
    if (!file) {
        return;
    }
    ScopedMutex lock(mutex);
    file->ignore = ignore;
}
