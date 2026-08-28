/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Crypto.h"

#include "gui/UIModels.h"

#include "SumatraConfig.h"
#include "Translations.h"
#include "Version.h"
#include "AppTools.h"

/* Returns true, if a Registry entry indicates that this executable has been
   created by an installer (and should be updated through an installer) */
static bool HasBeenInstalled() {
    // see GetDefaultInstallationDir() in Installer.cpp
    TempStr regPathUninst =
        str::JoinTemp(StrL("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"), StrL(kAppName));
    TempStr installedPath = LoggedReadRegStr2Temp(regPathUninst, StrL("InstallLocation"));
    if (!installedPath) {
        return false;
    }

    TempStr exePath = GetSelfExePathTemp();
    if (!str::EndsWithI(installedPath, StrL(".exe"))) {
        installedPath = path::JoinTemp(installedPath, path::GetBaseNameTemp(exePath));
    }
    return path::IsSame(installedPath, exePath);
}

static bool PathStripBaseNameInPlace(Str& path) {
    if (!path.s) {
        return false;
    }
    TempStr base = path::GetBaseNameTemp(path);
    if (base.s > path.s) {
        base.s[-1] = 0;
        path.len = (int)(base.s - path.s - 1);
        return true;
    }
    return false;
}

// return true if path is in a given dir, even if dir is a junction etc.
static bool IsPathInDirSmart(Str path, Str dir) {
    TempStr work = str::DupTemp(path);
    Str p = work;
    while (p) {
        if (path::IsSame(dir, p)) {
            return true;
        }
        if (!PathStripBaseNameInPlace(p)) {
            break;
        }
    }
    return false;
}

static bool IsExeInProgramFiles() {
    TempStr exePath = GetSelfExePathTemp();
    TempStr dir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES);
    if (IsPathInDirSmart(exePath, dir)) {
        return true;
    }
    dir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILESX86);
    if (IsPathInDirSmart(exePath, dir)) {
        return true;
    }
    return false;
}

/* Return false if this program has been started from "Program Files" directory
   (which is an indicator that it has been installed) or from the last known
   location of a SumatraPDF installation: */
bool IsRunningInPortableMode() {
    // cache the result so that it will be consistent during the lifetime of the process
    static int sCacheIsPortable = -1; // -1 == uninitialized, 0 == installed, 1 == portable
    if (sCacheIsPortable != -1) {
        return sCacheIsPortable != 0;
    }

    sCacheIsPortable = 0;
    if (gIsStoreBuild) {
        return false;
    }

    if (HasBeenInstalled()) {
        return false;
    }

    if (!IsExeInProgramFiles()) {
        sCacheIsPortable = 1;
    }
    return sCacheIsPortable != 0;
}

bool IsDllBuild() {
    HRSRC resSrc = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), RT_RCDATA);
    return resSrc != nullptr;
}

// true if the executable name indicates installer or uninstaller mode
// (e.g. SumatraPDF-prerel-64-install.exe)
bool IsInstallerOrUninstallerExe() {
    TempStr exeName = path::GetBaseNameTemp(GetSelfExePathTemp());
    return str::ContainsI(exeName, StrL("uninstall")) || str::ContainsI(exeName, StrL("install"));
}

static Str gAppDataDir;

void DeleteAppTools() {
    // gAppDataDir is allocated from gPermArena (freed wholesale on exit)
    gAppDataDir = {};
}

void SetAppDataDir(Str dir) {
    dir = path::NormalizeTemp(dir);
    // don't try to create root directories like d:\ (CreateAll would fail)
    bool isRootDir = len(dir) == 3 && dir.s[1] == ':' && dir.s[2] == '\\';
    if (!isRootDir) {
        bool ok = dir::CreateAll(dir);
        if (!ok) {
            logf("SetAppDataDir: failed to create directory '%s'\n", dir);
            LogLastError();
            ReportIf(true);
        }
    }
    // lives for the whole program: allocate from the perm arena. SetAppDataDir
    // is called at most a couple of times (default + a -appdata override), so the
    // (rare) replaced value being retained until exit is negligible.
    gAppDataDir = str::Dup(GetPermArena(), dir);
}

TempStr GetAppDataDirTemp() {
    if (gAppDataDir) {
        return gAppDataDir;
    }
    bool isPortable = IsRunningInPortableMode();
    TempStr dir;
    if (isPortable) {
        dir = GetSelfExeDirTemp();
        // sometimes people put executable in directory like c:\windows
        // and we can't write to it. in that case we'll fall back to %APPDATA%
        if (!dir::HasWriteAccess(dir)) {
            logf("GetAppDataDirTemp: no write access to '%s'\n", dir);
            dir = {};
        }
    }
    if (!dir) {
        dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
        if (!dir) {
            LogLastError();
            ReportIf(true);
            dir = GetTempDirTemp(); // shouldn't happen, last chance thing
        }
        dir = path::JoinTemp(dir, StrL(kAppName));
    }
    logf("GetAppDataDirTemp(): '%s'%s\n", dir, Str(isPortable ? " (portable)" : "(installed)"));
    SetAppDataDir(dir);
    return gAppDataDir;
}

// Generate full path for a file or directory for storing data
TempStr GetPathInAppDataDirTemp(Str name) {
    if (!name) {
        return {};
    }
    TempStr dir = GetAppDataDirTemp();
    return path::JoinTemp(dir, name);
}

// List of rules used to detect TeX editors.

#define kRegCurrentVer "Software\\Microsoft\\Windows\\CurrentVersion"

// clang-format off
static TextEditor editorRules[] = {
    {
        StrL("Code.exe"),
        StrL(R"(--goto "%f:%l")"),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\Uninstall\\{771FD6B0-FA20-440A-A002-3B3BAC16DC50}_is1"),
        // TODO: change back to Code.exe
        // the way vscode saves a file seems to break
        // our reloading of settings
        StrL("DisplayIcon"),
    },
    {
        StrL("WinEdt.exe"),
         StrL("\"[Open(|%f|);SelPar(%l,8)]\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\App Paths\\WinEdt.exe"),
        {}
    },
    {
        StrL("WinEdt.exe"),
        StrL("\"[Open(|%f|);SelPar(%l,8)]\""),
        RegType::BinaryDir,
        StrL("Software\\WinEdt"),
        StrL("Install Root"),
    },
    {
        StrL("notepad++.exe"),
        StrL("-n%l \"%f\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\App Paths\\notepad++.exe"),
        {}
    },
    {
        StrL("notepad++.exe"),
        StrL("-n%l \"%f\""),
        RegType::BinaryDir,
        StrL("Software\\Notepad++"),
        {}
    },
    {
        StrL("notepad++.exe"),
        StrL("-n%l \"%f\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\Uninstall\\Notepad++"),
        StrL("DisplayIcon"),
    },
    {
        StrL("sublime_text.exe"),
        StrL("\"%f:%l:%c\""),
       RegType:: BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\Sublime Text 3_is1"),
        StrL("InstallLocation"),
    },
    {
        StrL("sublime_text.exe"),
        StrL("\"%f:%l:%c\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\Uninstall\\Sublime Text 3_is1"),
        StrL("DisplayIcon"),
    },
    {
        StrL("sublime_text.exe"),
        StrL("\"%f:%l:%c\""),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\Sublime Text 2_is1"),
         StrL("InstallLocation"),
    },
    {
        StrL("sublime_text.exe"),
        StrL("\"%f:%l:%c\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\Uninstall\\Sublime Text 2_is1"),
        StrL("DisplayIcon"),
    },
    {
        StrL("sublime_text.exe"),
        StrL("\"%f:%l:%c\""),
        RegType::BinaryPath,
        StrL(kRegCurrentVer "\\Uninstall\\Sublime Text_is1"),
        StrL("DisplayIcon"),
    },
    {
        StrL("TeXnicCenter.exe"),
        StrL("/ddecmd \"[goto('%f', '%l')]\""),
        RegType::BinaryDir,
        StrL("Software\\ToolsCenter\\TeXnicCenterNT"),
        StrL("AppPath"),
    },
    {
        StrL("TeXnicCenter.exe"),
        StrL("/ddecmd \"[goto('%f', '%l')]\""),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\TeXnicCenter_is1"),
        StrL("InstallLocation"),
    },
    {
        StrL("TeXnicCenter.exe"),
        StrL("/ddecmd \"[goto('%f', '%l')]\""),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\TeXnicCenter Alpha_is1"),
        StrL("InstallLocation"),
    },
    {
        StrL("TEXCNTR.exe"),
        StrL("/ddecmd \"[goto('%f', '%l')]\""),
        RegType::BinaryDir,
        StrL("Software\\ToolsCenter\\TeXnicCenter"),
        StrL("AppPath"),
    },
    {
        StrL("TEXCNTR.exe"),
        StrL("/ddecmd \"[goto('%f', '%l')]\""),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\TeXnicCenter_is1"),
        StrL("InstallLocation"),
    },
    {
        StrL("WinShell.exe"),
        StrL("-c \"%f\" -l %l"),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\WinShell_is1"),
        StrL("InstallLocation"),
    },
    {
        StrL("gvim.exe"),
        StrL("\"%f\" +%l"),
        RegType::BinaryPath,
        StrL("Software\\Vim\\Gvim"),
        StrL("path"),
    },
    {
        // TODO: add this rule only if the latex-suite for ViM is installed
        // (http://vim-latex.sourceforge.net/documentation/latex-suite.txt)
        StrL("gvim.exe"),
        StrL("-c \":RemoteOpen +%l %f\""),
        RegType::BinaryPath,
        StrL("Software\\Vim\\Gvim"),
        StrL("path"),
    },
    {
        StrL("texmaker.exe"),
        StrL("\"%f\" -line %l"),
        RegType::SiblingPath,
        StrL(kRegCurrentVer "\\Uninstall\\Texmaker"),
        StrL("UninstallString"),
    },
    {
        StrL("TeXworks.exe"),
        StrL("-p=%l \"%f\""),
        RegType::BinaryDir,
        StrL(kRegCurrentVer "\\Uninstall\\{41DA4817-4D2A-4D83-AD02-6A2D95DC8DCB}_is1"),
        StrL("InstallLocation"),
        // TODO: find a way to detect where emacs is installed
        // "emacsclientw.exe","+%l \"%f\"", BinaryPath, "???", "???",
    },
    {
        StrL("notepad.exe"),
        StrL("\"%f\""),
        RegType::BinaryDir,
        StrL(R"(Software\Microsoft\Windows NT\CurrentVersion)"),
        StrL("SystemRoot"),
    }
};

// clang-format on

static bool didFindTextEditors = false;
static void FindTextEditors() {
    if (didFindTextEditors) {
        return;
    }
    StrVec found;
    // all but last entry, which is notepad.exe
    int n = dimofi(editorRules) - 1;
    for (int i = 0; i < n; i++) {
        auto& rule = editorRules[i];
        Str regKey = rule.regKey;
        Str regValue = rule.regValue;
        TempStr path = LoggedReadRegStr2Temp(regKey, regValue);
        if (!path) {
            continue;
        }

        TempStr exePath;
        Str binaryFileName = rule.binaryFilename;
        Str inverseSearchArgs = rule.inverseSearchArgs;
        if (rule.type == RegType::SiblingPath) {
            // remove file part
            TempStr dir = path::GetDirTemp(path);
            exePath = path::JoinTemp(dir, binaryFileName);
        } else if (rule.type == RegType::BinaryDir) {
            exePath = path::JoinTemp(path, binaryFileName);
        } else { // if (editor_rules[i].Type == BinaryPath)
            exePath = path;
        }
        // don't show duplicate entries
        if (found.FindI(exePath) != -1) {
            continue;
        }
        // don't show inexistent paths (and don't try again for them)
        if (!file::Exists(exePath)) {
            found.Append(exePath);
            continue;
        }

        rule.fullPath = str::Dup(exePath);
        rule.openFileCmd = str::Dup(fmt("\"%s\" %s", exePath, inverseSearchArgs));
        found.Append(exePath);
    }
    didFindTextEditors = true;
}

// Detect TeX editors installed on the system and construct the
// corresponding inverse search commands.
void DetectTextEditors(Vec<TextEditor*>& res) {
    FindTextEditors();
    int n = dimofi(editorRules);
    for (int i = 0; i < n; i++) {
        TextEditor* e = &editorRules[i];
        if (!e->openFileCmd) {
            continue;
        }
        VecAppend(res, e);
    }
}

// Detected text-editor command lines plus the current setting, if any.
void CollectInverseSearchCommands(StrVec& out, Str cmdLine) {
    out.Reset();
    Vec<TextEditor*> textEditors;
    DetectTextEditors(textEditors);
    for (auto* e : textEditors) {
        AppendIfNotExists(&out, e->openFileCmd);
    }
    if (cmdLine) {
        AppendIfNotExists(&out, cmdLine);
    }
}

/* Default size for the window, happens to be american A4 size (I think) */
constexpr double kDefPageRatio = 612.0 / 792.0;

constexpr int kMinWinDx = 50;
constexpr int kMinWinDy = 50;

void EnsureAreaVisibility(Rect& r) {
    // adjust to the work-area of the current monitor (not necessarily the primary one)
    Rect work = GetWorkAreaRect(r, nullptr);

    // make sure that the window is neither too small nor bigger than the monitor
    if (r.dx < kMinWinDx || r.dx > work.dx) {
        r.dx = std::min((int)((double)work.dy * kDefPageRatio), work.dx);
    }
    if (r.dy < kMinWinDy || r.dy > work.dy) {
        r.dy = work.dy;
    }

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    Rect halfCaption(r.x, r.y + (captionDy / 2), r.dx, captionDy / 2);
    if (halfCaption.Intersect(work).IsEmpty()) {
        r = Rect(work.TL(), r.Size());
    }
}

Rect GetDefaultWindowPos() {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    Rect work = ToRect(workArea);

    Rect r = work;
    r.dx = std::min((int)((double)r.dy * kDefPageRatio), work.dx);
    r.x = (work.dx - r.dx) / 2;

    return r;
}

void SaveCallstackLogs() {
    Str s = dbghelp::GetCallstacks();
    if (len(s) == 0) {
        return;
    }
    TempStr filePath = GetPathInAppDataDirTemp(StrL("callstacks.txt"));
    file::WriteFile(filePath, s);
    str::Free(s);
}

// TODO: this can be used for extracting other data
// cache because calculating sha1 of the whole executable
// might be relatively expensive
// sha1 is 20 bytes => 40 hex chars + null terminator
static char gAppSha1[41];

// return hex version of sha1 of app's executable (pointer to cached value)
// nullptr if there was an error
Str Sha1OfAppExe() {
    if (gAppSha1[0]) {
        return Str(gAppSha1);
    }

    TempStr appPath = GetSelfExePathTemp();
    if (!appPath) {
        return {};
    }
    Str d = file::ReadFile(appPath);
    if (len(d) == 0) {
        return {};
    }

    u8 sha1[20]{};
    CalcSHA1Digest(d, sha1);
    str::Free(d);

    for (size_t i = 0; i < 20; i++) {
        sprintf_s(&gAppSha1[2 * i], 3, "%02x", sha1[i]);
    }
    return Str(gAppSha1);
}

TempStr GetWebViewDataDirTemp() {
    TempStr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (!dir) {
        return {};
    }
    dir = path::JoinTemp(dir, StrL("SumatraPDF-data"));
    char id[7] = "000000";
    Str sha1 = Sha1OfAppExe();
    if (sha1) {
        str::BufSet(Str(id, dimof(id)), sha1);
    }
    dir = path::JoinTemp(dir, Str(id));
    return path::JoinTemp(dir, fmt("webview-%d", (int)GetCurrentProcessId()));
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
TempStr FormatFileSizeShortTransTemp(i64 size) {
    Str units[3] = {_TRA("GB"), _TRA("MB"), _TRA("KB")};
    return str::FormatSizeShortTemp(size, units);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTransTemp(i64 size) {
    if (size <= 0) {
        return fmt("%d", size);
    }
    TempStr n1 = FormatFileSizeShortTransTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt("%s (%s %s)", n1, n2, _TRA("Bytes"));
}

// returns true if file exists
bool LaunchFileIfExists(Str path) {
    if (!path) {
        return false;
    }
    if (!file::Exists(path)) {
        logf("LaunchFileIfExists: !file::Exists('%s')\n", path);
        return false;
    }
    if (gIsStoreBuild) {
        path = path::GetNonVirtualTemp(path);
        logf("LaunchFileIfExists: gIsStoreBuild, path='%s'\n", path);
    }
    LaunchFileShell(path, {}, StrL("open"));
    return true;
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
// returns true if the path has been changed
bool AdjustVariableDriveLetter(Str& path) {
    // Don't bother if the file path is still valid
    if (file::Exists(path)) {
        return false;
    }
    // only check absolute path on drives i.e. those that start with "d:\"
    if (len(path) < 4 || path.s[1] != ':') {
        return false;
    }

    // Iterate through all (other) removable drives and try to find the file there
    char szDrive[] = "A:\\";
    char origDrive = path.s[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::HasVariableDriveLetter(Str(szDrive))) {
            path.s[0] = szDrive[0];
            if (file::Exists(path)) {
                return true;
            }
        }
        szDrive[0]++;
    }
    path.s[0] = origDrive;
    return false;
}

// files are considered untrusted, if they're either loaded from a
// non-file URL in plugin mode, or if they're marked as being from
// an untrusted zone (e.g. by the browser that's downloaded them)
bool IsUntrustedFile(Str filePath, Str fileURL) {
    TempStr protocol;
    if (fileURL && !str::IsNull(str::Parse(fileURL, "%S:", &protocol))) {
        if (len(protocol) > 1 && !str::EqI(protocol, StrL("file"))) {
            return true;
        }
    }

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET) {
        return true;
    }

    // check all parents of embedded files and ADSs as well
    TempStr path = str::DupTemp(filePath);
    while (len(path) > 2 && str::ContainsChar(Str(path.s + 2, path.len - 2), ':')) {
        Str lastColon = str::SliceFromCharLast(path, ':');
        *lastColon.s = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET) {
            return true;
        }
    }

    return false;
}
