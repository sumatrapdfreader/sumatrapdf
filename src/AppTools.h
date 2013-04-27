/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppTools_h
#define AppTools_h

bool IsValidProgramVersion(const char *txt);
int CompareVersion(const WCHAR *txt1, const WCHAR *txt2);

bool IsRunningInPortableMode();
WCHAR *AppGenDataFilename(const WCHAR *pFilename);
bool AdjustVariableDriveLetter(WCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

WCHAR *ExtractFilenameFromURL(const WCHAR *url);
bool IsUntrustedFile(const WCHAR *filePath, const WCHAR *fileUrl=NULL);

WCHAR* AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);
void   DDEExecute(LPCWSTR server, LPCWSTR topic, LPCWSTR command);

bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureAreaVisibility(RectI& rect);
RectI GetDefaultWindowPos();
void SaveCallstackLogs();

#endif
