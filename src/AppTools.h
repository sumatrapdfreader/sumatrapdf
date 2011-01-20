/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef APP_TOOLS_H_
#define APP_TOOLS_H_

bool ValidProgramVersion(char *txt);
int CompareVersion(TCHAR *txt1, TCHAR *txt2);
const char *GuessLanguage();

TCHAR *ExePathGet();
bool IsRunningInPortableMode();
TCHAR *AppGenDataFilename(TCHAR *pFilename);
void AdjustRemovableDriveLetter(TCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

bool GetAcrobatPath(TCHAR *buffer=NULL, int bufSize=0);
LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize);
void Win32_Font_Delete(HFONT font);

#endif
