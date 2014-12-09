/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasBeenInstalled();
bool IsRunningInPortableMode();
WCHAR *AppGenDataFilename(const WCHAR *pFilename);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

WCHAR* AutoDetectInverseSearchCommands(HWND hwndCombo=nullptr);

bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureAreaVisibility(RectI& rect);
RectI GetDefaultWindowPos();
void SaveCallstackLogs();
