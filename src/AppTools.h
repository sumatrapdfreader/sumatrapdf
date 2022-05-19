/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasBeenInstalled();
bool IsRunningInPortableMode();
bool IsDllBuild();

char* AppGenDataFilenameTemp(const char* fileName);

void SetAppDataPath(const char* path);

// bool IsExeAssociatedWithPdfExtension();

char* AutoDetectInverseSearchCommands(HWND);

bool ExtendedEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void EnsureAreaVisibility(Rect& rect);
Rect GetDefaultWindowPos();
void SaveCallstackLogs();

WCHAR* FormatFileSize(i64);
char* FormatFileSizeA(i64);

WCHAR* FormatFileSizeNoTrans(i64);
bool LaunchFileIfExists(const char* path);
