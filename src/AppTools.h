/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasBeenInstalled();
bool IsRunningInPortableMode();
WCHAR* AppGenDataFilename(const WCHAR* pFilename);
void SetAppDataPath(const WCHAR* path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

WCHAR* AutoDetectInverseSearchCommands(HWND);

bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureAreaVisibility(RectI& rect);
RectI GetDefaultWindowPos();
void SaveCallstackLogs();
WCHAR* PathForFileInAppDataDir(const WCHAR* fileName);
const WCHAR* ExractUnrarDll();
