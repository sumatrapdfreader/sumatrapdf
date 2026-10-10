/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// code used in both Installer.cpp and Uninstaller.cpp
// ng: orig also draws the installer frame here (GDI+ Impact letters). The
// drawing moved to the gpui views in Installer.cpp / Uninstaller.cpp; what is
// left is the model those views paint (gLetters, gMsg) and the file / process
// helpers. The previewer and the search filter were dropped in step 17a, so
// everything that registered or unregistered their DLLs is gone.

#include "base/Base.h"
#include "base/File.h"

#if OS_WIN

#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "Translations.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"

#include "Installer.h"
#include "InstallerUtil.h"

#include "SumatraLog.h"

#include <tlhelp32.h>

constexpr Color gCol1 = MkRgb(196, 64, 50);
constexpr Color gCol1Shadow = MkRgb(134, 48, 39);
constexpr Color gCol2 = MkRgb(227, 107, 35);
constexpr Color gCol2Shadow = MkRgb(155, 77, 31);
constexpr Color gCol3 = MkRgb(93, 160, 40);
constexpr Color gCol3Shadow = MkRgb(51, 87, 39);
constexpr Color gCol4 = MkRgb(69, 132, 190);
constexpr Color gCol4Shadow = MkRgb(47, 89, 127);
constexpr Color gCol5 = MkRgb(112, 115, 207);
constexpr Color gCol5Shadow = MkRgb(66, 71, 118);

Color kColorMsgWelcome = gCol5;
Color kColorMsgOk = gCol5;
Color kColorMsgInstallation = gCol5;
Color kColorMsgFailed = gCol1;

Str gMsgError;
Str gMsg;
Color gMsgColor = gCol5;

Flags* gCli = nullptr;

Str gDefaultMsg; // Note: translation, not freeing

// ng: with -install-reg-root the shortcuts go here instead of the Desktop /
// Start Menu, so a test run can't overwrite the ones of a real installation
static Str gShortcutTestDir;

// ng: -install-reg-root. RegOverridePredefKey points HKCU and HKLM at subkeys
// of a throwaway key for this process only, so every LoggedWriteRegStr() below
// lands there instead of in the user's hive and nothing else has to change.
bool SetInstallRegistryTestRoot(Str keyName) {
    struct {
        HKEY predef;
        const char* sub;
    } roots[] = {{HKEY_CURRENT_USER, "HKCU"}, {HKEY_LOCAL_MACHINE, "HKLM"}};
    for (auto& r : roots) {
        TempStr key = str::JoinTemp(keyName, StrL("\\"), Str(r.sub));
        HKEY hkey = nullptr;
        LSTATUS st =
            RegCreateKeyExW(HKEY_CURRENT_USER, CWStrTemp(key), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hkey, nullptr);
        if (st != ERROR_SUCCESS) {
            logf("SetInstallRegistryTestRoot: RegCreateKeyExW('%s') failed with %d\n", key, (int)st);
            return false;
        }
        st = RegOverridePredefKey(r.predef, hkey);
        RegCloseKey(hkey);
        if (st != ERROR_SUCCESS) {
            logf("SetInstallRegistryTestRoot: RegOverridePredefKey('%s') failed with %d\n", key, (int)st);
            return false;
        }
        logf("SetInstallRegistryTestRoot: %s -> HKCU\\%s\n", Str(r.sub), key);
    }
    TempStr dir = path::JoinTemp(GetTempDirTemp(), StrL("SumatraPDF-ng-test-shortcuts"));
    dir::CreateAll(dir);
    gShortcutTestDir = str::Dup(GetPermArena(), dir);
    logf("SetInstallRegistryTestRoot: shortcuts -> '%s'\n", gShortcutTestDir);
    return true;
}

static StrVec gProcessesToClose;

void SetMsg(Str msg, Color color) {
    gMsg = str::Dup(GetPermArena(), msg);
    gMsgColor = color;
}

void SetDefaultMsg() {
    SetMsg(gDefaultMsg, kColorMsgWelcome);
}

void GetPreviousInstallInfo(PreviousInstallationInfo* info) {
    info->installationDir = str::Dup(GetExistingInstallationDirTemp());
    if (len(info->installationDir) == 0) {
        info->typ = PreviousInstallationType::None;
        log(StrL("GetPreviousInstallInfo: not installed\n"));
        return;
    }
    TempStr regPathUninst = GetRegPathUninstTemp(StrL(kAppName));
    TempStr dirLM = LoggedReadRegStrTemp(HKEY_LOCAL_MACHINE, regPathUninst, StrL("InstallLocation"));
    TempStr dirCU = LoggedReadRegStrTemp(HKEY_CURRENT_USER, regPathUninst, StrL("InstallLocation"));
    if (dirLM && dirCU) {
        info->typ = PreviousInstallationType::Both;
        info->allUsers = true;
    } else if (dirLM) {
        info->typ = PreviousInstallationType::Machine;
        info->allUsers = true;
    } else {
        info->typ = PreviousInstallationType::User;
        info->allUsers = false;
    }
    // HKCU-only uninstall key can still point at Program Files (broken / partial state).
    // Treat as machine-style so upgrades elevate and write HKLM correctly.
    if (!info->allUsers && IsPathUnderProgramFiles(info->installationDir)) {
        logf("GetPreviousInstallInfo: dir under Program Files with only HKCU key; forcing allUsers\n");
        info->allUsers = true;
    }
    DWORD desktopShortcut = 1;
    HKEY hkey = dirLM ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    if (ReadRegDWORD(hkey, regPathUninst, StrL(kRegDesktopShortcut), desktopShortcut)) {
        info->desktopShortcut = desktopShortcut != 0;
    }
    logf("GetPreviousInstallInfo: desktop shortcut: %d\n", (int)info->desktopShortcut);
    logf("GetPreviousInstallInfo: dir '%s', typ: %d, needsElevation: %d\n", info->installationDir, (int)info->typ,
         (int)info->allUsers);
}

TempStr GetShortcutPathTemp(int csidl) {
    if (gShortcutTestDir) {
        return path::JoinTemp(gShortcutTestDir, fmt("%s-%d.lnk", StrL(kAppName), csidl));
    }
    TempStr dir = GetSpecialFolderTemp(csidl, false);
    if (len(dir) == 0) {
        return {};
    }
    TempStr lnkName = str::JoinTemp(StrL(kAppName), StrL(".lnk"));
    return path::JoinTemp(dir, lnkName);
}

static bool IsProcessUsingFiles(DWORD procId, Str file1, Str file2) {
    // Note: don't know why procId 0 shows up as using our files
    if (procId == 0 || procId == GetCurrentProcessId()) {
        return false;
    }
    if (len(file1) == 0 && len(file2) == 0) {
        return false;
    }
    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W mod{};
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32FirstW(snap, &mod);
    while (cont) {
        WCHAR* exePathW = mod.szExePath;
        TempStr exePath = ToUtf8Temp(exePathW);
        if (file1 && path::IsSame(file1, exePath)) {
            return true;
        }
        if (file2 && path::IsSame(file2, exePath)) {
            return true;
        }
        cont = Module32NextW(snap, &mod);
    }
    return false;
}

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
static bool KillProcWithIdAndModule(DWORD processId, Str modulePath, bool waitUntilTerminated) {
    if (!IsProcWithModule(processId, modulePath)) {
        return false;
    }
    logf("KillProcWithIdAndModule() processId=%d, modulePath=%s\n", processId, modulePath);
    return KillProcWithId(processId, waitUntilTerminated);
}

// returns number of killed processes that have a module (exe or dll) with a given
// modulePath
// returns -1 on error, 0 if no matching processes
int KillProcessesWithModule(Str modulePath, bool waitUntilTerminated) {
    logf("KillProcessesWithModule: '%s'\n", modulePath);
    AutoCloseHandle hProcSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hProcSnapshot) {
        return -1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32FirstW(hProcSnapshot, &pe32)) {
        return -1;
    }

    int killCount = 0;
    do {
        if (KillProcWithIdAndModule(pe32.th32ProcessID, modulePath, waitUntilTerminated)) {
            logf("  killed process with id %d\n", (int)pe32.th32ProcessID);
            killCount++;
        }
    } while (Process32NextW(hProcSnapshot, &pe32));

    if (killCount > 0) {
        UpdateWindow(FindWindowW(nullptr, L"Shell_TrayWnd"));
        UpdateWindow(GetDesktopWindow());
    }
    return killCount;
}

// Kill processes that have our installed exe loaded.
// returns false if there are processes and we failed to kill them
static bool KillProcessesUsingInstallationDir(Str dir) {
    logf("KillProcessesUsingInstallationDir('%s')\n", dir);
    if (len(dir) == 0) {
        return true;
    }
    TempStr exePath = path::JoinTemp(dir, Str(kExeName));

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return false;
    }

    bool killedAllProcesses = true;
    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32FirstW(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        if (IsProcessUsingFiles(procID, exePath, {})) {
            TempStr s = ToUtf8Temp(proc.szExeFile);
            logf("  attempting to kill process %d '%s'\n", (int)procID, s);
            bool didKill = KillProcWithId(procID, true);
            logf("  KillProcWithId(%d) returned %d\n", (int)procID, (int)didKill);
            if (!didKill) {
                killedAllProcesses = false;
            }
        }
        proc.dwSize = sizeof(proc);
        ok = Process32NextW(snap, &proc);
    }

    if (file::Exists(exePath)) {
        int n = KillProcessesWithModule(exePath, true);
        if (n > 0) {
            logf("  KillProcessesWithModule('%s') killed=%d\n", exePath, n);
        }
    }
    return killedAllProcesses;
}

static bool KillProcessesUsingInstallation() {
    TempStr dir = GetExistingInstallationDirTemp();
    return KillProcessesUsingInstallationDir(dir);
}

// return names of processes that are running part of the installation
static void ProcessesUsingInstallation(StrVec& names) {
    log(StrL("ProcessesUsingInstallation()\n"));
    TempStr dir = GetExistingInstallationDirTemp();
    if (len(dir) == 0) {
        return;
    }
    TempStr exePath = path::JoinTemp(dir, Str(kExeName));

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return;
    }

    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32FirstW(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        if (IsProcessUsingFiles(procID, exePath, {})) {
            TempStr s = ToUtf8Temp(proc.szExeFile);
            TempStr name = fmt("%s (%d)", s, (int)procID);
            names.Append(name);
        }
        proc.dwSize = sizeof(proc);
        ok = Process32NextW(snap, &proc);
    }
}

// clang-format on

static void SetCloseProcessMsg() {
    int n = len(gProcessesToClose);
    Str procNames = ReadableProcName(gProcessesToClose[0]);
    for (int i = 1; i < n; i++) {
        Str name = ReadableProcName(gProcessesToClose[i]);
        if (i < n - 1) {
            procNames = str::JoinTemp(procNames, StrL(", "), name);
        } else {
            procNames = str::JoinTemp(procNames, StrL(" and "), name);
        }
    }
    TempStr s = fmt(Tr("Close %s to continue.").s, procNames);
    SetMsg(s, kColorMsgFailed);
}

bool CheckInstallUninstallPossible(bool silent) {
    logf("CheckInstallUninstallPossible(silent=%d)\n", silent);
    KillProcessesUsingInstallation();

    // now determine which processes are using installation files
    // and ask user to close them.
    // shouldn't be necessary given KillProcessesUsingInstallation(), we
    // do it just in case
    gProcessesToClose.Reset();
    ProcessesUsingInstallation(gProcessesToClose);

    bool possible = len(gProcessesToClose) == 0;
    if (possible) {
        SetDefaultMsg();
    } else {
        SetCloseProcessMsg();
        if (!silent) {
            MessageBeep(MB_ICONEXCLAMATION);
        }
    }
    return possible;
}

// clang-format off
LetterInfo gLetters[kSumatraLettersCount] = {
    {'S', gCol1, gCol1Shadow, -3.f, 0},
    {'U', gCol2, gCol2Shadow, 0.f, 0},
    {'M', gCol3, gCol3Shadow, 2.f, -2.f},
    {'A', gCol4, gCol4Shadow, 0.f, -2.4f},
    {'T', gCol5, gCol5Shadow, 0.f, 0},
    {'R', gCol5, gCol5Shadow, 2.3f, -1.4f},
    {'A', gCol4, gCol4Shadow, 0.f, 0},
    {'P', gCol3, gCol3Shadow, 0.f, -2.3f},
    {'D', gCol2, gCol2Shadow, 0.f, 3.f},
    {'F', gCol1, gCol1Shadow, 0.f, 0}
};
// clang-format on

static void SetLettersSumatraUpTo(int n) {
    Str s = StrL("SUMATRAPDF");
    for (int i = 0; i < kSumatraLettersCount; i++) {
        char c = ' ';
        if (i < n) {
            c = s.s[i];
        }
        gLetters[i].c = c;
    }
}

static void SetLettersSumatra() {
    SetLettersSumatraUpTo(kSumatraLettersCount);
}

// an animation that reveals letters one by one

// how long the animation lasts, in seconds
constexpr double kRevealingAnimDur = 2;

static bool gRevealingLettersAnim = false;
static TimeStamp gRevealingAnimStart{};
static int gRevealingLettersAnimLettersToShow;

void RevealingLettersAnimStart() {
    gRevealingLettersAnim = true;
    gRevealingAnimStart = TimeGet();
    gRevealingLettersAnimLettersToShow = 0;
    SetLettersSumatraUpTo(0);
}

bool IsRevealingLettersAnimRunning() {
    return gRevealingLettersAnim;
}

static void RevealingLettersAnimStop() {
    gRevealingLettersAnim = false;
    SetLettersSumatra();
}

static void RevealingLettersAnim() {
    double elapsed = TimeSinceInMs(gRevealingAnimStart) / 1000.0;
    if (elapsed > kRevealingAnimDur) {
        RevealingLettersAnimStop();
        return;
    }
    int want = (int)(elapsed * (double)kSumatraLettersCount / kRevealingAnimDur) + 1;
    if (want <= gRevealingLettersAnimLettersToShow) {
        return;
    }
    gRevealingLettersAnimLettersToShow = want;
    SetLettersSumatraUpTo(want);
}

void AnimStep() {
    if (gRevealingLettersAnim) {
        RevealingLettersAnim();
    }
}

#endif // OS_WIN
