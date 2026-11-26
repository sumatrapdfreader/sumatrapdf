/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include <ktmw32.h>

#include "base/File.h"
#include "base/Win.h"
#include "base/DirScan.h"
#include "base/AutoWin.h"

#include "SumatraConfig.h"
#include "Version.h"
#include "AppTools.h"
#include "Translations.h"
#include "Installer.h"

#pragma comment(lib, "KtmW32.lib")

// All registry manipulation needed for installer / uninstaller

struct InstallRegWriter {
    HKEY root;
    HANDLE transaction = nullptr;
    bool ok = true;

    void Set(Str key, Str name, DWORD type, const void* data, DWORD size);
    void String(Str key, Str name, Str value);
    void Dword(Str key, Str name, DWORD value);
    void None(Str key, Str name);
    void Delete(Str key, Str name);
    void Record(Str key, Str name, LSTATUS status);
};

void InstallRegWriter::Record(Str key, Str name, LSTATUS status) {
    if (status != ERROR_SUCCESS) {
        ok = false;
    }
    if (gLogRegistryCalls || status != ERROR_SUCCESS) {
        logf("Installer registry: %s, %s, %s => %d\n", RegKeyNameTemp(root), key, name, (int)status);
    }
}

void InstallRegWriter::Set(Str key, Str name, DWORD type, const void* data, DWORD size) {
    if (transaction && !ok) {
        return;
    }
    HKEY hkey;
    WCHAR* path = CWStrTemp(key);
    LSTATUS status = transaction ? RegCreateKeyTransactedW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hkey,
                                                           nullptr, transaction, nullptr)
                                 : RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hkey, nullptr);
    if (status == ERROR_SUCCESS) {
        status = RegSetValueExW(hkey, CWStrTemp(name), 0, type, (const BYTE*)data, size);
        RegCloseKey(hkey);
    }
    Record(key, name, status);
}

void InstallRegWriter::String(Str key, Str name, Str value) {
    int cch;
    WCHAR* wide = CWStrTemp(value, cch);
    Set(key, name, REG_SZ, wide, (DWORD)(cch + 1) * sizeof(WCHAR));
}

void InstallRegWriter::Dword(Str key, Str name, DWORD value) {
    Set(key, name, REG_DWORD, &value, sizeof(value));
}

void InstallRegWriter::None(Str key, Str name) {
    Set(key, name, REG_NONE, nullptr, 0);
}

void InstallRegWriter::Delete(Str key, Str name) {
    if (transaction && !ok) {
        return;
    }
    HKEY hkey;
    WCHAR* path = CWStrTemp(key);
    LSTATUS status = transaction ? RegOpenKeyTransactedW(root, path, 0, KEY_SET_VALUE, &hkey, transaction, nullptr)
                                 : RegOpenKeyExW(root, path, 0, KEY_SET_VALUE, &hkey);
    if (status == ERROR_SUCCESS) {
        status = RegDeleteValueW(hkey, CWStrTemp(name));
        RegCloseKey(hkey);
    }
    if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND) {
        Record(key, name, status);
    }
}

// Extensions registered for Open With and their icon resource IDs.
// Negative icon indices select resources by ID in SumatraPDF.rc.
constexpr int kDefaultFileIcon = 2;
// clang-format off
static const struct {
    Str ext;
    int iconId;
} gSupportedExts[] = {
    {StrL(".pdf"), 8}, {StrL(".xps"), kDefaultFileIcon}, {StrL(".oxps"), kDefaultFileIcon},
    {StrL(".cbz"), 4}, {StrL(".cbr"), 4}, {StrL(".cb7"), 4}, {StrL(".cbt"), 4},
    {StrL(".djvu"), 6}, {StrL(".chm"), 5}, {StrL(".mobi"), 9}, {StrL(".epub"), 3},
    {StrL(".md"), kDefaultFileIcon}, {StrL(".markdown"), kDefaultFileIcon}, {StrL(".svg"), kDefaultFileIcon}, {StrL(".azw"), 9},
    {StrL(".azw3"), 9}, {StrL(".azw4"), 9}, {StrL(".fb2"), kDefaultFileIcon}, {StrL(".fb2z"), kDefaultFileIcon},
    {StrL(".prc"), 9}, {StrL(".lit"), 9}, {StrL(".rtf"), kDefaultFileIcon}, {StrL(".tif"), 7}, {StrL(".tiff"), 7}, {StrL(".jp2"), 7},
    {StrL(".png"), 7}, {StrL(".jpg"), 7}, {StrL(".jpeg"), 7}, {StrL(".tga"), 7},
    {StrL(".gif"), 7}, {StrL(".avif"), 7}, {StrL(".heic"), 7}, {StrL(".heif"), 7},
    {StrL(".jfif"), 7}, {StrL(".webp"), 7}, {StrL(".jxl"), 7}, {StrL(".bmp"), 7},
    {StrL(".ico"), 7}, {StrL(".jxr"), 7}, {StrL(".hdp"), 7}, {StrL(".wdp"), 7},
    {StrL(".caj"), kDefaultFileIcon}, {StrL(".kdh"), kDefaultFileIcon}, {StrL(".nh"), kDefaultFileIcon},
    {StrL(".ofd"), kDefaultFileIcon},
};
// clang-format on

// notifies Shell that file associations changed.
// Invalidates the icon and thumbnail cache.
// https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify
static void ShellNotifyAssociationsChanged() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// CAJ files need caj2pdf, which is x64 only
static bool IsExtUsable(Str ext) {
    if (IsOs64()) {
        return true;
    }
    return !str::Eq(ext, StrL(".caj")) && !str::Eq(ext, StrL(".kdh")) && !str::Eq(ext, StrL(".nh"));
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
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
        if (IsExtUsable(ext) && !HasOurOpenWithEntry(hkey, ext)) {
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

static i64 GetDirSize(Str dir) {
    logf("GetDirSize(%s)\n", dir);
    i64 totalSize = 0;
    DirIter di{dir};
    for (DirIterEntry* de : di) {
        i64 fileSize = GetFileSize(de);
        totalSize += fileSize;
    }
    return totalSize;
}

static void WriteUninstallerInfo(InstallRegWriter& reg, Str installDir, DesktopShortcut shortcut) {
    TempStr installedExePath = path::JoinTemp(installDir, Str(kExeName));
    TempStr installDate = GetInstallDateTemp();
    // uninstaller is the same executable with a different flag
    TempStr uninstallCmdLine = fmt("\"%s\" -uninstall", installedExePath);
    if (reg.root == HKEY_LOCAL_MACHINE) {
        uninstallCmdLine = str::JoinTemp(uninstallCmdLine, StrL(" -all-users"));
    }

    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    // path to installed executable (or "$path,0" to force the first icon)
    reg.String(regPathUninst, StrL("DisplayIcon"), installedExePath);
    reg.String(regPathUninst, StrL("DisplayName"), StrL(kAppName));
    // version format: "1.2"
    reg.String(regPathUninst, StrL("DisplayVersion"), currentVersion);
    // non-recursive because we don't want to count space used for thumbnails
    // which is in installDir for local install
    DWORD size = (DWORD)(GetDirSize(installDir) / 1024);
    // size of installed directory after copying files
    reg.Dword(regPathUninst, StrL("EstimatedSize"), size);
    // current date as YYYYMMDD
    reg.String(regPathUninst, StrL("InstallDate"), installDate);
    reg.String(regPathUninst, StrL("InstallLocation"), installDir);
    reg.Dword(regPathUninst, StrL("NoModify"), 1);
    reg.Dword(regPathUninst, StrL("NoRepair"), 1);
    reg.String(regPathUninst, StrL("Publisher"), StrL(kPublisherStr));
    // command line for uninstaller
    reg.String(regPathUninst, StrL("UninstallString"), uninstallCmdLine);
    TempStr uninstallCmdLineSilent = str::JoinTemp(uninstallCmdLine, StrL(" -silent"));
    reg.String(regPathUninst, StrL("QuietUninstallString"), uninstallCmdLineSilent);
    reg.String(regPathUninst, StrL("URLInfoAbout"), StrL("https://www.sumatrapdfreader.org/"));
    reg.String(regPathUninst, StrL("URLUpdateInfo"),
               StrL("https://www.sumatrapdfreader.org/docs/Version-history.html"));
    reg.Dword(regPathUninst, StrL(kRegDesktopShortcut), shortcut == DesktopShortcut::Create ? 1 : 0);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static void RegisterForDefaultPrograms(InstallRegWriter& reg, Str installedExePath) {
    // L"SOFTWARE\\SumatraPDF\\Capabilities"
    TempStr appCapabilityPath = str::JoinTemp(StrL("SOFTWARE\\"), StrL(kAppName), StrL("\\Capabilities"));

    Str desc = StrL("SumatraPDF is a PDF reader.");
    reg.String(appCapabilityPath, StrL("ApplicationDescription"), desc);
    // ApplicationName must match the RegisteredApplications value name (kAppName).
    reg.String(appCapabilityPath, StrL("ApplicationName"), StrL(kAppName));
    // icon shown next to the app in Settings > Default Apps
    TempStr appIcon = str::JoinTemp(StrL("\""), installedExePath, StrL("\",0"));
    reg.String(appCapabilityPath, StrL("ApplicationIcon"), appIcon);

    // L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations"
    TempStr keyAssoc = str::JoinTemp(appCapabilityPath, StrL("\\FileAssociations"));

    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
        if (!IsExtUsable(ext)) {
            continue;
        }
        // must match the per-extension ProgID created by RegisterForOpenWith
        // (e.g. "SumatraPDF.pdf"); Default Apps UI hides the app if the
        // FileAssociations ProgID can't be resolved under HKCR
        TempStr progIDName = str::JoinTemp(StrL(kAppName), ext);
        reg.String(keyAssoc, ext, progIDName);
    }

    reg.String(StrL("SOFTWARE\\RegisteredApplications"), StrL(kAppName), appCapabilityPath);
}

// Register per-extension ProgIDs and their OpenWithProgids entries.
// Use the same registry root for both so either per-user or all-users works.
static void RegisterForOpenWith(InstallRegWriter& reg, Str installedExePath) {
    TempStr exePathQuoted = str::JoinTemp(StrL("\""), installedExePath, StrL("\""));
    TempStr cmdOpen = str::JoinTemp(exePathQuoted, StrL(" \"%1\""));
    TempStr cmdPrint = str::JoinTemp(exePathQuoted, StrL(" -print-to-default \"%1\""));
    TempStr cmdPrintTo = str::JoinTemp(exePathQuoted, StrL(" -print-to \"%2\" \"%1\""));
    TempStr key;
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
        if (!IsExtUsable(ext)) {
            continue;
        }
        TempStr progIDName = str::JoinTemp(StrL(kAppName), ext);
        TempStr progIDKey = str::JoinTemp(StrL("Software\\Classes\\"), progIDName);

        // Don't set the progID's friendly name (its (Default) value). A hardcoded
        // English string like "PDF File" overrides the localized type name that
        // Windows generates for the file type, so non-English systems wrongly show
        // English names in Explorer's "Type" column (issue #3323). Delete any value
        // a previous version wrote so Windows falls back to the localized name.
        reg.Delete(progIDKey, {});

        TempStr iconPath = fmt("%s,-%d", exePathQuoted, type.iconId);

        key = str::JoinTemp(progIDKey, StrL("\\Application"));
        reg.String(key, StrL("ApplicationCompany"), StrL("Krzysztof Kowalczyk"));
        reg.String(key, StrL("ApplicationName"), StrL(kAppName));

        key = str::JoinTemp(progIDKey, StrL("\\DefaultIcon"));
        reg.String(key, {}, iconPath);

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open"));
        reg.String(key, StrL("Icon"), iconPath);
        reg.String(key, StrL("MultiSelectModel"), StrL("Player"));

        key = str::JoinTemp(progIDKey, StrL("\\shell\\open\\command"));
        reg.String(key, {}, cmdOpen);

        // for PDF also register for Print/PrintTo shell actions
        if (str::Eq(ext, StrL(".pdf"))) {
            key = str::JoinTemp(progIDKey, StrL("\\shell\\Print\\command"));
            reg.String(key, {}, cmdPrint);

            key = str::JoinTemp(progIDKey, StrL("\\shell\\PrintTo\\command"));
            reg.String(key, {}, cmdPrintTo);
        }

        key = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        reg.None(key, progIDName);
    }
}

#define kRegExplorerPdfExt "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define kRegClassesPdf "Software\\Classes\\.pdf"

static TempStr GetRegClassesAppsTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Classes\\Applications\\"), appName, StrL(".exe"));
}

InstallRegResult InstallRegistryInfo(HKEY root, Str installDir, DesktopShortcut shortcut) {
    // Closing the transaction without a commit rolls back all of its changes.
    AutoCloseHandle transaction(CreateTransaction(nullptr, nullptr, TRANSACTION_DO_NOT_PROMOTE, 0, 0, 0, nullptr));
    if (!transaction.IsValid()) {
        LogLastError();
        return InstallRegResult::UninstallInfoFailed;
    }

    InstallRegWriter reg{root, transaction};
    WriteUninstallerInfo(reg, installDir, shortcut);
    if (!reg.ok) {
        return InstallRegResult::UninstallInfoFailed;
    }

    TempStr exePath = path::JoinTemp(installDir, Str(kExeName));
    if (IsWindows10OrGreater()) {
        RegisterForDefaultPrograms(reg, exePath);
    }
    RegisterForOpenWith(reg, exePath);

    // These shared MIME values are retained by the uninstaller.
    reg.String(StrL(kRegClassesPdf), StrL("Content Type"), StrL("application/pdf"));
    reg.String(StrL(R"(Software\Classes\MIME\Database\Content Type\application/pdf)"), StrL("Extension"), StrL(".pdf"));
    if (!reg.ok) {
        return InstallRegResult::AssociationsFailed;
    }
    if (!CommitTransaction(transaction)) {
        LogLastError();
        return InstallRegResult::CommitFailed;
    }

    return InstallRegResult::Complete;
}

Str InstallRegError(InstallRegResult result) {
    switch (result) {
        case InstallRegResult::UninstallInfoFailed:
        case InstallRegResult::CommitFailed:
            return Tr("Failed to write the uninstallation information to the registry");
        case InstallRegResult::AssociationsFailed:
            return Tr("Failed to write the extended file extension information to the registry");
        case InstallRegResult::Complete:
            return {};
    }
    return {};
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
    if (str::Eq(curr, StrL(kAppName))) {
        LoggedDeleteRegValue(hkey, StrL(kRegClassesPdf), {});
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    TempStr buf = LoggedReadRegStrTemp(hkey, StrL(kRegExplorerPdfExt), StrL("ProgId"));
    if (str::Eq(buf, StrL(kAppName))) {
        LoggedDeleteRegValue(hkey, StrL(kRegExplorerPdfExt), StrL("ProgId"));
    }
    buf = LoggedReadRegStrTemp(hkey, StrL(kRegExplorerPdfExt), StrL("Application"));
    if (str::EqI(buf, Str(kExeName))) {
        LoggedDeleteRegValue(hkey, StrL(kRegExplorerPdfExt), StrL("Application"));
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
    status = RegQueryInfoKeyW(hkey, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr,
                              nullptr, nullptr);
    bool isEmpty = status == ERROR_SUCCESS && subkeys == 0 && values == 0;
    RegCloseKey(hkey);
    if (isEmpty) {
        LoggedDeleteRegKey(root, keyName);
    }
    return isEmpty;
}

static void RemoveLegacyInstallKeys(HKEY hkey) {
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
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
        TempStr keyname = str::JoinTemp(StrL("Software\\Classes\\"), ext, StrL("\\OpenWithProgids"));
        LoggedDeleteRegValue(hkey, keyname, StrL(kAppName));
        DeleteEmptyRegKey(hkey, keyname);

        keyname = str::JoinTemp(StrL("Software\\Classes\\"), ext, openWithVal);
        if (LoggedDeleteRegKey(hkey, keyname)) {
            // remove empty parent keys that the installer might have created
            if (str::CutCharLast(keyname, '\\', &keyname, nullptr) && keyname && DeleteEmptyRegKey(hkey, keyname)) {
                if (str::CutCharLast(keyname, '\\', &keyname, nullptr) && keyname) {
                    DeleteEmptyRegKey(hkey, keyname);
                }
            }
        }
    }
}

void RemoveInstallRegistryKeys(HKEY hkey) {
    logf("RemoveInstallRegistryKeys(%s)\n", RegKeyNameTemp(hkey));
    RemoveLegacyInstallKeys(hkey);

    // those were introduced in 3.4
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
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

void RemovePreviousInstallRegistry(HKEY installed, bool wasAllUsers) {
    RemoveLegacyInstallKeys(installed);
    if (installed != HKEY_LOCAL_MACHINE && wasAllUsers) {
        RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
        RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    }
    if (installed != HKEY_CURRENT_USER) {
        RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
        RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    }
    ShellNotifyAssociationsChanged();
}

// Restore missing Open With entries that other apps may remove.
// Only installed copies register; machine entries require an all-users install.
void ReRegisterFileAssociations() {
    if (!IsOurExeInstalled()) {
        return;
    }
    TempStr exePath = GetSelfExePathTemp();
    if (len(exePath) == 0) {
        return;
    }

    bool didRegister = false;
    const HKEY roots[] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
    for (HKEY hkey : roots) {
        if (hkey == HKEY_LOCAL_MACHINE &&
            !HasRegistryValue(hkey, GetRegPathUninstTemp(StrL(kAppName)), StrL("InstallLocation"))) {
            continue;
        }
        if (HasAllOurOpenWithEntries(hkey)) {
            continue;
        }
        InstallRegWriter reg{hkey};
        RegisterForOpenWith(reg, exePath);
        if (IsWindows10OrGreater()) {
            RegisterForDefaultPrograms(reg, exePath);
        }
        didRegister = true;
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
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
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
    for (const auto& type : gSupportedExts) {
        Str ext = type.ext;
        // only list types with an explicit UserChoice that isn't us — otherwise
        // every registered format (e.g. .tga) would show after a fresh install
        TempStr userChoice = ReadUserChoiceProgIdTemp(ext);
        if (len(userChoice) == 0 || IsOurProgId(userChoice, ext)) {
            continue;
        }
        if (HaveRegisteredOpenWithForExt(ext) && !IsSumatraDefaultForExt(ext)) {
            out.Append(ext);
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
