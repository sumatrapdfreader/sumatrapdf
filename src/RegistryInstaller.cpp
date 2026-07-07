/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/DirIter.h"

#include "SumatraConfig.h"
#include "Version.h"
#include "Installer.h"

// All registry manipulation needed for installer / uninstaller

// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
// clang-format off
static SeqStrings gSupportedExts = 
    ".pdf\0.xps\0.oxps\0.cbz\0.cbr\0.cb7\0.cbt\0" \
    ".djvu\0.chm\0.mobi\0.epub\0.md\0.markdown\0.svg\0.azw\0.azw3\0.azw4\0" \
    ".fb2\0.fb2z\0.prc\0.tif\0.tiff\0.jp2\0.png\0" \
    ".jpg\0.jpeg\0.tga\0.gif\0.avif\0.heic\0.heif\0" \
    ".jfif\0.webp\0.jxl\0.bmp\0.jxr\0.hdp\0.wdp\0";
// clang-format on

// notifies Shell that file associations changed.
// Invalidates the icon and thumbnail cache.
// https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify
static void ShellNotifyAssociationsChanged() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

static bool HasRegistryValue(HKEY hkey, Str keyName, Str valName) {
    WCHAR* keyW = CWStrTemp(keyName);
    WCHAR* valW = CWStrTemp(valName);
    DWORD type = 0;
    DWORD cb = 0;
    LSTATUS res = SHGetValueW(hkey, keyW, valW, &type, nullptr, &cb);
    // success or ERROR_MORE_DATA means the value exists
    return (res == ERROR_SUCCESS || res == ERROR_MORE_DATA);
}

static bool HasOurOpenWithEntry(HKEY hkey, Str ext) {
    TempStr key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
    TempStr progID = str::JoinTemp(kAppName, ext);
    return HasRegistryValue(hkey, key, progID);
}

static bool HasAllOurOpenWithEntries(HKEY hkey) {
    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        if (!HasOurOpenWithEntry(hkey, SeqStrAt(gSupportedExts, off))) {
            return false;
        }
        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
    }
    return true;
}

static TempStr GetInstallDateTemp() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return fmt("%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(Str dir, bool recur) {
    logf("GetDirSize(%s)\n", dir);
    i64 totalSize = 0;
    DirIter di{dir};
    di.recurse = recur;
    for (DirIterEntry* de : di) {
        i64 fileSize = GetFileSize(de);
        totalSize += fileSize;
    }
    return (DWORD)totalSize;
}

bool WriteUninstallerRegistryInfo(HKEY hkey, bool allUsers, Str installDir) {
    logf("WriteUninstallerRegistryInfo(hKey: %s, allUsers: %d, installDir: '%s')\n", RegKeyNameTemp(hkey),
         (int)allUsers, installDir);
    bool ok = true;

    TempStr installedExePath = path::JoinTemp(installDir, kExeName);
    TempStr installDate = GetInstallDateTemp();
    // uninstaller is the same executable with a different flag
    Str uninstallerPath = installedExePath;
    TempStr uninstallCmdLine = fmt("\"%s\" -uninstall", uninstallerPath);
    if (allUsers) {
        uninstallCmdLine = str::JoinTemp(uninstallCmdLine, StrL(" -all-users"));
    }

    TempStr regPathUninst = GetRegPathUninstTemp(kAppName);
    // path to installed executable (or "$path,0" to force the first icon)
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "DisplayIcon", installedExePath);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "DisplayName", kAppName);
    // version format: "1.2"
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "DisplayVersion", CURR_VERSION_STRA);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsWindowsVistaOrGreater()) {
        TempStr key = str::JoinTemp(kAppName, StrL(" "), CURR_VERSION_STRA);
        ok &= LoggedWriteRegStr(hkey, regPathUninst, "DisplayName", key);
    }
    // non-recursive because we don't want to count space used for thumbnails
    // which is in installDir for local install
    DWORD size = GetDirSize(installDir, false) / 1024;
    // size of installed directory after copying files
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, "EstimatedSize", size);
    // current date as YYYYMMDD
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "InstallDate", installDate);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "InstallLocation", installDir);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, "NoModify", 1);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, "NoRepair", 1);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "Publisher", kPublisherStr);
    // command line for uninstaller
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "UninstallString", uninstallCmdLine);
    TempStr uninstallCmdLineSilent = str::JoinTemp(uninstallCmdLine, StrL(" -silent"));
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "QuietUninstallString", uninstallCmdLineSilent);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "URLInfoAbout", "https://www.sumatrapdfreader.org/");
    ok &= LoggedWriteRegStr(hkey, regPathUninst, "URLUpdateInfo",
                            "https://www.sumatrapdfreader.org/docs/Version-history.html");
    if (!ok) {
        log("WriteUninstallerRegistryInfo() failed\n");
    }
    return ok;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool RegisterForDefaultPrograms(HKEY hkey, Str installedExePath) {
    bool ok = true;

    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    TempStr appCapabilityPath = str::JoinTemp(StrL("SOFTWARE\\"), kAppName, StrL("\\Capabilities"));

    Str desc = "SumatraPDF is a PDF reader.";
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, "ApplicationDescription", desc);
    Str appLongName = "SumatraPDF Reader";
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, "ApplicationName", appLongName);
    // icon shown next to the app in Settings > Default Apps
    TempStr appIcon = str::JoinTemp(StrL("\""), installedExePath, StrL("\",0"));
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, "ApplicationIcon", appIcon);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    TempStr keyAssoc = str::JoinTemp(appCapabilityPath, StrL("\\FileAssociations"));

    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        Str ext = SeqStrAt(gSupportedExts, off);
        // must match the per-extension ProgID created by RegisterForOpenWith
        // (e.g. "SumatraPDF.pdf"); Default Apps UI hides the app if the
        // FileAssociations ProgID can't be resolved under HKCR
        TempStr progIDName = str::JoinTemp(kAppName, ext);
        ok &= LoggedWriteRegStr(hkey, keyAssoc, ext, progIDName);
        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
    }

    ok &= LoggedWriteRegStr(hkey, "SOFTWARE\\RegisteredApplications", kAppName, appCapabilityPath);
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
static bool RegisterForOpenWith(HKEY hkey, Str installedExePath) {
    TempStr exePathQuoted = str::JoinTemp(StrL("\""), installedExePath, StrL("\""));
    TempStr cmdOpen = str::JoinTemp(exePathQuoted, StrL(" \"%1\" \"%2\" \"%3\" \"%4\""));
    TempStr cmdPrint = str::JoinTemp(exePathQuoted, StrL(" -print-to-default \"%1\""));
    TempStr cmdPrintTo = str::JoinTemp(exePathQuoted, StrL(" -print-to \"%2\" \"%1\""));
    TempStr key;
    bool ok = true;
    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        Str ext = SeqStrAt(gSupportedExts, off);
        TempStr progIDName = str::JoinTemp(kAppName, ext);
        TempStr progIDKey = str::JoinTemp(StrL("Software\\Classes\\"), progIDName);
        // ok &= CreateRegKey(hkey, progIDKey);

        // Don't set the progID's friendly name (its (Default) value). A hardcoded
        // English string like "PDF File" overrides the localized type name that
        // Windows generates for the file type, so non-English systems wrongly show
        // English names in Explorer's "Type" column (issue #3323). Delete any value
        // a previous version wrote so Windows falls back to the localized name.
        ok &= LoggedDeleteRegValue(hkey, progIDKey, nullptr);
        // ok &= LoggedWriteRegStr(hkey, progIDKey, L"AppUserModelID", L"SumatraPDF"); // ???

        // Per https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-extracticona
        // ",${n}" => n is 0-based index of the icon
        // ",-${n}" => n is icon with resource id
        TempStr iconPath;
        if (str::Eq(ext, ".epub")) {
            iconPath = str::JoinTemp(exePathQuoted, StrL(",-3"));
        } else if (str::Eq(ext, ".cbr") || str::Eq(ext, ".cbz") || str::Eq(ext, ".cbt") || str::Eq(ext, ".cb7")) {
            iconPath = str::JoinTemp(exePathQuoted, StrL(",-4"));
        } else if (str::Eq(ext, ".chm")) {
            iconPath = str::JoinTemp(exePathQuoted, StrL(",-5"));
        } else if (str::Eq(ext, ".djvu")) {
            iconPath = str::JoinTemp(exePathQuoted, StrL(",-6"));
        } else {
            iconPath = str::JoinTemp(exePathQuoted, StrL(",-2"));
        }

        key = str::JoinTemp(progIDKey, StrL("\\Application"));
        ok &= LoggedWriteRegStr(hkey, key, "ApplicationCompany", "Krzysztof Kowalczyk");
        ok &= LoggedWriteRegStr(hkey, key, "ApplicationName", kAppName);

        key = str::JoinTemp(progIDKey, StrL("\\DefaultIcon"));
        ok &= LoggedWriteRegStr(hkey, key, nullptr, iconPath);

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open"));
        ok &= LoggedWriteRegStr(hkey, key, "Icon", iconPath);

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open\\command"));
        ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdOpen);

        // for PDF also register for Print/PrintTo shell actions
        if (str::Eq(ext, ".pdf")) {
            key = str::JoinTemp(progIDKey, StrL("\\shell\\Print\\command"));
            ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPrint);

            key = str::JoinTemp(progIDKey, StrL("\\shell\\PrintTo\\command"));
            ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPrintTo);
        }

        key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        ok &= LoggedWriteRegNone(hkey, key, progIDName);

        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
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

    TempWStr openWithVal = str::JoinTemp(WStrL(L"\\OpenWithList\\"), kExeName);
    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        TempWStr ext = ToWStrTemp(SeqStrAt(gSupportedExts, off));
        TempWStr name = str::JoinTemp(WStrL(L"Software\\Classes\\"), ext, openWithVal);
        ok &= CreateRegKey(hkey, name);
        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
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
  should be SumatraPDF (Foxit takes it over); only needed for HKEY_CURRENT_USER
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

#define kRegExplorerPdfExt "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define kRegClassesPdf "Software\\Classes\\.pdf"

TempStr GetRegClassesAppsTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Classes\\Applications\\"), appName, StrL(".exe"));
}

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
bool WriteExtendedFileExtensionInfo(HKEY hkey, Str installedExePath) {
    logf("WriteExtendedFileExtensionInfo('%s')\n", RegKeyNameTemp(hkey));
    bool ok = true;
    TempStr key;

    // ok &= OldWriteFileAssoc(hkey);

    if (IsWindows10OrGreater()) {
        ok &= RegisterForDefaultPrograms(hkey, installedExePath);
    }
    ok &= RegisterForOpenWith(hkey, installedExePath);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= LoggedWriteRegStr(hkey, kRegClassesPdf, "Content Type", "application/pdf");
    key = "Software\\Classes\\MIME\\Database\\Content Type\\application/pdf";
    ok &= LoggedWriteRegStr(hkey, key, "Extension", ".pdf");

    if (!ok) {
        log("WriteExtendedFileExtensionInfo() failed\n");
    }

    ShellNotifyAssociationsChanged();
    return ok;
}

bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    logf("RemoveUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    TempStr regPathUninst = GetRegPathUninstTemp(kAppName);
    bool ok1 = LoggedDeleteRegKey(hkey, regPathUninst);
    // legacy, this key was added by installers up to version 1.8
    TempStr key = str::JoinTemp(StrL("Software\\"), kAppName);
    bool ok2 = LoggedDeleteRegKey(hkey, key);
    return ok1 && ok2;
}

static TempStr GetRegClassesAppTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Classes\\"), appName);
}

// Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did.
// Used in pre-3.4
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    log("UnregisterFromBeingDefaultViewer()\n");
    TempStr curr = LoggedReadRegStrTemp(hkey, kRegClassesPdf, nullptr);
    if (!curr || !str::Eq(curr, kAppName)) {
        // not the default, do nothing
    } else {
        // TODO: is nullptr valid here?
        LoggedDeleteRegValue(hkey, kRegClassesPdf, nullptr);
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    TempStr buf = LoggedReadRegStrTemp(hkey, kRegExplorerPdfExt, "ProgId");
    if (str::Eq(buf, kAppName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt "ProgId", true);
    }
    buf = LoggedReadRegStrTemp(hkey, kRegExplorerPdfExt, "Application");
    if (str::EqI(buf, kExeName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt "Application", true);
    }
    buf = LoggedReadRegStrTemp(hkey, kRegExplorerPdfExt "\\UserChoice", "ProgId");
    if (str::Eq(buf, kAppName)) {
        LoggedDeleteRegKey(hkey, kRegExplorerPdfExt "\\UserChoice", true);
    }
}

// delete registry key but only if it's empty
static bool DeleteEmptyRegKey(HKEY root, Str keyName) {
    HKEY hkey;
    WCHAR* keyNameW = CWStrTemp(keyName);
    LSTATUS status = RegOpenKeyExW(root, keyNameW, 0, KEY_READ, &hkey);
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

// return keyName's parent key (everything before the last '\\'), or {} if there
// is none. Returns a properly-sized view (unlike poking a NUL into the buffer,
// which left .len stale).
static Str RegKeyParent(Str keyName) {
    int sep = str::LastIndexOfChar(keyName, '\\');
    if (sep < 0) {
        return {};
    }
    return Str(keyName.s, sep);
}

void RemoveInstallRegistryKeys(HKEY hkey) {
    logf("RemoveInstallRegistryKeys(%s)\n", RegKeyNameTemp(hkey));
    UnregisterFromBeingDefaultViewer(hkey);

    // those are registry keys written before 3.4
    TempStr regClassApp = GetRegClassesAppTemp(kAppName);
    LoggedDeleteRegKey(hkey, regClassApp);
    TempStr regPath = GetRegClassesAppsTemp(kAppName);
    LoggedDeleteRegKey(hkey, regPath);
    {
        TempStr key = str::JoinTemp(kRegClassesPdf, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, key, kAppName);
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        TempStr key = str::JoinTemp(StrL("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"), kExeName);
        LoggedDeleteRegKey(hkey, key);
    }

    // those are registry keys written before 3.4
    TempStr openWithVal = str::JoinTemp(StrL("\\OpenWithList\\"), kExeName);
    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        Str ext = SeqStrAt(gSupportedExts, off);
        TempStr keyname = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, keyname, kAppName);
        DeleteEmptyRegKey(hkey, keyname);

        keyname = str::JoinTemp(StrL("Software\\Classes\\"), ext, openWithVal);
        if (LoggedDeleteRegKey(hkey, keyname)) {
            // remove empty parent keys that the installer might have created
            keyname = RegKeyParent(keyname);
            if (keyname && DeleteEmptyRegKey(hkey, keyname)) {
                keyname = RegKeyParent(keyname);
                if (keyname) {
                    DeleteEmptyRegKey(hkey, keyname);
                }
            }
        }
        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
    }

    // those were introduced in 3.4
    for (int off = 0; SeqStrAt(gSupportedExts, off);) {
        Str ext = SeqStrAt(gSupportedExts, off);
        TempStr progIDName = str::JoinTemp(kAppName, ext);
        TempStr key = str::JoinTemp(StrL("Software\\Classes\\"), progIDName);

        LoggedDeleteRegKey(hkey, key);

        key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, key, progIDName);

        if (!SeqStrAdvance(gSupportedExts, off)) {
            break;
        }
    }

    // delete keys written in ListAsDefaultProgramWin10()
    LoggedDeleteRegValue(hkey, "SOFTWARE\\RegisteredApplications", kAppName);
    TempStr keyName = fmt("SOFTWARE\\%s\\Capabilities", StrL(kAppName));
    LoggedDeleteRegKey(hkey, keyName);

    ShellNotifyAssociationsChanged();
}

// re-register our "Open With" file association handlers (under OpenWithProgids
// and the corresponding ProgID entries) if this is an installed (non-portable)
// copy of SumatraPDF. We do this at startup to counter other apps (e.g. Microsoft
// Edge) that might remove us from the "Open with" context menu for .pdf etc. files.
// We only touch HKCU (always writable by the current user) and optionally HKLM
// (for all-users installs; fails gracefully without admin rights).
void ReRegisterFileAssociations() {
    if (!IsOurExeInstalled()) {
        return;
    }
    TempStr exePath = GetSelfExePathTemp();
    if (len(exePath) == 0) {
        return;
    }

    bool didRegister = false;
    if (!HasAllOurOpenWithEntries(HKEY_CURRENT_USER)) {
        RegisterForOpenWith(HKEY_CURRENT_USER, exePath);
        if (IsWindows10OrGreater()) {
            RegisterForDefaultPrograms(HKEY_CURRENT_USER, exePath);
        }
        didRegister = true;
    }

    // for all-users installs, also try to restore the HKLM entries (best effort)
    TempStr regPathUninst = GetRegPathUninstTemp(kAppName);
    if (HasRegistryValue(HKEY_LOCAL_MACHINE, regPathUninst, "InstallLocation")) {
        if (!HasAllOurOpenWithEntries(HKEY_LOCAL_MACHINE)) {
            RegisterForOpenWith(HKEY_LOCAL_MACHINE, exePath);
            if (IsWindows10OrGreater()) {
                RegisterForDefaultPrograms(HKEY_LOCAL_MACHINE, exePath);
            }
            didRegister = true;
        }
    }

    if (didRegister) {
        ShellNotifyAssociationsChanged();
    }
}
