/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasBeenInstalled();
bool IsRunningInPortableMode();
bool IsDllBuild();

char* AppGenDataFilenameTemp(const char* fileName);

void SetAppDataPath(const char* path);

void AutoDetectInverseSearchCommands(StrVec&);

bool ExtendedEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void EnsureAreaVisibility(Rect& rect);
Rect GetDefaultWindowPos();
void SaveCallstackLogs();

TempStr FormatFileSizeTemp(i64);
TempStr FormatFileSizeNoTransTemp(i64);

bool LaunchFileIfExists(const char* path);

bool IsValidProgramVersion(const char* txt);
int CompareVersion(const char* txt1, const char* txt2);
bool AdjustVariableDriveLetter(char* path);

bool IsUntrustedFile(const char* filePath, const char* fileUrl = nullptr);
void DrawCloseButton(HDC hdc, Rect& r, bool isHover);
