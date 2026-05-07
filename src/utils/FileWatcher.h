/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WatchedFile;

void FileWatcherInit(void);
WatchedFile* FileWatcherSubscribe(const char* path, const Func0& onFileChangedCb, bool enableManualCheck = false);
void FileWatcherUnsubscribe(WatchedFile* wf);
void FileWatcherWaitForShutdown(void);
void WatchedFileSetIgnore(WatchedFile* wf, bool ignore);
