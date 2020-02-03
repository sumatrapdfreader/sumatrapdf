/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// code used in both Installer.cpp and Uninstaller.cpp

// TODO: not all those are needed
#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include <tlhelp32.h>
#include <io.h>
#include "utils/FileUtil.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/CmdLineParser.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/Log.h"
#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/RegistryPaths.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"

#include "CrashHandler.h"
#include "Translations.h"

#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "Resource.h"
#include "Version.h"
#include "Installer.h"

// define to 1 to enable shadow effect, to 0 to disable
#define DRAW_TEXT_SHADOW 1
#define DRAW_MSG_TEXT_SHADOW 0

using namespace Gdiplus;

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
ButtonCtrl* gButtonExit = nullptr;
ButtonCtrl* gButtonInstUninst = nullptr;
HFONT gFontDefault = nullptr;
bool gShowOptions = false;
bool gForceCrash = false;
WCHAR* gMsgError = nullptr;
int gBottomPartDy = 0;
int gButtonDy = 0;

Flags* gCli = nullptr;

const WCHAR* gDefaultMsg = nullptr; // Note: translation, not freeing

static AutoFreeWstr gMsg;
static Color gMsgColor;

static WStrVec gProcessesToClose;

InstUninstGlobals gInstUninstGlobals = {
    nullptr, /* WCHAR *firstError */
};

// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
// clang-format off
const WCHAR* gSupportedExtsSumatra[] = {
    L".pdf",  L".xps",  L".oxps", L".cbz",  L".cbr",  L".cb7", L".cbt",
    L".djvu", L".chm", L".mobi", L".epub", L".azw",  L".azw3", L"azw4",
    L".fb2", L".fb2z", L".prc",  L".tif", L".tiff", L".jp2",  L".png",
    L".jpg",  L".jpeg", L".tga", L".gif",  nullptr
};
const WCHAR* gSupportedExtsRaMicro[] = { L".pdf", nullptr };
// clang-format on

const WCHAR** getSupportedExts() {
    if (gIsRaMicroBuild) {
        return gSupportedExtsRaMicro;
    }
    return gSupportedExtsSumatra;
}

void NotifyFailed(const WCHAR* msg) {
    if (!gInstUninstGlobals.firstError) {
        gInstUninstGlobals.firstError = str::Dup(msg);
    }
    logf(L"NotifyFailed: %s\n", msg);
}

void SetMsg(const WCHAR* msg, Color color) {
    gMsg.SetCopy(msg);
    gMsgColor = color;
}

static HFONT CreateDefaultGuiFont() {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT f = CreateFontIndirectW(&ncm.lfMenuFont);
    return f;
}

const WCHAR* GetOwnPath() {
    static WCHAR exePath[MAX_PATH];
    exePath[0] = '\0';
    GetModuleFileName(nullptr, exePath, dimof(exePath));
    exePath[dimof(exePath) - 1] = '\0';
    return exePath;
}

static void InvalidateFrame() {
    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, FALSE);
}

void InitInstallerUninstaller() {
    gFontDefault = CreateDefaultGuiFont();
    trans::SetCurrentLangByCode(trans::DetectUserLang());
}

WCHAR* GetInstallDirNoFree() {
    return gCli->installDir;
}

WCHAR* GetBrowserPluginPath() {
    WCHAR* dir = GetInstallDirNoFree();
    return path::Join(dir, L"npPdfViewer.dll");
}

WCHAR* GetPdfFilterPath() {
    WCHAR* dir = GetInstallDirNoFree();
    return path::Join(dir, L"PdfFilter.dll");
}

WCHAR* GetPdfPreviewerPath() {
    WCHAR* dir = GetInstallDirNoFree();
    return path::Join(dir, L"PdfPreview.dll");
}

WCHAR* GetInstalledExePath() {
    WCHAR* dir = GetInstallDirNoFree();
    return path::Join(dir, getExeName());
}

WCHAR* GetUninstallerPath() {
    return GetInstalledExePath();
}

WCHAR* GetShortcutPath(int csidl) {
    AutoFreeWstr dir = GetSpecialFolder(csidl, false);
    if (!dir) {
        return nullptr;
    }
    const WCHAR* appName = getAppName();
    AutoFreeWstr lnkName = str::Join(appName, L".lnk");
    return path::Join(dir, lnkName);
}

WCHAR* GetInstalledBrowserPluginPath() {
    return ReadRegStr2(REG_PATH_PLUGIN, L"Path");
}

static bool IsUsingInstallation(DWORD procId) {
    DWORD myProcID = GetCurrentProcessId();
    if (procId == myProcID) {
        return false;
    }

    AutoCloseHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId));
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    AutoFreeWstr libmupdf(path::Join(GetInstallDirNoFree(), L"libmupdf.dll"));
    AutoFreeWstr browserPlugin(GetBrowserPluginPath());
    const WCHAR* libmupdfName = path::GetBaseNameNoFree(libmupdf);
    const WCHAR* browserPluginName = path::GetBaseNameNoFree(browserPlugin);

    MODULEENTRY32 mod = {0};
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        WCHAR* exePath = mod.szExePath;
        const WCHAR* exeName = path::GetBaseNameNoFree(exePath);
        if (path::IsSame(libmupdf, exePath)) {
            return true;
        }
        if (path::IsSame(browserPlugin, exePath)) {
            return true;
        }
        cont = Module32Next(snap, &mod);
    }

    return false;
}

// http://support.microsoft.com/default.aspx?scid=kb;en-us;207132
static bool RegisterOrUnregisterServerDLL(const WCHAR* dllPath, bool install, const WCHAR* args = nullptr) {
    if (FAILED(OleInitialize(nullptr))) {
        return false;
    }

    // make sure that the DLL can find any DLLs it depends on and
    // which reside in the same directory (in this case: libmupdf.dll)
    if (DynSetDllDirectoryW) {
        AutoFreeWstr dllDir = path::GetDir(dllPath);
        DynSetDllDirectoryW(dllDir);
    }

    defer {
        if (DynSetDllDirectoryW) {
            DynSetDllDirectoryW(L"");
        }
        OleUninitialize();
    };

    HMODULE lib = LoadLibraryW(dllPath);
    if (!lib) {
        return false;
    }
    defer {
        FreeLibrary(lib);
    };

    bool ok = false;
    typedef HRESULT(WINAPI * DllInstallProc)(BOOL, LPCWSTR);
    typedef HRESULT(WINAPI * DllRegUnregProc)(VOID);
    if (args) {
        DllInstallProc DllInstall = (DllInstallProc)GetProcAddress(lib, "DllInstall");
        if (DllInstall) {
            ok = SUCCEEDED(DllInstall(install, args));
        } else {
            args = nullptr;
        }
    }

    if (!args) {
        const char* func = install ? "DllRegisterServer" : "DllUnregisterServer";
        DllRegUnregProc DllRegUnreg = (DllRegUnregProc)GetProcAddress(lib, func);
        if (DllRegUnreg) {
            ok = SUCCEEDED(DllRegUnreg());
        }
    }
    return ok;
}

static bool RegisterServerDLL(const WCHAR* dllPath, const WCHAR* args = nullptr) {
    return RegisterOrUnregisterServerDLL(dllPath, true, args);
}

static bool UnRegisterServerDLL(const WCHAR* dllPath, const WCHAR* args = nullptr) {
    if (!file::Exists(dllPath)) {
        return true;
    }
    return RegisterOrUnregisterServerDLL(dllPath, false, args);
}

void UninstallBrowserPlugin() {
    AutoFreeWstr dllPath = GetBrowserPluginPath();
    if (!file::Exists(dllPath)) {
        // uninstall the detected plugin, even if it isn't in the target installation path
        dllPath.Set(GetInstalledBrowserPluginPath());
        if (!file::Exists(dllPath)) {
            return;
        }
    }
    if (!UnRegisterServerDLL(dllPath)) {
        NotifyFailed(_TR("Couldn't uninstall browser plugin"));
    }
}

void InstallPdfFilter() {
    AutoFreeWstr dllPath = GetPdfFilterPath();
    if (!RegisterServerDLL(dllPath)) {
        NotifyFailed(_TR("Couldn't install PDF search filter"));
    }
}

void UninstallPdfFilter() {
    AutoFreeWstr dllPath = GetPdfFilterPath();
    if (!UnRegisterServerDLL(dllPath)) {
        NotifyFailed(_TR("Couldn't uninstall PDF search filter"));
    }
}

void InstallPdfPreviewer() {
    AutoFreeWstr dllPath = GetPdfPreviewerPath();
    // TODO: RegisterServerDLL(dllPath, true, L"exts:pdf,...");
    if (!RegisterServerDLL(dllPath)) {
        NotifyFailed(_TR("Couldn't install PDF previewer"));
    }
}

void UninstallPdfPreviewer() {
    AutoFreeWstr dllPath = GetPdfPreviewerPath();
    // TODO: RegisterServerDLL(dllPath, false, L"exts:pdf,...");
    if (!UnRegisterServerDLL(dllPath)) {
        NotifyFailed(_TR("Couldn't uninstall PDF previewer"));
    }
}

#define TEN_SECONDS_IN_MS 10 * 1000

static bool IsProcWithName(DWORD processId, const WCHAR* modulePath) {
    AutoCloseHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hModSnapshot.IsValid()) {
        return false;
    }

    MODULEENTRY32W me32 = {0};
    me32.dwSize = sizeof(me32);
    BOOL ok = Module32FirstW(hModSnapshot, &me32);
    while (ok) {
        if (path::IsSame(modulePath, me32.szExePath)) {
            return true;
        }
        ok = Module32NextW(hModSnapshot, &me32);
    }
    return false;
}

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
static bool KillProcIdWithName(DWORD processId, const WCHAR* modulePath, bool waitUntilTerminated) {
    if (!IsProcWithName(processId, modulePath)) {
        return false;
    }

    BOOL inheritHandle = FALSE;
    // Note: do I need PROCESS_QUERY_INFORMATION and PROCESS_VM_READ?
    DWORD dwAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE;
    AutoCloseHandle hProcess(OpenProcess(dwAccess, inheritHandle, processId));
    if (!hProcess.IsValid()) {
        return false;
    }

    BOOL killed = TerminateProcess(hProcess, 0);
    if (!killed) {
        return false;
    }

    if (waitUntilTerminated) {
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);
    }

    return true;
}

// returns number of killed processes that have a module (exe or dll) with a given
// modulePath
// returns -1 on error, 0 if no matching processes
int KillProcess(const WCHAR* modulePath, bool waitUntilTerminated) {
    AutoCloseHandle hProcSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
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
        if (KillProcIdWithName(pe32.th32ProcessID, modulePath, waitUntilTerminated)) {
            killCount++;
        }
    } while (Process32Next(hProcSnapshot, &pe32));

    if (killCount > 0) {
        UpdateWindow(FindWindow(nullptr, L"Shell_TrayWnd"));
        UpdateWindow(GetDesktopWindow());
    }
    return killCount;
}

static bool SkipProcessByID(DWORD procID) {
    // TODO: don't know why this process shows up as using
    // our files
    if (procID == 0) {
        return true;
    }
    if (procID == GetCurrentProcessId()) {
        return true;
    }
    return false;
}

// return names of processes that are running part of the installation
// (i.e. have libmupdf.dll or npPdfViewer.dll loaded)
static void ProcessesUsingInstallation(WStrVec& names) {
    AutoCloseHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == snap) {
        return;
    }

    PROCESSENTRY32W proc = {0};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        if (!SkipProcessByID(procID)) {
            if (IsUsingInstallation(procID)) {
                // TODO: this kils ReadableProcName logic
                WCHAR* name = str::Format(L"%s (%d)", proc.szExeFile, (int)procID);
                names.Append(name);
            }
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }
}

static const WCHAR* nameList[] = {
    nullptr,          nullptr,         L"plugin-container.exe", L"Mozilla Firefox", L"chrome.exe",
    L"Google Chrome", L"prevhost.exe", L"Windows Explorer",     L"dllhost.exe",     L"Windows Explorer",
};

static const WCHAR* ReadableProcName(const WCHAR* procPath) {
    const WCHAR* exeName = getExeName();
    const WCHAR* appName = getAppName();
    nameList[0] = exeName;
    nameList[1] = appName;
    const WCHAR* procName = path::GetBaseNameNoFree(procPath);
    for (size_t i = 0; i < dimof(nameList); i += 2) {
        if (str::EqI(procName, nameList[i])) {
            return nameList[i + 1];
        }
    }
    return procName;
}

static void SetCloseProcessMsg() {
    AutoFreeWstr procNames(str::Dup(ReadableProcName(gProcessesToClose.at(0))));
    for (size_t i = 1; i < gProcessesToClose.size(); i++) {
        const WCHAR* name = ReadableProcName(gProcessesToClose.at(i));
        if (i < gProcessesToClose.size() - 1) {
            procNames.Set(str::Join(procNames, L", ", name));
        } else {
            procNames.Set(str::Join(procNames, L" and ", name));
        }
    }
    AutoFreeWstr s(str::Format(_TR("Please close %s to proceed!"), procNames.get()));
    SetMsg(s, COLOR_MSG_FAILED);
}

void SetDefaultMsg() {
    SetMsg(gDefaultMsg, COLOR_MSG_WELCOME);
}

bool CheckInstallUninstallPossible(bool silent) {
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

ButtonCtrl* CreateDefaultButtonCtrl(HWND hwndParent, const WCHAR* s) {
    auto* b = new ButtonCtrl(hwndParent);
    b->SetText(s);
    b->Create();

    RECT r;
    GetClientRect(hwndParent, &r);
    SIZE size = b->GetIdealSize();
    int x = RectDx(r) - size.cx - WINDOW_MARGIN;
    int y = RectDy(r) - size.cy - WINDOW_MARGIN;
    r.left = x;
    r.right = x + size.cx;
    r.top = y;
    r.bottom = y + size.cy;
    b->SetPos(&r);
    return b;
}

void CreateButtonExit(HWND hwndParent) {
    gButtonExit = CreateDefaultButtonCtrl(hwndParent, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
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
    AutoFreeWstr s = str::Dup(msg);

    Font f(L"Impact", 16, FontStyleRegular);
    RectF maxbox(0, y, dx, 0);
    RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
    if (trans::IsCurrLangRtl()) {
        sft.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
    }
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
        if (s[0] == ' ') {
            return;
        }

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
    Gdiplus::Rect r2(r.ToGdipRect());
    r2.Inflate(1, 1);
    g.FillRectangle(&bgBrush, r2);

    Font f2(L"Impact", 16, FontStyleRegular);
    DrawSumatraLetters(g, &f, &f2, 18.f);

    if (gShowOptions) {
        return;
    }

    REAL msgY = (REAL)(r.dy / 2);
    if (gMsg) {
        msgY += DrawMessage(g, gMsg, msgY, (REAL)r.dx, gMsgColor) + 5;
    }
    if (gMsgError) {
        DrawMessage(g, gMsgError, msgY, (REAL)r.dx, COLOR_MSG_FAILED);
    }
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

void OnPaintFrame(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}
