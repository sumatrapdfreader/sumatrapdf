/* Copyright 2010-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * make it grow proportionally for DPI > 96
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

#include <windows.h>
#include <GdiPlus.h>
#include <tchar.h>
#include <shlobj.h>
#include <Tlhelp32.h>
#include <Shlwapi.h>
#include <objidl.h>
#include <WinSafer.h>
#include <io.h>

#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

#include "Resource.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Version.h"
#include "Vec.h"

#include "CmdLineParser.h"
#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"

// define for testing the uninstaller
// #define TEST_UNINSTALLER

using namespace Gdiplus;

// define for a shadow effect
#define DRAW_TEXT_SHADOW

#define INSTALLER_FRAME_CLASS_NAME    _T("SUMATRA_PDF_INSTALLER_FRAME")

#define INSTALLER_WIN_DX    420
#define INSTALLER_WIN_DY    340
#define UNINSTALLER_WIN_DX  420
#define UNINSTALLER_WIN_DY  340

#define ID_BUTTON_INSTALL             11
#define ID_BUTTON_UNINSTALL           12
#define ID_CHECKBOX_MAKE_DEFAULT      13
#define ID_CHECKBOX_BROWSER_PLUGIN    14
#define ID_BUTTON_START_SUMATRA       15
#define ID_BUTTON_EXIT                16
#define ID_BUTTON_OPTIONS             17
#define ID_BUTTON_BROWSE              18
#define ID_CHECKBOX_PDF_FILTER        19
#define ID_CHECKBOX_PDF_PREVIEWER     20

#define WM_APP_INSTALLATION_FINISHED        (WM_APP + 1)

// The window is divided in three parts:
// * top part, where we display nice graphics
// * middle part, where we either display messages or advanced options
// * bottom part, with install/uninstall button
// This is the height of the top part
#define TITLE_PART_DY  110
// This is the height of the lower part
#define BOTTOM_PART_DY 40

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static HWND             gHwndButtonInstall = NULL;
static HWND             gHwndButtonOptions = NULL;
static HWND             gHwndButtonExit = NULL;
static HWND             gHwndButtonRunSumatra = NULL;
static HWND             gHwndStaticInstDir = NULL;
static HWND             gHwndTextboxInstDir = NULL;
static HWND             gHwndButtonBrowseDir = NULL;
static HWND             gHwndCheckboxRegisterDefault = NULL;
static HWND             gHwndCheckboxRegisterBrowserPlugin = NULL;
static HWND             gHwndCheckboxRegisterPdfFilter = NULL;
static HWND             gHwndCheckboxRegisterPdfPreviewer = NULL;
static HWND             gHwndButtonUninstall = NULL;
static HWND             gHwndProgressBar = NULL;
static HFONT            gFontDefault;
static bool             gShowOptions = false;

static TCHAR *          gMsg;
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
#define REG_PATH_SOFTWARE   _T("Software\\") TAPP

#define REG_CLASSES_APP     _T("Software\\Classes\\") TAPP
#define REG_CLASSES_PDF     _T("Software\\Classes\\.pdf")
#define REG_CLASSES_APPS    _T("Software\\Classes\\Applications\\") EXENAME

#define REG_EXPLORER_PDF_EXT  _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf")
#define PROG_ID               _T("ProgId")

#define REG_PATH_PLUGIN     _T("Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin")
#define PLUGIN_PATH         _T("Path")

// Keys we'll set in REG_PATH_UNINST path

// REG_SZ, a path to installed executable (or "$path,0" to force the first icon)
#define DISPLAY_ICON _T("DisplayIcon")
// REG_SZ, e.g "SumatraPDF"
#define DISPLAY_NAME _T("DisplayName")
// REG_SZ, e.g. "1.2"
#define DISPLAY_VERSION _T("DisplayVersion")
// REG_DWORD, get size of installed directory after copying files
#define ESTIMATED_SIZE _T("EstimatedSize")
// REG_SZ, the current date as YYYYMMDD
#define INSTALL_DATE _T("InstallDate")
// REG_DWORD, set to 1
#define NO_MODIFY _T("NoModify")
// REG_DWORD, set to 1
#define NO_REPAIR _T("NoRepair")
// REG_SZ, e.g. "Krzysztof Kowalczyk"
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
    _T(".pdf"), _T(".xps"), _T(".cbz"), _T(".cbr"), _T(".djvu")
};

// Note: UN_INST_MARK is converted from unicode to ascii at runtime to make sure
//       that this sequence of bytes (i.e. ascii version) is not present in 
//       the executable, since it's appended to the end of executable to mark
//       it as uninstaller. An alternative would be search for it from the end.
#define UN_INST_MARK L"!uninst_end!"
static char *gUnInstMark = NULL; // str::ToMultiByte(UN_INST_MARK, CP_UTF8)

// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
struct {
    char *filepath;
    bool install;
} gPayloadData[] = {
    { "SumatraPDF.exe",         true    },
    { "libmupdf.dll",           true    },
    { "sumatrapdfprefs.dat",    false   },
    { "DroidSansFallback.ttf",  true    },
    { "npPdfViewer.dll",        true    },
    { "PdfFilter.dll",          true    },
    { "PdfPreview.dll",         true    },
    { "uninstall.exe",          false   },
};

struct {
    bool uninstall;
    bool silent;
    bool showUsageAndQuit;
    TCHAR *installDir;
    bool registerAsDefault;
    bool installBrowserPlugin;
    bool installPdfFilter;
    bool installPdfPreviewer;

    TCHAR *firstError;
    HANDLE hThread;
    bool success;
} gGlobalData = {
    false, /* bool uninstall */
    false, /* bool silent */
    false, /* bool showUsageAndQuit */
    NULL,  /* TCHAR *installDir */
    false, /* bool registerAsDefault */
    false, /* bool installBrowserPlugin */
    false, /* bool installPdfFilter */
    false, /* bool installPdfPreviewer */

    NULL,  /* TCHAR *firstError */
    NULL,  /* HANDLE hThread */
    false, /* bool success */
};

void NotifyFailed(TCHAR *msg)
{
    if (!gGlobalData.firstError)
        gGlobalData.firstError = str::Dup(msg);
    // MessageBox(gHwndFrame, msg, _T("Installation failed"),  MB_ICONEXCLAMATION | MB_OK);
}

#define TEN_SECONDS_IN_MS 10*1000

// Kill a process with given <processId> if it's loaded from <processPath>.
// If <waitUntilTerminated> is TRUE, will wait until process is fully killed.
// Returns TRUE if killed a process
BOOL KillProcIdWithName(DWORD processId, TCHAR *processPath, BOOL waitUntilTerminated)
{
    BOOL killed = FALSE;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    HANDLE hModSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (!hProcess || INVALID_HANDLE_VALUE == hModSnapshot)
        goto Exit;

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(me32);
    if (!Module32First(hModSnapshot, &me32))
        goto Exit;
    if (!path::IsSame(processPath, me32.szExePath))
        goto Exit;

    killed = TerminateProcess(hProcess, 0);
    if (!killed)
        goto Exit;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    UpdateWindow(FindWindow(NULL, _T("Shell_TrayWnd")));
    UpdateWindow(GetDesktopWindow());

Exit:
    CloseHandle(hModSnapshot);
    CloseHandle(hProcess);
    return killed;
}

#define MAX_PROCESSES 1024

static int KillProcess(TCHAR *processPath, BOOL waitUntilTerminated)
{
    int killCount = 0;

    HANDLE hProcSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hProcSnapshot)
        goto Error;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32First(hProcSnapshot, &pe32))
        goto Error;

    do {
        if (KillProcIdWithName(pe32.th32ProcessID, processPath, waitUntilTerminated))
            killCount++;
    } while (Process32Next(hProcSnapshot, &pe32));

Error:
    CloseHandle(hProcSnapshot);
    return killCount;
}

TCHAR *GetOwnPath()
{
    static TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, dimof(exePath));
    return exePath;
}

TCHAR *GetInstallationDir(bool forUninstallation=false)
{
    // try the previous installation directory first
    ScopedMem<TCHAR> dir(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_SOFTWARE, INSTALL_DIR));
    if (!dir) dir.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_SOFTWARE, INSTALL_DIR));
    if (!dir) dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, INSTALL_LOCATION));
    if (!dir) dir.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, INSTALL_LOCATION));
    if (dir) {
        if (str::EndsWithI(dir, _T(".exe"))) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

    if (forUninstallation) {
        // fall back to the uninstaller's path
        return path::GetDir(GetOwnPath());
    }

    // fall back to %ProgramFiles%
    TCHAR buf[MAX_PATH] = {0};
    BOOL ok = SHGetSpecialFolderPath(NULL, buf, CSIDL_PROGRAM_FILES, FALSE);
    if (!ok)
        return NULL;
    return path::Join(buf, TAPP);
}

// Try harder getting temporary directory
// Caller needs to free() the result.
// Returns NULL if fails for any reason.
TCHAR *GetValidTempDir()
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

TCHAR *GetUninstallerPath()
{
    return path::Join(gGlobalData.installDir, _T("uninstall.exe"));
}

TCHAR *GetTempUninstallerPath()
{
    ScopedMem<TCHAR> tempDir(GetValidTempDir());
    if (!tempDir)
        return NULL;
    // Using fixed (unlikely) name instead of GetTempFileName()
    // so that we don't litter temp dir with copies of ourselves
    return path::Join(tempDir, _T("sum~inst.exe"));
}

TCHAR *GetInstalledExePath()
{
    return path::Join(gGlobalData.installDir, EXENAME);
}

TCHAR *GetBrowserPluginPath()
{
    return path::Join(gGlobalData.installDir, _T("npPdfViewer.dll"));
}

TCHAR *GetPdfFilterPath()
{
    return path::Join(gGlobalData.installDir, _T("PdfFilter.dll"));
}

TCHAR *GetPdfPreviewerPath()
{
    return path::Join(gGlobalData.installDir, _T("PdfPreview.dll"));
}

TCHAR *GetStartMenuProgramsPath(bool allUsers)
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
    return path::Join(GetStartMenuProgramsPath(allUsers), TAPP _T(".lnk"));
}

int GetInstallationStepCount()
{
    /* Installation steps
     * - Create directory
     * - One per file to be copied (count extracted from gPayloadData)
     * - Optional registration (default viewer, browser plugin),
     *   Shortcut and Uninstaller
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

BOOL IsValidInstaller()
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

BOOL InstallCopyFiles()
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(GetOwnPath(), &ffunc);
    if (!uf) {
        NotifyFailed(_T("Invalid payload format"));
        goto Error;
    }

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        NotifyFailed(_T("Broken payload format (couldn't get global info)"));
        goto Error;
    }

    // extract all contained files one by one
    for (int count = 0; count < ginfo.number_entry; count++) {
        BOOL success = FALSE;
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

        // TODO: extract block by block instead of everything at once
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

        BOOL ok = file::WriteAll(extpath, data, (size_t)finfo.uncompressed_size);
        if (ok) {
            // set modification time to original value
            HANDLE hFile = CreateFile(extpath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                FILETIME ftModified, ftLocal;
                DosDateTimeToFileTime(HIWORD(finfo.dosDate), LOWORD(finfo.dosDate), &ftLocal);
                LocalFileTimeToFileTime(&ftLocal, &ftModified);
                SetFileTime(hFile, NULL, NULL, &ftModified);
                CloseHandle(hFile);
            }
            success = TRUE;
        }
        else {
            NotifyFailed(_T("Couldn't write extracted file to disk"));
            success = FALSE;
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
            return FALSE;
        }
    }
    return TRUE;

Error:
    if (uf) {
        unzCloseCurrentFile(uf);
        unzClose(uf);
    }
    return FALSE;
}

extern "C" {
// needed because we compile bzip2 with #define BZ_NO_STDIO
void bz_internal_error(int errcode)
{
    NotifyFailed(_T("fatal error: bz_internal_error()"));
}
}

BOOL CreateUninstaller()
{
    size_t installerSize;
    char *installerData = file::ReadAll(GetOwnPath(), &installerSize);
    if (!installerData) {
        NotifyFailed(_T("Couldn't access installer for uninstaller extraction"));
        return FALSE;
    }

    if (!gUnInstMark)
        gUnInstMark = str::ToMultiByte(UN_INST_MARK, CP_UTF8);
    int markSize = str::Len(gUnInstMark);

    // find the end of the (un)installer
    char *end = (char *)memchr(installerData, *gUnInstMark, installerSize);
    while (end && memcmp(end, gUnInstMark, markSize) != 0)
        end = (char *)memchr(end + 1, *gUnInstMark, installerSize - (end - installerData) - 1);

    // if it's not found, append a new uninstaller marker to the end of the executable
    if (!end) {
        char *extData = (char *)realloc(installerData, installerSize + markSize);
        if (!extData) {
            NotifyFailed(_T("Couldn't write uninstaller to disk"));
            free(installerData);
            return FALSE;
        }
        memcpy(installerData + installerSize, gUnInstMark, markSize);
        installerSize += markSize;
    }
    else {
        installerSize = end - installerData + markSize;
    }

    ScopedMem<TCHAR> uninstallerPath(GetUninstallerPath());
    BOOL ok = file::WriteAll(uninstallerPath, installerData, installerSize);
    free(installerData);

    if (!ok)
        NotifyFailed(_T("Couldn't write uninstaller to disk"));

    return ok;
}

bool IsUninstaller()
{
    ScopedMem<TCHAR> tempUninstaller(GetTempUninstallerPath());
    BOOL isTempUninstaller = path::IsSame(GetOwnPath(), tempUninstaller);
    if (isTempUninstaller)
        return true;

    char *data = NULL;
    if (!gUnInstMark)
        gUnInstMark = str::ToMultiByte(UN_INST_MARK, CP_UTF8);
    size_t markSize = str::Len(gUnInstMark);

    size_t uninstallerSize;
    ScopedMem<char> uninstallerData(file::ReadAll(GetOwnPath(), &uninstallerSize));
    if (!uninstallerData) {
        NotifyFailed(_T("Couldn't open myself for reading"));
        return false;
    }

    bool isUninstaller = uninstallerSize > markSize &&
                         !memcmp(uninstallerData + uninstallerSize - markSize, gUnInstMark, markSize);
    return isUninstaller;
}

static HFONT CreateDefaultGuiFont()
{
    HDC hdc = GetDC(NULL);
    HFONT font = win::font::GetSimple(hdc, _T("MS Shell Dlg"), 14);
    ReleaseDC(NULL, hdc);
    return font;
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

HANDLE CreateProcessHelper(const TCHAR *exe, const TCHAR *args=NULL)
{
    PROCESS_INFORMATION pi;
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    // per msdn, cmd has to be writeable
    ScopedMem<TCHAR> cmd(str::Format(_T("\"%s\" %s"), exe, args ? args : _T("")));
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        SeeLastError();
        return NULL;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// cf. http://support.microsoft.com/default.aspx?scid=kb;en-us;207132
bool RegisterServerDLL(TCHAR *dllPath, bool unregister=false)
{
    if (FAILED(OleInitialize(NULL)))
        return false;

    bool ok = false;
    // TODO: LoadLibrary seems to fail during uninstallation(?)
    HMODULE lib = LoadLibrary(dllPath);
    if (lib) {
        typedef HRESULT (WINAPI *DllInitProc)(VOID);
        DllInitProc CallDLL = (DllInitProc)GetProcAddress(lib, unregister ? "DllUnregisterServer" : "DllRegisterServer");
        if (CallDLL)
            ok = SUCCEEDED(CallDLL());
        FreeLibrary(lib);
    }

    OleUninitialize();

    return ok;
}

// Note: doesn't handle (total) sizes above 4GB
DWORD GetDirSize(TCHAR *dir)
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
TCHAR *GetInstallDate()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(_T("%04d%02d%02d"), st.wYear, st.wMonth, st.wDay);
}

bool WriteUninstallerRegistryInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<TCHAR> uninstallerPath(GetUninstallerPath());
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<TCHAR> installDate(GetInstallDate());
    ScopedMem<TCHAR> installDir(path::GetDir(installedExePath));

    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_ICON, installedExePath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_NAME, TAPP);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_VERSION, CURR_VERSION_STR);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, ESTIMATED_SIZE, GetDirSize(gGlobalData.installDir) / 1024);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_DATE, installDate);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_LOCATION, installDir);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_MODIFY, 1);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_REPAIR, 1);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, PUBLISHER, _T("Krzysztof Kowalczyk"));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, UNINSTALL_STRING, uninstallerPath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_INFO_ABOUT, _T("http://blog.kowalczyk.info/software/sumatrapdf/"));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_UPDATE_INFO, _T("http://blog.kowalczyk.info/software/sumatrapdf/news.html"));

    // TODO: stop writing this around version 1.8 and instead use INSTALL_LOCATION above
    success &= WriteRegStr(hkey,   REG_PATH_SOFTWARE, INSTALL_DIR, installDir);

    return success;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
bool WriteExtendedFileExtensionInfo(HKEY hkey)
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
        ScopedMem<TCHAR> keyname(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") TAPP));
        success &= CreateRegKey(hkey, keyname);
    }

    // in case these values don't exist yet (we won't delete these at uninstallation)
    success &= WriteRegStr(hkey, REG_CLASSES_PDF, _T("Content Type"), _T("application/pdf"));
    success &= WriteRegStr(hkey, _T("Software\\Classes\\MIME\\Database\\Content Type\\application/pdf"), _T("Extension"), _T(".pdf"));

    return success;
}

BOOL IsUninstallerNeeded()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    return file::Exists(exePath);
}

bool RemoveUninstallerRegistryInfo(HKEY hkey)
{
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    bool ok2 = DeleteRegKey(hkey, REG_PATH_SOFTWARE);
    return ok1 && ok2;
}

/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
void UnregisterFromBeingDefaultViewer(HKEY hkey)
{
    ScopedMem<TCHAR> curr(ReadRegStr(hkey, REG_CLASSES_PDF, NULL));
    ScopedMem<TCHAR> prev(ReadRegStr(hkey, REG_CLASSES_APP, _T("previous.pdf")));
    if (!curr || !str::Eq(curr, REG_CLASSES_APP)) {
        // not the default, do nothing
    } else if (prev) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, prev);
    } else {
        prev.Set(ReadRegStr(hkey, REG_CLASSES_PDF, NULL));
        if (str::Eq(TAPP, prev))
            DeleteRegKey(hkey, REG_CLASSES_PDF);
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID));
    if (str::Eq(buf, TAPP)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS)
            SeeLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), PROG_ID));
    if (str::Eq(buf, TAPP))
        DeleteRegKey(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), true);
}

void RemoveOwnRegistryKeys()
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
            keyname.Set(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") TAPP));
            DeleteRegKey(keys[i], keyname);
        }
    }
}

bool IsBrowserPluginInstalled()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_PLUGIN, PLUGIN_PATH));
    if (!buf)
        buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_PLUGIN, PLUGIN_PATH));
    return file::Exists(buf);
}

bool IsPdfFilterInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\PersistentHandler"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

bool IsPdfPreviewerInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
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
    RegisterServerDLL(dllPath, true);
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
    RegisterServerDLL(dllPath, true);
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
    RegisterServerDLL(dllPath, true);
}

/* Caller needs to free() the result. */
TCHAR *GetDefaultPdfViewer()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL);
}

BOOL RemoveEmptyDirectory(TCHAR *dir)
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

BOOL RemoveInstalledFiles()
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

bool CreateAppShortcut(bool allUsers)
{
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<TCHAR> shortcutPath(GetShortcutPath(allUsers));
    return CreateShortcut(shortcutPath, installedExePath);
}

bool RemoveShortcut(bool allUsers)
{
    ScopedMem<TCHAR> p(GetShortcutPath(allUsers));
    bool ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        SeeLastError();
        return false;
    }
    return true;
}

bool CreateInstallationDirectory()
{
    bool ok = CreateDirectory(gGlobalData.installDir, NULL);
    if (!ok && (GetLastError() != ERROR_ALREADY_EXISTS)) {
        SeeLastError();
        NotifyFailed(_T("Couldn't create installation directory"));
        return false;
    }
    return true;
}

void CreateButtonExit(HWND hwndParent)
{
    int     buttonDx = 80;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    ClientRect r(hwndParent);
    int x = r.dx - buttonDx - 8;
    int y = r.dy - buttonDy - 8;
    gHwndButtonExit = CreateWindow(WC_BUTTON, _T("Close"),
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        x, y, buttonDx, buttonDy, hwndParent, 
                        (HMENU)ID_BUTTON_EXIT,
                        ghinst, NULL);
    SetWindowFont(gHwndButtonExit, gFontDefault, TRUE);
}

void CreateButtonRunSumatra(HWND hwndParent)
{
    int     buttonDx = 120;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    ClientRect r(hwndParent);
    int x = r.dx - buttonDx - 8;
    int y = r.dy - buttonDy - 8;
    gHwndButtonRunSumatra= CreateWindow(WC_BUTTON, _T("Start ") TAPP,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        x, y, buttonDx, buttonDy, hwndParent, 
                        (HMENU)ID_BUTTON_START_SUMATRA,
                        ghinst, NULL);
    SetWindowFont(gHwndButtonRunSumatra, gFontDefault, TRUE);
}

static DWORD WINAPI InstallerThread(LPVOID data)
{
    gGlobalData.success = false;

    /* if the app is running, we have to kill it so that we can over-write the executable */
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    KillProcess(exePath, TRUE);

    if (!CreateInstallationDirectory())
        goto Error;
    ProgressStep();

    if (!InstallCopyFiles())
        goto Error;

    if (gGlobalData.registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
        HANDLE h = CreateProcessHelper(installedExePath, _T("-register-for-pdf"));
        CloseHandle(h);
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

    if (!CreateUninstaller())
        goto Error;

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

void OnButtonOptions();

bool IsCheckboxChecked(HWND hwnd)
{
    return (Button_GetState(hwnd) & BST_CHECKED) == BST_CHECKED;
}

void OnButtonInstall()
{
    TCHAR *userInstallDir = win::GetText(gHwndTextboxInstDir);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gGlobalData.installDir, userInstallDir);
    free(userInstallDir);

    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gGlobalData.registerAsDefault = gHwndCheckboxRegisterDefault == NULL ||
                                    IsCheckboxChecked(gHwndCheckboxRegisterDefault);

    gGlobalData.installBrowserPlugin = IsCheckboxChecked(gHwndCheckboxRegisterBrowserPlugin);
    gGlobalData.installPdfFilter = IsCheckboxChecked(gHwndCheckboxRegisterPdfFilter);
    // note: this checkbox isn't created on Windows 2000 and XP
    gGlobalData.installPdfPreviewer = gHwndCheckboxRegisterPdfPreviewer != NULL &&
                                      IsCheckboxChecked(gHwndCheckboxRegisterPdfPreviewer);

    if (gShowOptions)
        OnButtonOptions();

    // create a progress bar in place of the Options button
    RectI rc(0, 0, INSTALLER_WIN_DX / 2, 22);
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

    EnableWindow(gHwndButtonInstall, FALSE);

    gMsg = _T("Installation in progress...");
    gMsgColor = COLOR_MSG_INSTALLATION;
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, InstallerThread, NULL, 0, 0);
}

void OnInstallationFinished()
{
    DestroyWindow(gHwndButtonInstall);
    gHwndButtonInstall = NULL;
    DestroyWindow(gHwndProgressBar);
    gHwndProgressBar = NULL;

    if (gGlobalData.success) {
        CreateButtonRunSumatra(gHwndFrame);
        gMsg = _T("Thank you! ") TAPP _T(" has been installed.");
        gMsgColor = COLOR_MSG_OK;
    } else {
        CreateButtonExit(gHwndFrame);
        gMsg = _T("Installation failed!");
        gMsgColor = COLOR_MSG_FAILED;
    }
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

static DWORD WINAPI UninstallerThread(LPVOID data)
{
    /* if the app is running, we have to kill it to delete the files */
    TCHAR *exePath = GetInstalledExePath();
    KillProcess(exePath, TRUE);
    free(exePath);

    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    exePath = GetUninstallerPath();
    if (!path::IsSame(exePath, GetOwnPath()))
        KillProcess(exePath, TRUE);
    free(exePath);

    if (!RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) ||
        !RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to delete uninstaller registry keys"));
    }

    if (!RemoveShortcut(true) || !RemoveShortcut(false))
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

void OnButtonUninstall()
{
    // disable the button during uninstallation
    EnableWindow(gHwndButtonUninstall, FALSE);
    gMsg = _T("Uninstallation in progress...");
    gMsgColor = COLOR_MSG_INSTALLATION;
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, UninstallerThread, NULL, 0, 0);
}

void OnUninstallationFinished()
{
    DestroyWindow(gHwndButtonUninstall);
    gHwndButtonUninstall = NULL;
    CreateButtonExit(gHwndFrame);
    gMsg = TAPP _T(" has been uninstalled.");
    gMsgError = gGlobalData.firstError;
    if (!gMsgError)
        gMsgColor = COLOR_MSG_OK;
    else
        gMsgColor = COLOR_MSG_FAILED;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

void OnButtonExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
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

void OnButtonStartSumatra()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    // try to create the process as a normal user
    HANDLE h = CreateProcessAtLevel(exePath);
    // create the process as is (mainly for Windows 2000 compatibility)
    if (!h)
        h = CreateProcessHelper(exePath);
    CloseHandle(h);

    OnButtonExit();
}

void OnButtonOptions()
{
    gShowOptions = !gShowOptions;

    int nCmdShow = gShowOptions ? SW_SHOW : SW_HIDE;
    ShowWindow(gHwndStaticInstDir, nCmdShow);
    ShowWindow(gHwndTextboxInstDir, nCmdShow);
    ShowWindow(gHwndButtonBrowseDir, nCmdShow);
    ShowWindow(gHwndCheckboxRegisterDefault, nCmdShow);
    ShowWindow(gHwndCheckboxRegisterBrowserPlugin, nCmdShow);
    ShowWindow(gHwndCheckboxRegisterPdfFilter, nCmdShow);
    ShowWindow(gHwndCheckboxRegisterPdfPreviewer, nCmdShow);

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

BOOL BrowseForFolder(HWND hwnd, LPCTSTR lpszInitialFolder, LPCTSTR lpszCaption, LPTSTR lpszBuf, DWORD dwBufSize)
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

void OnButtonBrowse()
{
    TCHAR *installDir = win::GetText(gHwndTextboxInstDir);
    if (!dir::Exists(installDir)) {
        TCHAR *parentDir = path::GetDir(installDir);
        free(installDir);
        installDir = parentDir;
    }

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

    free(installDir);
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

char RandUppercaseLetter()
{
    // TODO: clearly, not random but seem to work ok anyway
    static char l = 'A' - 1;
    l++;
    if (l > 'Z')
        l = 'A';
    return l;
}

void RandomizeLetters()
{
    for (int i=0; i<dimof(gLetters); i++) {
        gLetters[i].c = RandUppercaseLetter();
    }
}

void SetLettersSumatraUpTo(int n)
{
    char *s = "SUMATRAPDF";
    for (int i=0; i<dimof(gLetters); i++) {
        if (i < n) {
            gLetters[i].c = s[i];
        } else {
            gLetters[i].c = ' ';
        }
    }
}

void SetLettersSumatra()
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

// an animation that 'rotates' random letters 
static FrameTimeoutCalculator *gRotatingLettersAnim = NULL;

void RotatingLettersAnimStart()
{
    gRotatingLettersAnim = new FrameTimeoutCalculator(20);
}

void RotatingLettersAnimStop()
{
    delete gRotatingLettersAnim;
    gRotatingLettersAnim = NULL;
    SetLettersSumatra();
    InvalidateFrame();
}

void RotatingLettersAnim()
{
    if (gRotatingLettersAnim->ElapsedTotal() > 3) {
        RotatingLettersAnimStop();
        return;
    }
    DWORD timeOut = gRotatingLettersAnim->GetTimeoutInMilliseconds();
    if (timeOut != 0)
        return;
    RandomizeLetters();
    InvalidateFrame();
    gRotatingLettersAnim->Step();
}

// an animation that reveals letters one by one

// how long the animation lasts, in seconds
#define REVEALING_ANIM_DUR double(2)

static FrameTimeoutCalculator *gRevealingLettersAnim = NULL;

int gRevealingLettersAnimLettersToShow;

void RevealingLettersAnimStart()
{
    int framesPerSec = (int)(double(SUMATRA_LETTERS_COUNT) / REVEALING_ANIM_DUR);
    gRevealingLettersAnim = new FrameTimeoutCalculator(framesPerSec);
    gRevealingLettersAnimLettersToShow = 0;
    SetLettersSumatraUpTo(0);
}

void RevealingLettersAnimStop()
{
    delete gRevealingLettersAnim;
    gRevealingLettersAnim = NULL;
    SetLettersSumatra();
    InvalidateFrame();
}

void RevealingLettersAnim()
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

void AnimStep() {
    if (gRotatingLettersAnim) {
        RotatingLettersAnim();
    }
    if (gRevealingLettersAnim) {
        RevealingLettersAnim();
    }
}

void CalcLettersLayout(Graphics& g, Font *f, int dx)
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
    for (int i=0; i<dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        g.MeasureString(s, 1, f, origin, &sfmt, &bbox);
        li->dx = bbox.Width;
        li->dy = bbox.Height;
        totalDx += li->dx;
        totalDx += letterSpacing;
    }

    REAL x = ((REAL)dx - totalDx) / 2.f;
    for (int i=0; i<dimof(gLetters); i++) {
        li = &gLetters[i];
        li->x = x;
        x += li->dx;
        x += letterSpacing;
    }
    //RotatingLettersAnimStart();
    RevealingLettersAnimStart();
    didLayout = TRUE;
}

REAL DrawMessage(Graphics &g, TCHAR *msg, REAL y, REAL dx, Color color)
{
    ScopedMem<WCHAR> s(str::conv::ToWStr(msg));

    Font f(L"Impact", 16, FontStyleRegular);
    Gdiplus::RectF maxbox(0, y, dx, 0);
    Gdiplus::RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
#ifdef DRAW_TEXT_SHADOW
    {
        bbox.X--; bbox.Y++;
        SolidBrush b(Color(255,255,255));
        g.DrawString(s, -1, &f, bbox, &sft, &b);
        bbox.X++; bbox.Y--;
    }
#endif
    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
}

void DrawSumatraLetters(Graphics &g, Font *f, Font *fVer, REAL y)
{
    LetterInfo *li;
    WCHAR s[2] = { 0 };
    for (int i=0; i<dimof(gLetters); i++) {
        li = &gLetters[i];
        s[0] = li->c;
        if (s[0] == ' ')
            return;

        g.RotateTransform(li->rotation, MatrixOrderAppend);
#ifdef DRAW_TEXT_SHADOW
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
#ifdef DRAW_TEXT_SHADOW
    SolidBrush b1(Color(0,0,0));
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2-2,y2-1), &b1);
#endif
    SolidBrush b2(Color(255,255,255));
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2,y2), &b2);
    g.ResetTransform();
}

void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Font f(L"Impact", 40, FontStyleRegular);
    CalcLettersLayout(g, &f, r.dx);

    SolidBrush bgBrush(Color(255,242,0));
    Gdiplus::Rect r2(r.y-1, r.x-1, r.dx+1, r.dy+1);
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

void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
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

void OnPaintFrame(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}

void OnMouseMove(HWND hwnd, int x, int y)
{
    // nothing so far
}

void OnCreateUninstaller(HWND hwnd)
{
    int     buttonDx = 140;
    int     buttonDy = 22;

    ClientRect r(hwnd);
    int x = r.dx - buttonDx - 8;
    int y = r.dy - buttonDy - 8;
    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    gHwndButtonUninstall = CreateWindow(WC_BUTTON, _T("Uninstall ") TAPP,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        x, y, buttonDx, buttonDy, hwnd,
                        (HMENU)ID_BUTTON_UNINSTALL, ghinst, NULL);
    SetWindowFont(gHwndButtonUninstall, gFontDefault, TRUE);
}

static LRESULT CALLBACK UninstallerWndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            if (!IsUninstallerNeeded()) {
                MessageBox(NULL, _T("No installation has been found. Please install ") TAPP _T(" first before uninstalling it..."), _T("Uninstallation failed"),  MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
            OnCreateUninstaller(hwnd);
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
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case IDOK:
                    if (gHwndButtonUninstall)
                        OnButtonUninstall();
                    else if (gHwndButtonExit)
                        OnButtonExit();
                    break;

                case ID_BUTTON_UNINSTALL:
                    OnButtonUninstall();
                    break;

                case ID_BUTTON_EXIT:
                case IDCANCEL:
                    OnButtonExit();
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnUninstallationFinished();
            SetFocus(gHwndButtonExit);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void OnCreateInstaller(HWND hwnd)
{
    int     buttonDx = 128;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    ClientRect r(hwnd);
    int x = r.dx - buttonDx - 8;
    int y = r.dy - buttonDy - 8;
    gHwndButtonInstall = CreateWindow(WC_BUTTON, _T("Install ") TAPP,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        x, y, buttonDx, buttonDy, hwnd, 
                        (HMENU)ID_BUTTON_INSTALL, ghinst, NULL);
    SetWindowFont(gHwndButtonInstall, gFontDefault, TRUE);

    x = 8;
    gHwndButtonOptions = CreateWindow(WC_BUTTON, _T("&Options"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        x, y, 96, buttonDy, hwnd, 
                        (HMENU)ID_BUTTON_OPTIONS, ghinst, NULL);
    SetWindowFont(gHwndButtonOptions, gFontDefault, TRUE);

    y = TITLE_PART_DY + x;
    gHwndStaticInstDir = CreateWindow(WC_STATIC, _T("Install ") TAPP _T(" into the following &folder:"),
                                      WS_CHILD,
                                      x, y, r.dx - 2 * x, 20, hwnd, 0, ghinst, NULL);
    SetWindowFont(gHwndStaticInstDir, gFontDefault, TRUE);
    y += 20;

    gHwndTextboxInstDir = CreateWindow(WC_EDIT, gGlobalData.installDir,
                                       WS_CHILD | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                                       x, y, r.dx - 3 * x - 20, 20, hwnd, 0, ghinst, NULL);
    SetWindowFont(gHwndTextboxInstDir, gFontDefault, TRUE);
    gHwndButtonBrowseDir = CreateWindow(WC_BUTTON, _T("&..."),
                                        BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP,
                                        r.dx - x - 20, y, 20, 20, hwnd, (HMENU)ID_BUTTON_BROWSE, ghinst, NULL);
    SetWindowFont(gHwndButtonBrowseDir, gFontDefault, TRUE);
    y += 40;

    ScopedMem<TCHAR> defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, TAPP);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        gHwndCheckboxRegisterDefault = CreateWindow(
            WC_BUTTON, _T("Use ") TAPP _T(" as the &default PDF reader"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, y, r.dx - 2 * x, 22, hwnd, (HMENU)ID_CHECKBOX_MAKE_DEFAULT, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterDefault, gFontDefault, TRUE);
        // only check the "Use as default" checkbox when no other PDF viewer
        // is currently selected (not going to intrude)
        Button_SetCheck(gHwndCheckboxRegisterDefault, !hasOtherViewer || gGlobalData.registerAsDefault);
        y += 22;
    }

    gHwndCheckboxRegisterBrowserPlugin = CreateWindow(
        WC_BUTTON, _T("Install PDF &browser plugin for Firefox, Chrome and Opera"),
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, r.dx - 2 * x, 22, hwnd, (HMENU)ID_CHECKBOX_BROWSER_PLUGIN, ghinst, NULL);
    SetWindowFont(gHwndCheckboxRegisterBrowserPlugin, gFontDefault, TRUE);
    Button_SetCheck(gHwndCheckboxRegisterBrowserPlugin, gGlobalData.installBrowserPlugin || IsBrowserPluginInstalled());
    y += 22;

    gHwndCheckboxRegisterPdfFilter = CreateWindow(
        WC_BUTTON, _T("Let Windows Desktop Search &search PDF documents"),
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, r.dx - 2 * x, 22, hwnd, (HMENU)ID_CHECKBOX_PDF_FILTER, ghinst, NULL);
    SetWindowFont(gHwndCheckboxRegisterPdfFilter, gFontDefault, TRUE);
    Button_SetCheck(gHwndCheckboxRegisterPdfFilter, gGlobalData.installPdfFilter || IsPdfFilterInstalled());
    y += 22;

    if (WindowsVerVistaOrGreater()) {
        gHwndCheckboxRegisterPdfPreviewer = CreateWindow(
            WC_BUTTON, _T("Let Windows show &previews of PDF documents"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, y, r.dx - 2 * x, 22, hwnd, (HMENU)ID_CHECKBOX_PDF_PREVIEWER, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterPdfPreviewer, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfPreviewer, gGlobalData.installPdfPreviewer || IsPdfPreviewerInstalled());
        y += 22;
    }

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    SetFocus(gHwndButtonInstall);
}

static LRESULT CALLBACK InstallerWndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            if (!IsValidInstaller()) {
                MessageBox(NULL, _T("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"), _T("Installation failed"),  MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
            OnCreateInstaller(hwnd);
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
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case IDOK:
                    if (gHwndButtonInstall)
                        OnButtonInstall();
                    else if (gHwndButtonRunSumatra)
                        OnButtonStartSumatra();
                    else if (gHwndButtonExit)
                        OnButtonExit();
                    break;

                case ID_BUTTON_INSTALL:
                    OnButtonInstall();
                    break;

                case ID_BUTTON_START_SUMATRA:
                    OnButtonStartSumatra();
                    break;

                case ID_BUTTON_EXIT:
                case IDCANCEL:
                    OnButtonExit();
                    break;

                case ID_BUTTON_OPTIONS:
                    OnButtonOptions();
                    break;

                case ID_BUTTON_BROWSE:
                    OnButtonBrowse();
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnInstallationFinished();
            if (gHwndButtonRunSumatra)
                SetFocus(gHwndButtonRunSumatra);
            else if (gHwndButtonExit)
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
    ATOM        atom;

    FillWndClassEx(wcex, hInstance);
    wcex.lpszClassName  = INSTALLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));

    if (gGlobalData.uninstall)
        wcex.lpfnWndProc    = UninstallerWndProcFrame;
    else
        wcex.lpfnWndProc    = InstallerWndProcFrame;

    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    return TRUE;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;
    
    gFontDefault = CreateDefaultGuiFont();

    if (gGlobalData.uninstall) {
        gHwndFrame = CreateWindow(
                INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Uninstaller"),
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT, 
                UNINSTALLER_WIN_DX, UNINSTALLER_WIN_DY,
                NULL, NULL,
                ghinst, NULL);
        gMsg = _T("Are you sure that you want to uninstall ") TAPP _T("?");
        gMsgColor = COLOR_MSG_WELCOME;
    } else {
        gHwndFrame = CreateWindow(
                INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Installer"),
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT,                
                INSTALLER_WIN_DX, INSTALLER_WIN_DY,
                NULL, NULL,
                ghinst, NULL);
        gMsg = _T("Thank you for downloading ") TAPP _T("!");
        gMsgColor = COLOR_MSG_WELCOME;
    }
    if (!gHwndFrame)
        return FALSE;

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// If this is uninstaller and we're running from installation directory,
// copy uninstaller to temp directory and execute from there, exiting
// ourselves. This is needed so that uninstaller can delete itself
// from installation directory and remove installation directory
// If returns TRUE, this is an installer and we sublaunched ourselves,
// so the caller needs to exit
bool ExecuteUninstallerFromTempDir()
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
    HANDLE h = CreateProcessHelper(tempPath, args);
    CloseHandle(h);

    // mark the uninstaller for removal at shutdown (note: works only for administrators)
    MoveFileEx(tempPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    return h != NULL;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
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
                return msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}

void ShowUsage()
{
    MessageBox(NULL, TAPP _T("-install.exe [/s][/d <path>][/default][/opt plugin,...]\n\
    \n\
    /s\tinstalls ") TAPP _T(" silently (without user interaction).\n\
    /d\tchanges the directory where ") TAPP _T(" will be installed.\n\
    /default\tinstalls ") TAPP _T(" as the default PDF viewer.\n\
    /opt\tenables optional components (currently: plugin, pdffilter, pdfpreviewer)."), TAPP _T(" Installer Usage"), MB_OK | MB_ICONINFORMATION);
}

void ParseCommandLine(TCHAR *cmdLine)
{
    CmdLineParser argList(cmdLine);

#define is_arg(param) str::EqI(arg + 1, _T(param))
#define is_arg_with_param(param) (is_arg(param) && i < argList.Count() - 1)

    // skip the first arg (exe path)
    for (size_t i = 1; i < argList.Count(); i++) {
        TCHAR *arg = argList[i];
        if ('-' != *arg && '/' != *arg)
            continue;

        if (is_arg("s"))
            gGlobalData.silent = true;
        else if (is_arg_with_param("d")) {
            free(gGlobalData.installDir);
            gGlobalData.installDir = str::Dup(argList[++i]);
        }
        else if (is_arg("register"))
            gGlobalData.registerAsDefault = true;
        else if (is_arg_with_param("opt")) {
            TCHAR *opts = argList[++i];
            str::ToLower(opts);
            str::TransChars(opts, _T(" "), _T(","));
            StrVec optlist;
            optlist.Split(opts, _T(","), true);
            if (optlist.Find(_T("plugin")) != -1)
                gGlobalData.installBrowserPlugin = true;
            if (optlist.Find(_T("pdffilter")) != -1)
                gGlobalData.installPdfFilter = true;
            if (optlist.Find(_T("pdfpreviewer")) != -1)
                gGlobalData.installPdfPreviewer = true;
        }
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
#ifdef TEST_UNINSTALLER
    gGlobalData.uninstall = true;
#endif
    if (!gGlobalData.uninstall)
        gGlobalData.uninstall = IsUninstaller();
    if (!gGlobalData.installDir)
        gGlobalData.installDir = GetInstallationDir(gGlobalData.uninstall);

#ifndef TEST_UNINSTALLER
    if (gGlobalData.uninstall && ExecuteUninstallerFromTempDir())
        return 0;
#endif

    if (gGlobalData.silent) {
        if (gGlobalData.uninstall) {
            UninstallerThread(NULL);
        } else {
            // make sure not to uninstall the plugins during silent installation
            if (!gGlobalData.installBrowserPlugin)
                gGlobalData.installBrowserPlugin = IsBrowserPluginInstalled();
            if (!gGlobalData.installPdfFilter)
                gGlobalData.installPdfFilter = IsPdfFilterInstalled();
            if (!gGlobalData.installPdfPreviewer)
                gGlobalData.installPdfPreviewer = IsPdfPreviewerInstalled();
            InstallerThread(NULL);
        }
        ret = gGlobalData.success ? 0 : 1;
        goto Exit;
    }

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

Exit:
    free(gUnInstMark);
    free(gGlobalData.installDir);
    free(gGlobalData.firstError);
    CoUninitialize();

    return ret;
}
