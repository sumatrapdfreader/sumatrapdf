/* TODO: those should be set from the makefile */
// Modify the following defines if you have to target a platform prior to the ones specified below.
// Their meaning: http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx
// and http://blogs.msdn.com/oldnewthing/archive/2007/04/11/2079137.aspx
// We set the features uniformly to Win 2000 or later.
#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT 
#define _WIN32_WINNT 0x0500
// the following is only defined for _WIN32_WINNT >= 0x0600
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0500
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <psapi.h>
#include <Shlwapi.h>
#include <objidl.h>

#include "zlib.h"

#include "Resource.h"
#include "base_util.h"
#include "str_util.h"
#include "tstr_util.h"
#include "win_util.h"
#include "WinUtil.hpp"
#include "../Version.h"

#ifdef DEBUG
// debug builds use a manifest created by the linker instead of our own, so ensure visual styles this way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

using namespace Gdiplus;

// set to 1 when testing as uninstaller
#define FORCE_TO_BE_UNINSTALLER 0

#define INSTALLER_FRAME_CLASS_NAME    _T("SUMATRA_PDF_INSTALLER_FRAME")
#define UNINSTALLER_FRAME_CLASS_NAME  _T("SUMATRA_PDF_UNINSTALLER_FRAME")

#define ID_BUTTON_INSTALL             1
#define ID_BUTTON_UNINSTALL           2
#define ID_CHECKBOX_MAKE_DEAFULT      3
#define ABOUT_BG_COLOR                RGB(255,242,0)
#define INVALID_SIZE                  DWORD(-1)

static HINSTANCE        ghinst;
static HWND             gHwndFrame;
static HWND             gHwndButtonInstall;
static HWND             gHwndCheckboxRegisterDefault;
static HWND             gHwndButtonUninstall;
static HFONT            gFontDefault;

static ULONG_PTR        gGdiplusToken;

int gBallX, gBallY;

#define APP                 "SumatraPDF"
#define TAPP                _T("SumatraPDF")
#define EXENAME             "SumatraPDF.exe"

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
#define REG_PATH_UNINST     _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") TAPP
#define REG_PATH_SOFTWARE   _T("Software\\") TAPP

// Keys we'll set in REG_PATH_UNINST path

// REG_SZ, a path to installed executable (or "$path,0" to force the first icon)
#define DISPLAY_ICON _T("DisplayIcon")
// REG_SZ, e.g "SumatraPDF"
#define DISPLAY_NAME _T("DisplayName")
// REG_SZ, e.g. "1.2"
#define DISPLAY_VERSION _T("DisplayVersion")
// REG_DWORD, get size of installed directory after copying files
#define ESTIMATED_SIZE _T("EstimatedSize")
// REG_DWORD, set to 1
#define NO_MODIFY _T("NoModify")
// REG_DWORD, set to 1
#define NO_REPAIR _T("NoRepair")
// REG_SZ, e.g. "Krzysztof Kowalczyk"
#define PUBLISHER _T("Publisher")
// REG_SZ, path to uninstaller exe
#define UNINSTALL_STRING _T("UninstallString")
// REG_SZ, e.g. "http://blog.kowalczyk/info/software/sumatrapdf/"
#define URL_INFO_ABOUT _T("UrlInfoAbout")

#define INSTALLER_PART_FILE         "kifi"
#define INSTALLER_PART_FILE_ZLIB    "kifz"
#define INSTALLER_PART_END          "kien"
#define INSTALLER_PART_UNINSTALLER  "kiun"

struct EmbeddedPart {
    EmbeddedPart *  next;
    char            type[5];     // we only use 4, 5th is for 0-termination
    // fields valid if type is INSTALLER_PART_FILE or INSTALLER_PART_FILE_ZLIB
    uint32_t        fileSize;    // size of the file
    uint32_t        fileOffset;  // offset in the executable of the file start
    char *          fileName;    // name of the file
};

static EmbeddedPart *   gEmbeddedParts;

void FreeEmbeddedParts(EmbeddedPart *root)
{
    EmbeddedPart *p = root;

    while (p) {
        EmbeddedPart *next = p->next;
        free(p->fileName);
        free(p);
        p = next;
    }
}

void ShowLastError(char *msg)
{
    char *msgBuf, *errorMsg;
    if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPSTR)&msgBuf, 0, NULL)) {
        errorMsg = str_printf("%s\n\n%s", msg, msgBuf);
        LocalFree(msgBuf);
    } else {
        errorMsg = str_printf("%s\n\nError %d", msg, (int)GetLastError());
    }
    MessageBoxA(gHwndFrame, errorMsg, "Installer failed", MB_OK | MB_ICONEXCLAMATION);
    free(errorMsg);
}

void NotifyFailed(char *msg)
{
    MessageBoxA(gHwndFrame, msg, "Installer failed",  MB_ICONINFORMATION | MB_OK);
}

BOOL ReadData(HANDLE h, LPVOID data, DWORD size, char *errMsg)
{
    DWORD bytesRead;
    BOOL ok = ReadFile(h, data, size, &bytesRead, NULL);
    char *msg;
    if (!ok || (bytesRead != size)) {        
        if (!ok) {
            msg = str_printf("%s: ok=%d", errMsg, ok);
        } else {
            msg = str_printf("%s: bytesRead=%d, wanted=%d", errMsg, (int)bytesRead, (int)size);
        }
        ShowLastError(msg);
        return FALSE;
    }
    return TRUE;        
}

#define SEEK_FAILED INVALID_SET_FILE_POINTER

DWORD SeekBackwards(HANDLE h, LONG distance, char *errMsg)
{
    DWORD res = SetFilePointer(h, -distance, NULL, FILE_CURRENT);
    if (INVALID_SET_FILE_POINTER == res) {
        ShowLastError(errMsg);
    }
    return res;
}

DWORD GetFilePos(HANDLE h)
{
    return SeekBackwards(h, 0, "");
}

#define TEN_SECONDS_IN_MS 10*1000

// Kill a process with given <processId> if it's named <processName>.
// If <waitUntilTerminated> is TRUE, will wait until process is fully killed.
// Returns TRUE if killed a process
BOOL KillProcIdWithName(DWORD processId, char *processName, BOOL waitUntilTerminated)
{
    HANDLE      hProcess = NULL;
    char        currentProcessName[1024];
    HMODULE     modulesArray[1024];
    DWORD       modulesCount;
    BOOL        killed = FALSE;

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess)
        return FALSE;

    BOOL ok = EnumProcessModules(hProcess, modulesArray, sizeof(HMODULE)*1024, &modulesCount);
    if (!ok)
        goto Exit;

    if (0 == GetModuleBaseNameA(hProcess, modulesArray[0], currentProcessName, 1024))
        goto Exit;

    if (!str_ieq(currentProcessName, processName))
        goto Exit;

    killed = TerminateProcess(hProcess, 0);
    if (!killed)
        goto Exit;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    UpdateWindow(FindWindowA(NULL, "Shell_TrayWnd"));    
    UpdateWindow(GetDesktopWindow());

Exit:
    CloseHandle(hProcess);
    return killed;
}

#define MAX_PROCESSES 1024

static int KillProcess(char *processName, BOOL waitUntilTerminated)
{
    DWORD  pidsArray[MAX_PROCESSES];
    DWORD  cbPidsArraySize;
    int    killedCount = 0;

    if (!EnumProcesses(pidsArray, MAX_PROCESSES, &cbPidsArraySize))
        return FALSE;

    int processesCount = cbPidsArraySize / sizeof(DWORD);

    for (int i = 0; i < processesCount; i++)
    {
        if (KillProcIdWithName(pidsArray[i], processName, waitUntilTerminated)) 
            killedCount++;
    }

    return killedCount;
}

TCHAR *GetExePath()
{
    static TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, dimof(exePath));
    return exePath;
}

TCHAR *GetInstallationDir()
{
    static TCHAR dir[MAX_PATH];
    BOOL ok = SHGetSpecialFolderPath(NULL, dir, CSIDL_PROGRAM_FILES, FALSE);
    if (!ok)
        return NULL;
    tstr_cat_s(dir, dimof(dir), _T("\\"));
    tstr_cat_s(dir, dimof(dir), TAPP);
    return dir;    
}

TCHAR *GetUninstallerPath()
{
    TCHAR *installDir = GetInstallationDir();
    return tstr_cat3(installDir, _T("\\"), _T("uninstall.exe"));
}

TCHAR *GetInstalledExePath()
{
    TCHAR *installDir = GetInstallationDir();
    return tstr_cat(installDir, _T("\\SumatraPDF.exe"));
}

TCHAR *GetStartMenuProgramsPath()
{
    static TCHAR dir[MAX_PATH];
    // CSIDL_COMMON_PROGRAMS => installing for all users
    BOOL ok = SHGetSpecialFolderPath(NULL, dir, CSIDL_COMMON_PROGRAMS, FALSE);
    if (!ok)
        return NULL;
    return dir;
}

TCHAR *GetShortcutPath()
{
    return tstr_cat(GetStartMenuProgramsPath(), _T("\\SumatraPDF.lnk"));
}

DWORD GetFilePointer(HANDLE h)
{
    return SetFilePointer(h, 0, NULL, FILE_CURRENT);
}


/* Load information about parts embedded in the installer.
   The format of the data is:

   For a part that is a file:
     $fileData      - blob
     $fileDataLen   - length of $data, 32-bit unsigned integer, little-endian
     $fileName      - ascii string, name of the file (without terminating zero!)
     $fileNameLen   - length of $fileName, 32-bit unsigned integer, little-endian
     'kifi'         - 4 byte unique header

   For a part that signifies end of parts:
     'kien'         - 4 byte unique header

   Data is laid out so that it can be read sequentially from the end, because
   it's easier for the installer to seek to the end of itself than parse
   PE header to figure out where the data starts. */
EmbeddedPart *GetEmbeddedPartsInfo() {
    EmbeddedPart *  root = NULL;
    EmbeddedPart *  part;
    DWORD           res;
    char *           msg;

    if (gEmbeddedParts)
        return gEmbeddedParts;

    TCHAR *exePath = GetExePath();
    HANDLE h = CreateFile(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        NotifyFailed("Couldn't open myself for reading");
        goto Error;
    }

    // position at the header of the last part
    res = SetFilePointer(h, -4, NULL, FILE_END);
    if (INVALID_SET_FILE_POINTER == res) {
        NotifyFailed("Couldn't seek to end");
        goto Error;
    }

ReadNextPart:
    part = SAZ(EmbeddedPart);
    part->next = root;
    root = part;

    res = GetFilePos(h);
#if 0
    msg = str_printf("Curr pos: %d", (int)res);
    MessageBoxA(gHwndFrame, msg, "Info", MB_ICONINFORMATION | MB_OK);
    free(msg);
#endif

    // at this point we have to be positioned in the file at the beginning of the header
    if (!ReadData(h, (LPVOID)part->type, 4, "Couldn't read the header"))
        goto Error;

    if (str_eqn(part->type, INSTALLER_PART_END, 4)) {
        part->fileSize = GetFilePointer(h);
        goto Exit;
    }

    if (str_eqn(part->type, INSTALLER_PART_UNINSTALLER, 4)) {
        goto Exit;
    }

    if (str_eqn(part->type, INSTALLER_PART_FILE, 4) ||
        str_eqn(part->type, INSTALLER_PART_FILE_ZLIB, 4)) {
        uint32_t nameLen;
        if (SEEK_FAILED == SeekBackwards(h, 8, "Couldn't seek to file name size"))
            goto Error;

        if (!ReadData(h, (LPVOID)&nameLen, 4, "Couldn't read file name size"))
            goto Error;
        if (SEEK_FAILED == SeekBackwards(h, 4 + nameLen, "Couldn't seek to file name"))
            goto Error;

        part->fileName = (char*)zmalloc(nameLen+1);
        if (!ReadData(h, (LPVOID)part->fileName, nameLen, "Couldn't read file name"))
            goto Error;
        if (SEEK_FAILED == SeekBackwards(h, 4 + nameLen, "Couldn't seek to file size"))
            goto Error;

        if (!ReadData(h, (LPVOID)&part->fileSize, 4, "Couldn't read file size"))
            goto Error;
        res = SeekBackwards(h, 4 + part->fileSize + 4,  "Couldn't seek to header");
        if (SEEK_FAILED == res)
            goto Error;

        part->fileOffset = res + 4;
#if 0
        msg = str_printf("Found file '%s' of size %d at offset %d", part->fileName, part->fileSize, part->fileOffset);
        MessageBoxA(gHwndFrame, msg, "Installer", MB_ICONINFORMATION | MB_OK);
        free(msg);
#endif
        goto ReadNextPart;
    }

    msg = str_printf("Unknown part: %s", part->type);
    NotifyFailed(msg);
    free(msg);
    goto Error;

Exit:
    CloseHandle(h);
    gEmbeddedParts = root;
    return root;
Error:
    FreeEmbeddedParts(root);
    root = NULL;
    goto Exit;
}

BOOL CopyFileData(HANDLE hSrc, HANDLE hDst, DWORD size)
{
    BOOL    ok;
    DWORD   bytesTransferred;
    char    buf[1024*8];
    DWORD   left = size;

    while (0 != left) {
        DWORD toRead = dimof(buf);
        if (toRead > left)
            toRead = left;

        ok = ReadFile(hSrc, (LPVOID)buf, toRead, &bytesTransferred, NULL);
        if (!ok || (toRead != bytesTransferred)) {
            NotifyFailed("Failed to read from file part");
            goto Error;
        }

        ok = WriteFile(hDst, (LPVOID)buf, toRead, &bytesTransferred, NULL);
        if (!ok || (toRead != bytesTransferred)) {
            NotifyFailed("Failed to write to hDst");
            goto Error;
        }

        left -= toRead;
    }       
    return TRUE;
Error:
    return FALSE;
}

BOOL CopyFileDataZipped(HANDLE hSrc, HANDLE hDst, DWORD size)
{
    BOOL                ok;
    DWORD               bytesTransferred;
    unsigned char       in[1024*8];
    unsigned char       out[1024*16];
    int                 ret;
    DWORD               left = size;

    z_stream    strm = {0};

    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        NotifyFailed("inflateInit() failed");
        return FALSE;
    }

    while (0 != left) {
        DWORD toRead = dimof(in);
        if (toRead > left)
            toRead = left;

        ok = ReadFile(hSrc, (LPVOID)in, toRead, &bytesTransferred, NULL);
        if (!ok || (toRead != bytesTransferred)) {
            NotifyFailed("Failed to read from file part");
            goto Error;
        }

        strm.avail_in = bytesTransferred;
        strm.next_in = in;

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);

            switch (ret) {
                case Z_NEED_DICT:
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    goto Error;
            }

            DWORD toWrite = sizeof(out) - strm.avail_out;

            ok = WriteFile(hDst, (LPVOID)out, toWrite, &bytesTransferred, NULL);
            if (!ok || (toWrite != bytesTransferred)) {
                NotifyFailed("Failed to write to hDst");
                goto Error;
            }
        } while (strm.avail_out == 0);

        left -= toRead;
    }
    if (ret == Z_STREAM_END)
        ret = Z_OK;
    ret = inflateEnd(&strm);
    return ret == Z_OK;
Error:
    inflateEnd(&strm);
    return FALSE;
}

BOOL OpenFileForReading(TCHAR *s, HANDLE *hOut)
{
    *hOut = CreateFile(s, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*hOut == INVALID_HANDLE_VALUE) {
        char *msg1 = tstr_to_utf8(s);
        char *msg2 = str_printf("Couldn't open %s for reading", msg1);
        NotifyFailed(msg2);
        free(msg2); free(msg1);
        return FALSE;
    }
    return TRUE;
}

BOOL OpenFileForWriting(TCHAR *s, HANDLE *hOut)
{
    *hOut = CreateFile(s, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*hOut == INVALID_HANDLE_VALUE) {
        char *msg1 = tstr_to_utf8(s);
        char *msg2 = str_printf("Couldn't open %s for writing", msg1);
        ShowLastError(msg2);
        free(msg2); free(msg1);
        return FALSE;
    }
    return TRUE;
}

BOOL ExtractPartFile(TCHAR *dir, EmbeddedPart *p)
{
    TCHAR * dstName = NULL, *dstPath = NULL;
    HANDLE  hDst = INVALID_HANDLE_VALUE, hSrc = INVALID_HANDLE_VALUE;
    BOOL    ok = FALSE;

    dstName = utf8_to_tstr(p->fileName);
    dstPath = tstr_cat3(dir, _T("\\"), dstName);
    TCHAR *exePath = GetExePath();

    if (!OpenFileForReading(exePath, &hSrc))
        goto Error;

    DWORD res = SetFilePointer(hSrc, p->fileOffset, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res) {
        ShowLastError("Couldn't seek to file part");
        goto Error;
    }

    if (!OpenFileForWriting(dstPath, &hDst))
        goto Error;

    if (str_ieq(INSTALLER_PART_FILE, p->type))
        ok = CopyFileData(hSrc, hDst, p->fileSize);
    else if (str_ieq(INSTALLER_PART_FILE_ZLIB, p->type))
        ok = CopyFileDataZipped(hSrc, hDst, p->fileSize);

Error:
    CloseHandle(hDst); CloseHandle(hSrc);
    free(dstPath); free(dstName);
    return ok;
}

BOOL InstallCopyFiles(EmbeddedPart *root)
{
    TCHAR *installDir = GetInstallationDir();
    EmbeddedPart *p = root;
    while (p) {
        EmbeddedPart *next = p->next;
        if (str_ieq(INSTALLER_PART_FILE, p->type) ||
            str_ieq(INSTALLER_PART_FILE_ZLIB, p->type)) {
            if (!ExtractPartFile(installDir, p))
                return FALSE;
        }
        p = next;
    }
    return TRUE;
}

DWORD GetInstallerTemplateSize(EmbeddedPart *parts)
{
    EmbeddedPart *p = parts;
    while (p) {
        EmbeddedPart *next = p->next;
        if (str_ieq(INSTALLER_PART_END, p->type))
            return p->fileSize;
        p = next;
    }
    return INVALID_SIZE;
}

BOOL CreateUninstaller(EmbeddedPart *parts)
{
    TCHAR *uninstallerPath = GetUninstallerPath();
    HANDLE hSrc = INVALID_HANDLE_VALUE, hDst = INVALID_HANDLE_VALUE;
    BOOL ok = FALSE;
    DWORD bytesTransferred;

    TCHAR *exePath = GetExePath();
    DWORD installerTemplateSize = GetInstallerTemplateSize(parts);
    if (INVALID_SIZE == installerTemplateSize)
        goto Error;

    if (!OpenFileForReading(exePath, &hSrc))
        goto Error;

    if (!OpenFileForWriting(uninstallerPath, &hDst))
        goto Error;

    ok = CopyFileData(hSrc, hDst, installerTemplateSize);
    if (!ok)
        goto Error;

    ok = WriteFile(hDst, (LPVOID)INSTALLER_PART_UNINSTALLER, 4, &bytesTransferred, NULL);
    if (!ok || (4 != bytesTransferred)) {
        NotifyFailed("Failed to write to hDst");
        goto Error;
    }

Exit:
    free(uninstallerPath);
    CloseHandle(hSrc); CloseHandle(hDst);
    return ok;
Error:
    ok = FALSE;
    goto Exit;
}

BOOL IsUninstaller()
{
#if FORCE_TO_BE_UNINSTALLER
    return TRUE;
#else
    EmbeddedPart *p = GetEmbeddedPartsInfo();
    while (p) {
        EmbeddedPart *next = p->next;
        if (str_ieq(p->type, INSTALLER_PART_UNINSTALLER))
            return TRUE;
        p = next;
    }
    return FALSE;
#endif
}

// Process all messages currently in a message queue.
// Required when a change of state done during message processing is followed
// by a lengthy process done on gui thread and we want the change to be
// visually shown (e.g. when disabling a button)
// Note: in a very unlikely scenario probably can swallow WM_QUIT. Wonder what
// would happen then.
void ProcessMessageLoop(HWND hwnd)
{
    MSG msg;
    BOOL hasMsg;
    for (;;) {
        hasMsg = PeekMessage(&msg, hwnd,  0, 0, PM_REMOVE);
        if (!hasMsg)
            return;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
} 

inline int RectDx(RECT *r)
{
    return r->right - r->left;
}

inline int RectDy(RECT *r)
{
    return r->bottom - r->top;
}

inline void SetFont(HWND hwnd, HFONT font)
{
    SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

void SetCheckboxState(HWND hwnd, int checked)
{
    SendMessage(hwnd, BM_SETCHECK, checked, 0L);
}

BOOL GetCheckboxState(HWND hwnd)
{
    return (BOOL)SendMessage(hwnd, BM_GETCHECK, 0, 0L);
}

#if 0
void ResizeClientArea(HWND hwnd, int dx, int dy)
{
    RECT rwin, rcln;
    GetClientRect(hwnd, &rwin);
}
#endif

static HFONT CreateDefaultGuiFont()
{
    NONCLIENTMETRICS m = { sizeof (NONCLIENTMETRICS) };
    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &m, 0))
    {
        // fonts: lfMenuFont, lfStatusFont, lfMessageFont, lfCaptionFont
        return CreateFontIndirect(&m.lfMessageFont);
    }
    return NULL;
}

HANDLE CreateProcessHelper(TCHAR *exe, TCHAR *args=NULL)
{
    PROCESS_INFORMATION pi;
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    TCHAR *cmd;
    // per msdn, cmd has to be writeable
    if (args) {
        // Note: doesn't quote the args if but it's good enough for us
        cmd = tstr_cat3(exe, _T(" "), args);
    }
    else
        cmd = tstr_dup(exe);
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmd);
        return NULL;
    }
    free(cmd);
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// Note: doesn't recurse and the size might overflow, but it's good enough for
// our purpose
DWORD GetDirSize(TCHAR *dir)
{
    LARGE_INTEGER size;
    DWORD totalSize = 0;
    WIN32_FIND_DATA findData;

    TCHAR *dirPattern = tstr_cat(dir, _T("\\*"));

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        goto Exit;

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            size.LowPart  = findData.nFileSizeLow;
            size.HighPart = findData.nFileSizeHigh;
            totalSize += (DWORD)size.QuadPart;
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);
Exit:
    free(dirPattern);
    return totalSize;
}

DWORD GetInstallationDirectorySize()
{
    return GetDirSize(GetInstallationDir());
}

void WriteUninstallerRegistryInfo()
{
    HKEY hkey = HKEY_LOCAL_MACHINE;
    TCHAR *uninstallerPath = GetUninstallerPath();
    TCHAR *installedExePath = GetInstalledExePath();
    WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_ICON, installedExePath);
    WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_NAME, TAPP);
    WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_VERSION, CURR_VERSION_STR);
    WriteRegDWORD(hkey, REG_PATH_UNINST, ESTIMATED_SIZE, GetInstallationDirectorySize());
    WriteRegDWORD(hkey, REG_PATH_UNINST, NO_MODIFY, 1);
    WriteRegDWORD(hkey, REG_PATH_UNINST, NO_REPAIR, 1);
    WriteRegStr(hkey,   REG_PATH_UNINST, PUBLISHER, _T("Krzysztof Kowalczyk"));
    WriteRegStr(hkey,   REG_PATH_UNINST, UNINSTALL_STRING, uninstallerPath);
    WriteRegStr(hkey,   REG_PATH_UNINST, URL_INFO_ABOUT, _T("http://blog.kowalczyk/info/software/sumatrapdf/"));
    free(uninstallerPath);
    free(installedExePath);
}

BOOL RegDelKeyRecurse(HKEY hkey, TCHAR *path)
{
    LSTATUS res;
    res = SHDeleteKey(hkey, path);
    if ((ERROR_SUCCESS != res) && (res != ERROR_FILE_NOT_FOUND)) {
        SeeLastError(res);
        return FALSE;
    }
    return TRUE;
}

void RemoveUninstallerRegistryInfo()
{
    BOOL ok1 = RegDelKeyRecurse(HKEY_LOCAL_MACHINE, REG_PATH_UNINST);
    // Note: we delete this key because the old nsis installer was setting it
    // but we're not setting or using it (I assume it's used by nsis to remember
    // installation directory to better support the case when they allow
    // changing it, but we don't so it's not needed).
    BOOL ok2 = RegDelKeyRecurse(HKEY_LOCAL_MACHINE, REG_PATH_SOFTWARE);

    if (!ok1 || !ok2)
        NotifyFailed("Failed to delete uninstaller registry keys");
}

#define REG_CLASSES_APP _T("Software\\Classes\\") TAPP
#define REG_CLASSES_PDF _T("Software\\Classes\\.pdf")

/* Undo what DoAssociateExeWithPdfExtension() in SumatraPDF.cpp did */
void UnregisterFromBeingDefaultViewer(HKEY hkey)
{
    TCHAR buf[MAX_PATH + 8];
    bool ok = ReadRegStr(hkey, REG_CLASSES_APP, _T("previous.pdf"), buf, dimof(buf));
    if (ok) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, buf);
    } else {
        bool ok = ReadRegStr(hkey, REG_CLASSES_PDF, NULL, buf, dimof(buf));
        if (ok && tstr_eq(TAPP, buf))
            RegDelKeyRecurse(hkey, REG_CLASSES_PDF);
    }
    RegDelKeyRecurse(hkey, REG_CLASSES_APP);
}

#define REG_EXPLORER_PDF_EXT  _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf")
#define PROG_ID _T("ProgId")

void UnregisterExplorerFileExts()
{
    TCHAR buf[MAX_PATH + 8];
    bool ok = ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID, buf, dimof(buf));
    if (!ok || !tstr_eq(buf, TAPP))
        return;

    HKEY hk;
    LONG res = RegOpenKeyEx(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, 0, KEY_SET_VALUE, &hk);
    if (ERROR_SUCCESS != res)
        return;

    res = RegDeleteValue(hk, PROG_ID);
    if (res != ERROR_SUCCESS) {
        SeeLastError(res);
    }
    RegCloseKey(hk);
}

void UnregisterFromBeingDefaultViewer()
{
    UnregisterFromBeingDefaultViewer(HKEY_LOCAL_MACHINE);
    UnregisterFromBeingDefaultViewer(HKEY_CURRENT_USER);
    UnregisterExplorerFileExts();
}

// Note: doesn't recurse, but it's good enough for us
void RemoveDirectoryWithFiles(TCHAR *dir)
{
    WIN32_FIND_DATA findData;

    TCHAR *dirPattern = tstr_cat(dir, _T("\\*"));
    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            TCHAR *path = tstr_cat3(dir, _T("\\"), findData.cFileName);
            DWORD attrs = findData.dwFileAttributes;
            // filter out directories. Even though there shouldn't be any
            // subdirectories, it also filters out the standard "." and ".."
            if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                // per http://msdn.microsoft.com/en-us/library/aa363915(v=VS.85).aspx
                // have to remove read-only attribute for DeleteFile() to work
                if (attrs & FILE_ATTRIBUTE_READONLY) {
                    attrs = attrs & ~FILE_ATTRIBUTE_READONLY;
                    SetFileAttributes(path, attrs);
                }
                DeleteFile(path);
            }
            free(path);
        } while (FindNextFile(h, &findData) != 0);
        FindClose(h);
    }

    if (!RemoveDirectory(dir)) {
        if (ERROR_FILE_NOT_FOUND != GetLastError()) {
            SeeLastError();
            NotifyFailed("Couldn't remove installation directory");
        }
    }
    free(dirPattern);
}

void RemoveInstallationDirectory()
{
    RemoveDirectoryWithFiles(GetInstallationDir());
}

void CreateShortcut(TCHAR *shortcutPath, TCHAR *exePath, TCHAR *workingDir, TCHAR *description)
{
    IShellLink* sl = NULL;
    IPersistFile* pf = NULL;

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&sl);
    if (FAILED(hr)) 
        goto Exit;

    hr = sl->QueryInterface(IID_IPersistFile, (void **)&pf);
    if (FAILED(hr))
        goto Exit;

    hr = sl->SetPath(exePath);
    sl->SetWorkingDirectory(workingDir);
    //sl->SetShowCmd(SW_SHOWNORMAL);
    //sl->SetHotkey(0);
    sl->SetIconLocation(exePath, 0);
    //sl->SetArguments(_T(""));
    if (description)
        sl->SetDescription(description);

#ifndef _UNICODE
#error "must be compiled as Unicode!"
#endif
    hr = pf->Save(shortcutPath,TRUE);

Exit:
    if (pf)
      pf->Release();
    if (sl)
      sl->Release();

    if (FAILED(hr)) {
        SeeLastError();
        NotifyFailed("Failed to create a shortcut");
    }    
}

void CreateAppShortcut()
{
    TCHAR *workingDir = GetInstallationDir();
    TCHAR *installedExePath = GetInstalledExePath();
    TCHAR *shortcutPath = GetShortcutPath();
    CreateShortcut(shortcutPath, installedExePath, workingDir, NULL);
    free(installedExePath);
    free(shortcutPath);
}

void RemoveShortcut()
{
    TCHAR *p = GetShortcutPath();
    BOOL ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        SeeLastError();
        NotifyFailed("Couldn't remove the shortcut");
    }
    free(p);
}

BOOL CreateInstallationDirectory()
{
    TCHAR *dir = GetInstallationDir();
    BOOL ok = CreateDirectory(dir, NULL);
    if (!ok && (GetLastError() != ERROR_ALREADY_EXISTS)) {
        SeeLastError();
        NotifyFailed("Couldn't create installation directory");
    } else {
        ok = TRUE;
    }
    return ok;
}

void OnButtonInstall()
{
    char *msg = NULL;
    BOOL registerAsDefault = GetCheckboxState(gHwndCheckboxRegisterDefault);

    // disable the button during installation
    EnableWindow(gHwndButtonInstall, FALSE);
    ProcessMessageLoop(gHwndFrame);

    // TODO: do it on a background thread so that UI is still responsive
    EmbeddedPart *parts = GetEmbeddedPartsInfo();
    if (NULL == parts) {
        msg = "Didn't find embedded parts";
        goto Error;
    }

    /* if the app is running, we have to kill it so that we can over-write the executable */
    KillProcess(EXENAME, TRUE);

    if (!CreateInstallationDirectory())
        goto Error;

    if (!InstallCopyFiles(parts))
        goto Error;

    if (!CreateUninstaller(parts))
        goto Error;

    WriteUninstallerRegistryInfo();
    CreateAppShortcut();

    if (registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        TCHAR *installedExePath = GetInstalledExePath();
        HANDLE h = CreateProcessHelper(installedExePath, _T("-register-for-pdf"));
        CloseHandle(h);
        free(installedExePath);
    }

Exit:
    EnableWindow(gHwndButtonInstall, TRUE);
    return;
Error:
    if (msg)
        NotifyFailed(msg);
    goto Exit;
}

void OnButtonUninstall()
{
    // disable the button during uninstallation
    EnableWindow(gHwndButtonUninstall, FALSE);
    ProcessMessageLoop(gHwndFrame);

    /* if the app is running, we have to kill it to delete the files */
    KillProcess(EXENAME, TRUE);
    RemoveUninstallerRegistryInfo();
    RemoveShortcut();
    UnregisterFromBeingDefaultViewer();
    RemoveInstallationDirectory();

    EnableWindow(gHwndButtonUninstall, TRUE);
}

#define BOTTOM_PART_DY 48

void DrawInstallerFrame(Graphics &g, RECT *r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Unit cu = g.GetPageUnit();
    g.SetPageUnit(UnitPixel);

    SolidBrush bgBrush(Color(255,242,0));
    Rect r2(r->top, r->left, RectDx(r), RectDy(r));
    g.FillRectangle(&bgBrush, r2);

    Rect ballRect(gBallX-5, gBallY-5, 10, 10);
    SolidBrush blackBrush(Color(255, 0, 0, 0));
    g.FillEllipse(&blackBrush, ballRect);

    FontFamily fontFamily(L"Arial");
    StringFormat strformat;
    TCHAR s[] = _T("SumatraPDF");

    GraphicsPath p;
    Pen pen(Color(0,0,0), 5);
    p.AddString(s, tstr_len(s), &fontFamily, FontStyleRegular, 48, Point(0, 0), &strformat);

#if 0 // TODO: centering doesn't work, the bounds of p seem to be too big
    // center the string
    Rect rp;
    p.GetBounds(&rp, NULL, &pen);
    int offX = (RectDx(r) - rp.Width) / 2;
    Matrix m;
    m.Translate((REAL)offX, 20.f, MatrixOrderAppend);
    p.Transform(&m);
#else
    Matrix m;
    m.Translate(5.f, 20.f, MatrixOrderAppend);
    p.Transform(&m);
#endif

    g.DrawPath(&pen, &p);
    g.FillPath(&bgBrush, &p);
}

void DrawUninstaller(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.top = rc.bottom - BOTTOM_PART_DY;
    RECT rcTmp;
    if (IntersectRect(&rcTmp, &rc, &ps->rcPaint)) {
        HBRUSH brushNativeBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(dc, &rc, brushNativeBg);
        DeleteObject(brushNativeBg);
    }

#if 1 // DoubleBuffer
        // TODO: cache bmp object?
        Graphics g(dc);
        GetClientRect(hwnd, &rc);
        rc.bottom -= BOTTOM_PART_DY;
        int dx = RectDx(&rc); int dy = RectDy(&rc);
        Bitmap bmp(dx, dy, &g);
        Graphics g2((Image*)&bmp);
        DrawInstallerFrame(g2, &rc);
        g.DrawImage(&bmp, 0, 0);
#else
        GetClientRect(hwnd, &rc);
        rc.bottom -= BOTTOM_PART_DY;
        Graphics g(dc);
        DrawInstallerFrame(g, &rc);
#endif
}

void DrawInstaller(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.top = rc.bottom - BOTTOM_PART_DY;
    RECT rcTmp;
    if (IntersectRect(&rcTmp, &rc, &ps->rcPaint)) {
        HBRUSH brushNativeBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(dc, &rc, brushNativeBg);
        DeleteObject(brushNativeBg);
    }

#if 1 // DoubleBuffer
    // TODO: cache bmp object?
    Graphics g(dc);
    GetClientRect(hwnd, &rc);
    rc.bottom -= BOTTOM_PART_DY;
    int dx = RectDx(&rc); int dy = RectDy(&rc);
    Bitmap bmp(dx, dy, &g);
    Graphics g2((Image*)&bmp);
    DrawInstallerFrame(g2, &rc);
    g.DrawImage(&bmp, 0, 0);
#else
    GetClientRect(hwnd, &rc);
    rc.bottom -= BOTTOM_PART_DY;
    Graphics g(dc);
    DrawInstallerFrame(g, &rc);
#endif
}

void OnPaintInstaller(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawInstaller(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}

void OnPaintUninstaller(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    DrawUninstaller(hwnd, hdc, &ps);
    EndPaint(hwnd, &ps);
}

void OnMouseMove(HWND hwnd, int x, int y)
{
    gBallX = x;
    gBallY = y;
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.bottom -= BOTTOM_PART_DY;
    InvalidateRect(hwnd, &rc, FALSE);
}

void OnCreateUninstaller(HWND hwnd)
{
    RECT        r;
    int         x, y;

    GetClientRect(hwnd, &r);
    x = RectDx(&r) - 128 - 8;
    y = RectDy(&r) - 22 - 8;
    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    gHwndButtonUninstall = CreateWindow(WC_BUTTON, _T("Uninstall SumatraPDF"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, 128, 22, hwnd, (HMENU)ID_BUTTON_UNINSTALL, ghinst, NULL);
    SetFont(gHwndButtonUninstall, gFontDefault);
}

static LRESULT CALLBACK UninstallerWndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            OnCreateUninstaller(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintUninstaller(hwnd);
            break;

        case WM_COMMAND:
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case ID_BUTTON_UNINSTALL:
                    OnButtonUninstall();
                    break;
                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void OnCreateInstaller(HWND hwnd)
{
    RECT        r;
    int         x, y;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    GetClientRect(hwnd, &r);
    x = RectDx(&r) - 120 - 8;
    y = RectDy(&r) - 22 - 8;
    gHwndButtonInstall = CreateWindow(WC_BUTTON, _T("Install SumatraPDF"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, 120, 22, hwnd, (HMENU)ID_BUTTON_INSTALL, ghinst, NULL);
    SetFont(gHwndButtonInstall, gFontDefault);

    gHwndCheckboxRegisterDefault = CreateWindow(
        WC_BUTTON, _T("Use as default PDF Reader"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        8, y, 160, 22, hwnd, (HMENU)ID_CHECKBOX_MAKE_DEAFULT, ghinst, NULL);
    SetFont(gHwndCheckboxRegisterDefault, gFontDefault);
    SetCheckboxState(gHwndCheckboxRegisterDefault, TRUE);
}

static LRESULT CALLBACK InstallerWndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            OnCreateInstaller(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintInstaller(hwnd);
            break;

        case WM_COMMAND:
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case ID_BUTTON_INSTALL:
                    OnButtonInstall();
                    break;
                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

#if 0
        case WM_SIZE:
            break;


        case WM_CHAR:
            break;

        case WM_KEYDOWN:
            break;

#endif

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) {
    memzero(&wcex, sizeof(wcex));
    wcex.cbSize         = sizeof(wcex);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = InstallerWndProcFrame;
    wcex.lpszClassName  = INSTALLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = UninstallerWndProcFrame;
    wcex.lpszClassName  = UNINSTALLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    return TRUE;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gGdiplusToken, &gdiplusStartupInput, NULL);
    
    gFontDefault = CreateDefaultGuiFont();

    if (IsUninstaller()) {
        gHwndFrame = CreateWindow(
                UNINSTALLER_FRAME_CLASS_NAME, _T("SumatraPDF Uninstaller"),
                //WS_OVERLAPPEDWINDOW,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 320, 480,
                NULL, NULL,
                ghinst, NULL);
    } else {
        gHwndFrame = CreateWindow(
                INSTALLER_FRAME_CLASS_NAME, _T("SumatraPDF Installer"),
                //WS_OVERLAPPEDWINDOW,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 320, 480,
                NULL, NULL,
                ghinst, NULL);
    }
    if (!gHwndFrame)
        return FALSE;
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// Try harder getting temporary directory
// Ensures that name ends with \, to make life easier on callers.
// Caller needs to free() the result.
// Returns NULL if fails for any reason.
TCHAR *GetValidTempDir()
{
    TCHAR d[MAX_PATH];
    DWORD res = GetTempPath(dimof(d), d);
    if ((0 == res) || (res >= MAX_PATH)) {
        NotifyFailed("Couldn't obtain temporary directory");
        return NULL;
    }
    if (!tstr_endswithi(d, _T("\\")))
        tstr_cat_s(d, dimof(d), _T("\\"));
    res = CreateDirectory(d, NULL);
    if ((res == 0) && (ERROR_ALREADY_EXISTS != GetLastError())) {
        SeeLastError();
        NotifyFailed("Couldn't create temporary directory");
        return NULL;
    }
    return tstr_dup(d);
}

// If this is uninstaller and we're running from installation directory,
// copy uninstaller to temp directory and execute from there, exiting
// ourselves. This is needed so that uninstaller can delete itself
// from installation directory and remove installation directory
// If returns TRUE, this is an installer and we sublaunched ourselves,
// so the caller needs to exit
BOOL ExecuteFromTempIfUninstaller()
{
    TCHAR *tempDir = NULL;
    if (!IsUninstaller())
        return FALSE;

    tempDir = GetValidTempDir();
    if (!tempDir)
        return FALSE;

    // already running from temp directory?
    //if (tstr_startswith(GetExePath(), tempDir))
    //    return FALSE;

    // only need to sublaunch if running from installation dir
    if (!tstr_startswith(GetExePath(), GetInstallationDir())) {
        // TODO: use MoveFileEx() to mark this file as 'delete on reboot'
        // with MOVEFILE_DELAY_UNTIL_REBOOT flag?
        return FALSE;
    }

    // Using fixed (unlikely) name instead of GetTempFileName()
    // so that we don't litter temp dir with copies of ourselves
    // Not sure how to ensure that we get deleted after we're done
    TCHAR *tempPath = tstr_cat(tempDir, _T("sum~inst.exe"));

    if (!CopyFile(GetExePath(), tempPath, FALSE)) {
        NotifyFailed("Failed to copy uninstaller to temp directory");
        free(tempPath);
        return FALSE;
    }

    HANDLE h = CreateProcessHelper(tempPath);
    if (!h) {
        free(tempPath);
        return FALSE;
    }

    CloseHandle(h);
    free(tempPath);
    return TRUE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg = {0};

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    if (ExecuteFromTempIfUninstaller())
        return 0;

    CoInitialize(NULL);

    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;


    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

Exit:
    FreeEmbeddedParts(gEmbeddedParts);
    GdiplusShutdown(gGdiplusToken);
    CoUninitialize();

    return msg.wParam;
}
