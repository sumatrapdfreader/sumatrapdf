/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by OpenFromUrlCommon.cpp and each app's OpenFromUrlDialog.cpp ---

TempStr NormalizeDownloadUrlTemp(Str s);
TempStr UrlFileNameTemp(Str url);
TempStr UniqueFilePathTemp(Str dir, Str name);
TempStr DownloadDirTemp();
bool StartOpenFromUrl(MainWindow* win, Str url, Str dir);

// implemented by each app
void OpenDownloadedFile(MainWindow* win, Str path);
void ShowOpenFromUrlDialog(MainWindow* win);
