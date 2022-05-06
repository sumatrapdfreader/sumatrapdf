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

// notifies Shell that file associations changed.
// Invalidates the icon and thumbnail cache.
// https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify
static void ShellNotifyAssociationsChanged() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// caller needs to str::Free() the result
static WCHAR* GetInstallDate() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir, bool recur) {
    logf(L"GetDirSize(%s)\n", dir);
    AutoFreeWstr dirPattern = path::Join(dir, L"*");
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD totalSize = 0;
    do {
        DWORD fattr = findData.dwFileAttributes;
        bool isDir = fattr & FILE_ATTRIBUTE_DIRECTORY;
        WCHAR* name = findData.cFileName;
        if (isDir) {
            if (recur && !str::Eq(name, L".") && !str::Eq(name, L"..")) {
                WCHAR* subdir = path::JoinTemp(dir, name);
                totalSize += GetDirSize(subdir, recur);
            }
        } else {
            bool isNormal = fattr & FILE_ATTRIBUTE_NORMAL;
            if (isNormal) {
                totalSize += findData.nFileSizeLow;
            }
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
    // uninstaller is the same executable with a different flag
    WCHAR* uninstallerPath = installedExePath;
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
    // non-recursive because we don't want to count space used for thumbnails
    // which is in installDir for local install
    DWORD size = GetDirSize(installDir, false) / 1024;
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

// clang-format off
// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
static SeqStrings gSupportedExts = 
    ".pdf\0.xps\0.oxps\0.cbz\0.cbr\0.cb7\0.cbt\0" \
    ".djvu\0.chm\0.mobi\0.epub\0.azw\0.azw3\0.azw4\0" \
    ".fb2\0.fb2z\0.prc\0.tif\0.tiff\0.jp2\0.png\0" \
    ".jpg\0.jpeg\0.tga\0.gif\0";
// clang-format on

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool RegisterForDefaultPrograms(HKEY hkey) {
    bool ok = true;

    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    WCHAR* appCapabilityPath = str::JoinTemp(L"SOFTWARE\\", kAppName, L"\\Capabilities");

    const WCHAR* desc = L"SumatraPDF is a PDF reader.";
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, L"ApplicationDescription", desc);
    const WCHAR* appLongName = L"SumatraPDF Reader";
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, L"ApplicationName", appLongName);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    WCHAR* keyAssoc = str::JoinTemp(appCapabilityPath, L"\\FileAssociations");

    auto ext = gSupportedExts;
    while (ext) {
        WCHAR* extw = ToWstrTemp(ext);
        ok &= LoggedWriteRegStr(hkey, keyAssoc, extw, kAppName);
        seqstrings::Next(ext);
    }

    ok &= LoggedWriteRegStr(hkey, L"SOFTWARE\\RegisteredApplications", kAppName, appCapabilityPath);
    return ok;
}

/*
ShCtx is either HKCU or HKLM

For each extension, create a progid:
ShCtx\Software\Classes\SumatraPDF.${ext}
  Application
    ApplicationCompany = Krzysztof Kowalczyk
    ApplicationName = SumatraPDF
  DefaultIcon
    (Default) = ${SumatraExePath},${OptIconIndex}
  shell\open
    AppUserModelID = ???
    Icon = ${SumatraExePath}
  shell\open\command
    (Default) = "${SumatraExePath}" "%1"
  shell\print\command

Then in:
ShCtx\Software\Classes\${ext}\OpenWithProgids
  SumatraPDF.${ext} = "" (empty REG_SZ value)
*/
bool RegisterForOpenWith(HKEY hkey) {
    WCHAR* exePath = GetInstalledExePathTemp();
    WCHAR* cmdOpen = str::JoinTemp(L"\"", exePath, L"\" \"%1\"");
    WCHAR* cmdPrint = str::JoinTemp(L"\"", exePath, L"\" -print-to-default \"%1\"");
    WCHAR* cmdPrintTo = str::JoinTemp(L"\"", exePath, L"\" -print-to \"%2\" \"%1\"");
    WCHAR* key;
    auto exts = gSupportedExts;
    bool ok = true;
    while (exts) {
        WCHAR* ext = ToWstrTemp(exts);
        WCHAR* progIDName = str::JoinTemp(kAppName, ext);
        WCHAR* progIDKey = str::JoinTemp(L"Software\\Classes\\", progIDName);
        // ok &= CreateRegKey(hkey, progIDKey);

        // .pdf => "PDF file"
        WCHAR* extUC = str::ToUpperInPlace(str::DupTemp(ext + 1));
        WCHAR* desc = str::JoinTemp(extUC, L" File");
        ok &= LoggedWriteRegStr(hkey, progIDKey, nullptr, desc);
        // ok &= LoggedWriteRegStr(hkey, progIDKey, L"AppUserModelID", L"SumatraPDF"); // ???

        // Per https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-extracticona
        // ",${n}" => n is 0-based index of the icon
        // ",-${n}" => n is icon with resource id
        WCHAR* iconPath = str::JoinTemp(exePath, L"");
        if (str::Eq(ext, L".epub")) {
            iconPath = str::JoinTemp(exePath, L",-3");
        }
        if (str::Eq(ext, L".cbr") || str::Eq(ext, L".cbz") || str::Eq(ext, L".cbt") || str::Eq(ext, L".cb7")) {
            iconPath = str::JoinTemp(exePath, L",-4");
        }

        key = str::JoinTemp(progIDKey, L"\\Application");
        ok &= LoggedWriteRegStr(hkey, key, L"ApplicationCompany", L"Krzysztof Kowalczyk");
        ok &= LoggedWriteRegStr(hkey, key, L"ApplicationName", kAppName);

        key = str::JoinTemp(progIDKey, L"\\DefaultIcon");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, iconPath);

        key = str::JoinTemp(progIDKey, L"\\shell\\open");
        ok &= LoggedWriteRegStr(hkey, key, L"Icon", iconPath);

        key = str::JoinTemp(progIDKey, L"\\shell\\open\\command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdOpen);

        // for PDF also register for Print/PrintTo shell actions
        if (str::Eq(ext, L".pdf")) {
            key = str::JoinTemp(progIDKey, L"\\shell\\Print\\command");
            ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPrint);

            key = str::JoinTemp(progIDKey, L"\\shell\\PrintTo\\command");
            ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPrintTo);
        }

        key = str::JoinTemp(L"Software\\Classes\\", ext, L"\\OpenWithProgids");
        ok &= LoggedWriteRegNone(hkey, key, progIDName);

        seqstrings::Next(exts);
    }
    return ok;
}

#if 0
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
    auto exts = gSupportedExts;
    while (exts) {
        WCHAR* ext = ToWstrTemp(exts);
        WCHAR* name = str::JoinTemp(L"Software\\Classes\\", ext, openWithVal);
        ok &= CreateRegKey(hkey, name);
        seqstrings::Next(exts);
    }
    return ok;
}
#endif

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
UnregisterFromBeingDefaultViewer() and RemoveInstallRegistryKeys() in Installer.cpp.
*/

#define kRegExplorerPdfExt L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define kRegClassesPdf L"Software\\Classes\\.pdf"

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

TempWstr GetRegClassesAppsTemp(const WCHAR* appName) {
    return str::JoinTemp(L"Software\\Classes\\Applications\\", appName, L".exe");
}

// pre-3.4, for reference so that we know what to delete
#if 0
bool OldWriteFileAssoc(HKEY hkey) {
    bool ok = true;
    WCHAR* exePath = GetInstalledExePathTemp();
    const WCHAR* key;
    if (HKEY_LOCAL_MACHINE == hkey) {
        key = str::JoinTemp(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", kExeName);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, exePath);
    }
    WCHAR* regPath = GetRegClassesAppsTemp(kAppName);

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    WCHAR* iconPath = str::JoinTemp(exePath, L",1");
    {
        key = str::JoinTemp(regPath, L"\\DefaultIcon");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, iconPath);
    }
    AutoFreeWstr cmdPath = str::Format(L"\"%s\" \"%%1\" %%*", exePath);
    {
        key = str::JoinTemp(regPath, L"\\Shell\\Open\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    }
    AutoFreeWstr printPath = str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath);
    {
        key = str::JoinTemp(regPath, L"\\Shell\\Print\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printPath);
    }
    AutoFreeWstr printToPath = str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath);
    {
        key = str::JoinTemp(regPath, L"\\Shell\\PrintTo\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printToPath);
    }
    return ok;
}
#endif

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    logf("WriteExtendedFileExtensionInfo('%s')\n", RegKeyNameTemp(hkey));
    bool ok = true;
    const WCHAR* key;

    // ok &= OldWriteFileAssoc(hkey);

    if (IsWindows10OrGreater()) {
        ok &= RegisterForDefaultPrograms(hkey);
    }
    ok &= RegisterForOpenWith(hkey);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= LoggedWriteRegStr(hkey, kRegClassesPdf, L"Content Type", L"application/pdf");
    key = L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf";
    ok &= LoggedWriteRegStr(hkey, key, L"Extension", L".pdf");

    if (!ok) {
        log("WriteExtendedFileExtensionInfo() failed\n");
    }

    ShellNotifyAssociationsChanged();
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

// Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did.
// Used in pre-3.4
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    log("UnregisterFromBeingDefaultViewer()\n");
    AutoFreeWstr curr = LoggedReadRegStr(hkey, kRegClassesPdf, nullptr);
    const WCHAR* regClassesApp = GetRegClassesAppTemp(kAppName);
    AutoFreeWstr prev = LoggedReadRegStr(hkey, regClassesApp, L"previous.pdf");
    if (!curr || !str::Eq(curr, kAppName)) {
        // not the default, do nothing
    } else {
        // TODO: is nullptr valid here?
        LoggedDeleteRegValue(hkey, kRegClassesPdf, nullptr);
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    AutoFreeWstr buf = LoggedReadRegStr(hkey, kRegExplorerPdfExt, L"ProgId");
    if (str::Eq(buf, kAppName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt, L"ProgId");
    }
    buf.Set(LoggedReadRegStr(hkey, kRegExplorerPdfExt, L"Application"));
    if (str::EqI(buf, kExeName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt, L"Application");
    }
    buf.Set(LoggedReadRegStr(hkey, kRegExplorerPdfExt L"\\UserChoice", L"ProgId"));
    if (str::Eq(buf, kAppName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt L"\\UserChoice", true);
    }
}

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

void RemoveInstallRegistryKeys(HKEY hkey) {
    logf("RemoveInstallRegistryKeys(%s)\n", RegKeyNameTemp(hkey));
    UnregisterFromBeingDefaultViewer(hkey);

    // those are registry keys written before 3.4
    const WCHAR* regClassApp = GetRegClassesAppTemp(kAppName);
    LoggedDeleteRegKey(hkey, regClassApp);
    WCHAR* regPath = GetRegClassesAppsTemp(kAppName);
    LoggedDeleteRegKey(hkey, regPath);
    {
        WCHAR* key = str::JoinTemp(kRegClassesPdf, L"\\OpenWithProgids");
        LoggedDeleteRegValue(hkey, key, kAppName);
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        WCHAR* key = str::JoinTemp(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", kExeName);
        LoggedDeleteRegKey(hkey, key);
    }

    // those are registry keys written before 3.4
    SeqStrings exts = gSupportedExts;
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

    // those were introduced in 3.4
    exts = gSupportedExts;
    while (exts) {
        WCHAR* ext = ToWstrTemp(exts);
        WCHAR* progIDName = str::JoinTemp(kAppName, ext);
        WCHAR* key = str::JoinTemp(L"Software\\Classes\\", progIDName);

        LoggedDeleteRegKey(hkey, key);

        key = str::JoinTemp(L"Software\\Classes\\", ext, L"\\OpenWithProgids");
        LoggedDeleteRegValue(hkey, key, progIDName);

        seqstrings::Next(exts);
    }

    // delete keys written in ListAsDefaultProgramWin10()
    LoggedDeleteRegValue(hkey, L"SOFTWARE\\RegisteredApplications", kAppName);
    AutoFreeWstr keyName = str::Format(L"SOFTWARE\\%s\\Capabilities", kAppName);
    LoggedDeleteRegKey(hkey, keyName);

    ShellNotifyAssociationsChanged();
}
