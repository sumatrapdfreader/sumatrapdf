/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

// define to allow testing crash handling via -crash cmd-line option
#define ENABLE_CRASH_TESTING

// define for testing the uninstaller
// #define TEST_UNINSTALLER
#if defined(TEST_UNINSTALLER) && !defined(BUILD_UNINSTALLER)
#define BUILD_UNINSTALLER
#endif

#include "BaseUtil.h"
#include "WinDynCalls.h"
#include <tlhelp32.h>
#include <io.h>
#include "FileUtil.h"
#include "Translations.h"
#include "Resource.h"
#include "Timer.h"
#include "Version.h"
#include "WinUtil.h"
#include "Installer.h"
#include "CmdLineParser.h"
#include "CrashHandler.h"
#include "Dpi.h"
#include "FrameTimeoutCalculator.h"
#include "DebugLog.h"

// TODO: can't build these separately without breaking TEST_UNINSTALLER
#ifdef BUILD_UNINSTALLER
#include "Uninstall.cpp"
#else
#include "ByteOrderDecoder.h"
#include "LzmaSimpleArchive.h"

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"
#include "Install.cpp"
#endif

using namespace Gdiplus;

// define to 1 to enable shadow effect, to 0 to disable
#define DRAW_TEXT_SHADOW 1
#define DRAW_MSG_TEXT_SHADOW 0

HWND gHwndFrame = nullptr;
HWND gHwndButtonExit = nullptr;
HWND gHwndButtonInstUninst = nullptr;
HFONT gFontDefault;
bool gShowOptions = false;
bool gForceCrash = false;
WCHAR* gMsgError = nullptr;
int gBottomPartDy = 0;
int gButtonDy = 0;

static WStrVec gProcessesToClose;
static float gUiDPIFactor = 1.0f;

static AutoFreeW gMsg;
static Color gMsgColor;

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

// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
WCHAR* gSupportedExts[] = {L".pdf",  L".xps", L".oxps", L".cbz",  L".cbr", L".cb7",  L".cbt",
                           L".djvu", L".chm", L".mobi", L".epub", L".fb2", L".fb2z", nullptr};

// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
PayloadInfo gPayloadData[] = {
    {"libmupdf.dll", true},          {"SumatraPDF.exe", true},   {"sumatrapdfprefs.dat", false},
    {"DroidSansFallback.ttf", true}, {"npPdfViewer.dll", false}, {"PdfFilter.dll", true},
    {"PdfPreview.dll", true},        {"uninstall.exe", true},    {nullptr, false},
};

GlobalData gGlobalData = {
    false,   /* bool silent */
    false,   /* bool showUsageAndQuit */
    nullptr, /* WCHAR *installDir */
#ifndef BUILD_UNINSTALLER
    false, /* bool registerAsDefault */
    false, /* bool installPdfFilter */
    false, /* bool installPdfPreviewer */
    true,  /* bool keepBrowserPlugin */
    false, /* bool extractFiles */
    false, /* bool autoUpdate */
#endif

    nullptr, /* WCHAR *firstError */
    nullptr, /* HANDLE hThread */
    false,   /* bool success */
};

void NotifyFailed(const WCHAR* msg) {
    if (!gGlobalData.firstError)
        gGlobalData.firstError = str::Dup(msg);
    plogf(L"%s", msg);
}

void SetMsg(const WCHAR* msg, Color color) {
    gMsg.SetCopy(msg);
    gMsgColor = color;
}

#define TEN_SECONDS_IN_MS 10 * 1000

static bool IsProcWithName(DWORD processId, const WCHAR* modulePath) {
    ScopedHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hModSnapshot.IsValid())
        return false;

    MODULEENTRY32W me32 = {0};
    me32.dwSize = sizeof(me32);
    BOOL ok = Module32FirstW(hModSnapshot, &me32);
    while (ok) {
        if (path::IsSame(modulePath, me32.szExePath))
            return true;
        ok = Module32NextW(hModSnapshot, &me32);
    }
    return false;
}

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
static bool KillProcIdWithName(DWORD processId, const WCHAR* modulePath, bool waitUntilTerminated) {
    if (!IsProcWithName(processId, modulePath))
        return false;

    BOOL inheritHandle = FALSE;
    // Note: do I need PROCESS_QUERY_INFORMATION and PROCESS_VM_READ?
    DWORD dwAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE;
    ScopedHandle hProcess(OpenProcess(dwAccess, inheritHandle, processId));
    if (!hProcess.IsValid())
        return false;

    BOOL killed = TerminateProcess(hProcess, 0);
    if (!killed)
        return false;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    return true;
}

// returns number of killed processes that have a module (exe or dll) with a given
// modulePath
// returns -1 on error, 0 if no matching processes
int KillProcess(const WCHAR* modulePath, bool waitUntilTerminated) {
    ScopedHandle hProcSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == hProcSnapshot)
        return -1;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32First(hProcSnapshot, &pe32))
        return -1;

    int killCount = 0;
    do {
        if (KillProcIdWithName(pe32.th32ProcessID, modulePath, waitUntilTerminated))
            killCount++;
    } while (Process32Next(hProcSnapshot, &pe32));

    if (killCount > 0) {
        UpdateWindow(FindWindow(nullptr, L"Shell_TrayWnd"));
        UpdateWindow(GetDesktopWindow());
    }
    return killCount;
}

const WCHAR* GetOwnPath() {
    static WCHAR exePath[MAX_PATH];
    exePath[0] = '\0';
    GetModuleFileName(nullptr, exePath, dimof(exePath));
    exePath[dimof(exePath) - 1] = '\0';
    return exePath;
}

static WCHAR* GetInstallationDir() {
    AutoFreeW dir(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, L"InstallLocation"));
    if (!dir)
        dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

#ifndef BUILD_UNINSTALLER
    // fall back to %ProgramFiles%
    dir.Set(GetSpecialFolder(CSIDL_PROGRAM_FILES));
    if (dir)
        return path::Join(dir, APP_NAME_STR);
    // fall back to C:\ as a last resort
    return str::Dup(L"C:\\" APP_NAME_STR);
#else
    // fall back to the uninstaller's path
    return path::GetDir(GetOwnPath());
#endif
}

WCHAR* GetUninstallerPath() {
    return path::Join(gGlobalData.installDir, L"uninstall.exe");
}

WCHAR* GetInstalledExePath() {
    return path::Join(gGlobalData.installDir, EXENAME);
}

static WCHAR* GetBrowserPluginPath() {
    return path::Join(gGlobalData.installDir, L"npPdfViewer.dll");
}

WCHAR* GetInstalledBrowserPluginPath() {
    WCHAR* path = ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_PLUGIN, L"Path");
    if (!path)
        path = ReadRegStr(HKEY_CURRENT_USER, REG_PATH_PLUGIN, L"Path");
    return path;
}

static WCHAR* GetPdfFilterPath() {
    return path::Join(gGlobalData.installDir, L"PdfFilter.dll");
}

static WCHAR* GetPdfPreviewerPath() {
    return path::Join(gGlobalData.installDir, L"PdfPreview.dll");
}

WCHAR* GetShortcutPath(bool allUsers) {
    // CSIDL_COMMON_PROGRAMS => installing for all users
    AutoFreeW dir(GetSpecialFolder(allUsers ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS));
    if (!dir)
        return nullptr;
    return path::Join(dir, APP_NAME_STR L".lnk");
}

/* if the app is running, we have to kill it so that we can over-write the executable */
void KillSumatra() {
    AutoFreeW exePath(GetInstalledExePath());
    KillProcess(exePath, true);
}

#if 1
static HFONT CreateDefaultGuiFont() {
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT f = CreateFontIndirectW(&ncm.lfMenuFont);
    return f;
}
#else
static HFONT CreateDefaultGuiFont() {
    HDC hdc = GetDC(nullptr);
    HFONT font = CreateSimpleFont(hdc, L"MS Shell Dlg", 14);
    ReleaseDC(nullptr, hdc);
    return font;
}
#endif

int dpiAdjust(int value) {
    return (int)(value * gUiDPIFactor);
}

void InvalidateFrame() {
    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, FALSE);
}

bool CreateProcessHelper(const WCHAR* exe, const WCHAR* args) {
    AutoFreeW cmd(str::Format(L"\"%s\" %s", exe, args ? args : L""));
    ScopedHandle process(LaunchProcess(cmd));
    return process != nullptr;
}

// cf. http://support.microsoft.com/default.aspx?scid=kb;en-us;207132
static bool RegisterServerDLL(const WCHAR* dllPath, bool install, const WCHAR* args = nullptr) {
    if (FAILED(OleInitialize(nullptr)))
        return false;

    // make sure that the DLL can find any DLLs it depends on and
    // which reside in the same directory (in this case: libmupdf.dll)
    if (DynSetDllDirectoryW) {
        AutoFreeW dllDir(path::GetDir(dllPath));
        DynSetDllDirectoryW(dllDir);
    }

    bool ok = false;
    HMODULE lib = LoadLibrary(dllPath);
    if (lib) {
        typedef HRESULT(WINAPI * DllInstallProc)(BOOL, LPCWSTR);
        typedef HRESULT(WINAPI * DllRegUnregProc)(VOID);
        if (args) {
            DllInstallProc DllInstall = (DllInstallProc)GetProcAddress(lib, "DllInstall");
            if (DllInstall)
                ok = SUCCEEDED(DllInstall(install, args));
            else
                args = nullptr;
        }
        if (!args) {
            const char* func = install ? "DllRegisterServer" : "DllUnregisterServer";
            DllRegUnregProc DllRegUnreg = (DllRegUnregProc)GetProcAddress(lib, func);
            if (DllRegUnreg)
                ok = SUCCEEDED(DllRegUnreg());
        }
        FreeLibrary(lib);
    }

    if (DynSetDllDirectoryW)
        DynSetDllDirectoryW(L"");

    OleUninitialize();

    return ok;
}

static bool IsUsingInstallation(DWORD procId) {
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId));
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    AutoFreeW libmupdf(path::Join(gGlobalData.installDir, L"libmupdf.dll"));
    AutoFreeW browserPlugin(GetBrowserPluginPath());

    MODULEENTRY32 mod = {0};
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        if (path::IsSame(libmupdf, mod.szExePath) || path::IsSame(browserPlugin, mod.szExePath)) {
            return true;
        }
        cont = Module32Next(snap, &mod);
    }

    return false;
}

// return names of processes that are running part of the installation
// (i.e. have libmupdf.dll or npPdfViewer.dll loaded)
static void ProcessesUsingInstallation(WStrVec& names) {
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == snap)
        return;

    PROCESSENTRY32 proc = {0};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        if (IsUsingInstallation(proc.th32ProcessID)) {
            names.Append(str::Dup(proc.szExeFile));
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }
}

static void SetDefaultMsg() {
#ifdef BUILD_UNINSTALLER
    SetMsg(_TR("Are you sure you want to uninstall SumatraPDF?"), COLOR_MSG_WELCOME);
#else
    SetMsg(_TR("Thank you for choosing SumatraPDF!"), COLOR_MSG_WELCOME);
#endif
}

static const WCHAR* ReadableProcName(const WCHAR* procPath) {
    const WCHAR* nameList[] = {
        EXENAME,          APP_NAME_STR,    L"plugin-container.exe", L"Mozilla Firefox", L"chrome.exe",
        L"Google Chrome", L"prevhost.exe", L"Windows Explorer",     L"dllhost.exe",     L"Windows Explorer",
    };
    const WCHAR* procName = path::GetBaseName(procPath);
    for (size_t i = 0; i < dimof(nameList); i += 2) {
        if (str::EqI(procName, nameList[i]))
            return nameList[i + 1];
    }
    return procName;
}

static void SetCloseProcessMsg() {
    AutoFreeW procNames(str::Dup(ReadableProcName(gProcessesToClose.At(0))));
    for (size_t i = 1; i < gProcessesToClose.Count(); i++) {
        const WCHAR* name = ReadableProcName(gProcessesToClose.At(i));
        if (i < gProcessesToClose.Count() - 1)
            procNames.Set(str::Join(procNames, L", ", name));
        else
            procNames.Set(str::Join(procNames, L" and ", name));
    }
    AutoFreeW s(str::Format(_TR("Please close %s to proceed!"), procNames));
    SetMsg(s, COLOR_MSG_FAILED);
}

bool CheckInstallUninstallPossible(bool silent) {
    gProcessesToClose.Reset();
    ProcessesUsingInstallation(gProcessesToClose);

    bool possible = gProcessesToClose.Count() == 0;
    if (possible) {
        SetDefaultMsg();
    } else {
        SetCloseProcessMsg();
        if (!silent)
            MessageBeep(MB_ICONEXCLAMATION);
    }
    InvalidateFrame();

    return possible;
}

void UninstallBrowserPlugin() {
    AutoFreeW dllPath(GetBrowserPluginPath());
    if (!file::Exists(dllPath)) {
        // uninstall the detected plugin, even if it isn't in the target installation path
        dllPath.Set(GetInstalledBrowserPluginPath());
        if (!file::Exists(dllPath))
            return;
    }
    if (!RegisterServerDLL(dllPath, false))
        NotifyFailed(_TR("Couldn't uninstall browser plugin"));
}

void InstallPdfFilter() {
    AutoFreeW dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_TR("Couldn't install PDF search filter"));
}

void UninstallPdfFilter() {
    AutoFreeW dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath, false))
        NotifyFailed(_TR("Couldn't uninstall PDF search filter"));
}

void InstallPdfPreviewer() {
    AutoFreeW dllPath(GetPdfPreviewerPath());
    // TODO: RegisterServerDLL(dllPath, true, L"exts:pdf,...");
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_TR("Couldn't install PDF previewer"));
}

void UninstallPdfPreviewer() {
    AutoFreeW dllPath(GetPdfPreviewerPath());
    // TODO: RegisterServerDLL(dllPath, false, L"exts:pdf,...");
    if (!RegisterServerDLL(dllPath, false))
        NotifyFailed(_TR("Couldn't uninstall PDF previewer"));
}

SIZE GetIdealButtonSize(HWND hwnd) {
    // adjust to real size and position to the right
    SIZE s;
    Button_GetIdealSize(hwnd, &s);
    // add padding
    s.cx += dpiAdjust(8) * 2;
    s.cy += dpiAdjust(2) * 2;
    return s;
}

SIZE SetButtonTextAndResize(HWND hwnd, const WCHAR* s) {
    win::SetText(hwnd, s);
    SIZE size = GetIdealButtonSize(hwnd);
    SetWindowPos(hwnd, nullptr, 0, 0, size.cx, size.cy,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return size;
}

// Creates a button that has a right size for it's text,
HWND CreateButton(HWND hwndParent, const WCHAR* s, int id, DWORD style, SIZE& sizeOut) {
    HMENU idMenu = (HMENU)(UINT_PTR)id;
    style |= WS_CHILD | WS_TABSTOP;
    HWND hwnd =
        CreateWindowExW(0, WC_BUTTON, L"", style, 0, 0, 100, 20, hwndParent, idMenu, GetModuleHandle(nullptr), nullptr);
    SetWindowFont(hwnd, gFontDefault, TRUE);
    sizeOut = SetButtonTextAndResize(hwnd, s);
    return hwnd;
}

HWND CreateDefaultButton(HWND hwndParent, const WCHAR* s, int id) {
    SIZE size;
    HWND hwnd = CreateButton(hwndParent, s, id, BS_DEFPUSHBUTTON, size);

    ClientRect r(hwndParent);
    int x = r.dx - size.cx - WINDOW_MARGIN;
    int y = r.dy - size.cy - WINDOW_MARGIN;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return hwnd;
}

void CreateButtonExit(HWND hwndParent) {
    gHwndButtonExit = CreateDefaultButton(hwndParent, _TR("Close"), ID_BUTTON_EXIT);
}

void OnButtonExit() {
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

// This display is inspired by http://letteringjs.com/
typedef struct {
    // part that doesn't change
    char c;
    Color col, colShadow;
    REAL rotation;
    REAL dyOff; // displacement

    // part calculated during layout
    REAL dx, dy;
    REAL x;
} LetterInfo;

LetterInfo gLetters[] = {{'S', gCol1, gCol1Shadow, -3.f, 0, 0, 0},   {'U', gCol2, gCol2Shadow, 0.f, 0, 0, 0},
                         {'M', gCol3, gCol3Shadow, 2.f, -2.f, 0, 0}, {'A', gCol4, gCol4Shadow, 0.f, -2.4f, 0, 0},
                         {'T', gCol5, gCol5Shadow, 0.f, 0, 0, 0},    {'R', gCol5, gCol5Shadow, 2.3f, -1.4f, 0, 0},
                         {'A', gCol4, gCol4Shadow, 0.f, 0, 0, 0},    {'P', gCol3, gCol3Shadow, 0.f, -2.3f, 0, 0},
                         {'D', gCol2, gCol2Shadow, 0.f, 3.f, 0, 0},  {'F', gCol1, gCol1Shadow, 0.f, 0, 0, 0}};

#define SUMATRA_LETTERS_COUNT (dimof(gLetters))

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

static void SetLettersSumatraUpTo(int n) {
    char* s = "SUMATRAPDF";
    for (int i = 0; i < dimof(gLetters); i++) {
        if (i < n) {
            gLetters[i].c = s[i];
        } else {
            gLetters[i].c = ' ';
        }
    }
}

static void SetLettersSumatra() {
    SetLettersSumatraUpTo(SUMATRA_LETTERS_COUNT);
}

// an animation that reveals letters one by one

// how long the animation lasts, in seconds
#define REVEALING_ANIM_DUR double(2)

static FrameTimeoutCalculator* gRevealingLettersAnim = nullptr;

int gRevealingLettersAnimLettersToShow;

static void RevealingLettersAnimStart() {
    int framesPerSec = (int)(double(SUMATRA_LETTERS_COUNT) / REVEALING_ANIM_DUR);
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
    if (timeOut != 0)
        return;
    SetLettersSumatraUpTo(++gRevealingLettersAnimLettersToShow);
    gRevealingLettersAnim->Step();
    InvalidateFrame();
}

static void AnimStep() {
    if (gRevealingLettersAnim)
        RevealingLettersAnim();
}

static void CalcLettersLayout(Graphics& g, Font* f, int dx) {
    static BOOL didLayout = FALSE;
    if (didLayout)
        return;

    LetterInfo* li;
    StringFormat sfmt;
    const REAL letterSpacing = -12.f;
    REAL totalDx = -letterSpacing; // counter last iteration of the loop
    WCHAR s[2] = {0};
    PointF origin(0.f, 0.f);
    RectF bbox;
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        g.MeasureString(s, 1, f, origin, &sfmt, &bbox);
        li->dx = bbox.Width;
        li->dy = bbox.Height;
        totalDx += li->dx;
        totalDx += letterSpacing;
    }

    REAL x = ((REAL)dx - totalDx) / 2.f;
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        li->x = x;
        x += li->dx;
        x += letterSpacing;
    }
    RevealingLettersAnimStart();
    didLayout = TRUE;
}

static REAL DrawMessage(Graphics& g, const WCHAR* msg, REAL y, REAL dx, Color color) {
    AutoFreeW s(str::Dup(msg));

    Font f(L"Impact", 16, FontStyleRegular);
    RectF maxbox(0, y, dx, 0);
    RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
    if (trans::IsCurrLangRtl())
        sft.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
#if DRAW_MSG_TEXT_SHADOW
    {
        bbox.X--;
        bbox.Y++;
        SolidBrush b(Color(0xff, 0xff, 0xff));
        g.DrawString(s, -1, &f, bbox, &sft, &b);
        bbox.X++;
        bbox.Y--;
    }
#endif
    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
}

static void DrawSumatraLetters(Graphics& g, Font* f, Font* fVer, REAL y) {
    LetterInfo* li;
    WCHAR s[2] = {0};
    for (int i = 0; i < dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        if (s[0] == ' ')
            return;

        g.RotateTransform(li->rotation, MatrixOrderAppend);
#if DRAW_TEXT_SHADOW
        // draw shadow first
        SolidBrush b2(li->colShadow);
        PointF o2(li->x - 3.f, y + 4.f + li->dyOff);
        g.DrawString(s, 1, f, o2, &b2);
#endif

        SolidBrush b1(li->col);
        PointF o1(li->x, y + li->dyOff);
        g.DrawString(s, 1, f, o1, &b1);
        g.RotateTransform(li->rotation, MatrixOrderAppend);
        g.ResetTransform();
    }

    // draw version number
    REAL x = gLetters[dimof(gLetters) - 1].x;
    g.TranslateTransform(x, y);
    g.RotateTransform(45.f);
    REAL x2 = 15;
    REAL y2 = -34;

    WCHAR* ver_s = L"v" CURR_VERSION_STR;
#if DRAW_TEXT_SHADOW
    SolidBrush b1(Color(0, 0, 0));
    g.DrawString(ver_s, -1, fVer, PointF(x2 - 2, y2 - 1), &b1);
#endif
    SolidBrush b2(Color(0xff, 0xff, 0xff));
    g.DrawString(ver_s, -1, fVer, PointF(x2, y2), &b2);
    g.ResetTransform();
}

static void DrawFrame2(Graphics& g, RectI r) {
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Font f(L"Impact", 40, FontStyleRegular);
    CalcLettersLayout(g, &f, r.dx);

    Gdiplus::Color bgCol;
    bgCol.SetFromCOLORREF(WIN_BG_COLOR);
    SolidBrush bgBrush(bgCol);
    Rect r2(r.ToGdipRect());
    r2.Inflate(1, 1);
    g.FillRectangle(&bgBrush, r2);

    Font f2(L"Impact", 16, FontStyleRegular);
    DrawSumatraLetters(g, &f, &f2, 18.f);

    if (gShowOptions)
        return;

    REAL msgY = (REAL)(r.dy / 2);
    if (gMsg)
        msgY += DrawMessage(g, gMsg, msgY, (REAL)r.dx, gMsgColor) + 5;
    if (gMsgError)
        DrawMessage(g, gMsgError, msgY, (REAL)r.dx, COLOR_MSG_FAILED);
}

static void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT*) {
    // TODO: cache bmp object?
    Graphics g(dc);
    ClientRect rc(hwnd);
    Bitmap bmp(rc.dx, rc.dy, &g);
    Graphics g2((Image*)&bmp);
    DrawFrame2(g2, rc);
    g.DrawImage(&bmp, 0, 0);
}

static void OnPaintFrame(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}

HBRUSH ghbrBackground = nullptr;

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    bool handled;
    switch (message) {
        case WM_CREATE:
#ifdef BUILD_UNINSTALLER
            if (!IsUninstallerNeeded()) {
                MessageBox(nullptr, _TR("SumatraPDF installation not found."), _TR("Uninstallation failed"),
                           MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
#endif
            OnCreateWindow(hwnd);
            break;

        case WM_CTLCOLORSTATIC: {
            if (ghbrBackground == nullptr) {
                ghbrBackground = CreateSolidBrush(RGB(0xff, 0xf2, 0));
            }
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)ghbrBackground;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintFrame(hwnd);
            break;

        case WM_COMMAND:
            handled = OnWmCommand(wParam);
            if (!handled)
                return DefWindowProc(hwnd, message, wParam, lParam);
            break;

        case WM_APP_INSTALLATION_FINISHED:
#ifndef BUILD_UNINSTALLER
            OnInstallationFinished();
            if (gHwndButtonRunSumatra)
                SetFocus(gHwndButtonRunSumatra);
#else
            OnUninstallationFinished();
#endif
            if (gHwndButtonExit)
                SetFocus(gHwndButtonExit);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex;

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcFrame);
    wcex.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_SUMATRAPDF));

    ATOM atom = RegisterClassEx(&wcex);
    CrashIf(!atom);
    return atom != 0;
}

static BOOL InstanceInit() {
    gFontDefault = CreateDefaultGuiFont();
    gUiDPIFactor = (float)DpiGet(HWND_DESKTOP)->dpiX / 96.f;
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    CreateMainWindow();
    if (!gHwndFrame)
        return FALSE;

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp() {
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    Timer t;
    for (;;) {
        const DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        if (timeout > 0) {
            res = MsgWaitForMultipleObjects(0, 0, TRUE, timeout, QS_ALLINPUT);
        }
        if (res == WAIT_TIMEOUT) {
            AnimStep();
            ftc.Step();
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        // check if there are processes that need to be closed but
        // not more frequently than once per ten seconds and
        // only before (un)installation starts.
        if (t.GetTimeInMs() > 10000 && gHwndButtonInstUninst && IsWindowEnabled(gHwndButtonInstUninst)) {
            CheckInstallUninstallPossible(true);
            t.Start();
        }
    }
}

static void ParseCommandLine(WCHAR* cmdLine) {
    WStrVec argList;
    ParseCmdLine(cmdLine, argList);

#define is_arg(param) str::EqI(arg + 1, TEXT(param))
#define is_arg_with_param(param) (is_arg(param) && i < argList.Count() - 1)

    // skip the first arg (exe path)
    for (size_t i = 1; i < argList.Count(); i++) {
        WCHAR* arg = argList.At(i);
        if ('-' != *arg && '/' != *arg)
            continue;

        if (is_arg("s"))
            gGlobalData.silent = true;
        else if (is_arg_with_param("d"))
            str::ReplacePtr(&gGlobalData.installDir, argList.At(++i));
#ifndef BUILD_UNINSTALLER
        else if (is_arg("register"))
            gGlobalData.registerAsDefault = true;
        else if (is_arg_with_param("opt")) {
            WCHAR* opts = argList.At(++i);
            str::ToLowerInPlace(opts);
            str::TransChars(opts, L" ;", L",,");
            WStrVec optlist;
            optlist.Split(opts, L",", true);
            if (optlist.Contains(L"pdffilter"))
                gGlobalData.installPdfFilter = true;
            if (optlist.Contains(L"pdfpreviewer"))
                gGlobalData.installPdfPreviewer = true;
            // uninstall the deprecated browser plugin if it's not
            // explicitly listed (only applies if the /opt flag is used)
            if (!optlist.Contains(L"plugin"))
                gGlobalData.keepBrowserPlugin = false;
        } else if (is_arg("x")) {
            gGlobalData.justExtractFiles = true;
            // silently extract files to the current directory (if /d isn't used)
            gGlobalData.silent = true;
            if (!gGlobalData.installDir)
                str::ReplacePtr(&gGlobalData.installDir, L".");
        } else if (is_arg("autoupdate")) {
            gGlobalData.autoUpdate = true;
        }
#endif
        else if (is_arg("h") || is_arg("help") || is_arg("?"))
            gGlobalData.showUsageAndQuit = true;
#ifdef ENABLE_CRASH_TESTING
        else if (is_arg("crash")) {
            // will induce crash when 'Install' button is pressed
            // for testing crash handling
            gForceCrash = true;
        }
#endif
    }
}

#define CRASH_DUMP_FILE_NAME L"suminstaller.dmp"

// no-op but must be defined for CrashHandler.cpp
void ShowCrashHandlerMessage() {}
void GetStressTestInfo(str::Str<char>* s) {
    UNUSED(s);
}

void GetProgramInfo(str::Str<char>& s) {
    s.AppendFmt("Ver: %s", CURR_VERSION_STRA);
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(" pre-release");
#endif
    if (IsProcess64()) {
        s.Append(" 64-bit");
    }
#ifdef DEBUG
    if (!str::Find(s.Get(), " (dbg)"))
        s.Append(" (dbg)");
#endif
    s.Append("\r\n");
#if defined(GIT_COMMIT_ID)
    const char* gitSha1 = QM(GIT_COMMIT_ID);
    s.AppendFmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/tree/%s)\r\n", gitSha1, gitSha1);
#endif
}

bool CrashHandlerCanUseNet() {
    return true;
}

#ifndef BUILD_UNINSTALLER
static void InstallInstallerCrashHandler() {
    // save symbols directly into %TEMP% (so that the installer doesn't
    // unnecessarily leave an empty directory behind if it doesn't have to)
    AutoFreeW tempDir(path::GetTempPath());
    if (!tempDir || !dir::Exists(tempDir))
        tempDir.Set(GetSpecialFolder(CSIDL_LOCAL_APPDATA, true));
    {
        if (!tempDir || !dir::Exists(tempDir))
            return;
    }
    AutoFreeW crashDumpPath(path::Join(tempDir, CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath, tempDir);
}
#endif

int APIENTRY WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /* lpCmdLine*/, int nCmdShow) {
    UNUSED(nCmdShow);
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    // Change current directory to prevent dll hijacking.
    // LoadLibrary first loads from current directory which could be
    // browser's download directory, which is an easy target
    // for attackers to put their own fake dlls).
    // For this to work we also have to /delayload all libraries otherwise
    // they will be loaded even before WinMain executes.
    auto currDir = GetSystem32Dir();
    SetCurrentDirectoryW(currDir);
    free(currDir);

    InitDynCalls();
    NoDllHijacking();

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

#ifndef BUILD_UNINSTALLER
    InstallInstallerCrashHandler();
#endif

    ParseCommandLine(GetCommandLine());
    if (gGlobalData.showUsageAndQuit) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }
    if (!gGlobalData.installDir)
        gGlobalData.installDir = GetInstallationDir();

#if defined(BUILD_UNINSTALLER) && !defined(TEST_UNINSTALLER)
    if (ExecuteUninstallerFromTempDir())
        return 0;
#endif

    if (gGlobalData.silent) {
#ifdef BUILD_UNINSTALLER
        UninstallerThread(nullptr);
#else
        // make sure not to uninstall the plugins during silent installation
        if (!gGlobalData.installPdfFilter)
            gGlobalData.installPdfFilter = IsPdfFilterInstalled();
        if (!gGlobalData.installPdfPreviewer)
            gGlobalData.installPdfPreviewer = IsPdfPreviewerInstalled();
        InstallerThread(nullptr);
#endif
        ret = gGlobalData.success ? 0 : 1;
        goto Exit;
    }

    if (!RegisterWinClass())
        goto Exit;

    if (!InstanceInit())
        goto Exit;

    ret = RunApp();

Exit:
    trans::Destroy();
    free(gGlobalData.installDir);
    free(gGlobalData.firstError);

    return ret;
}
