/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppTools_h
#define AppTools_h

bool IsValidProgramVersion(char *txt);
int CompareVersion(TCHAR *txt1, TCHAR *txt2);

bool IsRunningInPortableMode();
TCHAR *AppGenDataFilename(TCHAR *pFilename);
bool AdjustVariableDriveLetter(TCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

TCHAR *ExtractFilenameFromURL(const TCHAR *url);
bool IsUntrustedFile(const TCHAR *filePath, const TCHAR *fileUrl=NULL);

LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);
void   DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command);

bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureAreaVisibility(RectI& rect);
RectI GetDefaultWindowPos();

#endif
