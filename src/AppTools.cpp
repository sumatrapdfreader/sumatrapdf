/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"
#include "utils/WinDynCalls.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/DbgHelpDyn.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/CryptoUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "SumatraConfig.h"
#include "Translations.h"
#include "Version.h"
#include "AppTools.h"

#include "utils/Log.h"

/* Returns true, if a Registry entry indicates that this executable has been
   created by an installer (and should be updated through an installer) */
bool HasBeenInstalled() {
    // see GetDefaultInstallationDir() in Installer.cpp
    char* regPathUninst = str::JoinTemp("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", kAppName);
    char* installedPath = LoggedReadRegStr2Temp(regPathUninst, "InstallLocation");
    if (!installedPath) {
        return false;
    }

    char* exePath = GetExePathTemp();
    if (exePath) {
        return false;
    }

    if (!str::EndsWithI(installedPath, ".exe")) {
        installedPath = path::JoinTemp(installedPath, path::GetBaseNameTemp(exePath));
    }
    return path::IsSame(installedPath, exePath);
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
    sCacheIsPortable = 1;

    if (gIsStoreBuild) {
        return false;
    }

    if (HasBeenInstalled()) {
        sCacheIsPortable = 0;
        return false;
    }

    char* exePath = GetExePathTemp();
    char* programFilesDir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES);
    // if we can't get a path, assume we're not running from "Program Files"
    if (!exePath || !programFilesDir) {
        return true;
    }

    // check if one of the exePath's parent directories is "Program Files"
    // (or a junction to it)
    char* baseName;
    while ((baseName = (char*)path::GetBaseNameTemp(exePath)) > exePath) {
        baseName[-1] = '\0';
        if (path::IsSame(programFilesDir, exePath)) {
            sCacheIsPortable = 0;
            return false;
        }
    }

    return true;
}

bool IsDllBuild() {
    HRSRC resSrc = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), RT_RCDATA);
    return resSrc != nullptr;
}

static AutoFreeStr gAppDataDir;

void SetAppDataPath(const char* path) {
    path = path::NormalizeTemp(path);
    gAppDataDir.SetCopy(path);
}

// Generate the full path for a filename used by the app in the userdata path
// Caller needs to free() the result
char* AppGenDataFilenameTemp(const char* fileName) {
    if (!fileName) {
        return nullptr;
    }

    if (gAppDataDir && dir::Exists(gAppDataDir)) {
        return path::JoinTemp(gAppDataDir, fileName);
    }

    if (IsRunningInPortableMode()) {
        /* Use the same path as the binary */
        AutoFreeStr res = path::GetPathOfFileInAppDir(fileName);
        return str::DupTemp(res);
    }

    char* path = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
    if (!path) {
        return nullptr;
    }
    path = path::JoinTemp(path, kAppName);
    if (!path) {
        return nullptr;
    }

    // use a different path for store builds
    if (gIsStoreBuild) {
        // %APPLOCALDATA%/SumatraPDF Store
        // %APPLOCALDATA%/SumatraPDF Store Preview
        path = str::JoinTemp(path, " Store");
        if (gIsPreReleaseBuild) {
            path = str::JoinTemp(path, " Preview");
        }
    }
    bool ok = dir::Create(path);
    if (!ok) {
        return nullptr;
    }
    return path::JoinTemp(path, fileName);
}

// List of rules used to detect TeX editors.

#define kRegCurrentVer "Software\\Microsoft\\Windows\\CurrentVersion"

// clang-format off
static TextEditor editorRules[] = {
    {
        "Code.exe",
        R"(--goto %f:%l)",
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
        RegType::None,
        nullptr,
        nullptr,
        "notepad.exe",
        "notepad.exe \"%f\""
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
        const char* regKey = rule.regKey;
        const char* regValue = rule.regValue;
        char* path = LoggedReadRegStr2Temp(regKey, regValue);
        if (!path) {
            continue;
        }

        char* exePath = nullptr;
        const char* binaryFileName = rule.binaryFilename;
        const char* inverseSearchArgs = rule.inverseSearchArgs;
        if (rule.type == RegType::SiblingPath) {
            // remove file part
            char* dir = path::GetDirTemp(path);
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
        rule.openFileCmd = str::Format("\"%s\" %s", exePath, inverseSearchArgs);
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
char* BuildOpenFileCmd(const char* pattern, const char* path, int line, int col) {
    const char* perc;
    str::Str cmdline(256);

    logf("BuildOpenFileCmd: path: '%s', pattern: '%s'\n", path, pattern);
    const char* s = pattern;
    while ((perc = str::FindChar(s, '%')) != nullptr) {
        cmdline.Append(s, perc - s);
        s = perc + 2;
        perc++;

        if (*perc == 'f') {
            char* fname = path::NormalizeTemp(path);
            cmdline.Append(fname);
        } else if (*perc == 'l') {
            cmdline.AppendFmt("%d", line);
        } else if (*perc == 'c') {
            cmdline.AppendFmt("%d", col);
        } else if (*perc == '%') {
            cmdline.AppendChar('%');
        } else {
            cmdline.Append(perc - 1, 2);
        }
    }
    cmdline.Append(s);

    return cmdline.StealData();
}

void OpenFileWithTextEditor(const char* path) {
    Vec<TextEditor*> editors;
    DetectTextEditors(editors);
    const char* cmd = editors[0]->openFileCmd;

    char* cmdLine = BuildOpenFileCmd(cmd, path, 1, 1);
    logf("OpenFileWithTextEditor: '%s'\n", cmdLine);
    char* appDir = GetExeDirTemp();
    AutoCloseHandle process(LaunchProcess(cmdLine, appDir));
    str::Free(cmdLine);
}

#define UWM_DELAYED_SET_FOCUS (WM_APP + 1)

// selects all text in an edit box if it's selected either
// through a keyboard shortcut or a non-selecting mouse click
// (or responds to Ctrl+Backspace as nowadays expected)
bool ExtendedEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM) {
    static bool delayFocus = false;

    switch (msg) {
        case WM_LBUTTONDOWN:
            delayFocus = !IsFocused(hwnd);
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
    char* filePath = AppGenDataFilenameTemp("callstacks.txt");
    file::WriteFile(filePath, s);
    s.Free();
}

// TODO: this can be used for extracting other data
#if 0
// cache because calculating md5 of the whole executable
// might be relatively expensive
static AutoFreeWstr gAppMd5;

// return hex version of md5 of app's executable
// nullptr if there was an error
// caller needs to free the result
static const WCHAR* Md5OfAppExe() {
    if (gAppMd5.Get()) {
        return str::Dup(gAppMd5.Get());
    }

    auto appPath = GetExePathTemp();
    if (appPath.empty()) {
        return {};
    }
    ByteSlice d = file::ReadFile(appPath.data);
    if (d.empty()) {
        return nullptr;
    }

    u8 md5[16]{};
    CalcMD5Digest(d.data, d.size(), md5);

    AutoFree md5HexA(_MemToHex(&md5));
    AutoFreeWstr md5Hex = strconv::Utf8ToWchar(md5HexA.AsView());
    d.Free();
    return md5Hex.StealData();
}

// remove all directories except for ours
//. need to avoid acuumulating the directories when testing
// locally or using pre-release builds (both cases where
// exe and its md5 changes frequently)
void RemoveMd5AppDataDirectories() {
    AutoFreeWstr extractedDir = PathForFileInAppDataDir(L"extracted");
    if (extractedDir.empty()) {
        return;
    }

    auto dirs = CollectDirsFromDirectory(extractedDir.data);
    if (dirs.empty()) {
        return;
    }

    AutoFreeWstr md5App = Md5OfAppExe();
    if (md5App.empty()) {
        return;
    }

    AutoFreeWstr md5Dir = path::Join(extractedDir.data, md5App.data);

    for (auto& dir : dirs) {
        const WCHAR* s = dir.data();
        if (str::Eq(s, md5Dir.data)) {
            continue;
        }
        dir::RemoveAll(s);
    }
}

// return a path on disk to extracted unrar.dll or nullptr if couldn't extract
// memory has to be freed by the caller
const WCHAR* ExractUnrarDll() {
    RemoveMd5AppDataDirectories();

    AutoFreeWstr extractedDir = PathForFileInAppDataDir(L"extracted");
    if (extractedDir.empty()) {
        return nullptr;
    }

    AutoFreeWstr md5App = Md5OfAppExe();
    if (md5App.empty()) {
        return nullptr;
    }

    AutoFreeWstr md5Dir = path::Join(extractedDir.data, md5App.data);
    AutoFreeWstr dllPath = path::Join(md5Dir.data, unrarFileName);

    if (file::Exists(dllPath.data)) {
        const WCHAR* ret = dllPath.data;
        dllPath = nullptr; // don't free
        return ret;
    }

    bool ok = dir::CreateAll(md5Dir.data);
    if (!ok) {
        return nullptr;
    }

    HGLOBAL res = 0;
    auto h = GetModuleHandle(nullptr);
    WCHAR* resName = MAKEINTRESOURCEW(1);
    HRSRC resSrc = FindResourceW(h, resName, RT_RCDATA);
    if (!resSrc) {
        return nullptr;
    }
    res = LoadResource(nullptr, resSrc);
    if (!res) {
        return nullptr;
    }
    const char* data = (const char*)LockResource(res);
    defer {
        UnlockResource(res);
    };
    DWORD dataSize = SizeofResource(nullptr, resSrc);
    ok = file::WriteFile(dllPath, data, dataSize);
    if (!ok) {
        return nullptr;
    }

    const WCHAR* ret = dllPath;
    dllPath = nullptr; // don't free
    return ret;
}
#endif

constexpr double KB = 1024;
constexpr double MB = (double)1024 * (double)1024;
constexpr double GB = (double)1024 * (double)1024 * (double)1024;

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
static TempStr FormatSizeSuccintTemp(i64 size) {
    const char* unit = nullptr;
    double s = (double)size;

    if (s > GB) {
        s = s / GB;
        unit = _TRA("GB");
    } else if (s > MB) {
        s = s / MB;
        unit = _TRA("MB");
    } else {
        s = s / KB;
        unit = _TRA("KB");
    }

    char* sizestr = str::FormatFloatWithThousandSepTemp(s);
    if (!unit) {
        return sizestr;
    }
    return fmt::FormatTemp("%s %s", sizestr, unit);
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// To be used in a context where translations are not yet available
static TempStr FormatSizeSuccintNoTransTemp(i64 size) {
    const char* unit = nullptr;
    double s = (double)size;

    if (s > GB) {
        s = s / GB;
        unit = "GB";
    } else if (s > MB) {
        s = s / MB;
        unit = "MB";
    } else {
        s = s / KB;
        unit = "KB";
    }

    char* sizestr = str::FormatFloatWithThousandSepTemp(s);
    if (!unit) {
        return sizestr;
    }
    return fmt::FormatTemp("%s %s", sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return fmt::FormatTemp("%d", size);
    }
    char* n1 = FormatSizeSuccintTemp(size);
    char* n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt::FormatTemp("%s (%s %s)", n1, n2, _TRA("Bytes"));
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeNoTransTemp(i64 size) {
    if (size <= 0) {
        return str::Format("%d", (int)size);
    }
    char* n1 = FormatSizeSuccintNoTransTemp(size);
    char* n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt::FormatTemp("%s (%s %s)", n1, n2, "Bytes");
}

// returns true if file exists
bool LaunchFileIfExists(const char* path) {
    if (!path) {
        return false;
    }
    if (!file::Exists(path)) {
        return false;
    }
    LaunchFile(path, nullptr, "open");
    return true;
}

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(const char* txt) {
    if (!str::IsDigit(*txt)) {
        return false;
    }

    for (; *txt; txt++) {
        if (str::IsDigit(*txt)) {
            continue;
        }
        if (*txt == '.' && str::IsDigit(*(txt + 1))) {
            continue;
        }
        if (*txt == '\r' && *(txt + 1) == '\n') {
            continue;
        }
        if (*txt == '\n' && !*(txt + 1)) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(const char** txt) {
    unsigned int val = 0;
    const char* next = str::Parse(*txt, "%u%?.", &val);
    *txt = next ? next : *txt + str::Len(*txt);
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareVersion(const char* txt1, const char* txt2) {
    while (*txt1 || *txt2) {
        unsigned int v1 = ExtractNextNumber(&txt1);
        unsigned int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2) {
            return v1 - v2;
        }
    }
    return 0;
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
// returns true if the path has been changed
bool AdjustVariableDriveLetter(char* path) {
    // Don't bother if the file path is still valid
    if (file::Exists(path)) {
        return false;
    }
    // only check absolute path on drives i.e. those that start with "d:\"
    if (str::Len(path) < 4 || path[1] != ':') {
        return false;
    }

    // Iterate through all (other) removable drives and try to find the file there
    char szDrive[] = "A:\\";
    char origDrive = path[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::HasVariableDriveLetter(szDrive)) {
            path[0] = szDrive[0];
            if (file::Exists(path)) {
                return true;
            }
        }
        szDrive[0]++;
    }
    path[0] = origDrive;
    return false;
}

// files are considered untrusted, if they're either loaded from a
// non-file URL in plugin mode, or if they're marked as being from
// an untrusted zone (e.g. by the browser that's downloaded them)
bool IsUntrustedFile(const char* filePath, const char* fileURL) {
    AutoFreeStr protocol;
    if (fileURL && str::Parse(fileURL, "%S:", &protocol)) {
        if (str::Len(protocol) > 1 && !str::EqI(protocol, "file")) {
            return true;
        }
    }

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET) {
        return true;
    }

    // check all parents of embedded files and ADSs as well
    AutoFreeStr path(str::Dup(filePath));
    while (str::Len(path) > 2 && str::FindChar(path + 2, ':')) {
        *str::FindCharLast(path, ':') = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET) {
            return true;
        }
    }

    return false;
}

// Draws the 'x' close button in regular state or onhover state
// Tries to mimic visual style of Chrome tab close button
void DrawCloseButton(HDC hdc, Rect& r, bool isHover) {
    DrawCloseButtonArgs args;
    args.hdc = hdc;
    args.r = r;
    args.isHover = isHover;
    DrawCloseButton(args);
}
