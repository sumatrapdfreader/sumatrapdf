/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
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
#include "Installer.h"

#include "CmdLineParser.h"
#include "CrashHandler.h"
#include "FrameTimeoutCalculator.h"
#include "ParseCommandLine.h"

#include "DebugLog.h"

// TODO: can't build these separately without breaking TEST_UNINSTALLER
#ifdef BUILD_UNINSTALLER
#include "Uninstall.cpp"
#else
#include "Install.cpp"
#endif

using namespace Gdiplus;

// define to 1 to enable shadow effect, to 0 to disable
#define DRAW_TEXT_SHADOW 1
#define DRAW_MSG_TEXT_SHADOW 0

HINSTANCE       ghinst;
HWND            gHwndFrame = NULL;
HWND            gHwndButtonExit = NULL;
HWND            gHwndButtonInstUninst = NULL;
HFONT           gFontDefault;
bool            gShowOptions = false;
bool            gForceCrash = false;
TCHAR *         gMsgError = NULL;

static StrVec           gProcessesToClose;
static float            gUiDPIFactor = 1.0f;

static ScopedMem<TCHAR> gMsg;
static Color            gMsgColor;

Color gCol1(196, 64, 50); Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35); Color gCol2Shadow(155, 77, 31);
Color gCol3(93,  160, 40); Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190); Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207); Color gCol5Shadow(66, 71, 118);

Color            COLOR_MSG_WELCOME(gCol5);
Color            COLOR_MSG_OK(gCol5);
Color            COLOR_MSG_INSTALLATION(gCol5);
Color            COLOR_MSG_FAILED(gCol1);

// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
TCHAR *gSupportedExts[] = {
    _T(".pdf"), _T(".xps"), _T(".cbz"), _T(".cbr"), _T(".djvu"),
    _T(".chm"), _T(".mobi"), _T(".epub"), NULL
};

// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
PayloadInfo gPayloadData[] = {
    { "libmupdf.dll",           true    },
    { "SumatraPDF.exe",         true    },
    { "sumatrapdfprefs.dat",    false   },
    { "DroidSansFallback.ttf",  true    },
    { "npPdfViewer.dll",        true    },
    { "PdfFilter.dll",          true    },
    { "PdfPreview.dll",         true    },
    { "uninstall.exe",          true    },
    { NULL,                     false   },
};

GlobalData gGlobalData = {
    false, /* bool silent */
    false, /* bool showUsageAndQuit */
    NULL,  /* TCHAR *installDir */
#ifndef BUILD_UNINSTALLER
    false, /* bool registerAsDefault */
    false, /* bool installBrowserPlugin */
    false, /* bool installPdfFilter */
    false, /* bool installPdfPreviewer */
#endif

    NULL,  /* TCHAR *firstError */
    NULL,  /* HANDLE hThread */
    false, /* bool success */
};

void NotifyFailed(TCHAR *msg)
{
    if (!gGlobalData.firstError)
        gGlobalData.firstError = str::Dup(msg);
    plogf(_T("%s"), msg);
}

void SetMsg(TCHAR *msg, Color color)
{
    gMsg.Set(str::Dup(msg));
    gMsgColor = color;
}

#define TEN_SECONDS_IN_MS 10*1000

// Kill a process with given <processId> if it's loaded from <processPath>.
// If <waitUntilTerminated> is TRUE, will wait until process is fully killed.
// Returns TRUE if killed a process
static BOOL KillProcIdWithName(DWORD processId, TCHAR *processPath, BOOL waitUntilTerminated)
{
    ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId));
    ScopedHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hProcess || INVALID_HANDLE_VALUE == hModSnapshot)
        return FALSE;

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(me32);
    if (!Module32First(hModSnapshot, &me32))
        return FALSE;
    if (!path::IsSame(processPath, me32.szExePath))
        return FALSE;

    BOOL killed = TerminateProcess(hProcess, 0);
    if (!killed)
        return FALSE;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    UpdateWindow(FindWindow(NULL, _T("Shell_TrayWnd")));
    UpdateWindow(GetDesktopWindow());

    return TRUE;
}

#define MAX_PROCESSES 1024

int KillProcess(TCHAR *processPath, BOOL waitUntilTerminated)
{
    ScopedHandle hProcSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == hProcSnapshot)
        return -1;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32First(hProcSnapshot, &pe32))
        return -1;

    int killCount = 0;
    do {
        if (KillProcIdWithName(pe32.th32ProcessID, processPath, waitUntilTerminated))
            killCount++;
    } while (Process32Next(hProcSnapshot, &pe32));

    return killCount;
}

TCHAR *GetOwnPath()
{
    static TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, dimof(exePath));
    return exePath;
}

static TCHAR *GetInstallationDir()
{
    ScopedMem<TCHAR> dir(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, INSTALL_LOCATION));
    if (!dir) dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, INSTALL_LOCATION));
    // fall back to the legacy key if the official one isn't present yet
    if (!dir) dir.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_SOFTWARE, INSTALL_DIR));
    if (!dir) dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_SOFTWARE, INSTALL_DIR));
    if (dir) {
        if (str::EndsWithI(dir, _T(".exe"))) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

#ifndef BUILD_UNINSTALLER
    // fall back to %ProgramFiles%
    TCHAR buf[MAX_PATH] = {0};
    BOOL ok = SHGetSpecialFolderPath(NULL, buf, CSIDL_PROGRAM_FILES, FALSE);
    if (ok)
        return path::Join(buf, TAPP);
    // fall back to C:\ as a last resort
    return str::Dup(_T("C:\\" TAPP));
#else
    // fall back to the uninstaller's path
    return path::GetDir(GetOwnPath());
#endif
}

TCHAR *GetUninstallerPath()
{
    return path::Join(gGlobalData.installDir, _T("uninstall.exe"));
}

TCHAR *GetInstalledExePath()
{
    return path::Join(gGlobalData.installDir, EXENAME);
}

static TCHAR *GetBrowserPluginPath()
{
    return path::Join(gGlobalData.installDir, _T("npPdfViewer.dll"));
}

static TCHAR *GetPdfFilterPath()
{
    return path::Join(gGlobalData.installDir, _T("PdfFilter.dll"));
}

static TCHAR *GetPdfPreviewerPath()
{
    return path::Join(gGlobalData.installDir, _T("PdfPreview.dll"));
}

static TCHAR *GetStartMenuProgramsPath(bool allUsers)
{
    static TCHAR dir[MAX_PATH];
    // CSIDL_COMMON_PROGRAMS => installing for all users
    BOOL ok = SHGetSpecialFolderPath(NULL, dir, allUsers ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS, FALSE);
    if (!ok)
        return NULL;
    return dir;
}

TCHAR *GetShortcutPath(bool allUsers)
{
    TCHAR *path = GetStartMenuProgramsPath(allUsers);
    if (!path)
        return NULL;
    return path::Join(path, TAPP _T(".lnk"));
}

/* if the app is running, we have to kill it so that we can over-write the executable */
void KillSumatra()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    KillProcess(exePath, TRUE);
}

static HFONT CreateDefaultGuiFont()
{
    HDC hdc = GetDC(NULL);
    HFONT font = GetSimpleFont(hdc, _T("MS Shell Dlg"), 14);
    ReleaseDC(NULL, hdc);
    return font;
}

int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

void InvalidateFrame()
{
    ClientRect rc(gHwndFrame);
    if (gShowOptions)
        rc.dy = TITLE_PART_DY;
    else
        rc.dy -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);
}

bool CreateProcessHelper(const TCHAR *exe, const TCHAR *args)
{
    ScopedMem<TCHAR> cmd(str::Format(_T("\"%s\" %s"), exe, args ? args : _T("")));
    ScopedHandle process(LaunchProcess(cmd));
    return process != NULL;
}

// cf. http://support.microsoft.com/default.aspx?scid=kb;en-us;207132
bool RegisterServerDLL(TCHAR *dllPath, bool unregister=false)
{
    if (FAILED(OleInitialize(NULL)))
        return false;

    // make sure that the DLL can find any DLLs it depends on and
    // which reside in the same directory (in this case: libmupdf.dll)
    typedef BOOL (WINAPI *SetDllDirectoryProc)(LPCTSTR);
#ifdef UNICODE
    SetDllDirectoryProc _SetDllDirectory = (SetDllDirectoryProc)LoadDllFunc(_T("Kernel32.dll"), "SetDllDirectoryW");
#else
    SetDllDirectoryProc _SetDllDirectory = (SetDllDirectoryProc)LoadDllFunc(_T("Kernel32.dll"), "SetDllDirectoryA");
#endif
    if (_SetDllDirectory) {
        ScopedMem<TCHAR> dllDir(path::GetDir(dllPath));
        _SetDllDirectory(dllDir);
    }

    bool ok = false;
    HMODULE lib = LoadLibrary(dllPath);
    if (lib) {
        typedef HRESULT (WINAPI *DllInitProc)(VOID);
        const char *func = unregister ? "DllUnregisterServer" : "DllRegisterServer";
        DllInitProc CallDLL = (DllInitProc)GetProcAddress(lib, func);
        if (CallDLL)
            ok = SUCCEEDED(CallDLL());
        FreeLibrary(lib);
    }

    if (_SetDllDirectory)
        _SetDllDirectory(_T(""));

    OleUninitialize();

    return ok;
}

static bool IsUsingInstallation(DWORD procId)
{
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId));
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    ScopedMem<TCHAR> libmupdf(path::Join(gGlobalData.installDir, _T("libmupdf.dll")));
    ScopedMem<TCHAR> browserPlugin(GetBrowserPluginPath());

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        if (path::IsSame(libmupdf, mod.szExePath) ||
            path::IsSame(browserPlugin, mod.szExePath)) {
            return true;
        }
        cont = Module32Next(snap, &mod);
    }

    return false;
}

// return names of processes that are running part of the installation
// (i.e. have libmupdf.dll or npPdfViewer.dll loaded)
static void ProcessesUsingInstallation(StrVec& names)
{
    FreeVecMembers(names);
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == snap)
        return;

    PROCESSENTRY32 proc;
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

static void SetDefaultMsg()
{
#ifdef BUILD_UNINSTALLER
    SetMsg(_T("Are you sure that you want to uninstall ") TAPP _T("?"), COLOR_MSG_WELCOME);
#else
    SetMsg(_T("Thank you for choosing ") TAPP _T("!"), COLOR_MSG_WELCOME);
#endif
}

static const TCHAR *ReadableProcName(const TCHAR *procPath)
{
    const TCHAR *nameList[] = {
        EXENAME, TAPP,
        _T("plugin-container.exe"), _T("Mozilla Firefox"),
        _T("chrome.exe"), _T("Google Chrome"),
        _T("prevhost.exe"), _T("Windows Explorer"),
        _T("dllhost.exe"), _T("Windows Explorer"),
    };
    const TCHAR *procName = path::GetBaseName(procPath);
    for (size_t i = 0; i < dimof(nameList); i += 2) {
        if (str::EqI(procName, nameList[i]))
            return nameList[i + 1];
    }
    return procName;
}

static void SetCloseProcessMsg()
{
    ScopedMem<TCHAR> procNames(str::Dup(ReadableProcName(gProcessesToClose.At(0))));
    for (size_t i = 1; i < gProcessesToClose.Count(); i++) {
        const TCHAR *name = ReadableProcName(gProcessesToClose.At(i));
        if (i < gProcessesToClose.Count() - 1)
            procNames.Set(str::Join(procNames, _T(", "), name));
        else
            procNames.Set(str::Join(procNames, _T(" and "), name));
    }
    ScopedMem<TCHAR> s(str::Format(_T("Please close %s to proceed!"), procNames));
    SetMsg(s, COLOR_MSG_FAILED);
}

bool CheckInstallUninstallPossible(bool silent)
{
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

void InstallBrowserPlugin()
{
    ScopedMem<TCHAR> dllPath(GetBrowserPluginPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install browser plugin"));
}

void UninstallBrowserPlugin()
{
    ScopedMem<TCHAR> dllPath(GetBrowserPluginPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall browser plugin"));
}

void InstallPdfFilter()
{
    ScopedMem<TCHAR> dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install PDF search filter"));
}

void UninstallPdfFilter()
{
    ScopedMem<TCHAR> dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall PDF search filter"));
}

void InstallPdfPreviewer()
{
    ScopedMem<TCHAR> dllPath(GetPdfPreviewerPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install PDF previewer"));
}

void UninstallPdfPreviewer()
{
    ScopedMem<TCHAR> dllPath(GetPdfPreviewerPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall PDF previewer"));
}

HWND CreateDefaultButton(HWND hwndParent, const TCHAR *label, int width, int id)
{
    RectI rc(0, 0, dpiAdjust(width), PUSH_BUTTON_DY);

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    ClientRect r(hwndParent);
    rc.x = r.dx - rc.dx - WINDOW_MARGIN;
    rc.y = r.dy - rc.dy - WINDOW_MARGIN;
    HWND button = CreateWindow(WC_BUTTON, label,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        rc.x, rc.y, rc.dx, rc.dy, hwndParent,
                        (HMENU)id, ghinst, NULL);
    SetWindowFont(button, gFontDefault, TRUE);

    return button;
}

void CreateButtonExit(HWND hwndParent)
{
    gHwndButtonExit = CreateDefaultButton(hwndParent, _T("Close"), 80, ID_BUTTON_EXIT);
}

void OnButtonExit()
{
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

LetterInfo gLetters[] = {
    { 'S', gCol1, gCol1Shadow, -3.f,     0, 0, 0 },
    { 'U', gCol2, gCol2Shadow,  0.f,     0, 0, 0 },
    { 'M', gCol3, gCol3Shadow,  2.f,  -2.f, 0, 0 },
    { 'A', gCol4, gCol4Shadow,  0.f, -2.4f, 0, 0 },
    { 'T', gCol5, gCol5Shadow,  0.f,     0, 0, 0 },
    { 'R', gCol5, gCol5Shadow, 2.3f, -1.4f, 0, 0 },
    { 'A', gCol4, gCol4Shadow,  0.f,     0, 0, 0 },
    { 'P', gCol3, gCol3Shadow,  0.f, -2.3f, 0, 0 },
    { 'D', gCol2, gCol2Shadow,  0.f,   3.f, 0, 0 },
    { 'F', gCol1, gCol1Shadow,  0.f,     0, 0, 0 }
};

#define SUMATRA_LETTERS_COUNT (dimof(gLetters))

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

static void SetLettersSumatraUpTo(int n)
{
    char *s = "SUMATRAPDF";
    for (int i = 0; i < dimof(gLetters); i++) {
        if (i < n) {
            gLetters[i].c = s[i];
        } else {
            gLetters[i].c = ' ';
        }
    }
}

static void SetLettersSumatra()
{
    SetLettersSumatraUpTo(SUMATRA_LETTERS_COUNT);
}

// an animation that reveals letters one by one

// how long the animation lasts, in seconds
#define REVEALING_ANIM_DUR double(2)

static FrameTimeoutCalculator *gRevealingLettersAnim = NULL;

int gRevealingLettersAnimLettersToShow;

static void RevealingLettersAnimStart()
{
    int framesPerSec = (int)(double(SUMATRA_LETTERS_COUNT) / REVEALING_ANIM_DUR);
    gRevealingLettersAnim = new FrameTimeoutCalculator(framesPerSec);
    gRevealingLettersAnimLettersToShow = 0;
    SetLettersSumatraUpTo(0);
}

static void RevealingLettersAnimStop()
{
    delete gRevealingLettersAnim;
    gRevealingLettersAnim = NULL;
    SetLettersSumatra();
    InvalidateFrame();
}

static void RevealingLettersAnim()
{
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

static void AnimStep()
{
    if (gRevealingLettersAnim)
        RevealingLettersAnim();
}

static void CalcLettersLayout(Graphics& g, Font *f, int dx)
{
    static BOOL didLayout = FALSE;
    if (didLayout) return;

    LetterInfo *li;
    StringFormat sfmt;
    const REAL letterSpacing = -12.f;
    REAL totalDx = -letterSpacing; // counter last iteration of the loop
    WCHAR s[2] = { 0 };
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

static REAL DrawMessage(Graphics &g, TCHAR *msg, REAL y, REAL dx, Color color)
{
    ScopedMem<WCHAR> s(str::conv::ToWStr(msg));

    Font f(L"Impact", 16, FontStyleRegular);
    RectF maxbox(0, y, dx, 0);
    RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
#if DRAW_MSG_TEXT_SHADOW
    {
        bbox.X--; bbox.Y++;
        SolidBrush b(Color(0xff, 0xff, 0xff));
        g.DrawString(s, -1, &f, bbox, &sft, &b);
        bbox.X++; bbox.Y--;
    }
#endif
    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
}

static void DrawSumatraLetters(Graphics &g, Font *f, Font *fVer, REAL y)
{
    LetterInfo *li;
    WCHAR s[2] = { 0 };
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
    REAL x = gLetters[dimof(gLetters)-1].x;
    g.TranslateTransform(x, y);
    g.RotateTransform(45.f);
    REAL x2 = 15; REAL y2 = -34;

    ScopedMem<WCHAR> ver_s(str::conv::ToWStr(_T("v") CURR_VERSION_STR));
#if DRAW_TEXT_SHADOW
    SolidBrush b1(Color(0, 0, 0));
    g.DrawString(ver_s, -1, fVer, PointF(x2 - 2, y2 - 1), &b1);
#endif
    SolidBrush b2(Color(0xff, 0xff, 0xff));
    g.DrawString(ver_s, -1, fVer, PointF(x2, y2), &b2);
    g.ResetTransform();
}

static void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Font f(L"Impact", 40, FontStyleRegular);
    CalcLettersLayout(g, &f, r.dx);

    SolidBrush bgBrush(Color(0xff, 0xf2, 0));
    Rect r2(r.y - 1, r.x - 1, r.dx + 1, r.dy + 1);
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

static void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (gShowOptions)
        rc.top = TITLE_PART_DY;
    else
        rc.top = rc.bottom - BOTTOM_PART_DY;
    RECT rcTmp;
    if (IntersectRect(&rcTmp, &rc, &ps->rcPaint)) {
        HBRUSH brushNativeBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(dc, &rc, brushNativeBg);
        DeleteObject(brushNativeBg);
    }

    // TODO: cache bmp object?
    Graphics g(dc);
    ClientRect rc2(hwnd);
    if (gShowOptions)
        rc2.dy = TITLE_PART_DY;
    else
        rc2.dy -= BOTTOM_PART_DY;
    Bitmap bmp(rc2.dx, rc2.dy, &g);
    Graphics g2((Image*)&bmp);
    DrawFrame2(g2, rc2);
    g.DrawImage(&bmp, 0, 0);
}

static void OnPaintFrame(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool handled;
    switch (message)
    {
        case WM_CREATE:
#ifndef BUILD_UNINSTALLER
            if (!IsValidInstaller()) {
                MessageBox(NULL, _T("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"), _T("Installation failed"),  MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
#else
            if (!IsUninstallerNeeded()) {
                MessageBox(NULL, _T("No installation has been found. Please install ") TAPP _T(" first before uninstalling it..."), _T("Uninstallation failed"),  MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
#endif
            OnCreateWindow(hwnd);
            break;

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

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;

    FillWndClassEx(wcex, hInstance);
    wcex.lpszClassName  = INSTALLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpfnWndProc    = WndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;
    gFontDefault = CreateDefaultGuiFont();
    win::GetHwndDpi(NULL, &gUiDPIFactor);

    CreateMainWindow();
    if (!gHwndFrame)
        return FALSE;

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    Timer t(true);
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

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
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
        if (t.GetTimeInMs() > 10000 &&
            gHwndButtonInstUninst && IsWindowEnabled(gHwndButtonInstUninst)) {
            CheckInstallUninstallPossible(true);
            t.Start();
        }
    }
}

static void ParseCommandLine(TCHAR *cmdLine)
{
    CmdLineParser argList(cmdLine);

#define is_arg(param) str::EqI(arg + 1, _T(param))
#define is_arg_with_param(param) (is_arg(param) && i < argList.Count() - 1)

    // skip the first arg (exe path)
    for (size_t i = 1; i < argList.Count(); i++) {
        TCHAR *arg = argList.At(i);
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
            TCHAR *opts = argList.At(++i);
            str::ToLower(opts);
            str::TransChars(opts, _T(" ;"), _T(",,"));
            StrVec optlist;
            optlist.Split(opts, _T(","), true);
            if (optlist.Find(_T("plugin")) != -1)
                gGlobalData.installBrowserPlugin = true;
            if (optlist.Find(_T("pdffilter")) != -1)
                gGlobalData.installPdfFilter = true;
            if (optlist.Find(_T("pdfpreviewer")) != -1)
                gGlobalData.installPdfPreviewer = true;
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

#define CRASH_DUMP_FILE_NAME         _T("suminstaller.dmp")

// no-op but must be defined for CrashHandler.cpp
void CrashHandlerMessage() { }
void GetStressTestInfo(str::Str<char>* s) { }

void GetProgramInfo(str::Str<char>& s)
{
    // TODO: implement me
}

bool CrashHandlerCanUseNet()
{
    return true;
}

static void InstallInstallerCrashHandler()
{
    TCHAR tempDir[MAX_PATH] = { 0 };
    DWORD res = GetTempPath(dimof(tempDir), tempDir);
    if ((0 == res) || !dir::Exists(tempDir)) {
        BOOL ok = SHGetSpecialFolderPath(NULL, tempDir, CSIDL_LOCAL_APPDATA, TRUE);
        if (!ok)
            return;
    }

    // save symbols directly into %TEMP% (so that the installer doesn't
    // unnecessarily leave an empty directory behind if it doesn't have to)
    ScopedMem<TCHAR> crashDumpPath(path::Join(tempDir, CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath, tempDir);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

#if !defined(BUILD_UNINSTALLER)
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
        UninstallerThread(NULL);
#else
        // make sure not to uninstall the plugins during silent installation
        if (!gGlobalData.installBrowserPlugin)
            gGlobalData.installBrowserPlugin = IsBrowserPluginInstalled();
        if (!gGlobalData.installPdfFilter)
            gGlobalData.installPdfFilter = IsPdfFilterInstalled();
        if (!gGlobalData.installPdfPreviewer)
            gGlobalData.installPdfPreviewer = IsPdfPreviewerInstalled();
        InstallerThread(NULL);
#endif
        ret = gGlobalData.success ? 0 : 1;
        goto Exit;
    }

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

Exit:
    free(gGlobalData.installDir);
    free(gGlobalData.firstError);

    return ret;
}
