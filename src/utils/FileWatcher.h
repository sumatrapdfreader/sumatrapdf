/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WatchedFile;

WatchedFile* FileWatcherSubscribe(const char* path, const Func0& onFileChangedCb);
void FileWatcherUnsubscribe(WatchedFile* wf);
void FileWatcherWaitForShutdown();
void WatchedFileSetIgnore(WatchedFile* wf, bool ignore);
