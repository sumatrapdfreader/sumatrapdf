/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Translations.h"
#include "Settings.h"
#include "AppSettings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "SumatraPDF.h"
#include "Installer.h"
#include "InstallerUtil.h"

#include "SumatraLog.h"

// The parts of the installer and uninstaller that orig and ng have in common:
// finding and checking the installation directory, shortcuts, the PATH entry,
// killing processes that hold our files, relaunching the uninstaller from the
// temp directory. Each app draws its own installer window.

Str gFirstError;

// case-insensitive check whether dir is a ';'-delimited component of path.
// substring matching would wrongly match a longer sibling entry (e.g.
// "...\SumatraPDFViewer" contains "...\SumatraPDF"), so compare whole entries.
// case-insensitive check whether dir is a ';'-delimited component of a PATH-like string
bool IsDirInPath(Str path, Str dir) {
    StrVec parts;
    Split(&parts, path, StrL(";"));
    for (Str part : parts) {
        if (str::EqI(part, dir)) {
            return true;
        }
    }
    return false;
}

// write value as REG_EXPAND_SZ (PATH may contain %vars%) under root\keyName:valueName
bool WriteRegExpandSz(HKEY root, Str keyName, Str valueName, Str value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valueNameW = CWStrTemp(valueName);
    int cch;
    WCHAR* valueW = CWStrTemp(value, cch);
    DWORD cbData = (DWORD)(cch + 1) * sizeof(WCHAR);
    HKEY hKey;
    LONG res = RegOpenKeyExW(root, keyNameW, 0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) {
        logf("WriteRegExpandSz: RegOpenKeyExW('%s') failed with %d\n", keyName, (int)res);
        return false;
    }
    res = RegSetValueExW(hKey, valueNameW, 0, REG_EXPAND_SZ, (const BYTE*)valueW, cbData);
    RegCloseKey(hKey);
    if (res != ERROR_SUCCESS) {
        logf("WriteRegExpandSz: RegSetValueExW failed with %d\n", (int)res);
        return false;
    }
    return true;
}

PreviousInstallationInfo::~PreviousInstallationInfo() {
    str::Free(installationDir);
}

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
TempStr GetRegPathUninstTemp(Str appName) {
    return str::JoinTemp(StrL("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"), appName);
}

void NotifyFailed(Str msg) {
    if (len(gFirstError) == 0) {
        gFirstError = str::Dup(msg);
    }
    logf("NotifyFailed: %s\n", msg);
}

static Str gCachedExistingInstallationDir;

// the result is borrowed (perm-arena cache): callers must dup to persist it
TempStr GetExistingInstallationDirTemp() {
    if (gCachedExistingInstallationDir) {
        // no logging if returning cached
        return gCachedExistingInstallationDir;
    }
    log(StrL("GetExistingInstallationDir()\n"));
    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    TempStr dir = LoggedReadRegStr2Temp(regPathUninst, StrL("InstallLocation"));
    if (len(dir) == 0) {
        return {};
    }
    if (str::EndsWithI(dir, StrL(".exe"))) {
        dir = path::GetDirTemp(dir);
    }
    if (len(dir) > 0 && dir::Exists(dir)) {
        gCachedExistingInstallationDir = str::Dup(GetPermArena(), dir);
        return gCachedExistingInstallationDir;
    }
    return {};
}

bool IsOurExeInstalled() {
    TempStr installedDir = GetExistingInstallationDirTemp();
    if (len(installedDir) == 0) {
        return false;
    }
    TempStr exeDir = GetSelfExeDirTemp();
    return str::EqI(installedDir, exeDir);
}

// Walk path and parents; true if any component equals dir (junction-aware via path::IsSame).
static bool IsPathUnderOrEqualDir(Str path, Str dir) {
    if (len(path) == 0 || len(dir) == 0) {
        return false;
    }
    TempStr cur = str::DupTemp(path);
    while (cur) {
        if (path::IsSame(dir, cur)) {
            return true;
        }
        TempStr parent = path::GetDirTemp(cur);
        if (len(parent) == 0 || len(parent) >= len(cur)) {
            break;
        }
        cur = parent;
    }
    return false;
}

// true if path is under Program Files / Program Files (x86)
bool IsPathUnderProgramFiles(Str path) {
    if (len(path) == 0) {
        return false;
    }
    TempStr pf = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES);
    if (IsPathUnderOrEqualDir(path, pf)) {
        return true;
    }
    TempStr pfx86 = GetSpecialFolderTemp(CSIDL_PROGRAM_FILESX86);
    if (IsPathUnderOrEqualDir(path, pfx86)) {
        return true;
    }
    return false;
}

// Probe whether the current process can create a file under dir (or a parent that exists).
static bool CanWriteToDirectory(Str dir) {
    if (len(dir) == 0) {
        return false;
    }
    TempStr probeDir = str::DupTemp(dir);
    while (probeDir && !dir::Exists(probeDir)) {
        TempStr parent = path::GetDirTemp(probeDir);
        if (len(parent) == 0 || len(parent) >= len(probeDir)) {
            break;
        }
        probeDir = parent;
    }
    if (len(probeDir) == 0 || !dir::Exists(probeDir)) {
        return false;
    }
    TempStr probe = path::JoinTemp(probeDir, fmt("sumatra-write-test-%u.tmp", GetCurrentProcessId()));
    HANDLE h = CreateFileW(CWStrTemp(probe), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        logf("CanWriteToDirectory: CreateFile failed for '%s' err=%u\n", probe, GetLastError());
        return false;
    }
    CloseHandle(h);
    return true;
}

// true if install needs a UAC elevation (all-users, Program Files, or not writable)
bool InstallNeedsElevation(Str installDir, bool allUsers) {
    if (allUsers) {
        return true;
    }
    if (IsPathUnderProgramFiles(installDir)) {
        return true;
    }
    // Already admin: no further elevation needed even if write probe is odd.
    if (IsProcessRunningElevated()) {
        return false;
    }
    if (!CanWriteToDirectory(installDir)) {
        return true;
    }
    return false;
}

TempStr GetInstallationFilePathTemp(Str installDir, Str name) {
    TempStr res = path::JoinTemp(installDir, name);
    logf("GetInstallationFilePath(%s) = > %s\n", name, res);
    return res;
}

bool IsProcWithModule(DWORD processId, Str modulePath) {
    AutoCloseHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hModSnapshot.IsValid()) {
        return false;
    }

    MODULEENTRY32W me32{};
    me32.dwSize = sizeof(me32);
    BOOL ok = Module32FirstW(hModSnapshot, &me32);
    while (ok) {
        TempStr path = ToUtf8Temp(me32.szExePath);
        if (path::IsSame(modulePath, path)) {
            return true;
        }
        ok = Module32NextW(hModSnapshot, &me32);
    }
    return false;
}

bool KillProcWithId(DWORD processId, bool waitUntilTerminated) {
    logf("KillProcWithId(processId=%d)\n", (int)processId);
    BOOL inheritHandle = FALSE;
    // Note: do I need PROCESS_QUERY_INFORMATION and PROCESS_VM_READ?
    DWORD dwAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE;
    AutoCloseHandle hProcess = OpenProcess(dwAccess, inheritHandle, processId);
    if (!hProcess.IsValid()) {
        return false;
    }

    BOOL killed = TerminateProcess(hProcess, 0);
    if (!killed) {
        return false;
    }

    if (waitUntilTerminated) {
        WaitForSingleObject(hProcess, kTenSecondsInMs);
    }

    return true;
}

// clang-format off
static Str readableProcessNames[] = {
    Str(), Str(), // to be filled with our process
    StrL("prevhost.exe"), StrL("Windows Explorer"),
    StrL("dllhost.exe"), StrL("Windows Explorer")
};

Str ReadableProcName(Str procPath) {
    readableProcessNames[0] = Str(kExeName);
    readableProcessNames[1] = StrL(kAppName);
    TempStr procName = path::GetBaseNameTemp(procPath);
    for (size_t i = 0; i < dimof(readableProcessNames); i += 2) {
        if (str::EqI(procName, readableProcessNames[i])) {
            return readableProcessNames[i + 1];
        }
    }
    return procName;
}

PreviousInstallationInfo gPrevInstall;

Flags gCliNew;

bool HasPreviousInstall() {
    bool hasPrev = (gPrevInstall.typ != PreviousInstallationType::None);
    logf("HasPreviousInstall(): hasPrev: %d\n", hasPrev);
    return hasPrev;
}

constexpr const char* kLogFileName = "sumatra-install-log.txt";

// caller has to free()
Str GetInstallerLogPath() {
    TempStr dir = GetTempDirTemp();
    if (len(dir) == 0) {
        return str::Dup(Str(kLogFileName));
    }
    return path::Join(dir, Str(kLogFileName));
}

void ClearReadOnly(Str path) {
    DWORD attrs = file::GetAttributes(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        logf("  clearing READONLY on '%s'\n", path);
        file::SetAttributes(path, attrs & ~FILE_ATTRIBUTE_READONLY);
    }
}

bool IsDiskFullError(DWORD err) {
    return err == ERROR_DISK_FULL || err == ERROR_HANDLE_DISK_FULL;
}

void CopySettingsFile() {
    log(StrL("CopySettingsFile()\n"));
    // Settings moved from %APPDATA% to %LOCALAPPDATA% in 3.2; copy from the old location on upgrade.

    // seen a crash when running elevated
    TempStr srcDir = GetSpecialFolderTemp(CSIDL_APPDATA, false);
    if (len(srcDir) == 0) {
        return;
    }
    TempStr dstDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (len(dstDir) == 0) {
        return;
    }

    TempStr prefsFileName = GetSettingsFileNameTemp();
    TempStr srcPath = path::JoinTemp(srcDir, StrL(kAppName), prefsFileName);
    TempStr dstPath = path::JoinTemp(dstDir, StrL(kAppName), prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath, srcPath, failIfExists);
    logf("  copied '%s' to '%s'\n", srcPath, dstPath);
}

static bool CreateAppShortcut(int csidl, Str installedExePath) {
    TempStr shortcutPath = GetShortcutPathTemp(csidl);
    if (len(shortcutPath) == 0) {
        log(StrL("CreateAppShortcut() failed\n"));
        return false;
    }
    logf("CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath);
    return CreateShortcut(shortcutPath, installedExePath);
}

// https://docs.microsoft.com/en-us/windows/win32/shell/csidl
// CSIDL_COMMON_DESKTOPDIRECTORY - files and folders on desktop for all users. C:\Documents and Settings\All
// Users\Desktop
// CSIDL_COMMON_PROGRAMS - Programs item in Start menu for all users, C:\Documents and Settings\All Users\Start
// Menu\Programs
// CSIDL_DESKTOP - virtual folder, desktop for current user
// CSIDL_PROGRAMS - Programs item in Start menu for current user. Settings\username\Start Menu\Programs
static int shortcutDirs[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_PROGRAMS, CSIDL_DESKTOP, CSIDL_PROGRAMS};

void CreateAppShortcuts(bool forAllUsers, bool withDesktop, Str installedExePath) {
    logf("CreateAppShortcuts(forAllUsers=%d, withDesktop=%d)\n", (int)forAllUsers, (int)withDesktop);
    size_t start = forAllUsers ? 0 : 2;
    size_t end = forAllUsers ? 2 : dimof(shortcutDirs);
    for (size_t i = start; i < end; i++) {
        int csidl = shortcutDirs[i];
        bool isDesktop = csidl == CSIDL_COMMON_DESKTOPDIRECTORY || csidl == CSIDL_DESKTOP;
        if (isDesktop && !withDesktop) {
            continue;
        }
        CreateAppShortcut(csidl, installedExePath);
    }
}

static void RemoveShortcutFile(int csidl) {
    TempStr path = GetShortcutPathTemp(csidl);
    if (len(path) == 0 || !file::Exists(path)) {
        return;
    }
    file::Delete(path);
    logf("RemoveShortcutFile: deleted '%s'\n", path);
}

// those are shortcuts created by versions before 3.4
static int shortcutDirsPre34[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

// those are shortcuts created by versions 3.4 through 3.6
static int shortcutDirs34To36[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU, CSIDL_DESKTOP,
                                   CSIDL_STARTMENU};

// Installer.cpp
void RemoveAppShortcuts() {
    for (int csidl : shortcutDirs) {
        RemoveShortcutFile(csidl);
    }
    for (int csidl : shortcutDirsPre34) {
        RemoveShortcutFile(csidl);
    }
    for (int csidl : shortcutDirs34To36) {
        RemoveShortcutFile(csidl);
    }
}

Str GetEnvRegKey(bool allUsers) {
    if (allUsers) {
        return StrL(R"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)");
    }
    return StrL("Environment");
}

void AddInstallDirToPath(bool allUsers, Str installDir) {
    HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    Str keyName = GetEnvRegKey(allUsers);
    TempStr currPath = ReadRegStrTemp(root, keyName, StrL("Path"));
    // check if installDir is already in PATH (case-insensitive)
    if (currPath && IsDirInPath(currPath, installDir)) {
        logf("AddInstallDirToPath: '%s' already in PATH\n", installDir);
        return;
    }
    str::Builder newPath;
    if (len(currPath) > 0) {
        newPath.Append(currPath);
        if (newPath.LastChar() != ';') {
            newPath.Append(StrL(";"));
        }
    }
    newPath.Append(installDir);

    if (!WriteRegExpandSz(root, keyName, StrL("Path"), ToStr(newPath))) {
        return;
    }
    logf("AddInstallDirToPath: added '%s' to PATH\n", installDir);
    // notify other processes that environment has changed
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

// in pre-release the window is wider to accommodate bigger version number
// TODO: instead of changing size of the window, change how we draw version number
int GetInstallerWinDx() {
    if (gIsPreReleaseBuild) {
        return 492;
    }
    return 420;
}

TempStr GetInstalledExePathTemp(Flags* cli) {
    TempStr dir = cli->installDir;
    return path::JoinTemp(dir, Str(kExeName));
}

void StartSumatra() {
    TempStr exePath = GetInstalledExePathTemp(&gCliNew);
    RunNonElevated(exePath);
}

TempStr GetDefaultInstallationDirTemp(bool forAllUsers, bool ignorePrev) {
    logf("GetDefaultInstallationDir(forAllUsers=%d, ignorePrev=%d)\n", (int)forAllUsers, (int)ignorePrev);

    Str dirPrevInstall = gPrevInstall.installationDir;

    if (dirPrevInstall && !ignorePrev) {
        logf("  using %s from previous install\n", dirPrevInstall);
        return dirPrevInstall;
    }

    if (forAllUsers) {
        TempStr dirAll = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, false);
        TempStr dir = path::JoinTemp(dirAll, StrL(kAppName));
        logf("  using '%s' from GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", dir);
        return dir;
    }

    // %APPLOCALDATA%\SumatraPDF
    TempStr dirUser = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    TempStr dir = path::JoinTemp(dirUser, StrL(kAppName));
    logf("  using '%s' from GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", dir);
    return dir;
}

static TempStr GetUninstallerPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    ReportIf(res == 0 || res >= dimof(tempDir));
    TempStr dirA = ToUtf8Temp(tempDir);
    return path::JoinTemp(dirA, StrL("Sumatra-Uninstaller.exe"));
}

// %SystemRoot%\Temp, used instead of the per-user temp directory for the
// elevated copy. A file an elevated process creates there is owned by
// Administrators, so a non-elevated process running as the same user can't
// swap it for something else before we launch it.
static TempStr GetUninstallerPathInSystemTemp() {
    WCHAR winDir[MAX_PATH]{};
    UINT n = GetWindowsDirectoryW(winDir, dimof(winDir));
    if (n == 0 || n >= dimof(winDir)) {
        return {};
    }
    TempStr dir = path::JoinTemp(ToUtf8Temp(winDir), StrL("Temp"));
    return path::JoinTemp(dir, StrL("Sumatra-Uninstaller.exe"));
}

// to be able to delete installation directory we must copy
// ourselves to temp directory and re-launch
void RelaunchMaybeElevatedFromTempDirectory(Flags* cli) {
    log(StrL("RelaunchMaybeElevatedFromTempDirectory()\n"));
    if (gIsDebugBuild) {
        // for easier debugging, debug build doesn't need
        // to be copied / re-launched
        return;
    }

    TempStr ownPath = GetSelfExePathTemp();
    // TODO: should extract cmd-line from GetCommandLineW() by skipping the first
    // item, which is path to the executable
    str::Builder cmdLine;
    cmdLine.Append(StrL("-uninstall"));
    if (cli->silent) {
        cmdLine.Append(StrL(" -silent"));
    }
    if (cli->log) {
        cmdLine.Append(StrL(" -log"));
    }
    if (cli->allUsers) {
        cmdLine.Append(StrL(" -all-users"));
    }
    Str cl = ToStr(cmdLine);

    if (cli->allUsers) {
        if (!IsProcessRunningElevated()) {
            // Elevate the installed executable directly. Copying it to a
            // user-writable temporary path before elevation would let another
            // process replace it between the copy and ShellExecute.
            logf("LaunchElevated('%s', '%s')\n", ownPath, cl);
            bool okElev = LaunchElevated(ownPath, cl);
            if (!okElev) {
                logf("LaunchElevated() failed to launch '%s' '%s'\n", ownPath, cl);
                LogLastError();
            } else {
                logf("LaunchElevated() launched '%s' '%s' ok!\n", ownPath, cl);
            }
            ::ExitProcess(0);
        }

        // Elevated, but still running out of the directory we're about to
        // delete. Windows keeps a running image's file open, so dir::RemoveAll()
        // fails on our own exe and leaves the whole installation directory
        // behind (issue #5904). Re-launch from somewhere else first.
        //
        // Not the per-user temp directory: a copy there can be swapped for
        // something else before it runs, which is the elevation hole that
        // launching the installed exe directly closed. Use %SystemRoot%\Temp,
        // where a non-elevated process can create a file but cannot list the
        // directory or delete what's in it.
        //
        // "Can create" is enough for an attacker to plant a file at our path
        // ahead of time and keep write access to it through CREATOR OWNER, so
        // getting the directory right isn't sufficient on its own:
        //  - delete anything already there (we're elevated, they can't)
        //  - copy with dontOverwrite, so we only continue if we created it
        //  - hold it open denying writers while we launch it
        TempStr sysTempPath = GetUninstallerPathInSystemTemp();
        if (str::IsEmptyOrWhiteSpace(sysTempPath) || str::EqI(sysTempPath, ownPath)) {
            log(StrL("  already running from the system temp dir (or couldn't find it)\n"));
            return;
        }
        file::Delete(sysTempPath);
        logf("  copying uninstaller '%s' to '%s'\n", ownPath, sysTempPath);
        if (!file::Copy(sysTempPath, ownPath, true)) {
            // best effort: uninstalling from the install dir still removes the
            // registry entries and most files, it just can't remove the folder
            logf("  failed to copy uninstaller to '%s', uninstalling in place\n", sysTempPath);
            return;
        }
        // deny writers for as long as the path is a launch target
        HANDLE hLock = CreateFileW(CWStrTemp(sysTempPath), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        logf("LaunchProcessWithCmdLine('%s' '%s')\n", sysTempPath, cl);
        HANDLE hElev = LaunchProcessWithCmdLine(sysTempPath, cl);
        if (hLock != INVALID_HANDLE_VALUE) {
            CloseHandle(hLock);
        }
        if (!hElev) {
            logf("LaunchProcessWithCmdLine() failed to launch '%s' '%s'\n", sysTempPath, cl);
            LogLastError();
            // the copy never ran, so keep going here rather than doing nothing
            return;
        }
        logf("LaunchProcessWithCmdLine() launched '%s' '%s' ok!\n", sysTempPath, cl);
        ::ExitProcess(0);
    }

    TempStr installerTempPath = GetUninstallerPathInTemp();
    if (str::EqI(installerTempPath, ownPath)) {
        log(StrL("  already running from temp dir\n"));
        return;
    }
    logf("  copying installer '%s' to '%s'\n", ownPath, installerTempPath);
    bool ok = file::Copy(installerTempPath, ownPath, false);
    if (!ok) {
        logf("  failed to copy installer\n");
        return;
    }
    logf("LaunchProcessWithCmdLine('%s' '%s')\n", installerTempPath, cl);
    HANDLE h = LaunchProcessWithCmdLine(installerTempPath, cl);
    if (!h) {
        logf("LaunchProcessWithCmdLine() failed to launch '%s' '%s'\n", installerTempPath, cl);
        LogLastError();
    } else {
        logf("LaunchProcessWithCmdLine() launched '%s' '%s' ok!\n", installerTempPath, cl);
    }
    ::ExitProcess(0);
}

static TempStr GetSystem32PathTemp(Str exeName) {
    WCHAR sysDir[MAX_PATH]{};
    UINT n = GetSystemDirectoryW(sysDir, dimof(sysDir));
    if (n == 0 || n >= dimof(sysDir)) {
        return {};
    }
    return path::JoinTemp(ToUtf8Temp(sysDir), exeName);
}

// A process can't delete its own executable: Windows keeps the image file open
// for as long as it runs, and even a POSIX-semantics unlink
// (FileDispositionInfoEx) is refused with ERROR_ACCESS_DENIED. Something else
// has to do it once we've exited.
//
// That something used to be a batch file written to the per-user temp directory
// and run with cmd.exe. When the uninstaller is elevated that's an escalation:
// a non-elevated process can rewrite the script between our write and cmd.exe
// reading it, and its contents then run as admin. 699acf313 closed that by
// scheduling the delete for the next reboot instead, which is safe but means
// the uninstaller is still sitting there afterwards.
//
// Put the commands on cmd.exe's command line instead. There's no intermediate
// file for anyone to tamper with, cmd.exe comes from System32 by absolute path
// so PATH can't redirect it, and the file goes away seconds after we exit
// rather than at the next boot. Same code path elevated or not.
void InitSelfDelete() {
    log(StrL("InitSelfDelete()\n"));
    TempStr exePath = GetSelfExePathTemp();
    TempStr cmdExe = GetSystem32PathTemp(StrL("cmd.exe"));
    if (str::IsEmptyOrWhiteSpace(cmdExe)) {
        log(StrL("InitSelfDelete(): couldn't find cmd.exe\n"));
        return;
    }
    // ping, not timeout: timeout.exe exits immediately with "Input redirection
    // is not supported" whenever stdin isn't a console, which is exactly what a
    // child of a windowless process gets - the del then ran while we were still
    // running and failed. 3 pings to loopback is ~2s, enough for us to exit.
    TempStr cmdLine = fmt("\"%s\" /C ping -n 3 127.0.0.1 >nul & del \"%s\"", cmdExe, exePath);
    logf("InitSelfDelete(): '%s'\n", cmdLine);
    HANDLE h = LaunchProcessInDir(cmdLine, {}, CREATE_NO_WINDOW);
    if (!h) {
        logf("InitSelfDelete(): failed to launch, scheduling delete for next reboot\n");
        LogLastError();
        MoveFileExW(CWStrTemp(exePath), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return;
    }
    CloseHandle(h);
}

void RemoveInstallDirFromPath(bool allUsers, Str installDir) {
    HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    Str keyName = GetEnvRegKey(allUsers);
    TempStr currPath = ReadRegStrTemp(root, keyName, StrL("Path"));
    if (len(currPath) == 0) {
        return;
    }
    if (!IsDirInPath(currPath, installDir)) {
        logf("RemoveInstallDirFromPath: '%s' not found in PATH\n", installDir);
        return;
    }

    str::Builder newPath;
    StrVec parts;
    Split(&parts, currPath, StrL(";"));
    for (Str entry : parts) {
        // skip empty entries and the one matching installDir (case-insensitive)
        if (len(entry) == 0 || str::EqI(entry, installDir)) {
            continue;
        }
        if (len(newPath) > 0) {
            newPath.Append(StrL(";"));
        }
        newPath.Append(entry);
    }

    if (!WriteRegExpandSz(root, keyName, StrL("Path"), ToStr(newPath))) {
        return;
    }
    logf("RemoveInstallDirFromPath: removed '%s' from PATH\n", installDir);
    // notify other processes that environment has changed
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

TempStr GetInstalledExePathTemp() {
    TempStr dir = gCli->installDir;
    return path::JoinTemp(dir, Str(kExeName));
}

// Copy the running installer to installDir\SumatraPDF.exe. Retries and uses
// temp+rename like WriteInstallerFileRobust: a single CopyFileW often fails
// with ACCESS_DENIED (AV / Controlled Folder Access) or a sharing race after
// TerminateProcess of the previous instance.
bool CopySelfToDir(Str destDir) {
    logf("CopySelfToDir(%s)\n", destDir);
    TempStr exePath = GetSelfExePathTemp();
    TempStr dstPath = path::JoinTemp(destDir, Str(kExeName));
    TempStr tmpPath = str::JoinTemp(dstPath, StrL(".tmp"));
    DWORD lastErr = 0;

    auto tryDirectCopy = [&]() -> bool {
        ClearReadOnly(dstPath);
        BOOL ok = CopyFileW(CWStrTemp(exePath), CWStrTemp(dstPath), FALSE);
        if (!ok) {
            lastErr = GetLastError();
            logf("  CopyFileW('%s' -> '%s') failed lastError=%u\n", exePath, dstPath, lastErr);
            LogLastError(lastErr);
            return false;
        }
        return true;
    };

    auto tryTempCopyRename = [&]() -> bool {
        ClearReadOnly(tmpPath);
        file::Delete(tmpPath);
        BOOL ok = CopyFileW(CWStrTemp(exePath), CWStrTemp(tmpPath), FALSE);
        if (!ok) {
            lastErr = GetLastError();
            logf("  CopyFileW('%s' -> '%s') failed lastError=%u\n", exePath, tmpPath, lastErr);
            LogLastError(lastErr);
            return false;
        }
        ClearReadOnly(dstPath);
        if (!MoveFileExW(CWStrTemp(tmpPath), CWStrTemp(dstPath), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            lastErr = GetLastError();
            logf("  MoveFileExW('%s' -> '%s') failed lastError=%u\n", tmpPath, dstPath, lastErr);
            LogLastError(lastErr);
            file::Delete(tmpPath);
            return false;
        }
        return true;
    };

    for (int attempt = 1; attempt <= 4; attempt++) {
        logf("  attempt %d/4\n", attempt);
        if (tryDirectCopy() || tryTempCopyRename()) {
            // strip zone identifier (if exists) to avoid windows
            // complaining when launching the file
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1782
            file::DeleteZoneIdentifier(dstPath);
            logf("  copied '%s' to '%s'\n", exePath, dstPath);
            return true;
        }
        int killed = KillProcessesWithModule(dstPath, true);
        logf("  KillProcessesWithModule('%s') killed=%d\n", dstPath, killed);
        if (file::Exists(dstPath)) {
            ClearReadOnly(dstPath);
            bool delOk = file::Delete(dstPath);
            logf("  Delete('%s') => %d\n", dstPath, (int)delOk);
            if (!delOk) {
                LogLastError();
            }
        }
        file::Delete(tmpPath);
        if (attempt < 4) {
            DWORD sleepMs = 300u * (DWORD)attempt;
            logf("  sleep %u ms before retry\n", sleepMs);
            Sleep(sleepMs);
        }
    }

    logf("  failed to copy '%s' to '%s' lastError=%u\n", exePath, dstPath, lastErr);
    if (lastErr == ERROR_ACCESS_DENIED) {
        NotifyFailed(
            Tr("Couldn't copy SumatraPDF.exe to the installation directory (access denied). "
               "Temporarily disable antivirus or Controlled Folder Access for this folder, "
               "run the installer as administrator, or choose a different install folder. "
               "See https://www.sumatrapdfreader.org/docs/Installation"));
    } else if (lastErr == ERROR_SHARING_VIOLATION || lastErr == ERROR_LOCK_VIOLATION) {
        NotifyFailed(
            Tr("Couldn't copy SumatraPDF.exe to the installation directory (file in use). "
               "Close all SumatraPDF windows and Explorer PDF previews, then try again. "
               "See https://www.sumatrapdfreader.org/docs/Installation"));
    } else if (IsDiskFullError(lastErr)) {
        NotifyFailed(
            Tr("Not enough free disk space to copy SumatraPDF.exe to the installation directory.\n\n"
               "Free up space on this drive and try again."));
    } else {
        NotifyFailed(Tr("Couldn't copy SumatraPDF.exe to the installation directory"));
    }
    return false;
}
