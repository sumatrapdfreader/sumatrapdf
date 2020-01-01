/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WatchedFile;

WatchedFile* FileWatcherSubscribe(const WCHAR* path, const std::function<void()>& onFileChangedCb);
void FileWatcherUnsubscribe(WatchedFile* wf);
void FileWatcherWaitForShutdown();
