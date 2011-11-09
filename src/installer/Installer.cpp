/* Copyright 2010-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

// define for testing the uninstaller
// #define TEST_UNINSTALLER
#if defined(TEST_UNINSTALLER) && !defined(BUILD_UNINSTALLER)
#define BUILD_UNINSTALLER
#endif

#include <windows.h>
#include <GdiPlus.h>
#include <shlobj.h>
#include <Tlhelp32.h>
#include <Shlwapi.h>
#include <objidl.h>
#include <io.h>

#include "Resource.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Version.h"
#include "Vec.h"
#include "CmdLineParser.h"
#include "Transactions.h"
#include "Scopes.h"

#ifndef BUILD_UNINSTALLER
#include <WinSafer.h>

#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"
#endif

using namespace Gdiplus;

// define to 1 to enable shadow effect, to 0 to disable
#define DRAW_TEXT_SHADOW 1
#define DRAW_MSG_TEXT_SHADOW 0

#define INSTALLER_FRAME_CLASS_NAME    _T("SUMATRA_PDF_INSTALLER_FRAME")

#define INSTALLER_WIN_DX    420
#define INSTALLER_WIN_DY    340

#define ID_BUTTON_EXIT                11
#ifndef BUILD_UNINSTALLER
#define ID_CHECKBOX_MAKE_DEFAULT      14
#define ID_CHECKBOX_BROWSER_PLUGIN    15
#define ID_BUTTON_START_SUMATRA       16
#define ID_BUTTON_OPTIONS             17
#define ID_BUTTON_BROWSE              18
#define ID_CHECKBOX_PDF_FILTER        19
#define ID_CHECKBOX_PDF_PREVIEWER     20
#else
#define UNINSTALLER_WIN_DX  INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY  INSTALLER_WIN_DY
#endif

#define WM_APP_INSTALLATION_FINISHED        (WM_APP + 1)

#define PUSH_BUTTON_DY  dpiAdjust(22)
#define WINDOW_MARGIN   8
// The window is divided in three parts:
// * top part, where we display nice graphics
// * middle part, where we either display messages or advanced options
// * bottom part, with install/uninstall button
// This is the height of the top part
#define TITLE_PART_DY  dpiAdjust(110)
// This is the height of the lower part
#define BOTTOM_PART_DY (PUSH_BUTTON_DY + 2 * WINDOW_MARGIN)

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static HWND             gHwndButtonExit = NULL;
static HWND             gHwndButtonInstUninst = NULL;
#ifndef BUILD_UNINSTALLER
static HWND             gHwndButtonOptions = NULL;
static HWND             gHwndButtonRunSumatra = NULL;
static HWND             gHwndStaticInstDir = NULL;
static HWND             gHwndTextboxInstDir = NULL;
static HWND             gHwndButtonBrowseDir = NULL;
static HWND             gHwndCheckboxRegisterDefault = NULL;
static HWND             gHwndCheckboxRegisterBrowserPlugin = NULL;
static HWND             gHwndCheckboxRegisterPdfFilter = NULL;
static HWND             gHwndCheckboxRegisterPdfPreviewer = NULL;
static HWND             gHwndProgressBar = NULL;
#endif
static HFONT            gFontDefault;
static bool             gShowOptions = false;
static StrVec           gProcessesToClose;

static float            gUiDPIFactor = 1.0f;

static ScopedMem<TCHAR> gMsg;
static TCHAR *          gMsgError = NULL;
static Color            gMsgColor;

Color gCol1(196, 64, 50); Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35); Color gCol2Shadow(155, 77, 31);
Color gCol3(93,  160, 40); Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190); Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207); Color gCol5Shadow(66, 71, 118);

static Color            COLOR_MSG_WELCOME(gCol5);
static Color            COLOR_MSG_OK(gCol5);
static Color            COLOR_MSG_INSTALLATION(gCol5);
static Color            COLOR_MSG_FAILED(gCol1);

#define TAPP                _T("SumatraPDF")
#define EXENAME             TAPP _T(".exe")

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
#define REG_PATH_UNINST     _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") TAPP
// Legacy key, only read during an update and removed at uninstallation
#define REG_PATH_SOFTWARE   _T("Software\\") TAPP

#define REG_CLASSES_APP     _T("Software\\Classes\\") TAPP
#define REG_CLASSES_PDF     _T("Software\\Classes\\.pdf")
#define REG_CLASSES_APPS    _T("Software\\Classes\\Applications\\") EXENAME

#define REG_EXPLORER_PDF_EXT  _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf")
#define PROG_ID               _T("ProgId")
#define APPLICATION           _T("Application")

#define REG_PATH_PLUGIN     _T("Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin")
#define PLUGIN_PATH         _T("Path")

// Keys we'll set in REG_PATH_UNINST path

// REG_SZ, a path to installed executable (or "$path,0" to force the first icon)
#define DISPLAY_ICON _T("DisplayIcon")
// REG_SZ, e.g "SumatraPDF" (TAPP)
#define DISPLAY_NAME _T("DisplayName")
// REG_SZ, e.g. "1.2" (CURR_VERSION_STR)
#define DISPLAY_VERSION _T("DisplayVersion")
// REG_DWORD, get size of installed directory after copying files
#define ESTIMATED_SIZE _T("EstimatedSize")
// REG_SZ, the current date as YYYYMMDD
#define INSTALL_DATE _T("InstallDate")
// REG_DWORD, set to 1
#define NO_MODIFY _T("NoModify")
// REG_DWORD, set to 1
#define NO_REPAIR _T("NoRepair")
// REG_SZ, e.g. "Krzysztof Kowalczyk" (PUBLISHER_STR)
#define PUBLISHER _T("Publisher")
// REG_SZ, path to uninstaller exe
#define UNINSTALL_STRING _T("UninstallString")
// REG_SZ, e.g. "http://blog.kowalczyk.info/software/sumatrapdf/"
#define URL_INFO_ABOUT _T("URLInfoAbout")
// REG_SZ, e.g. "http://blog.kowalczyk.info/software/sumatrapdf/news.html"
#define URL_UPDATE_INFO _T("URLUpdateInfo")
// REG_SZ, same as INSTALL_DIR below
#define INSTALL_LOCATION _T("InstallLocation")

// Installation directory (set in HKLM REG_PATH_SOFTWARE
// for compatibility with the old NSIS installer)
#define INSTALL_DIR _T("Install_Dir")

// list of supported file extensions for which SumatraPDF.exe will
// be registered as a candidate for the Open With dialog's suggestions
static TCHAR *gSupportedExts[] = {
    _T(".pdf"), _T(".xps"), _T(".cbz"), _T(".cbr"), _T(".djvu"), _T(".chm")
};

// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
struct {
    char *filepath;
    bool install;
} gPayloadData[] = {
    // TODO: extract libmupdf.dll first, so that the installation fails as soon
    //       as possible, if SumatraPDF.exe or any DLL is currently in use
    { "libmupdf.dll",           true    },
    { "SumatraPDF.exe",         true    },
    { "sumatrapdfprefs.dat",    false   },
    { "DroidSansFallback.ttf",  true    },
    { "npPdfViewer.dll",        true    },
    { "PdfFilter.dll",          true    },
    { "PdfPreview.dll",         true    },
    { "uninstall.exe",          true    },
};

struct {
    bool silent;
    bool showUsageAndQuit;
    TCHAR *installDir;
#ifndef BUILD_UNINSTALLER
    bool registerAsDefault;
    bool installBrowserPlugin;
    bool installPdfFilter;
    bool installPdfPreviewer;
#endif

    TCHAR *firstError;
    HANDLE hThread;
    bool success;
} gGlobalData = {
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

static void NotifyFailed(TCHAR *msg)
{
    if (!gGlobalData.firstError)
        gGlobalData.firstError = str::Dup(msg);
    // MessageBox(gHwndFrame, msg, _T("Installation failed"),  MB_ICONEXCLAMATION | MB_OK);
}

static void SetMsg(TCHAR *msg, Color color)
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

static int KillProcess(TCHAR *processPath, BOOL waitUntilTerminated)
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

static TCHAR *GetOwnPath()
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
    if (!ok)
        return NULL;
    return path::Join(buf, TAPP);
#else
    // fall back to the uninstaller's path
    return path::GetDir(GetOwnPath());
#endif
}

static TCHAR *GetUninstallerPath()
{
    return path::Join(gGlobalData.installDir, _T("uninstall.exe"));
}

static TCHAR *GetInstalledExePath()
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

static TCHAR *GetShortcutPath(bool allUsers)
{
    return path::Join(GetStartMenuProgramsPath(allUsers), TAPP _T(".lnk"));
}

/* if the app is running, we have to kill it so that we can over-write the executable */
static void KillSumatra()
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

inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static void InvalidateFrame()
{
    ClientRect rc(gHwndFrame);
    if (gShowOptions)
        rc.dy = TITLE_PART_DY;
    else
        rc.dy -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);
}

static bool CreateProcessHelper(const TCHAR *exe, const TCHAR *args=NULL)
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
        DllInitProc CallDLL = (DllInitProc)GetProcAddress(lib, unregister ? "DllUnregisterServer" : "DllRegisterServer");
        if (CallDLL)
            ok = SUCCEEDED(CallDLL());
        FreeLibrary(lib);
    }

    if (_SetDllDirectory)
        _SetDllDirectory(_T(""));

    OleUninitialize();

    return ok;
}

#if !defined(BUILD_UNINSTALLER) || defined(TEST_UNINSTALLER)
extern "C" {
// needed because we compile bzip2 with #define BZ_NO_STDIO
void bz_internal_error(int errcode)
{
    NotifyFailed(_T("fatal error: bz_internal_error()"));
}
}
#endif

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
    for (size_t i = 0; i < dimof(nameList); i += 2)
        if (str::EqI(procName, nameList[i]))
            return nameList[i + 1];
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

static bool CheckInstallUninstallPossible(bool silent=false)
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

#ifndef BUILD_UNINSTALLER
static int GetInstallationStepCount()
{
    /* Installation steps
     * - Create directory
     * - One per file to be copied (count extracted from gPayloadData)
     * - Optional registration (default viewer, browser plugin),
     *   Shortcut and Registry keys
     * 
     * Most time is taken by file extraction/copying, so we just add
     * one step before - so that we start with some initial progress
     * - and one step afterwards.
     */
    int count = 2;
    for (int i = 0; i < dimof(gPayloadData); i++)
        if (gPayloadData[i].install)
            count++;
    return count;
}

static inline void ProgressStep()
{
    if (gHwndProgressBar)
        PostMessage(gHwndProgressBar, PBM_STEPIT, 0, 0);
}

static BOOL IsValidInstaller()
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(GetOwnPath(), &ffunc);
    if (!uf)
        return FALSE;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    unzClose(uf);

    return err == UNZ_OK && ginfo.number_entry > 0;
}

static bool InstallCopyFiles()
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(GetOwnPath(), &ffunc);
    if (!uf) {
        NotifyFailed(_T("Invalid payload format"));
        return false;
    }

    FileTransaction trans;
    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        NotifyFailed(_T("Broken payload format (couldn't get global info)"));
        goto Error;
    }

    // extract all contained files one by one (transacted, if possible)
    for (int count = 0; count < ginfo.number_entry; count++) {
        char filename[MAX_PATH];
        unz_file_info64 finfo;
        err = unzGetCurrentFileInfo64(uf, &finfo, filename, dimof(filename), NULL, 0, NULL, 0);
        if (err != UNZ_OK) {
            NotifyFailed(_T("Broken payload format (couldn't get file info)"));
            goto Error;
        }

        err = unzOpenCurrentFilePassword(uf, NULL);
        if (err != UNZ_OK) {
            NotifyFailed(_T("Can't access payload data"));
            goto Error;
        }

        char *data = SAZA(char, (size_t)finfo.uncompressed_size);
        if (!data) {
            NotifyFailed(_T("Not enough memory to extract all files"));
            goto Error;
        }

        err = unzReadCurrentFile(uf, data, (unsigned int)finfo.uncompressed_size);
        if (err != (int)finfo.uncompressed_size) {
            NotifyFailed(_T("Payload data was damaged (parts missing)"));
            free(data);
            goto Error;
        }

        TCHAR *inpath = str::conv::FromAnsi(filename);
        TCHAR *extpath = path::Join(gGlobalData.installDir, path::GetBaseName(inpath));

        bool success = trans.WriteAll(extpath, data, (size_t)finfo.uncompressed_size);
        if (success) {
            // set modification time to original value
            FILETIME ftModified, ftLocal;
            DosDateTimeToFileTime(HIWORD(finfo.dosDate), LOWORD(finfo.dosDate), &ftLocal);
            LocalFileTimeToFileTime(&ftLocal, &ftModified);
            trans.SetModificationTime(extpath, ftModified);
        }
        else {
            ScopedMem<TCHAR> msg(str::Format(_T("Couldn't write %s to disk"), inpath));
            NotifyFailed(msg);
        }

        free(inpath);
        free(extpath);
        free(data);

        err = unzCloseCurrentFile(uf);
        if (err != UNZ_OK) {
            NotifyFailed(_T("Payload data was damaged (CRC failed)"));
            goto Error;
        }

        for (int i = 0; i < dimof(gPayloadData); i++) {
            if (success && gPayloadData[i].install && str::EqI(filename, gPayloadData[i].filepath)) {
                gPayloadData[i].install = false;
                break;
            }
        }
        ProgressStep();

        err = unzGoToNextFile(uf);
        if (err != UNZ_OK || !success)
            break;
    }

    unzClose(uf);

    for (int i = 0; i < dimof(gPayloadData); i++) {
        if (gPayloadData[i].install) {
            NotifyFailed(_T("Some files to be installed are missing"));
            return false;
        }
    }
    return trans.Commit();

Error:
    if (uf) {
        unzCloseCurrentFile(uf);
        unzClose(uf);
    }
    return false;
}

/* Caller needs to free() the result. */
static TCHAR *GetDefaultPdfViewer()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL);
}

static bool IsBrowserPluginInstalled()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_PLUGIN, PLUGIN_PATH));
    if (!buf)
        buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_PLUGIN, PLUGIN_PATH));
    return file::Exists(buf);
}

static bool IsPdfFilterInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\PersistentHandler"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

static bool IsPdfPreviewerInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(TCHAR *dir)
{
    ScopedMem<TCHAR> dirPattern(path::Join(dir, _T("*")));
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        }
        else if (!str::Eq(findData.cFileName, _T(".")) && !str::Eq(findData.cFileName, _T(".."))) {
            ScopedMem<TCHAR> subdir(path::Join(dir, findData.cFileName));
            totalSize += GetDirSize(subdir);
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);

    return totalSize;
}

// caller needs to free() the result
static TCHAR *GetInstallDate()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(_T("%04d%02d%02d"), st.wYear, st.wMonth, st.wDay);
}

static bool WriteUninstallerRegistryInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<TCHAR> uninstallerPath(GetUninstallerPath());
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<TCHAR> installDate(GetInstallDate());
    ScopedMem<TCHAR> installDir(path::GetDir(installedExePath));

    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_ICON, installedExePath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_NAME, TAPP);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_VERSION, CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance, so include it in the DisplayName
    if (!WindowsVerVistaOrGreater())
        success &= WriteRegStr(hkey, REG_PATH_UNINST, DISPLAY_NAME, TAPP _T(" ") CURR_VERSION_STR);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, ESTIMATED_SIZE, GetDirSize(gGlobalData.installDir) / 1024);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_DATE, installDate);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_LOCATION, installDir);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_MODIFY, 1);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_REPAIR, 1);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, PUBLISHER, _T(PUBLISHER_STR));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, UNINSTALL_STRING, uninstallerPath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_INFO_ABOUT, _T("http://blog.kowalczyk.info/software/sumatrapdf/"));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_UPDATE_INFO, _T("http://blog.kowalczyk.info/software/sumatrapdf/news.html"));

    return success;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey)
        success &= WriteRegStr(hkey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") EXENAME, NULL, exePath);

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    ScopedMem<TCHAR> iconPath(str::Join(exePath, _T(",1")));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\DefaultIcon"), NULL, iconPath);
    ScopedMem<TCHAR> cmdPath(str::Format(_T("\"%s\" \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\Open\\Command"), NULL, cmdPath);
    ScopedMem<TCHAR> printPath(str::Format(_T("\"%s\" -print-to-default \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\Print\\Command"), NULL, printPath);
    ScopedMem<TCHAR> printToPath(str::Format(_T("\"%s\" -print-to \"%%2\" \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\PrintTo\\Command"), NULL, printToPath);
    // don't add REG_CLASSES_APPS _T("\\SupportedTypes"), as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)

    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    for (int i = 0; i < dimof(gSupportedExts); i++) {
        ScopedMem<TCHAR> keyname(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") EXENAME));
        success &= CreateRegKey(hkey, keyname);
        // TODO: stop removing this after version 1.8 (was wrongly created for version 1.6)
        keyname.Set(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") TAPP));
        DeleteRegKey(hkey, keyname);
    }

    // in case these values don't exist yet (we won't delete these at uninstallation)
    success &= WriteRegStr(hkey, REG_CLASSES_PDF, _T("Content Type"), _T("application/pdf"));
    success &= WriteRegStr(hkey, _T("Software\\Classes\\MIME\\Database\\Content Type\\application/pdf"), _T("Extension"), _T(".pdf"));

    return success;
}

static bool CreateInstallationDirectory()
{
    bool ok = dir::CreateAll(gGlobalData.installDir);
    if (!ok) {
        SeeLastError();
        NotifyFailed(_T("Couldn't create the installation directory"));
    }
    return ok;
}

#else

// Try harder getting temporary directory
// Caller needs to free() the result.
// Returns NULL if fails for any reason.
static TCHAR *GetValidTempDir()
{
    TCHAR d[MAX_PATH];
    DWORD res = GetTempPath(dimof(d), d);
    if ((0 == res) || (res >= MAX_PATH)) {
        NotifyFailed(_T("Couldn't obtain temporary directory"));
        return NULL;
    }
    BOOL success = CreateDirectory(d, NULL);
    if (!success && (ERROR_ALREADY_EXISTS != GetLastError())) {
        SeeLastError();
        NotifyFailed(_T("Couldn't create temporary directory"));
        return NULL;
    }
    return str::Dup(d);
}

static TCHAR *GetTempUninstallerPath()
{
    ScopedMem<TCHAR> tempDir(GetValidTempDir());
    if (!tempDir)
        return NULL;
    // Using fixed (unlikely) name instead of GetTempFileName()
    // so that we don't litter temp dir with copies of ourselves
    return path::Join(tempDir, _T("sum~inst.exe"));
}

static BOOL IsUninstallerNeeded()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    return file::Exists(exePath);
}

static bool RemoveUninstallerRegistryInfo(HKEY hkey)
{
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // this key was added by installers up to version 1.8
    bool ok2 = DeleteRegKey(hkey, REG_PATH_SOFTWARE);
    return ok1 && ok2;
}

/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey)
{
    ScopedMem<TCHAR> curr(ReadRegStr(hkey, REG_CLASSES_PDF, NULL));
    ScopedMem<TCHAR> prev(ReadRegStr(hkey, REG_CLASSES_APP, _T("previous.pdf")));
    if (!curr || !str::Eq(curr, TAPP)) {
        // not the default, do nothing
    } else if (prev) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, prev);
    } else {
        SHDeleteValue(hkey, REG_CLASSES_PDF, NULL);
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID));
    if (str::Eq(buf, TAPP)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS)
            SeeLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION));
    if (str::EqI(buf, EXENAME)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION);
        if (res != ERROR_SUCCESS)
            SeeLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), PROG_ID));
    if (str::Eq(buf, TAPP))
        DeleteRegKey(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), true);
}

static void RemoveOwnRegistryKeys()
{
    HKEY keys[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    // remove all keys from both HKLM and HKCU (wherever they exist)
    for (int i = 0; i < dimof(keys); i++) {
        UnregisterFromBeingDefaultViewer(keys[i]);
        DeleteRegKey(keys[i], REG_CLASSES_APP);
        DeleteRegKey(keys[i], REG_CLASSES_APPS);
        SHDeleteValue(keys[i], REG_CLASSES_PDF _T("\\OpenWithProgids"), TAPP);

        for (int i = 0; i < dimof(gSupportedExts); i++) {
            ScopedMem<TCHAR> keyname(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithProgids")));
            SHDeleteValue(keys[i], keyname, TAPP);
            keyname.Set(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") EXENAME));
            DeleteRegKey(keys[i], keyname);
        }
    }
}

static BOOL RemoveEmptyDirectory(TCHAR *dir)
{
    WIN32_FIND_DATA findData;
    BOOL success = TRUE;

    ScopedMem<TCHAR> dirPattern(path::Join(dir, _T("*")));
    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            ScopedMem<TCHAR> path(path::Join(dir, findData.cFileName));
            DWORD attrs = findData.dwFileAttributes;
            // filter out directories. Even though there shouldn't be any
            // subdirectories, it also filters out the standard "." and ".."
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                !str::Eq(findData.cFileName, _T(".")) &&
                !str::Eq(findData.cFileName, _T(".."))) {
                success &= RemoveEmptyDirectory(path);
            }
        } while (FindNextFile(h, &findData) != 0);
        FindClose(h);
    }

    if (!RemoveDirectory(dir)) {
        DWORD lastError = GetLastError();
        if (ERROR_DIR_NOT_EMPTY != lastError && ERROR_FILE_NOT_FOUND != lastError) {
            SeeLastError(lastError);
            success = FALSE;
        }
    }

    return success;
}

static BOOL RemoveInstalledFiles()
{
    BOOL success = TRUE;

    for (int i = 0; i < dimof(gPayloadData); i++) {
        ScopedMem<TCHAR> relPath(str::conv::FromUtf8(gPayloadData[i].filepath));
        ScopedMem<TCHAR> path(path::Join(gGlobalData.installDir, relPath));

        if (file::Exists(path))
            success &= DeleteFile(path);
    }

    RemoveEmptyDirectory(gGlobalData.installDir);
    return success;
}

#endif

static void InstallBrowserPlugin()
{
    ScopedMem<TCHAR> dllPath(GetBrowserPluginPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install browser plugin"));
}

static void UninstallBrowserPlugin()
{
    ScopedMem<TCHAR> dllPath(GetBrowserPluginPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall browser plugin"));
}

static void InstallPdfFilter()
{
    ScopedMem<TCHAR> dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install PDF search filter"));
}

static void UninstallPdfFilter()
{
    ScopedMem<TCHAR> dllPath(GetPdfFilterPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall PDF search filter"));
}

static void InstallPdfPreviewer()
{
    ScopedMem<TCHAR> dllPath(GetPdfPreviewerPath());
    if (!RegisterServerDLL(dllPath))
        NotifyFailed(_T("Couldn't install PDF previewer"));
}

static void UninstallPdfPreviewer()
{
    ScopedMem<TCHAR> dllPath(GetPdfPreviewerPath());
    if (!RegisterServerDLL(dllPath, true))
        NotifyFailed(_T("Couldn't uninstall PDF previewer"));
}

static bool CreateAppShortcut(bool allUsers)
{
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<TCHAR> shortcutPath(GetShortcutPath(allUsers));
    return CreateShortcut(shortcutPath, installedExePath);
}

static bool RemoveShortcut(bool allUsers)
{
    ScopedMem<TCHAR> p(GetShortcutPath(allUsers));
    bool ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        SeeLastError();
        return false;
    }
    return true;
}

static HWND CreateDefaultButton(HWND hwndParent, const TCHAR *label, int width, int id=IDOK)
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

static void CreateButtonExit(HWND hwndParent)
{
    gHwndButtonExit = CreateDefaultButton(hwndParent, _T("Close"), 80, ID_BUTTON_EXIT);
}

static void OnButtonExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

#ifndef BUILD_UNINSTALLER

static void CreateButtonRunSumatra(HWND hwndParent)
{
    gHwndButtonRunSumatra = CreateDefaultButton(hwndParent, _T("Start ") TAPP, 120, ID_BUTTON_START_SUMATRA);
}

static DWORD WINAPI InstallerThread(LPVOID data)
{
    gGlobalData.success = false;

    if (!CreateInstallationDirectory())
        goto Error;
    ProgressStep();

    if (!InstallCopyFiles())
        goto Error;

    if (gGlobalData.registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
        CreateProcessHelper(installedExePath, _T("-register-for-pdf"));
    }

    if (gGlobalData.installBrowserPlugin)
        InstallBrowserPlugin();
    else if (IsBrowserPluginInstalled())
        UninstallBrowserPlugin();

    if (gGlobalData.installPdfFilter)
        InstallPdfFilter();
    else if (IsPdfFilterInstalled())
        UninstallPdfFilter();

    if (gGlobalData.installPdfPreviewer)
        InstallPdfPreviewer();
    else if (IsPdfPreviewerInstalled())
        UninstallPdfPreviewer();

    if (!CreateAppShortcut(true) && !CreateAppShortcut(false)) {
        NotifyFailed(_T("Failed to create a shortcut"));
        goto Error;
    }

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gGlobalData.success = true;

    if (!WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) &&
        !WriteUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to write the uninstallation information to the registry"));
    }
    if (!WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE) &&
        !WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to write the extended file extension information to the registry"));
    }
    ProgressStep();

Error:
    // TODO: roll back installation on failure (restore previous installation!)
    if (gHwndFrame && !gGlobalData.silent) {
        Sleep(500); // allow a glimpse of the completed progress bar before hiding it
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void OnButtonOptions();

static bool IsCheckboxChecked(HWND hwnd)
{
    return (Button_GetState(hwnd) & BST_CHECKED) == BST_CHECKED;
}

static void OnButtonInstall()
{
    if (gShowOptions)
        OnButtonOptions();

    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    TCHAR *userInstallDir = win::GetText(gHwndTextboxInstDir);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gGlobalData.installDir, userInstallDir);
    free(userInstallDir);

    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gGlobalData.registerAsDefault = gHwndCheckboxRegisterDefault == NULL ||
                                    IsCheckboxChecked(gHwndCheckboxRegisterDefault);

    gGlobalData.installBrowserPlugin = IsCheckboxChecked(gHwndCheckboxRegisterBrowserPlugin);
    // note: this checkbox isn't created when running inside Wow64
    gGlobalData.installPdfFilter = gHwndCheckboxRegisterPdfFilter != NULL &&
                                   IsCheckboxChecked(gHwndCheckboxRegisterPdfFilter);
    // note: this checkbox isn't created on Windows 2000 and XP
    gGlobalData.installPdfPreviewer = gHwndCheckboxRegisterPdfPreviewer != NULL &&
                                      IsCheckboxChecked(gHwndCheckboxRegisterPdfPreviewer);

    // create a progress bar in place of the Options button
    RectI rc(0, 0, dpiAdjust(INSTALLER_WIN_DX / 2), PUSH_BUTTON_DY);
    rc = MapRectToWindow(rc, gHwndButtonOptions, gHwndFrame);
    gHwndProgressBar = CreateWindow(PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
                                    rc.x, rc.y, rc.dx, rc.dy,
                                    gHwndFrame, 0, ghinst, NULL);
    SendMessage(gHwndProgressBar, PBM_SETRANGE32, 0, GetInstallationStepCount());
    SendMessage(gHwndProgressBar, PBM_SETSTEP, 1, 0);

    // disable the install button and remove all the installation options
    DestroyWindow(gHwndStaticInstDir);
    gHwndStaticInstDir = NULL;
    DestroyWindow(gHwndTextboxInstDir);
    gHwndTextboxInstDir = NULL;
    DestroyWindow(gHwndButtonBrowseDir);
    gHwndButtonBrowseDir = NULL;
    DestroyWindow(gHwndCheckboxRegisterDefault);
    gHwndCheckboxRegisterDefault = NULL;
    DestroyWindow(gHwndCheckboxRegisterBrowserPlugin);
    gHwndCheckboxRegisterBrowserPlugin = NULL;
    DestroyWindow(gHwndCheckboxRegisterPdfFilter);
    gHwndCheckboxRegisterPdfFilter = NULL;
    DestroyWindow(gHwndCheckboxRegisterPdfPreviewer);
    gHwndCheckboxRegisterPdfPreviewer = NULL;
    DestroyWindow(gHwndButtonOptions);
    gHwndButtonOptions = NULL;

    EnableWindow(gHwndButtonInstUninst, FALSE);

    SetMsg(_T("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, InstallerThread, NULL, 0, 0);
}

void OnInstallationFinished()
{
    DestroyWindow(gHwndButtonInstUninst);
    gHwndButtonInstUninst = NULL;
    DestroyWindow(gHwndProgressBar);
    gHwndProgressBar = NULL;

    if (gGlobalData.success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_T("Thank you! ") TAPP _T(" has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_T("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

typedef BOOL WINAPI SaferCreateLevelProc(DWORD dwScopeId, DWORD dwLevelId, DWORD OpenFlags, SAFER_LEVEL_HANDLE *pLevelHandle, LPVOID lpReserved);
typedef BOOL WINAPI SaferComputeTokenFromLevelProc(SAFER_LEVEL_HANDLE LevelHandle, HANDLE InAccessToken, PHANDLE OutAccessToken, DWORD dwFlags, LPVOID lpReserved);
typedef BOOL WINAPI SaferCloseLevelProc(SAFER_LEVEL_HANDLE hLevelHandle);

static HANDLE CreateProcessAtLevel(const TCHAR *exe, const TCHAR *args=NULL, DWORD level=SAFER_LEVELID_NORMALUSER)
{
    HMODULE h = SafeLoadLibrary(_T("Advapi32.dll"));
    if (!h)
        return NULL;
#define ImportProc(func) func ## Proc *_ ## func = (func ## Proc *)GetProcAddress(h, #func)
    ImportProc(SaferCreateLevel);
    ImportProc(SaferComputeTokenFromLevel);
    ImportProc(SaferCloseLevel);
#undef ImportProc
    if (!_SaferCreateLevel || !_SaferComputeTokenFromLevel || !_SaferCloseLevel)
        return NULL;

    SAFER_LEVEL_HANDLE slh;
    if (!_SaferCreateLevel(SAFER_SCOPEID_USER, level, 0, &slh, NULL))
        return NULL;

    ScopedMem<TCHAR> cmd(str::Format(_T("\"%s\" %s"), exe, args ? args : _T("")));
    PROCESS_INFORMATION pi;
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    HANDLE token;
    if (!_SaferComputeTokenFromLevel(slh, NULL, &token, 0, NULL))
        goto Error;
    if (!CreateProcessAsUser(token, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        goto Error;

    CloseHandle(pi.hThread);
    _SaferCloseLevel(slh);
    return pi.hProcess;

Error:
    SeeLastError();
    _SaferCloseLevel(slh);
    return NULL;
}

static void OnButtonStartSumatra()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    // try to create the process as a normal user
    ScopedHandle h(CreateProcessAtLevel(exePath));
    // create the process as is (mainly for Windows 2000 compatibility)
    if (!h)
        CreateProcessHelper(exePath);

    OnButtonExit();
}

inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

static void OnButtonOptions()
{
    gShowOptions = !gShowOptions;

    EnableAndShow(gHwndStaticInstDir, gShowOptions);
    EnableAndShow(gHwndTextboxInstDir, gShowOptions);
    EnableAndShow(gHwndButtonBrowseDir, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterDefault, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterBrowserPlugin, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfFilter, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfPreviewer, gShowOptions);

    win::SetText(gHwndButtonOptions, gShowOptions ? _T("Hide &Options") : _T("&Options"));

    ClientRect rc(gHwndFrame);
    rc.dy -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);

    SetFocus(gHwndButtonOptions);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
    switch (msg) {
    case BFFM_INITIALIZED:
        if (!str::IsEmpty((TCHAR *)lpData))
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
        break;

    // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
    case BFFM_SELCHANGED:
        {
            TCHAR szDir[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lParam, szDir) && _taccess(szDir, 00) == 0) {
                SHFILEINFO sfi;
                SHGetFileInfo((LPCTSTR)lParam, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK))
                    break;
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        }
        break;
    }

    return 0;
}

static BOOL BrowseForFolder(HWND hwnd, LPCTSTR lpszInitialFolder, LPCTSTR lpszCaption, LPTSTR lpszBuf, DWORD dwBufSize)
{
    if (lpszBuf == NULL || dwBufSize < MAX_PATH)
        return FALSE;

    BROWSEINFO bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = lpszCaption;
    bi.lpfn      = BrowseCallbackProc;
    bi.lParam    = (LPARAM)lpszInitialFolder;

    BOOL success = FALSE;
    LPITEMIDLIST pidlFolder = SHBrowseForFolder(&bi);
    if (pidlFolder) {
        success = SHGetPathFromIDList(pidlFolder, lpszBuf);

        IMalloc *pMalloc = NULL; 
        if (SUCCEEDED(SHGetMalloc(&pMalloc)) && pMalloc) {
            pMalloc->Free(pidlFolder);  
            pMalloc->Release(); 
        }
    }

    return success;
}

static void OnButtonBrowse()
{
    ScopedMem<TCHAR> installDir(win::GetText(gHwndTextboxInstDir));
    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir))
        installDir.Set(path::GetDir(installDir));

    TCHAR path[MAX_PATH];
    if (BrowseForFolder(gHwndFrame, installDir, _T("Select the folder into which ") TAPP _T(" should be installed:"), path, dimof(path))) {
        TCHAR *installPath = path;
        // force paths that aren't entered manually to end in ...\SumatraPDF
        // to prevent unintended installations into e.g. %ProgramFiles% itself
        if (!str::EndsWithI(path, _T("\\") TAPP))
            installPath = path::Join(path, TAPP);
        win::SetText(gHwndTextboxInstDir, installPath);
        Edit_SetSel(gHwndTextboxInstDir, 0, -1);
        SetFocus(gHwndTextboxInstDir);
        if (installPath != path)
            free(installPath);
    }
    else
        SetFocus(gHwndButtonBrowseDir);
}

#else

static DWORD WINAPI UninstallerThread(LPVOID data)
{
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    TCHAR *exePath = GetUninstallerPath();
    if (!path::IsSame(exePath, GetOwnPath()))
        KillProcess(exePath, TRUE);
    free(exePath);

    if (!RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) &&
        !RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to delete uninstaller registry keys"));
    }

    if (!RemoveShortcut(true) && !RemoveShortcut(false))
        NotifyFailed(_T("Couldn't remove the shortcut"));

    RemoveOwnRegistryKeys();
    UninstallBrowserPlugin();
    UninstallPdfFilter();
    UninstallPdfPreviewer();

    if (!RemoveInstalledFiles())
        NotifyFailed(_T("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    gGlobalData.success = true;

    if (!gGlobalData.silent)
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    return 0;
}

static void OnButtonUninstall()
{
    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    // disable the button during uninstallation
    EnableWindow(gHwndButtonInstUninst, FALSE);
    SetMsg(_T("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, UninstallerThread, NULL, 0, 0);
}

static void OnUninstallationFinished()
{
    DestroyWindow(gHwndButtonInstUninst);
    gHwndButtonInstUninst = NULL;
    CreateButtonExit(gHwndFrame);
    SetMsg(TAPP _T(" has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

#endif

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
    // TODO: clearly, not random but seem to work ok anyway
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

class FrameTimeoutCalculator {

    LARGE_INTEGER   timeStart;
    LARGE_INTEGER   timeLast;
    LONGLONG        ticksPerFrame;
    LONGLONG        ticsPerMs;
    LARGE_INTEGER   timeFreq;

public:
    FrameTimeoutCalculator(int framesPerSecond) {
        QueryPerformanceFrequency(&timeFreq); // number of ticks per second
        ticsPerMs = timeFreq.QuadPart / 1000;
        ticksPerFrame = timeFreq.QuadPart / framesPerSecond;
        QueryPerformanceCounter(&timeStart);
        timeLast = timeStart;
    }

    // in seconds, as a double
    double ElapsedTotal() {
        LARGE_INTEGER timeCurr;
        QueryPerformanceCounter(&timeCurr);
        LONGLONG elapsedTicks =  timeCurr.QuadPart - timeStart.QuadPart;
        double res = (double)elapsedTicks / (double)timeFreq.QuadPart;
        return res;
    }

    DWORD GetTimeoutInMilliseconds() {
        LARGE_INTEGER timeCurr;
        LONGLONG elapsedTicks;
        QueryPerformanceCounter(&timeCurr);
        elapsedTicks = timeCurr.QuadPart - timeLast.QuadPart;
        if (elapsedTicks > ticksPerFrame) {
            return 0;
        } else {
            LONGLONG timeoutMs = (ticksPerFrame - elapsedTicks) / ticsPerMs;
            return (DWORD)timeoutMs;
        }
    }

    void Step() {
        timeLast.QuadPart += ticksPerFrame;
    }
};

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
    Gdiplus::RectF maxbox(0, y, dx, 0);
    Gdiplus::RectF bbox;
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
        Gdiplus::PointF o2(li->x - 3.f, y + 4.f + li->dyOff);
        g.DrawString(s, 1, f, o2, &b2);
#endif

        SolidBrush b1(li->col);
        Gdiplus::PointF o1(li->x, y + li->dyOff);
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
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2 - 2, y2 - 1), &b1);
#endif
    SolidBrush b2(Color(0xff, 0xff, 0xff));
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2, y2), &b2);
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
    Gdiplus::Rect r2(r.y - 1, r.x - 1, r.dx + 1, r.dy + 1);
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

#ifndef BUILD_UNINSTALLER

// TODO: since we have a variable UI, for better layout (anchored to the bottom,
// not the top), we should layout controls starting at the bottom and go up
static void OnCreateWindow(HWND hwnd)
{
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _T("Install ") TAPP, 140);

    RectI rc(WINDOW_MARGIN, 0, dpiAdjust(96), PUSH_BUTTON_DY);
    ClientRect r(hwnd);
    rc.y = r.dy - rc.dy - WINDOW_MARGIN;

    gHwndButtonOptions = CreateWindow(WC_BUTTON, _T("&Options"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        rc.x, rc.y, rc.dx, rc.dy, hwnd,
                        (HMENU)ID_BUTTON_OPTIONS, ghinst, NULL);
    SetWindowFont(gHwndButtonOptions, gFontDefault, TRUE);

    int staticDy = dpiAdjust(20);
    rc.y = TITLE_PART_DY + WINDOW_MARGIN;
    gHwndStaticInstDir = CreateWindow(WC_STATIC, _T("Install ") TAPP _T(" into the following &folder:"),
                                      WS_CHILD,
                                      rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
                                      hwnd, NULL, ghinst, NULL);
    SetWindowFont(gHwndStaticInstDir, gFontDefault, TRUE);
    rc.y += staticDy;

    gHwndTextboxInstDir = CreateWindow(WC_EDIT, gGlobalData.installDir,
                                       WS_CHILD | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                                       rc.x, rc.y, r.dx - 3 * rc.x - staticDy, staticDy,
                                       hwnd, NULL, ghinst, NULL);
    SetWindowFont(gHwndTextboxInstDir, gFontDefault, TRUE);
    gHwndButtonBrowseDir = CreateWindow(WC_BUTTON, _T("&..."),
                                        BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP,
                                        r.dx - rc.x - staticDy, rc.y, staticDy, staticDy,
                                        hwnd, (HMENU)ID_BUTTON_BROWSE, ghinst, NULL);
    SetWindowFont(gHwndButtonBrowseDir, gFontDefault, TRUE);
    rc.y += 2 * staticDy;

    ScopedMem<TCHAR> defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, TAPP);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        gHwndCheckboxRegisterDefault = CreateWindow(
            WC_BUTTON, _T("Use ") TAPP _T(" as the &default PDF reader"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_MAKE_DEFAULT, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterDefault, gFontDefault, TRUE);
        // only check the "Use as default" checkbox when no other PDF viewer
        // is currently selected (not going to intrude)
        Button_SetCheck(gHwndCheckboxRegisterDefault, !hasOtherViewer || gGlobalData.registerAsDefault);
        rc.y += staticDy;
    }

    gHwndCheckboxRegisterBrowserPlugin = CreateWindow(
        WC_BUTTON, _T("Install PDF &browser plugin for Firefox, Chrome and Opera"),
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
        hwnd, (HMENU)ID_CHECKBOX_BROWSER_PLUGIN, ghinst, NULL);
    SetWindowFont(gHwndCheckboxRegisterBrowserPlugin, gFontDefault, TRUE);
    Button_SetCheck(gHwndCheckboxRegisterBrowserPlugin, gGlobalData.installBrowserPlugin || IsBrowserPluginInstalled());
    rc.y += staticDy;

    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
#ifndef _WIN64
    if (!IsRunningInWow64())
#endif
    {
        gHwndCheckboxRegisterPdfFilter = CreateWindow(
            WC_BUTTON, _T("Let Windows Desktop Search &search PDF documents"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_PDF_FILTER, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterPdfFilter, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfFilter, gGlobalData.installPdfFilter || IsPdfFilterInstalled());
        rc.y += staticDy;
    }

    // only show this checkbox on systems that actually support
    // IThumbnailProvider and IPreviewHandler
    if (WindowsVerVistaOrGreater()) {
        gHwndCheckboxRegisterPdfPreviewer = CreateWindow(
            WC_BUTTON, _T("Let Windows show &previews of PDF documents"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_PDF_PREVIEWER, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterPdfPreviewer, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfPreviewer, gGlobalData.installPdfPreviewer || IsPdfPreviewerInstalled());
        rc.y += staticDy;
    }

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    SetFocus(gHwndButtonInstUninst);
}

#else

static void OnCreateWindow(HWND hwnd)
{
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _T("Uninstall ") TAPP, 150);
}

#endif

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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
            switch (LOWORD(wParam))
            {
#ifndef BUILD_UNINSTALLER
                case IDOK:
                    if (gHwndButtonInstUninst)
                        OnButtonInstall();
                    else if (gHwndButtonRunSumatra)
                        OnButtonStartSumatra();
                    else if (gHwndButtonExit)
                        OnButtonExit();
                    break;

                case ID_BUTTON_START_SUMATRA:
                    OnButtonStartSumatra();
                    break;

                case ID_BUTTON_OPTIONS:
                    OnButtonOptions();
                    break;

                case ID_BUTTON_BROWSE:
                    OnButtonBrowse();
                    break;
#else
                case IDOK:
                    if (gHwndButtonInstUninst)
                        OnButtonUninstall();
                    else if (gHwndButtonExit)
                        OnButtonExit();
                    break;
#endif

                case ID_BUTTON_EXIT:
                case IDCANCEL:
                    OnButtonExit();
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
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

#ifdef BUILD_UNINSTALLER
    gHwndFrame = CreateWindow(
            INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Uninstaller"),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY),
            NULL, NULL,
            ghinst, NULL);
#else
    gHwndFrame = CreateWindow(
            INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Installer"),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY),
            NULL, NULL,
            ghinst, NULL);
#endif
    if (!gHwndFrame)
        return FALSE;

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

#ifdef BUILD_UNINSTALLER

// If this is uninstaller and we're running from installation directory,
// copy uninstaller to temp directory and execute from there, exiting
// ourselves. This is needed so that uninstaller can delete itself
// from installation directory and remove installation directory
// If returns TRUE, this is an installer and we sublaunched ourselves,
// so the caller needs to exit
static bool ExecuteUninstallerFromTempDir()
{
    // only need to sublaunch if running from installation dir
    ScopedMem<TCHAR> ownDir(path::GetDir(GetOwnPath()));
    ScopedMem<TCHAR> tempPath(GetTempUninstallerPath());

    // no temp directory available?
    if (!tempPath)
        return false;

    // not running from the installation directory?
    // (likely a test uninstaller that shouldn't be removed anyway)
    if (!path::IsSame(ownDir, gGlobalData.installDir))
        return false;

    // already running from temp directory?
    if (path::IsSame(GetOwnPath(), tempPath))
        return false;

    if (!CopyFile(GetOwnPath(), tempPath, FALSE)) {
        NotifyFailed(_T("Failed to copy uninstaller to temp directory"));
        return false;
    }

    ScopedMem<TCHAR> args(str::Format(_T("/d \"%s\" %s"), gGlobalData.installDir, gGlobalData.silent ? _T("/s") : _T("")));
    bool ok = CreateProcessHelper(tempPath, args);

    // mark the uninstaller for removal at shutdown (note: works only for administrators)
    MoveFileEx(tempPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    return ok;
}

static void ShowUsage()
{
    MessageBox(NULL, _T("uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls ") TAPP _T(" silently (without user interaction).\n\
    /d\tchanges the directory from where ") TAPP _T(" will be uninstalled."), TAPP _T(" Uninstaller Usage"), MB_OK | MB_ICONINFORMATION);
}

#else

static void ShowUsage()
{
    MessageBox(NULL, TAPP _T("-install.exe [/s][/d <path>][/register][/opt plugin,...]\n\
    \n\
    /s\tinstalls ") TAPP _T(" silently (without user interaction).\n\
    /d\tchanges the directory where ") TAPP _T(" will be installed.\n\
    /register\tregisters ") TAPP _T(" as the default PDF viewer.\n\
    /opt\tenables optional components (currently: plugin, pdffilter, pdfpreviewer)."), TAPP _T(" Installer Usage"), MB_OK | MB_ICONINFORMATION);
}

#endif

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    MillisecondTimer t;
    t.Start();
    for (;;) {
        const DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        if (timeout > 0) {
            res = MsgWaitForMultipleObjects(0, 0, TRUE, timeout, QS_ALLEVENTS);
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
        if (t.GetCurrTimeInMs() > 10000 &&
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
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

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
