/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define REG_EXPLORER_PDF_EXT L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define PROG_ID L"ProgId"
#define APPLICATION L"Application"

#ifndef _WIN64
#define REG_PATH_PLUGIN L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin"
#else
#define REG_PATH_PLUGIN L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin_x64"
#endif

#define REG_CLASSES_PDF L"Software\\Classes\\.pdf"

#define REG_WIN_CURR L"Software\\Microsoft\\Windows\\CurrentVersion"

WCHAR* getRegPathUninst(const WCHAR* appName);
WCHAR* getRegClassesApp(const WCHAR* appName);
WCHAR* getRegClassesApps(const WCHAR* appName);
bool ListAsDefaultProgramWin10(const WCHAR* appName, const WCHAR* exeName, const WCHAR* extensions[]);
bool ListAsDefaultProgramPreWin10(const WCHAR* exeName, const WCHAR* extensions[], HKEY hkey);
