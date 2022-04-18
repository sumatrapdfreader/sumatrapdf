/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define kRegExplorerPdfExt L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define kRegProgId L"ProgId"

#define kRegClassesPdf L"Software\\Classes\\.pdf"

#define kRegWinCurrentVer L"Software\\Microsoft\\Windows\\CurrentVersion"

WCHAR* GetRegPathUninst(const WCHAR* appName);
WCHAR* GetRegClassesApp(const WCHAR* appName);
WCHAR* GetRegClassesApps(const WCHAR* appName);
bool ListAsDefaultProgramWin10(const WCHAR* appName, const WCHAR* exeName, const WCHAR* extensions[]);
bool ListAsDefaultProgramPreWin10(const WCHAR* exeName, const WCHAR* extensions[], HKEY hkey);
