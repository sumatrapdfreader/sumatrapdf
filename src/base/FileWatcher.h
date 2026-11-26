/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WatchedFile;

// Copied when a notification is collected, so the ID survives unsubscribe.
struct FileWatchCallback {
    FileWatchCallback() = default;
    FileWatchCallback(Func0 callback) : fn(callback) {}
    FileWatchCallback(void (*callback)(u64), u64 id) : fnWithId(callback), id(id) {}

    void Call() const {
        if (fnWithId) {
            fnWithId(id);
            return;
        }
        fn.Call();
    }

  private:
    Func0 fn;
    void (*fnWithId)(u64) = nullptr;
    u64 id = 0;
};

void FileWatcherSetSkipPath(Str path);
void FileWatcherInit(void);
WatchedFile* FileWatcherSubscribe(Str path, const FileWatchCallback& onFileChangedCb, bool enableManualCheck = false);
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
