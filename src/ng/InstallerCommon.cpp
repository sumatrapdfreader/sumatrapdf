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

Str gMsg;
Color gMsgColor = gCol5;

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

void SetMsg(Str msg, Color color) {
    gMsg = str::Dup(GetPermArena(), msg);
    gMsgColor = color;
}

// ng installs no search filter or previewer
void GetPrevInstallShellExt(PreviousInstallationInfo*) {}

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

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
bool KillProcWithIdAndModule(DWORD processId, Str modulePath, bool waitUntilTerminated) {
    if (!IsProcWithModule(processId, modulePath)) {
        return false;
    }
    logf("KillProcWithIdAndModule() processId=%d, modulePath=%s\n", processId, modulePath);
    return KillProcWithId(processId, waitUntilTerminated);
}

// Kill processes that have our installed exe loaded.
// returns false if there are processes and we failed to kill them
bool KillProcessesUsingInstallationDir(Str dir) {
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

// return names of processes that are running part of the installation
void ProcessesUsingInstallation(StrVec& names) {
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

// ng repaints on its next frame
void InstallerRepaintNow() {}

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

void RevealingLettersAnim() {
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

#endif // OS_WIN
