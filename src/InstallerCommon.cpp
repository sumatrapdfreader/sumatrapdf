/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// code used in both Installer.cpp and Uninstaller.cpp

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/FrameTimeoutCalculator.h"

#include "Translations.h"

#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "resource.h"
#include "Version.h"
#include "Installer.h"

#include "utils/Log.h"

// set to true to enable shadow effect
constexpr bool kDrawTextShadow = true;
constexpr bool kDrawMsgTextShadow = false;

constexpr COLORREF kInstallerWinBgColor = RGB(0xff, 0xf2, 0); // yellow

constexpr DWORD kTenSecondsInMs = 10 * 1000;

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontStyleRegular;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;

Color gCol1(196, 64, 50);
Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35);
Color gCol2Shadow(155, 77, 31);
Color gCol3(93, 160, 40);
Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190);
Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207);
Color gCol5Shadow(66, 71, 118);

Color COLOR_MSG_WELCOME(gCol5);
Color COLOR_MSG_OK(gCol5);
Color COLOR_MSG_INSTALLATION(gCol5);
Color COLOR_MSG_FAILED(gCol1);

HWND gHwndFrame = nullptr;
WCHAR* gFirstError = nullptr;
bool gForceCrash = false;
WCHAR* gMsgError = nullptr;
int gBottomPartDy = 0;
int gButtonDy = 0;

Flags* gCli = nullptr;

const WCHAR* gDefaultMsg = nullptr; // Note: translation, not freeing

static AutoFreeWstr gMsg;
static Color gMsgColor;

static StrVec gProcessesToClose;

PreviousInstallationInfo::~PreviousInstallationInfo() {
    free(installationDir);
}

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
TempStr GetRegPathUninstTemp(const char* appName) {
    return str::JoinTemp("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", appName);
}

void NotifyFailed(const char* msg) {
    if (!gFirstError) {
        gFirstError = ToWstr(msg);
    }
    logf("NotifyFailed: %s\n", msg);
}

void SetMsg(const WCHAR* msg, Color color) {
    gMsg.SetCopy(msg);
    gMsgColor = color;
}

char* gCachedExistingInstallationDir = nullptr;

// caller has to free()
char* GetExistingInstallationDir() {
    if (gCachedExistingInstallationDir) {
        // no logging if returning cached
        return str::Dup(gCachedExistingInstallationDir);
    }
    log("GetExistingInstallationDir()\n");
    TempStr regPathUninst = GetRegPathUninstTemp(kAppName);
    TempStr dir = LoggedReadRegStr2Temp(regPathUninst, "InstallLocation");
    if (!dir) {
        return nullptr;
    }
    if (str::EndsWithI(dir, ".exe")) {
        dir = path::GetDirTemp(dir);
    }
    if (!str::IsEmpty(dir) && dir::Exists(dir)) {
        gCachedExistingInstallationDir = str::Dup(dir);
        return str::Dup(dir);
    }
    return nullptr;
}

void GetPreviousInstallInfo(PreviousInstallationInfo* info) {
    info->installationDir = GetExistingInstallationDir();
    if (!info->installationDir) {
        info->typ = PreviousInstallationType::None;
        log("GetPreviousInstallInfo: not installed\n");
        return;
    }
    info->searchFilterInstalled = IsSearchFilterInstalled();
    info->previewInstalled = IsPreviewInstalled();
    char* regPathUninst = GetRegPathUninstTemp(kAppName);
    char* dirLM = LoggedReadRegStrTemp(HKEY_LOCAL_MACHINE, regPathUninst, "InstallLocation");
    char* dirCU = LoggedReadRegStrTemp(HKEY_CURRENT_USER, regPathUninst, "InstallLocation");
    if (dirLM && dirCU) {
        info->typ = PreviousInstallationType::Both;
    } else if (dirLM) {
        info->typ = PreviousInstallationType::Machine;
    } else {
        info->typ = PreviousInstallationType::User;
    }
    logf("GetPreviousInstallInfo: dir '%s', search filter: %d, preview: %d, typ: %d\n", info->installationDir,
         (int)info->searchFilterInstalled, (int)info->previewInstalled, (int)info->typ);
}

static char* GetExistingInstallationFilePathTemp(const char* name) {
    char* dir = GetExistingInstallationDir();
    if (!dir) {
        return nullptr;
    }
    return path::JoinTemp(dir, name);
}

char* GetInstallDirTemp() {
    logf("GetInstallDirTemp() => %s\n", gCli->installDir);
    return gCli->installDir;
}

char* GetInstallationFilePathTemp(const char* name) {
    TempStr res = path::JoinTemp(gCli->installDir, name);
    logf("GetInstallationFilePath(%s) = > %s\n", name, res);
    return res;
}

TempStr GetInstalledExePathTemp() {
    TempStr dir = GetInstallDirTemp();
    return path::JoinTemp(dir, kExeName);
}

TempStr GetShortcutPathTemp(int csidl) {
    TempStr dir = GetSpecialFolderTemp(csidl, false);
    if (!dir) {
        return {};
    }
    TempStr lnkName = str::JoinTemp(kAppName, ".lnk");
    return path::JoinTemp(dir, lnkName);
}

static char* GetInstalledBrowserPluginPathTemp() {
#ifndef _WIN64
    const char* kRegPathPlugin = "Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin";
#else
    const char* kRegPathPlugin = "Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin_x64";
#endif
    return LoggedReadRegStr2Temp(kRegPathPlugin, "Path");
}

static bool IsProcessUsingFiles(DWORD procId, const char* file1, const char* file2) {
    // Note: don't know why procId 0 shows up as using our files
    if (procId == 0 || procId == GetCurrentProcessId()) {
        return false;
    }
    if (!file1 && !file2) {
        return false;
    }
    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32 mod{};
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        WCHAR* exePathW = mod.szExePath;
        char* exePath = ToUtf8Temp(exePathW);
        if (file1 && path::IsSame(file1, exePath)) {
            return true;
        }
        if (file2 && path::IsSame(file2, exePath)) {
            return true;
        }
        cont = Module32Next(snap, &mod);
    }
    return false;
}

constexpr const char* kBrowserPluginName = "npPdfViewer.dll";

void UninstallBrowserPlugin() {
    log("UninstallBrowserPlugin()\n");
    char* dllPath = GetExistingInstallationFilePathTemp(kBrowserPluginName);
    if (!file::Exists(dllPath)) {
        // uninstall the detected plugin, even if it isn't in the target installation path
        dllPath = GetInstalledBrowserPluginPathTemp();
        if (!file::Exists(dllPath)) {
            return;
        }
    }
    bool ok = UnRegisterServerDLL(dllPath);
    if (ok) {
        log("  did uninstall browser plugin\n");
        return;
    }
    log("  failed to uninstall browser plugin\n");
    NotifyFailed(_TRA("Couldn't uninstall browser plugin"));
}

constexpr const char* kSearchFilterDllName = "PdfFilter.dll";

void RegisterSearchFilter(bool allUsers) {
    char* dllPath = GetInstallationFilePathTemp(kSearchFilterDllName);
    logf("RegisterSearchFilter() dllPath=%s\n", dllPath);
    bool ok = InstallSearchFiler(dllPath, allUsers);
    if (ok) {
        log("  did registe\n");
        return;
    }
    log("  failed to register\n");
    NotifyFailed(_TRA("Couldn't install PDF search filter"));
}

void UnRegisterSearchFilter() {
    char* dllPath = GetExistingInstallationFilePathTemp(kSearchFilterDllName);
    logf("UnRegisterSearchFilter() dllPath=%s\n", dllPath);
    bool ok = UninstallSearchFilter();
    if (ok) {
        log("  did unregister\n");
        return;
    }
    log("  failed to unregister\n");
    NotifyFailed(_TRA("Couldn't uninstall Sumatra search filter"));
}

constexpr const char* kPreviewDllName = "PdfPreview.dll";

void RegisterPreviewer(bool allUsers) {
    char* dllPath = GetInstallationFilePathTemp(kPreviewDllName);
    logf("RegisterPreviewer() dllPath=%s\n", dllPath);
    bool ok = InstallPreviewDll(dllPath, allUsers);
    if (ok) {
        log("  did register\n");
        return;
    }
    log("  failed to register\n");
    NotifyFailed(_TRA("Couldn't install PDF previewer"));
}

void UnRegisterPreviewer() {
    char* dllPath = GetExistingInstallationFilePathTemp(kPreviewDllName);
    logf("UnRegisterPreviewer() dllPath=%s\n", dllPath);
    bool ok = UninstallPreviewDll();
    if (ok) {
        log("  did unregister\n");
        return;
    }
    log(" failed to unregister\n");
    NotifyFailed(_TRA("Couldn't uninstall PDF previewer"));
}

static bool IsProcWithModule(DWORD processId, const char* modulePath) {
    AutoCloseHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hModSnapshot.IsValid()) {
        return false;
    }

    MODULEENTRY32W me32{};
    me32.dwSize = sizeof(me32);
    BOOL ok = Module32FirstW(hModSnapshot, &me32);
    char* path;
    while (ok) {
        path = ToUtf8Temp(me32.szExePath);
        if (path::IsSame(modulePath, path)) {
            return true;
        }
        ok = Module32NextW(hModSnapshot, &me32);
    }
    return false;
}

static bool KillProcWithId(DWORD processId, bool waitUntilTerminated) {
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

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
static bool KillProcWithIdAndModule(DWORD processId, const char* modulePath, bool waitUntilTerminated) {
    if (!IsProcWithModule(processId, modulePath)) {
        return false;
    }
    logf("KillProcWithIdAndModule() processId=%d, modulePath=%s\n", processId, modulePath);

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

// returns number of killed processes that have a module (exe or dll) with a given
// modulePath
// returns -1 on error, 0 if no matching processes
int KillProcessesWithModule(const char* modulePath, bool waitUntilTerminated) {
    logf("KillProcessesWithModule: '%s'\n", modulePath);
    AutoCloseHandle hProcSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hProcSnapshot) {
        return -1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32First(hProcSnapshot, &pe32)) {
        return -1;
    }

    int killCount = 0;
    do {
        if (KillProcWithIdAndModule(pe32.th32ProcessID, modulePath, waitUntilTerminated)) {
            logf("  killed process with id %d\n", (int)pe32.th32ProcessID);
            killCount++;
        }
    } while (Process32Next(hProcSnapshot, &pe32));

    if (killCount > 0) {
        UpdateWindow(FindWindow(nullptr, L"Shell_TrayWnd"));
        UpdateWindow(GetDesktopWindow());
    }
    return killCount;
}

// In order to install over existing installation or uninstall
// we need to kill all processes that that use files from
// installation directory. We only need to check processes that
// have libmupdf.dll from installation directory loaded
// because that covers SumatraPDF.exe and processes like dllhost.exe
// that load PdfPreview.dll or PdfFilter.dll (which link to libmupdf.dll)
// returns false if there are processes and we failed to kill them
static bool KillProcessesUsingInstallation() {
    log("KillProcessesUsingInstallation()\n");
    AutoFreeStr dir = GetExistingInstallationDir();
    if (dir.empty()) {
        return true;
    }
    TempStr libmupdf = path::JoinTemp(dir, "libmupdf.dll");
    TempStr browserPlugin = path::JoinTemp(dir, kBrowserPluginName);

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return false;
    }

    bool killedAllProcesses = true;
    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        if (IsProcessUsingFiles(procID, libmupdf, browserPlugin)) {
            char* s = ToUtf8Temp(proc.szExeFile);
            logf("  attempting to kill process %d '%s'\n", (int)procID, s);
            bool didKill = KillProcWithId(procID, true);
            logf("  KillProcWithId(%d) returned %d\n", procID, (int)didKill);
            if (!didKill) {
                killedAllProcesses = false;
            }
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }
    return killedAllProcesses;
}

// return names of processes that are running part of the installation
// (i.e. have libmupdf.dll or npPdfViewer.dll loaded)
static void ProcessesUsingInstallation(StrVec& names) {
    log("ProcessesUsingInstallation()\n");
    AutoFreeStr dir = GetExistingInstallationDir();
    if (dir.empty()) {
        return;
    }
    char* libmupdf = path::JoinTemp(dir, "libmupdf.dll");
    char* browserPlugin = path::JoinTemp(dir, kBrowserPluginName);

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return;
    }

    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        if (IsProcessUsingFiles(procID, libmupdf, browserPlugin)) {
            // TODO: this kils ReadableProcName logic
            TempStr s = ToUtf8Temp(proc.szExeFile);
            TempStr name = str::FormatTemp("%s (%d)", s, (int)procID);
            names.Append(name);
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }
}

// clang-format off
static const char* readableProcessNames[] = {
    nullptr, nullptr, // to be filled with our process
    "plugin-container.exe", "Mozilla Firefox",
    "chrome.exe", "Google Chrome",
    "prevhost.exe", "Windows Explorer",
    "dllhost.exe", "Windows Explorer"
};
// clang-format on

static const char* ReadableProcName(const char* procPath) {
    readableProcessNames[0] = kExeName;
    readableProcessNames[1] = kAppName;
    TempStr procName = path::GetBaseNameTemp(procPath);
    for (size_t i = 0; i < dimof(readableProcessNames); i += 2) {
        if (str::EqI(procName, readableProcessNames[i])) {
            return readableProcessNames[i + 1];
        }
    }
    return procName;
}

static void SetCloseProcessMsg() {
    int n = gProcessesToClose.Size();
    const char* procNames = ReadableProcName(gProcessesToClose.at(0));
    for (int i = 1; i < n; i++) {
        const char* name = ReadableProcName(gProcessesToClose.at(i));
        if (i < n - 1) {
            procNames = str::JoinTemp(procNames, ", ", name);
        } else {
            procNames = str::JoinTemp(procNames, " and ", name);
        }
    }
    TempStr s = str::FormatTemp(_TRA("Please close %s to proceed!"), procNames);
    SetMsg(ToWStrTemp(s), COLOR_MSG_FAILED);
}

void SetDefaultMsg() {
    SetMsg(gDefaultMsg, COLOR_MSG_WELCOME);
}

void InvalidateFrame() {
    HwndInvalidate(gHwndFrame);
}

bool CheckInstallUninstallPossible(bool silent) {
    logf("CheckInstallUninstallPossible(silent=%d)\n", silent);
    KillProcessesUsingInstallation();
    // logf("CheckInstallUninstallPossible: KillProcessesUsingInstallation() returned %d\n", ok);

    // now determine which processes are using installation files
    // and ask user to close them.
    // shouldn't be necessary given KillProcessesUsingInstallation(), we
    // do it just in case
    gProcessesToClose.Reset();
    ProcessesUsingInstallation(gProcessesToClose);

    bool possible = gProcessesToClose.size() == 0;
    if (possible) {
        SetDefaultMsg();
    } else {
        SetCloseProcessMsg();
        if (!silent) {
            MessageBeep(MB_ICONEXCLAMATION);
        }
    }
    InvalidateFrame();

    return possible;
}

// This display is inspired by http://letteringjs.com/
typedef struct {
    // part that doesn't change
    char c;
    Color col, colShadow;
    float rotation;
    float dyOff; // displacement

    // part calculated during layout
    float dx, dy;
    float x;
} LetterInfo;

// clang-format off
LetterInfo gLetters[] = {
    {'S', gCol1, gCol1Shadow, -3.f, 0, 0, 0},
    {'U', gCol2, gCol2Shadow, 0.f, 0, 0, 0},
    {'M', gCol3, gCol3Shadow, 2.f, -2.f, 0, 0},
    {'A', gCol4, gCol4Shadow, 0.f, -2.4f, 0, 0},
    {'T', gCol5, gCol5Shadow, 0.f, 0, 0, 0},
    {'R', gCol5, gCol5Shadow, 2.3f, -1.4f, 0, 0},
    {'A', gCol4, gCol4Shadow, 0.f, 0, 0, 0},
    {'P', gCol3, gCol3Shadow, 0.f, -2.3f, 0, 0},
    {'D', gCol2, gCol2Shadow, 0.f, 3.f, 0, 0},
    {'F', gCol1, gCol1Shadow, 0.f, 0, 0, 0}
};
// clang-format on

#if 0
static char RandUppercaseLetter()
{
    // note: clearly, not random but seems to work ok anyway
    static char l = 'A' - 1;
    l++;
    if (l > 'Z')
        l = 'A';
    return l;
}

static void RandomizeLetters()
{
    for (int i = 0; i < dimof(gLetters); i++) {
        gLetters[i].c = RandUppercaseLetter();
    }
}
#endif

#define kSumatraLettersCount (dimof(gLetters))

static void SetLettersSumatraUpTo(size_t n) {
    const char* s = "SUMATRAPDF";
    for (size_t i = 0; i < kSumatraLettersCount; i++) {
        char c = ' ';
        if (i < n) {
            c = s[i];
        }
        gLetters[i].c = c;
    }
}

static void SetLettersSumatra() {
    SetLettersSumatraUpTo(kSumatraLettersCount);
}

// an animation that reveals letters one by one

// how long the animation lasts, in seconds
#define REVEALING_ANIM_DUR double(2)

static FrameTimeoutCalculator* gRevealingLettersAnim = nullptr;

int gRevealingLettersAnimLettersToShow;

static void RevealingLettersAnimStart() {
    int framesPerSec = (int)(double(kSumatraLettersCount) / REVEALING_ANIM_DUR);
    gRevealingLettersAnim = new FrameTimeoutCalculator(framesPerSec);
    gRevealingLettersAnimLettersToShow = 0;
    SetLettersSumatraUpTo(0);
}

static void RevealingLettersAnimStop() {
    delete gRevealingLettersAnim;
    gRevealingLettersAnim = nullptr;
    SetLettersSumatra();
    InvalidateFrame();
}

static void RevealingLettersAnim() {
    if (gRevealingLettersAnim->ElapsedTotal() > REVEALING_ANIM_DUR) {
        RevealingLettersAnimStop();
        return;
    }
    DWORD timeOut = gRevealingLettersAnim->GetTimeoutInMilliseconds();
    if (timeOut != 0) {
        return;
    }
    SetLettersSumatraUpTo(++gRevealingLettersAnimLettersToShow);
    gRevealingLettersAnim->Step();
    InvalidateFrame();
}

void AnimStep() {
    if (gRevealingLettersAnim) {
        RevealingLettersAnim();
    }
}

static void CalcLettersLayout(Graphics& g, Font* f, int dx) {
    static BOOL didLayout = FALSE;
    if (didLayout) {
        return;
    }

    LetterInfo* li;
    StringFormat sfmt;
    const float letterSpacing = -12.f;
    float totalDx = -letterSpacing; // counter last iteration of the loop
    WCHAR s[2]{};
    Gdiplus::PointF origin(0.f, 0.f);
    Gdiplus::RectF bbox;
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        g.MeasureString(s, 1, f, origin, &sfmt, &bbox);
        li->dx = bbox.Width;
        li->dy = bbox.Height;
        totalDx += li->dx;
        totalDx += letterSpacing;
    }

    float x = ((float)dx - totalDx) / 2.f;
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        li->x = x;
        x += li->dx;
        x += letterSpacing;
    }
    RevealingLettersAnimStart();
    didLayout = TRUE;
}

static float DrawMessage(Graphics& g, const WCHAR* msg, float y, float dx, Color color) {
    AutoFreeWstr s = str::Dup(msg);

    Font f(L"Impact", 16, FontStyleRegular);
    Gdiplus::RectF maxbox(0, y, dx, 0);
    Gdiplus::RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
    if (trans::IsCurrLangRtl()) {
        sft.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
    }

    if (kDrawMsgTextShadow) {
        bbox.X--;
        bbox.Y++;
        SolidBrush b(Color(0xff, 0xff, 0xff));
        g.DrawString(s, -1, &f, bbox, &sft, &b);
        bbox.X++;
        bbox.Y--;
    }

    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
}

static void DrawSumatraLetters(Graphics& g, Font* f, Font* fVer, float y) {
    LetterInfo* li;
    WCHAR s[2]{};
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        if (s[0] == ' ') {
            return;
        }

        g.RotateTransform(li->rotation, MatrixOrderAppend);
        if (kDrawTextShadow) {
            // draw shadow first
            SolidBrush b2(li->colShadow);
            Gdiplus::PointF o2(li->x - 3.f, y + 4.f + li->dyOff);
            g.DrawString(s, 1, f, o2, &b2);
        }

        SolidBrush b1(li->col);
        Gdiplus::PointF o1(li->x, y + li->dyOff);
        g.DrawString(s, 1, f, o1, &b1);
        g.RotateTransform(li->rotation, MatrixOrderAppend);
        g.ResetTransform();
    }

    // draw version number
    float x = gLetters[dimof(gLetters) - 1].x;
    g.TranslateTransform(x, y);
    g.RotateTransform(45.f);
    float x2 = 15;
    float y2 = -34;

    const WCHAR* ver_s = L"v" CURR_VERSION_STR;
    if (kDrawTextShadow) {
        SolidBrush b1(Color(0, 0, 0));
        g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2 - 2, y2 - 1), &b1);
    }
    SolidBrush b2(Color(0xff, 0xff, 0xff));
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2, y2), &b2);
    g.ResetTransform();
}

static void DrawFrame2(Graphics& g, Rect r, bool skipMessage) {
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(Gdiplus::UnitPixel);

    Font f(L"Impact", 40, FontStyleRegular);
    CalcLettersLayout(g, &f, r.dx);

    Gdiplus::Color bgCol;
    bgCol.SetFromCOLORREF(kInstallerWinBgColor);
    SolidBrush bgBrush(bgCol);
    Gdiplus::Rect r2(ToGdipRect(r));
    r2.Inflate(1, 1);
    g.FillRectangle(&bgBrush, r2);

    Font f2(L"Impact", 16, FontStyleRegular);
    DrawSumatraLetters(g, &f, &f2, 18.f);

    if (skipMessage) {
        return;
    }

    float msgY = (float)(r.dy / 2);
    if (gMsg) {
        msgY += DrawMessage(g, gMsg, msgY, (float)r.dx, gMsgColor) + 5;
    }
    if (gMsgError) {
        DrawMessage(g, gMsgError, msgY, (float)r.dx, COLOR_MSG_FAILED);
    }
}

static void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT*, bool skipMessage) {
    // TODO: cache bmp object?
    Graphics g(dc);
    Rect rc = ClientRect(hwnd);
    Bitmap bmp(rc.dx, rc.dy, &g);
    Graphics g2((Image*)&bmp);
    DrawFrame2(g2, rc, skipMessage);
    g.DrawImage(&bmp, 0, 0);
}

void OnPaintFrame(HWND hwnd, bool skipMessage) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps, skipMessage);
    EndPaint(hwnd, &ps);
}
