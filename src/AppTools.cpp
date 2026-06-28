/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/CryptoUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "SumatraConfig.h"
#include "Translations.h"
#include "Version.h"
#include "AppTools.h"

bool NeedsWindowEmbeddingHacks();

#include "utils/Log.h"

/* Returns true, if a Registry entry indicates that this executable has been
   created by an installer (and should be updated through an installer) */
static bool HasBeenInstalled() {
    // see GetDefaultInstallationDir() in Installer.cpp
    TempStr regPathUninst = str::JoinTemp("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", kAppName);
    TempStr installedPath = LoggedReadRegStr2Temp(regPathUninst, "InstallLocation");
    if (!installedPath) {
        return false;
    }

    TempStr exePath = GetSelfExePathTemp();
    if (!str::EndsWithI(installedPath, ".exe")) {
        installedPath = path::JoinTemp(installedPath.s, path::GetBaseNameTemp(exePath).s);
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
    TempStr work = StrDupTemp(path);
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
    return str::FindI(exeName, Str("uninstall")).s || str::FindI(exeName, Str("install")).s;
}

static Str gAppDataDir;

void DeleteAppTools() {
    // gAppDataDir is allocated from gLifetimeArena (freed wholesale on exit)
    gAppDataDir = {};
}

void SetAppDataDir(Str dir) {
    dir = path::NormalizeTemp(dir);
    // don't try to create root directories like d:\ (CreateAll would fail)
    bool isRootDir = str::Len(dir) == 3 && dir.s[1] == ':' && dir.s[2] == '\\';
    if (!isRootDir) {
        bool ok = dir::CreateAll(dir);
        if (!ok) {
            logf("SetAppDataDir: failed to create directory '%s'\n", dir.s);
            LogLastError();
            ReportIf(true);
        }
    }
    // lives for the whole program: allocate from the lifetime arena. SetAppDataDir
    // is called at most a couple of times (default + a -appdata override), so the
    // (rare) replaced value being retained until exit is negligible.
    gAppDataDir = Str(str::Dup(GetLifetimeArena(), dir));
}

TempStr GetAppDataDirTemp() {
    if (gAppDataDir) {
        return gAppDataDir.s;
    }
    bool isPortable = IsRunningInPortableMode();
    TempStr dir = nullptr;
    if (isPortable) {
        dir = GetSelfExeDirTemp();
        // sometimes people put executable in directory like c:\windows
        // and we can't write to it. in that case we'll fall back to %APPDATA%
        if (!dir::HasWriteAccess(dir)) {
            logf("GetAppDataDirTemp: no write access to '%s'\n", dir);
            dir = nullptr;
        }
    }
    if (!dir) {
        dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
        if (!dir) {
            LogLastError();
            ReportIf(true);
            dir = GetTempDirTemp(); // shouldn't happen, last chance thing
        }
        dir = path::JoinTemp(dir, kAppName);
    }
    logf("GetAppDataDirTemp(): '%s'%s\n", dir, isPortable ? " (portable)" : "(installed)");
    SetAppDataDir(dir);
    return gAppDataDir.s;
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
        "Code.exe",
        R"(--goto "%f:%l")",
        RegType::BinaryPath,
        kRegCurrentVer "\\Uninstall\\{771FD6B0-FA20-440A-A002-3B3BAC16DC50}_is1",
        // TODO: change back to Code.exe
        // the way vscode saves a file seems to break
        // our reloading of settings
        "DisplayIcon"
    },
    {
        "WinEdt.exe",
         "\"[Open(|%f|);SelPar(%l,8)]\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\App Paths\\WinEdt.exe",
        nullptr
    },
    {
        "WinEdt.exe",
        "\"[Open(|%f|);SelPar(%l,8)]\"",
        RegType::BinaryDir,
        "Software\\WinEdt",
        "Install Root"
    },
    {
        "notepad++.exe",
        "-n%l \"%f\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\App Paths\\notepad++.exe",
        nullptr
    },
    {
        "notepad++.exe",
        "-n%l \"%f\"",
        RegType::BinaryDir,
        "Software\\Notepad++",
        nullptr
    },
    {
        "notepad++.exe",
        "-n%l \"%f\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\Uninstall\\Notepad++",
        "DisplayIcon"
    },
    {
        "sublime_text.exe",
        "\"%f:%l:%c\"",
       RegType:: BinaryDir,
        kRegCurrentVer "\\Uninstall\\Sublime Text 3_is1",
        "InstallLocation"
    },
    {
        "sublime_text.exe",
        "\"%f:%l:%c\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\Uninstall\\Sublime Text 3_is1",
        "DisplayIcon"
    },
    {
        "sublime_text.exe",
        "\"%f:%l:%c\"",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\Sublime Text 2_is1",
         "InstallLocation"
    },
    {
        "sublime_text.exe",
        "\"%f:%l:%c\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\Uninstall\\Sublime Text 2_is1",
        "DisplayIcon"
    },
    {
        "sublime_text.exe",
        "\"%f:%l:%c\"",
        RegType::BinaryPath,
        kRegCurrentVer "\\Uninstall\\Sublime Text_is1",
        "DisplayIcon"
    },
    {
        "TeXnicCenter.exe",
        "/ddecmd \"[goto('%f', '%l')]\"",
        RegType::BinaryDir,
        "Software\\ToolsCenter\\TeXnicCenterNT",
        "AppPath"
    },
    {
        "TeXnicCenter.exe",
        "/ddecmd \"[goto('%f', '%l')]\"",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\TeXnicCenter_is1",
        "InstallLocation"
    },
    {
        "TeXnicCenter.exe",
        "/ddecmd \"[goto('%f', '%l')]\"",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\TeXnicCenter Alpha_is1",
        "InstallLocation"
    },
    {
        "TEXCNTR.exe",
        "/ddecmd \"[goto('%f', '%l')]\"",
        RegType::BinaryDir,
        "Software\\ToolsCenter\\TeXnicCenter",
        "AppPath"
    },
    {
        "TEXCNTR.exe",
        "/ddecmd \"[goto('%f', '%l')]\"",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\TeXnicCenter_is1",
        "InstallLocation"
    },
    {
        "WinShell.exe",
        "-c \"%f\" -l %l",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\WinShell_is1",
        "InstallLocation"
    },
    {
        "gvim.exe",
        "\"%f\" +%l",
        RegType::BinaryPath,
        "Software\\Vim\\Gvim",
        "path"
    },
    {
        // TODO: add this rule only if the latex-suite for ViM is installed
        // (http://vim-latex.sourceforge.net/documentation/latex-suite.txt)
        "gvim.exe",
        "-c \":RemoteOpen +%l %f\"",
        RegType::BinaryPath,
        "Software\\Vim\\Gvim",
        "path"
    },
    {
        "texmaker.exe",
        "\"%f\" -line %l",
        RegType::SiblingPath,
        kRegCurrentVer "\\Uninstall\\Texmaker",
        "UninstallString"
    },
    {
        "TeXworks.exe",
        "-p=%l \"%f\"",
        RegType::BinaryDir,
        kRegCurrentVer "\\Uninstall\\{41DA4817-4D2A-4D83-AD02-6A2D95DC8DCB}_is1",
        "InstallLocation",
        // TODO: find a way to detect where emacs is installed
        // "emacsclientw.exe","+%l \"%f\"", BinaryPath, "???", "???",
    },
    {
        "notepad.exe",
        "\"%f\"",
        RegType::BinaryDir,
        "Software\\Microsoft\\Windows NT\\CurrentVersion",
        "SystemRoot",
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
    int n = (int)dimof(editorRules) - 1;
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

        rule.fullPath = Str(str::Dup(exePath));
        rule.openFileCmd = Str(str::Format("\"%s\" %s", exePath, inverseSearchArgs));
        found.Append(exePath);
    }
    didFindTextEditors = true;
}

// Detect TeX editors installed on the system and construct the
// corresponding inverse search commands.
void DetectTextEditors(Vec<TextEditor*>& res) {
    FindTextEditors();
    int n = (int)dimof(editorRules);
    for (int i = 0; i < n; i++) {
        TextEditor* e = &editorRules[i];
        if (!e->openFileCmd) {
            continue;
        }
        res.Append(e);
    }
}

// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the caller must free() the result
Str BuildOpenFileCmd(Str pattern, Str path, int line, int col) {
    StrBuilder cmdline(256);

    logf("BuildOpenFileCmd: path: '%s', pattern: '%s'\n", path, pattern);
    Str s = pattern;
    while (s) {
        Str perc = str::FindChar(s, '%');
        if (!perc) {
            cmdline.Append(s);
            break;
        }
        cmdline.Append(Str(s.s, (int)(perc.s - s.s)));
        if (perc.len < 2) {
            cmdline.Append(perc);
            break;
        }
        char spec = perc.s[1];
        if (spec == 'f') {
            cmdline.Append(path);
        } else if (spec == 'l') {
            cmdline.AppendFmt("%d", line);
        } else if (spec == 'c') {
            cmdline.AppendFmt("%d", col);
        } else if (spec == '%') {
            cmdline.AppendChar('%');
        } else {
            cmdline.Append(Str(perc.s, 2));
        }
        s = Str(perc.s + 2, s.len - (int)(perc.s - s.s) - 2);
    }

    return Str(cmdline.StealData());
}

#define UWM_DELAYED_SET_FOCUS (WM_APP + 1)

// selects all text in an edit box if it's selected either
// through a keyboard shortcut or a non-selecting mouse click
// (or responds to Ctrl+Backspace as nowadays expected)
bool ExtendedEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM) {
    static bool delayFocus = false;

    switch (msg) {
        case WM_LBUTTONDOWN:
            delayFocus = !HwndIsFocused(hwnd);
            if (delayFocus && NeedsWindowEmbeddingHacks()) {
                HWND hwndFg = GetForegroundWindow();
                DWORD fgTid = hwndFg ? GetWindowThreadProcessId(hwndFg, nullptr) : 0;
                DWORD ourTid = GetCurrentThreadId();
                bool attached = false;
                if (fgTid && fgTid != ourTid) {
                    attached = AttachThreadInput(ourTid, fgTid, TRUE) != 0;
                }
                SetFocus(hwnd);
                if (attached) {
                    AttachThreadInput(ourTid, fgTid, FALSE);
                }
            }
            return true;

        case WM_LBUTTONUP: {
            if (delayFocus) {
                DWORD sel = Edit_GetSel(hwnd);
                if (LOWORD(sel) == HIWORD(sel)) {
                    PostMessageW(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
                }
                delayFocus = false;
            }
            return true;
        }

        case WM_KILLFOCUS:
            return false; // for easier debugging (make setting a breakpoint possible)

        case WM_SETFOCUS: {
            if (!delayFocus) {
                PostMessageW(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
            }
            return true;
        }

        case UWM_DELAYED_SET_FOCUS: {
            EditSelectAll(hwnd);
            return true;
        }

        case WM_KEYDOWN: {
            bool isCtrlBack = (VK_BACK == wp) && IsCtrlPressed() && !IsShiftPressed() && !IsAltPressed();
            if (isCtrlBack) {
                PostMessageW(hwnd, UWM_DELAYED_CTRL_BACK, 0, 0);
                return true;
            }
            return false;
        }

        case UWM_DELAYED_CTRL_BACK: {
            EditImplementCtrlBack(hwnd);
            return true;
        }

        default:
            return false;
    }
}

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO (612.0 / 792.0)

#define MIN_WIN_DX 50
#define MIN_WIN_DY 50

void EnsureAreaVisibility(Rect& r) {
    // adjust to the work-area of the current monitor (not necessarily the primary one)
    Rect work = GetWorkAreaRect(r, nullptr);

    // make sure that the window is neither too small nor bigger than the monitor
    if (r.dx < MIN_WIN_DX || r.dx > work.dx) {
        r.dx = std::min((int)((double)work.dy * DEF_PAGE_RATIO), work.dx);
    }
    if (r.dy < MIN_WIN_DY || r.dy > work.dy) {
        r.dy = work.dy;
    }

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    Rect halfCaption(r.x, r.y + captionDy / 2, r.dx, captionDy / 2);
    if (halfCaption.Intersect(work).IsEmpty()) {
        r = Rect(work.TL(), r.Size());
    }
}

Rect GetDefaultWindowPos() {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    Rect work = ToRect(workArea);

    Rect r = work;
    r.dx = std::min((int)((double)r.dy * DEF_PAGE_RATIO), work.dx);
    r.x = (work.dx - r.dx) / 2;

    return r;
}

void SaveCallstackLogs() {
    ByteSlice s = dbghelp::GetCallstacks();
    if (s.empty()) {
        return;
    }
    TempStr filePath = GetPathInAppDataDirTemp("callstacks.txt");
    file::WriteFile(filePath, s);
    s.Free();
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
        return nullptr;
    }
    ByteSlice d = file::ReadFile(appPath);
    if (d.empty()) {
        return nullptr;
    }

    u8 sha1[20]{};
    CalcSHA1Digest(d.data(), d.Size(), sha1);
    d.Free();

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
    dir = path::JoinTemp(dir, "SumatraPDF-data");
    char id[7] = "000000";
    Str sha1 = Sha1OfAppExe();
    if (sha1) {
        str::BufSet(id, dimof(id), sha1);
    }
    dir = path::JoinTemp(dir, id);
    return path::JoinTemp(dir, "webview");
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
TempStr FormatSizeShortTransTemp(i64 size) {
    Str units[3] = {_TRA("GB"), _TRA("MB"), _TRA("KB")};
    return str::FormatSizeShortTemp(size, units);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTransTemp(i64 size) {
    if (size <= 0) {
        return fmt::FormatTemp("%d", size);
    }
    TempStr n1 = FormatSizeShortTransTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt::FormatTemp("%s (%s %s)", n1.s, n2.s, _TRA("Bytes").s);
}

// returns true if file exists
bool LaunchFileIfExists(Str path) {
    if (!path) {
        return false;
    }
    if (!file::Exists(path)) {
        logf("LaunchFileIfExists: !file::Exists('%s')\n", path.s);
        return false;
    }
    if (gIsStoreBuild) {
        path = path::GetNonVirtualTemp(path);
        logf("LaunchFileIfExists: gIsStoreBuild, path='%s'\n", path.s);
    }
    LaunchFileShell(path, nullptr, "open");
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
    if (str::Leni(path) < 4 || path.s[1] != ':') {
        return false;
    }

    // Iterate through all (other) removable drives and try to find the file there
    char szDrive[] = "A:\\";
    char origDrive = path.s[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::HasVariableDriveLetter(szDrive)) {
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
    AutoFreeStr protocol;
    if (fileURL && str::Parse(fileURL, "%S:", &protocol)) {
        if (str::Leni(protocol) > 1 && !str::EqI(protocol, "file")) {
            return true;
        }
    }

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET) {
        return true;
    }

    // check all parents of embedded files and ADSs as well
    TempStr path = str::DupTemp(filePath);
    while (str::Leni(path) > 2 && str::FindChar(path.s + 2, ':')) {
        *str::FindCharLast(path.s, ':') = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET) {
            return true;
        }
    }

    return false;
}
