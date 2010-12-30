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

#include <windows.h>
#include <windowsx.h>
#include <GdiPlus.h>
#include <tchar.h>
#include <shlobj.h>
#include <psapi.h>
#include <Shlwapi.h>
#include <objidl.h>

#include <zlib.h>

#include "Resource.h"
#include "base_util.h"
#include "tstr_util.h"
#include "win_util.h"
#include "WinUtil.hpp"
#include "../Version.h"

using namespace Gdiplus;

#ifdef DEBUG
// debug builds use a manifest created by the linker instead of our own, so ensure visual styles this way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// set to 1 when testing as uninstaller
#define FORCE_TO_BE_UNINSTALLER 0

#define INSTALLER_FRAME_CLASS_NAME    _T("SUMATRA_PDF_INSTALLER_FRAME")

#define INSTALLER_WIN_DX    420
#define INSTALLER_WIN_DY    320
#define UNINSTALLER_WIN_DX  420
#define UNINSTALLER_WIN_DY  320

#define ID_BUTTON_INSTALL             1
#define ID_BUTTON_UNINSTALL           2
#define ID_CHECKBOX_MAKE_DEFAULT      3
#define ID_BUTTON_START_SUMATRA       4
#define ID_BUTTON_EXIT                5

#define WM_APP_INSTALLATION_FINISHED        (WM_APP + 1)

#define INVALID_SIZE                  DWORD(-1)

// The window is divided in two parts:
// * top part, where we display nice graphics
// * bottom part, with install/uninstall button
// This is the height of the lower part
#define BOTTOM_PART_DY 38

static HINSTANCE        ghinst;
static HWND             gHwndFrame;
static HWND             gHwndButtonInstall = NULL;
static HWND             gHwndButtonExit = NULL;
static HWND             gHwndButtonRunSumatra = NULL;
static HWND             gHwndCheckboxRegisterDefault = NULL;
static HWND             gHwndButtonUninstall = NULL;
static HFONT            gFontDefault;

static TCHAR *          gMsg;
static Color            gMsgColor;
static bool             gMsgDrawShadow = true;

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

#define REG_EXPLORER_PDF_EXT  _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf")
#define PROG_ID               _T("ProgId")

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
    TCHAR *         fileName;    // name of the file (UTF-8 encoded in file)
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

void ShowLastError(TCHAR *msg)
{
    TCHAR *msgBuf, *errorMsg;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPTSTR)&msgBuf, 0, NULL)) {
        errorMsg = tstr_printf(_T("%s\n\n%s"), msg, msgBuf);
        LocalFree(msgBuf);
    } else {
        errorMsg = tstr_printf(_T("%s\n\nError %d"), msg, (int)GetLastError());
    }
    MessageBox(gHwndFrame, errorMsg, _T("Installer failed"), MB_OK | MB_ICONEXCLAMATION);
    free(errorMsg);
}

void NotifyFailed(TCHAR *msg)
{
    MessageBox(gHwndFrame, msg, _T("Installer failed"),  MB_ICONEXCLAMATION | MB_OK);
}

BOOL ReadData(HANDLE h, LPVOID data, DWORD size, TCHAR *errMsg)
{
    DWORD bytesRead;
    BOOL ok = ReadFile(h, data, size, &bytesRead, NULL);
    TCHAR *msg;
    if (!ok || (bytesRead != size)) {        
        if (!ok) {
            msg = tstr_printf(_T("%s: ok=%d"), errMsg, ok);
        } else {
            msg = tstr_printf(_T("%s: bytesRead=%d, wanted=%d"), errMsg, (int)bytesRead, (int)size);
        }
        ShowLastError(msg);
        return FALSE;
    }
    return TRUE;        
}

#define SEEK_FAILED INVALID_SET_FILE_POINTER

DWORD SeekBackwards(HANDLE h, LONG distance, TCHAR *errMsg)
{
    DWORD res = SetFilePointer(h, -distance, NULL, FILE_CURRENT);
    if (INVALID_SET_FILE_POINTER == res) {
        ShowLastError(errMsg);
    }
    return res;
}

DWORD GetFilePos(HANDLE h)
{
    return SeekBackwards(h, 0, _T(""));
}

#define TEN_SECONDS_IN_MS 10*1000

// Kill a process with given <processId> if it's named <processName>.
// If <waitUntilTerminated> is TRUE, will wait until process is fully killed.
// Returns TRUE if killed a process
BOOL KillProcIdWithName(DWORD processId, TCHAR *processName, BOOL waitUntilTerminated)
{
    HANDLE      hProcess = NULL;
    TCHAR       currentProcessName[1024];
    HMODULE     modulesArray[1024];
    DWORD       modulesCount;
    BOOL        killed = FALSE;

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess)
        return FALSE;

    BOOL ok = EnumProcessModules(hProcess, modulesArray, sizeof(HMODULE)*1024, &modulesCount);
    if (!ok)
        goto Exit;

    if (0 == GetModuleBaseName(hProcess, modulesArray[0], currentProcessName, 1024))
        goto Exit;

    if (!tstr_ieq(currentProcessName, processName))
        goto Exit;

    killed = TerminateProcess(hProcess, 0);
    if (!killed)
        goto Exit;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    UpdateWindow(FindWindow(NULL, _T("Shell_TrayWnd")));
    UpdateWindow(GetDesktopWindow());

Exit:
    CloseHandle(hProcess);
    return killed;
}

#define MAX_PROCESSES 1024

static int KillProcess(TCHAR *processName, BOOL waitUntilTerminated)
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
    tstr_cat_s(dir, dimof(dir), _T("\\") TAPP);
    return dir;    
}

TCHAR *GetUninstallerPath()
{
    TCHAR *installDir = GetInstallationDir();
    return tstr_cat(installDir, _T("\\") _T("uninstall.exe"));
}

TCHAR *GetInstalledExePath()
{
    TCHAR *installDir = GetInstallationDir();
    return tstr_cat(installDir, _T("\\") EXENAME);
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
    return tstr_cat(GetStartMenuProgramsPath(), _T("\\") TAPP _T(".lnk"));
}

DWORD GetFilePointer(HANDLE h)
{
    return SetFilePointer(h, 0, NULL, FILE_CURRENT);
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

/* Load information about parts embedded in the installer.
   The format of the data is:

   For a part that is a file:
     $fileData      - blob
     $fileDataLen   - length of $data, 32-bit unsigned integer, little-endian
     $fileName      - UTF-8 string, name of the file (without terminating zero!)
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
    TCHAR *         msg;

    if (gEmbeddedParts)
        return gEmbeddedParts;

    TCHAR *exePath = GetExePath();
    HANDLE h = CreateFile(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        NotifyFailed(_T("Couldn't open myself for reading"));
        goto Error;
    }

    // position at the header of the last part
    res = SetFilePointer(h, -4, NULL, FILE_END);
    if (INVALID_SET_FILE_POINTER == res) {
        NotifyFailed(_T("Couldn't seek to end"));
        goto Error;
    }

ReadNextPart:
    part = SAZ(EmbeddedPart);
    part->next = root;
    root = part;

    res = GetFilePos(h);
#if 0
    msg = tstr_printf(_T("Curr pos: %d"), (int)res);
    MessageBox(gHwndFrame, msg, _T("Info"), MB_ICONINFORMATION | MB_OK);
    free(msg);
#endif

    // at this point we have to be positioned in the file at the beginning of the header
    if (!ReadData(h, (LPVOID)part->type, 4, _T("Couldn't read the header")))
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
        if (SEEK_FAILED == SeekBackwards(h, 8, _T("Couldn't seek to file name size")))
            goto Error;

        if (!ReadData(h, (LPVOID)&nameLen, 4, _T("Couldn't read file name size")))
            goto Error;
        if (SEEK_FAILED == SeekBackwards(h, 4 + nameLen, _T("Couldn't seek to file name")))
            goto Error;

        char *fileNameUTF8 = (char*)zmalloc(nameLen+1);
        if (!ReadData(h, (LPVOID)fileNameUTF8, nameLen, _T("Couldn't read file name")))
            goto Error;
        if (SEEK_FAILED == SeekBackwards(h, 4 + nameLen, _T("Couldn't seek to file size")))
            goto Error;
        part->fileName = utf8_to_tstr(fileNameUTF8);
        free(fileNameUTF8);

        if (!ReadData(h, (LPVOID)&part->fileSize, 4, _T("Couldn't read file size")))
            goto Error;
        res = SeekBackwards(h, 4 + part->fileSize + 4,  _T("Couldn't seek to header"));
        if (SEEK_FAILED == res)
            goto Error;

        part->fileOffset = res + 4;
#if 0
        msg = tstr_printf(_T("Found file '%s' of size %d at offset %d"), part->fileName, part->fileSize, part->fileOffset);
        MessageBox(gHwndFrame, msg, _T("Installer"), MB_ICONINFORMATION | MB_OK);
        free(msg);
#endif
        goto ReadNextPart;
    }

    TCHAR *ttype = utf8_to_tstr(part->type);
    msg = tstr_printf(_T("Unknown part: %s"), ttype);
    NotifyFailed(msg);
    free(msg); free(ttype);
    goto Error;

Error:
    FreeEmbeddedParts(root);
    root = NULL;

Exit:
    CloseHandle(h);
    gEmbeddedParts = root;
    return root;
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
            NotifyFailed(_T("Failed to read from file part"));
            goto Error;
        }

        ok = WriteFile(hDst, (LPVOID)buf, toRead, &bytesTransferred, NULL);
        if (!ok || (toRead != bytesTransferred)) {
            NotifyFailed(_T("Failed to write to hDst"));
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
        NotifyFailed(_T("inflateInit() failed"));
        return FALSE;
    }

    while (0 != left) {
        DWORD toRead = dimof(in);
        if (toRead > left)
            toRead = left;

        ok = ReadFile(hSrc, (LPVOID)in, toRead, &bytesTransferred, NULL);
        if (!ok || (toRead != bytesTransferred)) {
            NotifyFailed(_T("Failed to read from file part"));
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
                NotifyFailed(_T("Failed to write to hDst"));
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
        TCHAR *msg = tstr_printf(_T("Couldn't open %s for reading"), s);
        NotifyFailed(msg);
        free(msg);
        return FALSE;
    }
    return TRUE;
}

BOOL OpenFileForWriting(TCHAR *s, HANDLE *hOut)
{
    *hOut = CreateFile(s, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*hOut == INVALID_HANDLE_VALUE) {
        TCHAR *msg = tstr_printf(_T("Couldn't open %s for writing"), s);
        ShowLastError(msg);
        free(msg);
        return FALSE;
    }
    return TRUE;
}

BOOL ExtractPartFile(TCHAR *dir, EmbeddedPart *p)
{
    TCHAR * dstName = NULL, *dstPath = NULL;
    HANDLE  hDst = INVALID_HANDLE_VALUE, hSrc = INVALID_HANDLE_VALUE;
    BOOL    ok = FALSE;

    dstPath = tstr_cat3(dir, _T("\\"), p->fileName);
    TCHAR *exePath = GetExePath();

    if (!OpenFileForReading(exePath, &hSrc))
        goto Error;

    DWORD res = SetFilePointer(hSrc, p->fileOffset, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res) {
        ShowLastError(_T("Couldn't seek to file part"));
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
    free(dstPath);
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
        NotifyFailed(_T("Failed to write to hDst"));
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

void InvalidateFrame()
{
    RECT rc;
    GetClientRect(gHwndFrame, &rc);
    rc.bottom -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc, FALSE);
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
        NotifyFailed(_T("Failed to delete uninstaller registry keys"));
}

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

/* Caller needs to free() the result. */
TCHAR *GetDefaultPdfViewer()
{
    TCHAR buf[MAX_PATH];
    bool ok = ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL, buf, dimof(buf));
    if (!ok)
        return NULL;
    return tstr_dup(buf);
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
            NotifyFailed(_T("Couldn't remove installation directory"));
        }
    }
    free(dirPattern);
}

void RemoveInstallationDirectory()
{
    RemoveDirectoryWithFiles(GetInstallationDir());
}

BOOL CreateShortcut(TCHAR *shortcutPath, TCHAR *exePath, TCHAR *workingDir, TCHAR *description)
{
    IShellLink* sl = NULL;
    IPersistFile* pf = NULL;
    BOOL ok = TRUE;

    ComScope comScope;

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
    WCHAR *shortcutPathW = multibyte_to_wstr(shortcutPath, CP_ACP);
    hr = pf->Save(shortcutPathW, TRUE);
    free(shortcutPathW);
#else
    hr = pf->Save(shortcutPath, TRUE);
#endif

Exit:
    if (pf)
      pf->Release();
    if (sl)
      sl->Release();

    if (FAILED(hr)) {
        ok = FALSE;
        SeeLastError();
        NotifyFailed(_T("Failed to create a shortcut"));
    }
    return ok;
}

BOOL CreateAppShortcut()
{
    TCHAR *workingDir = GetInstallationDir();
    TCHAR *installedExePath = GetInstalledExePath();
    TCHAR *shortcutPath = GetShortcutPath();
    BOOL ok = CreateShortcut(shortcutPath, installedExePath, workingDir, NULL);
    free(installedExePath);
    free(shortcutPath);
    return ok;
}

void RemoveShortcut()
{
    TCHAR *p = GetShortcutPath();
    BOOL ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        SeeLastError();
        NotifyFailed(_T("Couldn't remove the shortcut"));
    }
    free(p);
}

BOOL CreateInstallationDirectory()
{
    TCHAR *dir = GetInstallationDir();
    BOOL ok = CreateDirectory(dir, NULL);
    if (!ok && (GetLastError() != ERROR_ALREADY_EXISTS)) {
        SeeLastError();
        NotifyFailed(_T("Couldn't create installation directory"));
    } else {
        ok = TRUE;
    }
    return ok;
}

void CreateButtonExit(HWND hwndParent)
{
    RECT    r;
    int     x, y;
    int     buttonDx = 80;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    GetClientRect(hwndParent, &r);
    x = RectDx(&r) - buttonDx - 8;
    y = RectDy(&r) - buttonDy - 8;
    gHwndButtonExit = CreateWindow(WC_BUTTON, _T("Close"),
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, buttonDx, buttonDy, hwndParent, 
                        (HMENU)ID_BUTTON_EXIT,
                        ghinst, NULL);
    SetFont(gHwndButtonExit, gFontDefault);

    SendMessage(gHwndButtonExit, WM_SETFOCUS, 0, 0);
}

void CreateButtonRunSumatra(HWND hwndParent)
{
    RECT    r;
    int     x, y;
    int     buttonDx = 120;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    GetClientRect(hwndParent, &r);
    x = RectDx(&r) - buttonDx - 8;
    y = RectDy(&r) - buttonDy - 8;
    gHwndButtonRunSumatra= CreateWindow(WC_BUTTON, _T("Start ") TAPP,
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, buttonDx, buttonDy, hwndParent, 
                        (HMENU)ID_BUTTON_START_SUMATRA,
                        ghinst, NULL);
    SetFont(gHwndButtonRunSumatra, gFontDefault);

    SendMessage(gHwndButtonRunSumatra, WM_SETFOCUS, 0, 0);
}

void OnButtonStartSumatra()
{
    TCHAR *s = GetInstalledExePath();
    HANDLE h = CreateProcessHelper(s);
    CloseHandle(h);
    free(s);
}

typedef struct {
    // arguments
    BOOL    registerAsDefault;
    HWND    hwndToNotify;
    HANDLE  hThread;

    // results
    BOOL    ok;
    TCHAR * msg;
} InstallerThreadData;

static DWORD WINAPI InstallerThread(LPVOID data)
{
    InstallerThreadData *td = (InstallerThreadData *)data;
    td->ok = TRUE;
    td->msg = NULL;

    EmbeddedPart *parts = GetEmbeddedPartsInfo();
    if (NULL == parts) {
        td->msg = _T("Didn't find embedded parts");
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
    if (!CreateAppShortcut())
        goto Error;

    if (td->registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        TCHAR *installedExePath = GetInstalledExePath();
        HANDLE h = CreateProcessHelper(installedExePath, _T("-register-for-pdf"));
        CloseHandle(h);
        free(installedExePath);
    }

Exit:
    PostMessage(td->hwndToNotify, WM_APP_INSTALLATION_FINISHED, (WPARAM)td, (LPARAM)0);
    return 0;
Error:
    td->ok = FALSE;
    goto Exit;
}

void OnButtonInstall()
{
    InstallerThreadData *td = SA(InstallerThreadData);
    td->hwndToNotify = gHwndFrame;
    td->registerAsDefault = GetCheckboxState(gHwndCheckboxRegisterDefault);

    // disable the install button and remove checkbox during installation
    DestroyWindow(gHwndCheckboxRegisterDefault);
    gHwndCheckboxRegisterDefault = NULL;
    EnableWindow(gHwndButtonInstall, FALSE);

    gMsg = _T("Installation in progress...");
    gMsgColor = COLOR_MSG_INSTALLATION;
    InvalidateFrame();

    td->hThread = CreateThread(NULL, 0, InstallerThread, td, 0, 0);
}

void OnInstallationFinished(InstallerThreadData *td)
{
    DestroyWindow(gHwndButtonInstall);
    gHwndButtonInstall = NULL;

    if (td->ok) {
        CreateButtonRunSumatra(gHwndFrame);
        gMsg = _T("Thank you! ") TAPP _T(" has been installed.");
        gMsgColor = COLOR_MSG_OK;
    } else {
        CreateButtonExit(gHwndFrame);
        gMsg = _T("Installation failed!");
        gMsgColor = COLOR_MSG_FAILED;
    }
    InvalidateFrame();

    if (td->msg)
        NotifyFailed(td->msg);

    CloseHandle(td->hThread);
    free(td);
}

void OnButtonUninstall()
{
    // disable the button during uninstallation
    EnableWindow(gHwndButtonUninstall, FALSE);
    gMsg = _T("Uninstallation in progress...");
    gMsgColor = COLOR_MSG_INSTALLATION;
    InvalidateFrame();
    ProcessMessageLoop(gHwndFrame);

    /* if the app is running, we have to kill it to delete the files */
    KillProcess(EXENAME, TRUE);
    RemoveUninstallerRegistryInfo();
    RemoveShortcut();
    UnregisterFromBeingDefaultViewer();
    RemoveInstallationDirectory();

    DestroyWindow(gHwndButtonUninstall);
    gHwndButtonUninstall = NULL;
    CreateButtonExit(gHwndFrame);

    gMsg = TAPP _T(" has been uninstalled.");
    gMsgColor = COLOR_MSG_OK;
    InvalidateFrame();
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
    PointF origin(0.f, 0.f);
    RectF bbox;
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

void DrawMessage(Graphics &g, REAL y, REAL dx)
{
    if (!gMsg)
        return;
    WCHAR *s = tstr_to_wstr(gMsg);

    Font f(L"Impact", 16, FontStyleRegular);
    StringFormat sfmt;

    RectF bbox;
    g.MeasureString(s, -1, &f, PointF(0,0), &sfmt, &bbox);

    REAL x = (dx - bbox.Width) / 2.f;
    if (gMsgDrawShadow) {
        SolidBrush b(Color(255,255,255));
        g.DrawString(s, -1, &f, PointF(x-1,y+1), &b);
    }
    SolidBrush b(gMsgColor);
    g.DrawString(s, -1, &f, PointF(x,y), &b);
    free(s);
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
        // draw shadow first
        SolidBrush b2(li->colShadow);
        PointF o2(li->x - 3.f, y + 4.f + li->dyOff);
        g.DrawString(s, 1, f, o2, &b2);

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
    SolidBrush b1(Color(0,0,0));

    WCHAR *ver_s = tstr_to_wstr(_T("v") CURR_VERSION_STR);
    g.DrawString(ver_s, -1, fVer, PointF(x2-2,y2-1), &b1);
    SolidBrush b2(Color(255,255,255));
    g.DrawString(ver_s, -1, fVer, PointF(x2,y2), &b2);
    g.ResetTransform();
    free(ver_s);
}

void DrawFrame2(Graphics &g, RECT *r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Font f(L"Impact", 40, FontStyleRegular);
    CalcLettersLayout(g, &f, RectDx(r));

    SolidBrush bgBrush(Color(255,242,0));
    Rect r2(r->top-1, r->left-1, RectDx(r)+1, RectDy(r)+1);
    g.FillRectangle(&bgBrush, r2);

    Font f2(L"Impact", 16, FontStyleRegular);
    DrawSumatraLetters(g, &f, &f2, 18.f);

    REAL msgY = (REAL)(RectDy(r) / 2);
    DrawMessage(g, msgY, (REAL)RectDx(r));
}

void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
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

    // TODO: cache bmp object?
    Graphics g(dc);
    GetClientRect(hwnd, &rc);
    rc.bottom -= BOTTOM_PART_DY;
    int dx = RectDx(&rc); int dy = RectDy(&rc);
    Bitmap bmp(dx, dy, &g);
    Graphics g2((Image*)&bmp);
    DrawFrame2(g2, &rc);
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

void OnChar(HWND hwnd, int key)
{
    if (VK_TAB == key) {
        // TODO: this always returns gHwndFrame - why?
        HWND focus = GetFocus();
        if (gHwndButtonInstall && focus != gHwndButtonInstall)
            SetFocus(gHwndButtonInstall);
        else if (gHwndCheckboxRegisterDefault && focus != gHwndCheckboxRegisterDefault)
            SetFocus(gHwndCheckboxRegisterDefault);
        else if (gHwndButtonRunSumatra && focus != gHwndButtonRunSumatra)
            SetFocus(gHwndButtonRunSumatra);
        else if (gHwndButtonExit && focus != gHwndButtonExit)
            SetFocus(gHwndButtonExit);
        else if (gHwndButtonUninstall && focus != gHwndButtonUninstall)
            SetFocus(gHwndButtonUninstall);
    } else if (VK_ESCAPE == key) {
        if (gHwndButtonExit || gHwndButtonRunSumatra ||
            gHwndButtonInstall && IsWindowEnabled(gHwndButtonInstall) ||
            gHwndButtonUninstall && IsWindowEnabled(gHwndButtonUninstall)) {
            SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }
}

void OnCreateUninstaller(HWND hwnd)
{
    RECT    r;
    int     x, y;
    int     buttonDx = 128;
    int     buttonDy = 22;

    GetClientRect(hwnd, &r);
    x = RectDx(&r) - buttonDx - 8;
    y = RectDy(&r) - buttonDy - 8;
    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    gHwndButtonUninstall = CreateWindow(WC_BUTTON, _T("Uninstall ") TAPP,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, buttonDx, buttonDy, hwnd,
                        (HMENU)ID_BUTTON_UNINSTALL, ghinst, NULL);
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
            OnPaintFrame(hwnd);
            break;

        case WM_COMMAND:
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case ID_BUTTON_UNINSTALL:
                    OnButtonUninstall();
                    break;

                case ID_BUTTON_EXIT:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

        case WM_CHAR:
            OnChar(hwnd, wParam);
            break;

        case WM_SETFOCUS:
            OnChar(hwnd, VK_TAB);
            break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void OnCreateInstaller(HWND hwnd)
{
    RECT    r;
    int     x, y;
    int     buttonDx = 120;
    int     buttonDy = 22;

    // TODO: determine the sizes of buttons by measuring their real size
    // and adjust size of the window appropriately
    GetClientRect(hwnd, &r);
    x = RectDx(&r) - buttonDx - 8;
    y = RectDy(&r) - buttonDy - 8;
    gHwndButtonInstall = CreateWindow(WC_BUTTON, _T("Install ") TAPP,
                        BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE,
                        x, y, buttonDx, buttonDy, hwnd, 
                        (HMENU)ID_BUTTON_INSTALL, ghinst, NULL);
    SetFont(gHwndButtonInstall, gFontDefault);

    gHwndCheckboxRegisterDefault = CreateWindow(
        WC_BUTTON, _T("Use as default PDF Reader"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        8, y, 160, 22, hwnd, (HMENU)ID_CHECKBOX_MAKE_DEFAULT, ghinst, NULL);
    SetFont(gHwndCheckboxRegisterDefault, gFontDefault);
    // only check the "Use as default" checkbox when no other PDF viewer
    // is currently selected (not going to intrude)
    TCHAR *defaultViewer = GetDefaultPdfViewer();
    BOOL hasOtherViewer = defaultViewer && !tstr_eq(defaultViewer, TAPP);
    SetCheckboxState(gHwndCheckboxRegisterDefault, !hasOtherViewer);
    // disable the checkbox, if we're already the default PDF viewer
    if (defaultViewer && !hasOtherViewer)
        EnableWindow(gHwndCheckboxRegisterDefault, FALSE);
    free(defaultViewer);

    SetFocus(gHwndButtonInstall);
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
            OnPaintFrame(hwnd);
            break;

        case WM_COMMAND:
            wmId = LOWORD(wParam);
            switch (wmId)
            {
                case ID_BUTTON_INSTALL:
                    OnButtonInstall();
                    break;

                case ID_BUTTON_START_SUMATRA:
                    OnButtonStartSumatra();
                    break;

                case ID_BUTTON_EXIT:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

        case WM_CHAR:
            OnChar(hwnd, wParam);
            break;

        case WM_SETFOCUS:
            OnChar(hwnd, VK_TAB);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            {
                InstallerThreadData *td = (InstallerThreadData*)wParam;
                OnInstallationFinished(td);
            }
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

    if (IsUninstaller())
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

    if (IsUninstaller()) {
        gHwndFrame = CreateWindow(
                INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Installer"),
                //WS_OVERLAPPEDWINDOW,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT, 
                UNINSTALLER_WIN_DX, UNINSTALLER_WIN_DY,
                NULL, NULL,
                ghinst, NULL);
        gMsg = _T("Welcome to ") TAPP _T(" uninstaller");
        gMsgColor = COLOR_MSG_WELCOME;
    } else {
        gHwndFrame = CreateWindow(
                INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Uninstaller"),
                //WS_OVERLAPPEDWINDOW,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT,                
                INSTALLER_WIN_DX, INSTALLER_WIN_DY,
                NULL, NULL,
                ghinst, NULL);
        gMsg = _T("Welcome to ") TAPP _T(" installer!");
        gMsgColor = COLOR_MSG_WELCOME;
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
        NotifyFailed(_T("Couldn't obtain temporary directory"));
        return NULL;
    }
    if (!tstr_endswithi(d, _T("\\")))
        tstr_cat_s(d, dimof(d), _T("\\"));
    res = CreateDirectory(d, NULL);
    if ((res == 0) && (ERROR_ALREADY_EXISTS != GetLastError())) {
        SeeLastError();
        NotifyFailed(_T("Couldn't create temporary directory"));
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
        NotifyFailed(_T("Failed to copy uninstaller to temp directory"));
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

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

class GdiPlusScope {
protected:
    GdiplusStartupInput si;
    ULONG_PTR           token;
public:
    GdiPlusScope() { GdiplusStartup(&token, &si, NULL); }
    ~GdiPlusScope() { GdiplusShutdown(token); }
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 0;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    if (ExecuteFromTempIfUninstaller())
        return 0;

    ComScope comScope;
    InitAllCommonControls();
    GdiPlusScope gdiScope;

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

Exit:
    FreeEmbeddedParts(gEmbeddedParts);
    CoUninitialize();

    return ret;
}
