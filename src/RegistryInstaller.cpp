/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "SumatraConfig.h"
#include "Version.h"
#include "Installer.h"

#include "utils/Log.h"

// All registry manipulation needed for installer / uninstaller

// caller needs to str::Free() the result
static WCHAR* GetInstallDate() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir) {
    logf(L"GetDirSize(%s)\n", dir);
    AutoFreeWstr dirPattern = path::Join(dir, L"*");
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        } else if (!str::Eq(findData.cFileName, L".") && !str::Eq(findData.cFileName, L"..")) {
            AutoFreeWstr subdir = path::Join(dir, findData.cFileName);
            totalSize += GetDirSize(subdir);
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);

    return totalSize;
}

bool WriteUninstallerRegistryInfo(HKEY hkey) {
    logf("WriteUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    bool ok = true;

    AutoFreeWstr installedExePath = GetInstallationFilePath(kExeName);
    AutoFreeWstr installDate = GetInstallDate();
    WCHAR* installDir = GetInstallDirTemp();
    WCHAR* uninstallerPath = installedExePath; // same as
    AutoFreeWstr uninstallCmdLine = str::Format(L"\"%s\" -uninstall", uninstallerPath);

    WCHAR* regPathUninst = GetRegPathUninstTemp(kAppName);
    // path to installed executable (or "$path,0" to force the first icon)
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayIcon", installedExePath);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayName", kAppName);
    // version format: "1.2"
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayVersion", CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsWindowsVistaOrGreater()) {
        WCHAR* key = str::JoinTemp(kAppName, L" ", CURR_VERSION_STR);
        ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayName", key);
    }
    DWORD size = GetDirSize(installDir) / 1024;
    // size of installed directory after copying files
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"EstimatedSize", size);
    // current date as YYYYMMDD
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"InstallDate", installDate);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"InstallLocation", installDir);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"NoModify", 1);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"NoRepair", 1);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"Publisher", TEXT(PUBLISHER_STR));
    // command line for uninstaller
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"UninstallString", uninstallCmdLine);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"URLInfoAbout", L"https://www.sumatrapdfreader.org/");
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"URLUpdateInfo",
                            L"https://www.sumatrapdfreader.org/docs/Version-history.html");
    if (!ok) {
        log("WriteUninstallerRegistryInfo() failed\n");
    }
    return ok;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool ListAsDefaultProgramWin10(HKEY hkey) {
    bool ok = true;

    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    WCHAR* capKey = str::JoinTemp(L"SOFTWARE\\", kAppName, L"\\Capabilities");
    ok &= LoggedWriteRegStr(hkey, L"SOFTWARE\\RegisteredApplications", kAppName, capKey);
    WCHAR* desc = str::JoinTemp(kAppName, L" is a PDF reader.");
    ok &= LoggedWriteRegStr(hkey, capKey, L"ApplicationDescription", desc);
    WCHAR* appLongName = str::JoinTemp(kAppName, L" Reader");
    ok &= LoggedWriteRegStr(hkey, capKey, L"ApplicationName", appLongName);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    WCHAR* keyAssoc = str::JoinTemp(capKey, L"\\FileAssociations");

    auto ext = GetSupportedExts();
    while (ext) {
        WCHAR* extw = ToWstrTemp(ext);
        ok &= LoggedWriteRegStr(hkey, keyAssoc, extw, kExeName);
        seqstrings::Next(ext);
    }
    return ok;
}

bool ListAsDefaultProgramPreWin10(HKEY hkey) {
    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    // TODO: per http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx we shouldn't be
    // using OpenWithList but OpenWithProgIds. Also, it doesn't seem to work on my win7 32bit
    // (HKLM\Software\Classes\.mobi\OpenWithList\SumatraPDF.exe key is present but "Open With"
    // menu item doesn't even exist for .mobi files
    // It's not so easy, though, because if we just set it to SumatraPDF,
    // all GetSupportedExts() will be reported as "PDF Document" by Explorer, so this needs
    // to be more intelligent. We should probably mimic Windows Media Player scheme i.e.
    // set OpenWithProgIds to SumatraPDF.AssocFile.Mobi etc. and create apropriate
    // \SOFTWARE\Classes\CLSID\{GUID}\ProgID etc. entries
    // Also, if Sumatra is the only program handling those docs, our
    // PDF icon will be shown (we need icons and properly configure them)
    bool ok = true;

    WCHAR* openWithVal = str::JoinTemp(L"\\OpenWithList\\", kExeName);
    auto exts = GetSupportedExts();
    while (exts) {
        WCHAR* ext = ToWstrTemp(exts);
        WCHAR* name = str::JoinTemp(L"Software\\Classes\\", ext, openWithVal);
        ok &= CreateRegKey(hkey, name);
        seqstrings::Next(exts);
    }
    return ok;
}

/*
Structure of registry entries for associating Sumatra with PDF files.

The following paths exist under both HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER.
HKCU has precedence over HKLM.

Software\Classes\.pdf default key is name of reg entry describing the app
  handling opening PDF files. In our case it's SumatraPDF
Software\Classes\.pdf\OpenWithProgids
  should contain SumatraPDF so that it's easier for the user to later
  restore SumatraPDF to become the default app through Windows Explorer,
  cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx

Software\Classes\SumatraPDF\DefaultIcon = $exePath,1
  1 means the second icon resource within the executable
Software\Classes\SumatraPDF\shell\open\command = "$exePath" "%1"
  tells how to call sumatra to open PDF file. %1 is replaced by PDF file path

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Progid
  should be SumatraPDF (FoxIt takes it over); only needed for HKEY_CURRENT_USER
  TODO: No other app seems to set this one, and only UserChoice seems to make
        a difference - is this still required for Windows XP?

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Application
  should be SumatraPDF.exe; only needed for HKEY_CURRENT_USER
  Windows XP seems to use this instead of:

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\UserChoice\Progid
  should be SumatraPDF as well (also only needed for HKEY_CURRENT_USER);
  this key is used for remembering a user's choice with Explorer's Open With dialog
  and can't be written to - so we delete it instead!

HKEY_CLASSES_ROOT\.pdf\OpenWithList
  list of all apps that can be used to open PDF files. We don't touch that.

HKEY_CLASSES_ROOT\.pdf default comes from either HKCU\Software\Classes\.pdf or
HKLM\Software\Classes\.pdf (HKCU has priority over HKLM)

Note: When making changes below, please also adjust WriteExtendedFileExtensionInfo(),
UnregisterFromBeingDefaultViewer() and RemoveOwnRegistryKeys() in Installer.cpp.

*/

// TODO: this method no longer valid
#if 0
void DoAssociateExeWithPdfExtension(HKEY hkey) {
    auto exePath = GetExePathTemp();
    if (exePath.empty()) {
        return;
    }

    AutoFreeWstr regClassesApp = str::Join(LR"(Software\Classes\)", kAppName);

    AutoFreeWstr prevHandler;
    // Remember the previous default app for the Uninstaller
    prevHandler.Set(LoggedReadRegStr(hkey, kRegClassesPdf, nullptr));

    bool ok = false;
    if (prevHandler && !str::Eq(prevHandler, kAppName)) {
        LoggedWriteRegStr(hkey, regClassesApp, L"previous.pdf", prevHandler);
    }

    LoggedWriteRegStr(hkey, regClassesApp, nullptr, _TR("PDF Document"));
    AutoFreeWstr icon_path = str::Join(exePath, L",1");
    {
        AutoFreeWstr key = str::Join(regClassesApp, LR"(\DefaultIcon)");
        LoggedWriteRegStr(hkey, key, nullptr, icon_path);
    }

    {
        AutoFreeWstr key = str::Join(regClassesApp, LR"(\shell)");
        LoggedWriteRegStr(hkey, key, nullptr, L"open");
    }

    // "${exePath}" "%1" %*
    AutoFreeWstr cmdPath = str::Format(LR"("%s" "%%1" %%*)", exePath.Get());
    {
        AutoFreeWstr key = str::Join(regClassesApp, LR"(\shell\open\command)");
        ok = LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    }

    // register for printing: "${exePath}" -print-to-default "%1"
    cmdPath.Set(str::Format(LR"("%s" -print-to-default "%%1")", exePath.Get()));
    {
        AutoFreeWstr key = str::Join(regClassesApp, LR"(\shell\print\command)");
        LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    }

    // register for printing to specific printer:
    // "${exePath}" -print-to "%2" "%1"
    cmdPath.Set(str::Format(LR"("%s" -print-to "%%2" "%%1")", exePath.Get()));
    {
        AutoFreeWstr key = str::Join(regClassesApp, LR"(\shell\printto\command)");
        LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    };

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (!ok) {
        return;
    }

    LoggedWriteRegStr(hkey, kRegClassesPdf, nullptr, appName);
    // TODO: also add SumatraPDF to the Open With lists for the other supported extensions?
    LoggedWriteRegStr(hkey, kRegClassesPdf LR"(\OpenWithProgids)", appName, L"");
    if (hkey == HKEY_CURRENT_USER) {
        LoggedWriteRegStr(hkey, kRegExplorerPdfExt, L"Progid", appName);
        CrashIf(hkey == nullptr); // to appease prefast
        LoggedDeleteRegValue(hkey, kRegExplorerPdfExt, L"Application");
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt LR"(\UserChoice)", true);
    }
}
#endif

// TODO: this method no longer valid
#if 0
// verify that all registry entries that need to be set in order to associate
// Sumatra with .pdf files exist and have the right values
bool IsExeAssociatedWithPdfExtension() {
    // this one doesn't have to exist but if it does, it must be kAppName
    const WCHAR* appName = kAppName;

    AutoFreeWstr tmp(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, L"Progid"));
    if (tmp && !str::Eq(tmp, appName)) {
        return false;
    }

    // this one doesn't have to exist but if it does, it must be ${kAppName}.exe
    tmp.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, L"Application"));
    AutoFreeWstr exeName = str::Join(appName, L".exe");
    if (tmp && !str::EqI(tmp, exeName)) {
        return false;
    }

    // this one doesn't have to exist but if it does, it must be kAppName
    tmp.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt LR"(\UserChoice)", L"Progid"));
    if (tmp && !str::Eq(tmp, appName)) {
        return false;
    }

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to kAppName
    tmp.Set(LoggedReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr));
    if (!str::Eq(tmp, appName)) {
        return false;
    }

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    {
        AutoFreeWstr key = str::Join(appName, LR"(\shell)");
        tmp.Set(LoggedReadRegStr(HKEY_CLASSES_ROOT, key, nullptr));
    }
    if (!str::EqI(tmp, L"open")) {
        return false;
    }

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    {
        AutoFreeWstr key = str::Join(appName, LR"(\shell\open\command)");
        tmp.Set(LoggedReadRegStr(HKEY_CLASSES_ROOT, key, nullptr));
    }
    if (!tmp) {
        return false;
    }

    auto exePath = GetExePathTemp().Get();
    if (!exePath || !str::Find(tmp, LR"("%1")")) {
        return false;
    }

    CmdLineArgsIter argList(tmp);
    bool hasPerc1 = false;
    for (int i = 1; i < argList.nArgs; i++) {
        if (str::Eq(argList.args[i], L"%1")) {
            hasPerc1 = true;
        }
    }
    if (!hasPerc1) {
        return false;
    }

    return path::IsSame(exePath, argList.at(0));
}
#endif

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    logf("WriteExtendedFileExtensionInfo('%s')\n", RegKeyNameTemp(hkey));
    bool ok = true;

    AutoFreeWstr exePath = GetInstalledExePath();
    if (HKEY_LOCAL_MACHINE == hkey) {
        WCHAR* key = str::JoinTemp(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", kExeName);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, exePath);
    }
    WCHAR* REG_CLASSES_APPS = GetRegClassesAppsTemp(kAppName);

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    WCHAR* iconPath = str::JoinTemp(exePath, L",1");
    {
        WCHAR* key = str::JoinTemp(REG_CLASSES_APPS, L"\\DefaultIcon");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, iconPath);
    }
    AutoFreeWstr cmdPath = str::Format(L"\"%s\" \"%%1\" %%*", exePath.Get());
    {
        WCHAR* key = str::JoinTemp(REG_CLASSES_APPS, L"\\Shell\\Open\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    }
    AutoFreeWstr printPath = str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath.Get());
    {
        WCHAR* key = str::JoinTemp(REG_CLASSES_APPS, L"\\Shell\\Print\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printPath);
    }
    AutoFreeWstr printToPath = str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath.Get());
    {
        WCHAR* key = str::JoinTemp(REG_CLASSES_APPS, L"\\Shell\\PrintTo\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printToPath);
    }

    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)
    ok &= ListAsDefaultProgramPreWin10(hkey);

    ok &= ListAsDefaultProgramWin10(hkey);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= LoggedWriteRegStr(hkey, kRegClassesPdf, L"Content Type", L"application/pdf");
    const WCHAR* key = L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf";
    ok &= LoggedWriteRegStr(hkey, key, L"Extension", L".pdf");

    if (!ok) {
        log("WriteExtendedFileExtensionInfo() failed\n");
    }

    return ok;
}

bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    logf("RemoveUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    WCHAR* regPathUninst = GetRegPathUninstTemp(kAppName);
    bool ok1 = LoggedDeleteRegKey(hkey, regPathUninst);
    // legacy, this key was added by installers up to version 1.8
    WCHAR* key = str::JoinTemp(L"Software\\", kAppName);
    bool ok2 = LoggedDeleteRegKey(hkey, key);
    return ok1 && ok2;
}

static const WCHAR* GetRegClassesAppTemp(const WCHAR* appName) {
    return str::JoinTemp(L"Software\\Classes\\", appName);
}

// TODO: this method no longer works
#if 0
/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    logf("UnregisterFromBeingDefaultViewer()\n");
    AutoFreeWstr curr = LoggedReadRegStr(hkey, kRegClassesPdf, nullptr);
    const WCHAR* regClassesApp = GetRegClassesAppTemp(kAppName);
    AutoFreeWstr prev = LoggedReadRegStr(hkey, regClassesApp, L"previous.pdf");
    if (!curr || !str::Eq(curr, kAppName)) {
        // not the default, do nothing
    } else if (prev) {
        LoggedWriteRegStr(hkey, kRegClassesPdf, nullptr, prev);
    } else {
#pragma warning(push)
#pragma warning(disable : 6387) // silence /analyze: '_Param_(3)' could be '0':  this does not adhere to the
                                // specification for the function 'SHDeleteValueW'
        LoggedDeleteRegValue(hkey, kRegClassesPdf, nullptr);
#pragma warning(pop)
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    AutoFreeWstr buf = LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegProgId);
    if (str::Eq(buf, kAppName)) {
        LONG res = SHDeleteValueW(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegProgId);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    const WCHAR* kRegApplication = L"Application";
    buf.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegApplication));
    if (str::EqI(buf, kExeName)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegApplication);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    buf.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt L"\\UserChoice", kRegProgId));
    if (str::Eq(buf, kAppName)) {
        LoggedDeleteRegKey(HKEY_CURRENT_USER, kRegExplorerPdfExt L"\\UserChoice", true);
    }
}
#endif

// delete registry key but only if it's empty
static bool DeleteEmptyRegKey(HKEY root, const WCHAR* keyName) {
    HKEY hkey;
    LSTATUS status = RegOpenKeyExW(root, keyName, 0, KEY_READ, &hkey);
    if (status != ERROR_SUCCESS) {
        return true;
    }

    DWORD subkeys, values;
    bool isEmpty = false;
    status = RegQueryInfoKeyW(hkey, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr,
                              nullptr, nullptr);
    if (status == ERROR_SUCCESS) {
        isEmpty = 0 == subkeys && 0 == values;
    }
    RegCloseKey(hkey);
    if (!isEmpty) {
        return isEmpty;
    }

    LoggedDeleteRegKey(root, keyName);
    return isEmpty;
}

void RemoveOwnRegistryKeys(HKEY hkey) {
    logf("RemoveOwnRegistryKeys(%s)\n", RegKeyNameTemp(hkey));
    // UnregisterFromBeingDefaultViewer(hkey);
    const WCHAR* regClassApp = GetRegClassesAppTemp(kAppName);
    LoggedDeleteRegKey(hkey, regClassApp);
    WCHAR* regClassApps = GetRegClassesAppsTemp(kAppName);
    LoggedDeleteRegKey(hkey, regClassApps);
    {
        WCHAR* key = str::JoinTemp(kRegClassesPdf, L"\\OpenWithProgids");
        LoggedDeleteRegValue(hkey, key, kAppName);
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        WCHAR* key = str::JoinTemp(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", kExeName);
        LoggedDeleteRegKey(hkey, key);
    }

    SeqStrings exts = GetSupportedExts();
    WCHAR* openWithVal = str::JoinTemp(L"\\OpenWithList\\", kExeName);
    while (exts) {
        WCHAR* ext = ToWstrTemp(exts);
        WCHAR* keyname = str::JoinTemp(L"Software\\Classes\\", ext, L"\\OpenWithProgids");
        LoggedDeleteRegValue(hkey, keyname, kAppName);
        DeleteEmptyRegKey(hkey, keyname);

        keyname = str::JoinTemp(L"Software\\Classes\\", ext, openWithVal);
        if (!LoggedDeleteRegKey(hkey, keyname)) {
            continue;
        }
        // remove empty keys that the installer might have created
        WCHAR* p = str::FindCharLast(keyname, '\\');
        *p = 0;
        if (!DeleteEmptyRegKey(hkey, keyname)) {
            continue;
        }
        p = str::FindCharLast(keyname, '\\');
        *p = 0;
        DeleteEmptyRegKey(hkey, keyname);

        seqstrings::Next(exts);
    }

    // delete keys written in ListAsDefaultProgramWin10()
    LoggedDeleteRegValue(hkey, L"SOFTWARE\\RegisteredApplications", kAppName);
    AutoFreeWstr keyName = str::Format(L"SOFTWARE\\%s\\Capabilities", kAppName);
    LoggedDeleteRegKey(hkey, keyName);
}

//------------- pdf preview
