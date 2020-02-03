/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Winutil.h"

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
WCHAR* getRegPathUninst(const WCHAR* appName) {
    return str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", appName);
}

WCHAR* getRegClassesApp(const WCHAR* appName) {
    return str::Join(L"Software\\Classes\\", appName);
}

WCHAR* getRegClassesApps(const WCHAR* appName) {
    return str::Join(L"Software\\Classes\\Applications\\", appName, L".exe");
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool WriteWin10Registry(const WCHAR* appName, const WCHAR* exeName, const WCHAR* extensions[], HKEY hkey) {
    bool ok = true;

    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    AutoFreeWstr capKey = str::Join(L"SOFTWARE\\", appName, L"\\Capabilities");
    ok &= WriteRegStr(hkey, L"SOFTWARE\\RegisteredApplications", appName, capKey);
    AutoFreeWstr desc = str::Join(appName, L" is a PDF reader.");
    ok &= WriteRegStr(hkey, capKey, L"ApplicationDescription", desc);
    AutoFreeWstr appLongName = str::Join(appName, L" Reader");
    ok &= WriteRegStr(hkey, capKey, L"ApplicationName", appLongName);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    AutoFreeWstr keyAssoc = str::Join(capKey, L"\\FileAssociations");

    for (int i = 0; nullptr != extensions[i]; i++) {
        const WCHAR* ext = extensions[i];
        ok &= WriteRegStr(hkey, keyAssoc, ext, exeName);
    }
    return ok;
}

bool ListAsDefaultProgramWin10(const WCHAR* appName, const WCHAR* exeName, const WCHAR* extensions[]) {
    bool ok = WriteWin10Registry(appName, exeName, extensions, HKEY_LOCAL_MACHINE);
    if (ok) {
        return true;
    }
    return WriteWin10Registry(appName, exeName, extensions, HKEY_CURRENT_USER);
}

bool ListAsDefaultProgramPreWin10(const WCHAR* exeName, const WCHAR* extensions[], HKEY hkey) {
    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    // TODO: per http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx we shouldn't be
    // using OpenWithList but OpenWithProgIds. Also, it doesn't seem to work on my win7 32bit
    // (HKLM\Software\Classes\.mobi\OpenWithList\SumatraPDF.exe key is present but "Open With"
    // menu item doesn't even exist for .mobi files
    // It's not so easy, though, because if we just set it to SumatraPDF,
    // all getSupportedExts() will be reported as "PDF Document" by Explorer, so this needs
    // to be more intelligent. We should probably mimic Windows Media Player scheme i.e.
    // set OpenWithProgIds to SumatraPDF.AssocFile.Mobi etc. and create apropriate
    // \SOFTWARE\Classes\CLSID\{GUID}\ProgID etc. entries
    // Also, if Sumatra is the only program handling those docs, our
    // PDF icon will be shown (we need icons and properly configure them)
    bool ok = true;
    AutoFreeWstr openWithVal = str::Join(L"\\OpenWithList\\", exeName);
    for (int i = 0; nullptr != extensions[i]; i++) {
        const WCHAR* ext = extensions[i];
        AutoFreeWstr name = str::Join(L"Software\\Classes\\", ext, openWithVal);
        ok &= CreateRegKey(hkey, name.get());
    }
    return ok;
}
