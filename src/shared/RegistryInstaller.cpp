/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include "base/File.h"
#include "base/Win.h"
#include "base/DirScan.h"
#include "base/AutoWin.h"

#include "SumatraConfig.h"
#include "Version.h"
#include "AppTools.h"
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
    ".jfif\0.webp\0.jxl\0.bmp\0.ico\0.jxr\0.hdp\0.wdp\0";

// Resource ids in SumatraPDF.rc; negative icon indices select by id.
static const int kDefaultFileIcon = 2;
static const struct {
    SeqStrings extensions;
    int resourceId;
} fileIcons[] = {
    {".epub\0", 3},
    {".cbr\0.cbz\0.cbt\0.cb7\0", 4},
    {".chm\0", 5},
    {".djvu\0", 6},
    {".tif\0.tiff\0.jp2\0.png\0.jpg\0.jpeg\0.tga\0.gif\0.avif\0.heic\0.heif\0"
     ".jfif\0.webp\0.jxl\0.bmp\0.ico\0.jxr\0.hdp\0.wdp\0", 7},
    {".pdf\0", 8},
    {".mobi\0.azw\0.azw3\0.azw4\0.prc\0", 9},
};
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
    TempStr progID = str::JoinTemp(StrL(kAppName), ext);
    return HasRegistryValue(hkey, key, progID);
}

static bool HasAllOurOpenWithEntries(HKEY hkey) {
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        if (!HasOurOpenWithEntry(hkey, ext)) {
            return false;
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
static DWORD GetDirSize(Str dir) {
    logf("GetDirSize(%s)\n", dir);
    i64 totalSize = 0;
    DirIter di{dir};
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

    TempStr installedExePath = path::JoinTemp(installDir, Str(kExeName));
    TempStr installDate = GetInstallDateTemp();
    // uninstaller is the same executable with a different flag
    TempStr uninstallCmdLine = fmt("\"%s\" -uninstall", installedExePath);
    if (allUsers) {
        uninstallCmdLine = str::JoinTemp(uninstallCmdLine, StrL(" -all-users"));
    }

    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    // path to installed executable (or "$path,0" to force the first icon)
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("DisplayIcon"), installedExePath);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("DisplayName"), StrL(kAppName));
    // version format: "1.2"
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("DisplayVersion"), StrL(CURR_VERSION_STRA));
    // non-recursive because we don't want to count space used for thumbnails
    // which is in installDir for local install
    DWORD size = GetDirSize(installDir) / 1024;
    // size of installed directory after copying files
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, StrL("EstimatedSize"), size);
    // current date as YYYYMMDD
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("InstallDate"), installDate);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("InstallLocation"), installDir);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, StrL("NoModify"), 1);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, StrL("NoRepair"), 1);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("Publisher"), StrL(kPublisherStr));
    // command line for uninstaller
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("UninstallString"), uninstallCmdLine);
    TempStr uninstallCmdLineSilent = str::JoinTemp(uninstallCmdLine, StrL(" -silent"));
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("QuietUninstallString"), uninstallCmdLineSilent);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("URLInfoAbout"), StrL("https://www.sumatrapdfreader.org/"));
    ok &= LoggedWriteRegStr(hkey, regPathUninst, StrL("URLUpdateInfo"),
                            StrL("https://www.sumatrapdfreader.org/docs/Version-history.html"));
    if (!ok) {
        log(StrL("WriteUninstallerRegistryInfo() failed\n"));
    }
    return ok;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool RegisterForDefaultPrograms(HKEY hkey, Str installedExePath) {
    bool ok = true;

    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    TempStr appCapabilityPath = str::JoinTemp(StrL("SOFTWARE\\"), StrL(kAppName), StrL("\\Capabilities"));

    Str desc = StrL("SumatraPDF is a PDF reader.");
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, StrL("ApplicationDescription"), desc);
    // ApplicationName must match the RegisteredApplications value name (kAppName).
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, StrL("ApplicationName"), StrL(kAppName));
    // icon shown next to the app in Settings > Default Apps
    TempStr appIcon = str::JoinTemp(StrL("\""), installedExePath, StrL("\",0"));
    ok &= LoggedWriteRegStr(hkey, appCapabilityPath, StrL("ApplicationIcon"), appIcon);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    TempStr keyAssoc = str::JoinTemp(appCapabilityPath, StrL("\\FileAssociations"));

    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        // must match the per-extension ProgID created by RegisterForOpenWith
        // (e.g. "SumatraPDF.pdf"); Default Apps UI hides the app if the
        // FileAssociations ProgID can't be resolved under HKCR
        TempStr progIDName = str::JoinTemp(StrL(kAppName), ext);
        ok &= LoggedWriteRegStr(hkey, keyAssoc, ext, progIDName);
    }

    ok &= LoggedWriteRegStr(hkey, StrL("SOFTWARE\\RegisteredApplications"), StrL(kAppName), appCapabilityPath);
    return ok;
}

// Register per-extension ProgIDs and their OpenWithProgids entries.
// Use the same registry root for both so either per-user or all-users works.
static bool RegisterForOpenWith(HKEY hkey, Str installedExePath) {
    TempStr exePathQuoted = str::JoinTemp(StrL("\""), installedExePath, StrL("\""));
    TempStr cmdOpen = str::JoinTemp(exePathQuoted, StrL(" \"%1\""));
    TempStr cmdPrint = str::JoinTemp(exePathQuoted, StrL(" -print-to-default \"%1\""));
    TempStr cmdPrintTo = str::JoinTemp(exePathQuoted, StrL(" -print-to \"%2\" \"%1\""));
    TempStr key;
    bool ok = true;
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        TempStr progIDName = str::JoinTemp(StrL(kAppName), ext);
        TempStr progIDKey = str::JoinTemp(StrL("Software\\Classes\\"), progIDName);

        // Don't set the progID's friendly name (its (Default) value). A hardcoded
        // English string like "PDF File" overrides the localized type name that
        // Windows generates for the file type, so non-English systems wrongly show
        // English names in Explorer's "Type" column (issue #3323). Delete any value
        // a previous version wrote so Windows falls back to the localized name.
        ok &= LoggedDeleteRegValue(hkey, progIDKey, {});

        int iconId = kDefaultFileIcon;
        for (const auto& icon : fileIcons) {
            if (SeqStrIndexI(icon.extensions, ext) >= 0) {
                iconId = icon.resourceId;
                break;
            }
        }
        TempStr iconPath = fmt("%s,-%d", exePathQuoted, iconId);

        key = str::JoinTemp(progIDKey, StrL("\\Application"));
        ok &= LoggedWriteRegStr(hkey, key, StrL("ApplicationCompany"), StrL("Krzysztof Kowalczyk"));
        ok &= LoggedWriteRegStr(hkey, key, StrL("ApplicationName"), StrL(kAppName));

        key = str::JoinTemp(progIDKey, StrL("\\DefaultIcon"));
        ok &= LoggedWriteRegStr(hkey, key, {}, iconPath);

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open"));
        ok &= LoggedWriteRegStr(hkey, key, StrL("Icon"), iconPath);
        ok &= LoggedWriteRegStr(hkey, key, StrL("MultiSelectModel"), StrL("Player"));

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open\\command"));
        ok &= LoggedWriteRegStr(hkey, key, {}, cmdOpen);

        // for PDF also register for Print/PrintTo shell actions
        if (str::Eq(ext, StrL(".pdf"))) {
            key = str::JoinTemp(progIDKey, StrL("\\shell\\Print\\command"));
            ok &= LoggedWriteRegStr(hkey, key, {}, cmdPrint);

            key = str::JoinTemp(progIDKey, StrL("\\shell\\PrintTo\\command"));
            ok &= LoggedWriteRegStr(hkey, key, {}, cmdPrintTo);
        }

        key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        ok &= LoggedWriteRegNone(hkey, key, progIDName);
    }
    return ok;
}

#define kRegExplorerPdfExt "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define kRegClassesPdf "Software\\Classes\\.pdf"

static TempStr GetRegClassesAppsTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Classes\\Applications\\"), appName, StrL(".exe"));
}

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
bool WriteExtendedFileExtensionInfo(HKEY hkey, Str installedExePath) {
    logf("WriteExtendedFileExtensionInfo('%s')\n", RegKeyNameTemp(hkey));
    bool ok = true;
    TempStr key;

    if (IsWindows10OrGreater()) {
        ok &= RegisterForDefaultPrograms(hkey, installedExePath);
    }
    ok &= RegisterForOpenWith(hkey, installedExePath);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= LoggedWriteRegStr(hkey, StrL(kRegClassesPdf), StrL("Content Type"), StrL("application/pdf"));
    key = StrL(R"(Software\Classes\MIME\Database\Content Type\application/pdf)");
    ok &= LoggedWriteRegStr(hkey, key, StrL("Extension"), StrL(".pdf"));

    if (!ok) {
        log(StrL("WriteExtendedFileExtensionInfo() failed\n"));
    }

    ShellNotifyAssociationsChanged();
    return ok;
}

bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    logf("RemoveUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    bool ok1 = LoggedDeleteRegKey(hkey, regPathUninst);
    // legacy, this key was added by installers up to version 1.8
    TempStr key = str::JoinTemp(StrL("Software\\"), StrL(kAppName));
    bool ok2 = LoggedDeleteRegKey(hkey, key);
    return ok1 && ok2;
}

static TempStr GetRegClassesAppTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Classes\\"), appName);
}

// Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did.
// Used in pre-3.4
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    log(StrL("UnregisterFromBeingDefaultViewer()\n"));
    TempStr curr = LoggedReadRegStrTemp(hkey, StrL(kRegClassesPdf), {});
    if (len(curr) == 0 || !str::Eq(curr, StrL(kAppName))) {
        // not the default, do nothing
    } else {
        LoggedDeleteRegValue(hkey, StrL(kRegClassesPdf), {});
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    TempStr buf = LoggedReadRegStrTemp(hkey, StrL(kRegExplorerPdfExt), StrL("ProgId"));
    if (str::Eq(buf, StrL(kAppName))) {
        LoggedDeleteRegKey(hkey, StrL(kRegExplorerPdfExt "ProgId"), true);
    }
    buf = LoggedReadRegStrTemp(hkey, StrL(kRegExplorerPdfExt), StrL("Application"));
    if (str::EqI(buf, Str(kExeName))) {
        LoggedDeleteRegKey(hkey, StrL(kRegExplorerPdfExt "Application"), true);
    }
    buf = LoggedReadRegStrTemp(hkey, StrL(kRegExplorerPdfExt "\\UserChoice"), StrL("ProgId"));
    if (str::Eq(buf, StrL(kAppName))) {
        LoggedDeleteRegKey(hkey, StrL(kRegExplorerPdfExt "\\UserChoice"), true);
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
    TempStr regClassApp = GetRegClassesAppTemp(StrL(kAppName));
    LoggedDeleteRegKey(hkey, regClassApp);
    TempStr regPath = GetRegClassesAppsTemp(StrL(kAppName));
    LoggedDeleteRegKey(hkey, regPath);
    {
        TempStr key = str::JoinTemp(StrL(kRegClassesPdf), StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, key, StrL(kAppName));
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        TempStr key = str::JoinTemp(StrL("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"), Str(kExeName));
        LoggedDeleteRegKey(hkey, key);
    }

    // those are registry keys written before 3.4
    TempStr openWithVal = str::JoinTemp(StrL("\\OpenWithList\\"), Str(kExeName));
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        TempStr keyname = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, keyname, StrL(kAppName));
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
    }

    // those were introduced in 3.4
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        TempStr progIDName = str::JoinTemp(StrL(kAppName), ext);
        TempStr key = str::JoinTemp(StrL("Software\\Classes\\"), progIDName);

        LoggedDeleteRegKey(hkey, key);

        key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, key, progIDName);
    }

    // delete keys written in ListAsDefaultProgramWin10()
    LoggedDeleteRegValue(hkey, StrL("SOFTWARE\\RegisteredApplications"), StrL(kAppName));
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
    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    if (HasRegistryValue(HKEY_LOCAL_MACHINE, regPathUninst, StrL("InstallLocation"))) {
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

// Normalize to ".pdf" form (leading dot, lowercased for display is caller's job).
static TempStr NormalizeExtTemp(Str ext) {
    if (len(ext) == 0) {
        return {};
    }
    if (ext.s[0] == '.') {
        return str::DupTemp(ext);
    }
    return str::JoinTemp(StrL("."), ext);
}

static bool IsOurProgId(Str progId, Str ext) {
    if (len(progId) == 0) {
        return false;
    }
    // current scheme: SumatraPDF.pdf
    TempStr ours = str::JoinTemp(StrL(kAppName), ext);
    if (str::EqI(progId, ours)) {
        return true;
    }
    // pre-3.4 scheme used plain "SumatraPDF" for .pdf
    return str::EqI(progId, StrL(kAppName));
}

// true if we appear under OpenWithProgids for this extension (HKCU or HKLM)
static bool HaveRegisteredOpenWithForExt(Str ext) {
    return HasOurOpenWithEntry(HKEY_CURRENT_USER, ext) || HasOurOpenWithEntry(HKEY_LOCAL_MACHINE, ext);
}

// true when ShellExecute for this extension would launch our exe / ProgID
static bool IsSumatraDefaultForExt(Str ext) {
    WCHAR* extW = CWStrTemp(ext);
    if (!extW || !*extW) {
        return false;
    }
    WCHAR* appNameW = CWStrTemp(StrL(kAppName));

    AutoReleaseComPtr<IApplicationAssociationRegistration> aar;
    HRESULT hr =
        CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&aar));
    if (SUCCEEDED(hr) && aar) {
        // RegisteredApplications name is StrL(kAppName) ("SumatraPDF")
        BOOL isDefault = FALSE;
        hr = aar->QueryAppIsDefault(extW, AT_FILEEXTENSION, AL_EFFECTIVE, appNameW, &isDefault);
        if (SUCCEEDED(hr)) {
            return isDefault != FALSE;
        }

        LPWSTR assoc = nullptr;
        hr = aar->QueryCurrentDefault(extW, AT_FILEEXTENSION, AL_EFFECTIVE, &assoc);
        if (SUCCEEDED(hr) && assoc) {
            TempStr progId = ToUtf8Temp(assoc);
            CoTaskMemFree(assoc);
            if (IsOurProgId(progId, ext)) {
                return true;
            }
        }
    }

    // Fallback: resolve the open executable and compare to ourselves
    WCHAR pathW[MAX_PATH]{};
    DWORD cch = dimofi(pathW);
    hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, extW, nullptr, pathW, &cch);
    if (SUCCEEDED(hr) && pathW[0]) {
        TempStr path = ToUtf8Temp(pathW);
        TempStr self = GetSelfExePathTemp();
        return path::IsSame(path, self);
    }
    return false;
}

// Explicit user/system default under FileExts\.\UserChoice. Empty means no
// UserChoice key — we don't nag about those (no app was ever chosen for that type).
static TempStr ReadUserChoiceProgIdTemp(Str ext) {
    TempStr key = fmt("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\UserChoice", ext);
    return LoggedReadRegStrTemp(HKEY_CURRENT_USER, key, StrL("ProgId"));
}

static TempStr ReadDefaultProgIdTemp(Str ext) {
    TempStr progId = ReadUserChoiceProgIdTemp(ext);
    if (len(progId) > 0) {
        return progId;
    }

    TempStr key = str::JoinTemp(StrL("Software\\Classes\\"), ext);
    progId = ReadRegStrTemp(HKEY_CURRENT_USER, key, {});
    if (len(progId) > 0) {
        return progId;
    }
    return ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, {});
}

static TempStr GetProgIdExePathTemp(Str progId) {
    WCHAR* progIdW = CWStrTemp(progId);
    WCHAR pathW[MAX_PATH]{};
    DWORD cch = dimofi(pathW);
    HRESULT hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, progIdW, nullptr, pathW, &cch);
    if (FAILED(hr) || !pathW[0]) {
        return {};
    }
    return ToUtf8Temp(pathW);
}

void LogNonDefaultRegisteredExtensions() {
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        TempStr progId = ReadDefaultProgIdTemp(ext);
        if (len(progId) > 0 && !IsOurProgId(progId, ext)) {
            TempStr app = GetProgIdExePathTemp(progId);
            logf("Not default app: extension=%s, progId=%s, app=%s\n", ext, progId, app ? app : StrL("(none)"));
        }
    }
}

// Extensions we registered for Open With where something else is explicitly the
// default (UserChoice points at another ProgId). Used by the home-page bottom bar.
void CollectNonDefaultRegisteredExtensions(StrVec& out) {
    out.Reset();
    if (!IsOurExeInstalled()) {
        return;
    }
    for (Str ext = SeqStrFirst(gSupportedExts); len(ext) > 0; ext = SeqStrNext(ext)) {
        if (HaveRegisteredOpenWithForExt(ext) && !IsSumatraDefaultForExt(ext)) {
            // only list types with an explicit UserChoice that isn't us — otherwise
            // every registered format (e.g. .tga) would show after a fresh install
            TempStr userChoice = ReadUserChoiceProgIdTemp(ext);
            if (len(userChoice) > 0 && !IsOurProgId(userChoice, ext)) {
                out.Append(ext);
            }
        }
    }
}

// Open the OS UI to pick/set the default app for ext (".pdf" or "pdf").
void LaunchDefaultAppDialogForExtension(HWND hwnd, Str extIn) {
    TempStr ext = NormalizeExtTemp(extIn);
    if (len(ext) == 0) {
        return;
    }

    // SHOpenWithDialog: the classic "How do you want to open this file?" UI, which
    // can set Always use this app. Needs a path-looking string ending in the ext;
    // the file does not need to exist.
    TempStr sample = str::JoinTemp(StrL("document"), ext);
    OPENASINFO info{};
    info.pcszFile = CWStrTemp(sample);
    info.oaifInFlags = OAIF_FORCE_REGISTRATION | OAIF_REGISTER_EXT | OAIF_ALLOW_REGISTRATION;
    HRESULT hr = SHOpenWithDialog(hwnd, &info);
    if (SUCCEEDED(hr)) {
        return;
    }

    // Fall back to Settings > Default apps, preferably focused on our app entry
    // (Win11 registeredAppUser / registeredAppMachine deep link).
    TempStr uri;
    if (HasRegistryValue(HKEY_CURRENT_USER, StrL("SOFTWARE\\RegisteredApplications"), StrL(kAppName))) {
        uri = fmt("ms-settings:defaultapps?registeredAppUser=%s", StrL(kAppName));
    } else if (HasRegistryValue(HKEY_LOCAL_MACHINE, StrL("SOFTWARE\\RegisteredApplications"), StrL(kAppName))) {
        uri = fmt("ms-settings:defaultapps?registeredAppMachine=%s", StrL(kAppName));
    } else {
        uri = StrL("ms-settings:defaultapps");
    }
    ShellExecuteW(hwnd, L"open", CWStrTemp(uri), nullptr, nullptr, SW_SHOWNORMAL);
}

void LaunchDefaultAppDialogForExtension(Str ext) {
    LaunchDefaultAppDialogForExtension(nullptr, ext);
}
#endif
