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

#include "Resource.h"
#include "base_util.h"
#include "str_util.h"
#include "tstr_util.h"
#include "win_util.h"

#ifdef DEBUG
// debug builds use a manifest created by the linker instead of our own, so ensure visual styles this way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

using namespace Gdiplus;

#define INSATLLER_FRAME_CLASS_NAME  _T("SUMATRA_PDF_INSTALLER_FRAME")
#define ID_BUTTON_INSTALL 1
#define ABOUT_BG_COLOR          RGB(255,242,0)

static HINSTANCE        ghinst;
static HWND             gHwndFrame;
static HWND             gHwndButtonInstall;
static HFONT            gFontDefault;

static ULONG_PTR        gGdiplusToken;

static NONCLIENTMETRICS gNonClientMetrics = { sizeof (NONCLIENTMETRICS) };

int gBallX, gBallY;

#define APP                 "SumatraPDF"
#define TAPP                _T("SumatraPDF")
#define EXE                 "SumatraPDF.exe"

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
#define REG_PATH_UNINST     "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" APP

// Keys we'll set in REG_PATH_UNINST path

// REG_SZ, a path to installed executable (or "$path,0" to force the first icon)
#define DISPLAY_ICON "DisplayIcon"
// REG_SZ, e.g "SumatraPDF"
#define DISPLAY_NAME "DisplayName"
// REG_SZ, e.g. "1.2"
#define DISPLAY_VERSION "DisplayVersion"
// REG_DWORD, get size of installed directory after copying files
#define ESTIMATED_SIZE "EstimatedSize"
// REG_DWORD, set to 1
#define NO_MODIFY "NoModify"
// REG_DWORD, set to 1
#define NO_REPAIR "NoRepair"
// REG_SZ, e.g. "Krzysztof Kowalczyk"
#define PUBLISHER "Publisher"
// REG_SZ, path to uninstaller exe
#define UNINSTALL_STRING "UninstallString"
// REG_SZ, e.g. "http://blog.kowalczyk/info/software/sumatrapdf/
#define URL_INFO_ABOUT "UrlInfoAbout"

#define INSTALLER_PART_FILE         "kifi"
#define INSTALLER_PART_END          "kien"
#define INSTALLER_PART_UNINSTALLER  "kiun"

struct EmbeddedPart {
    EmbeddedPart *  next;
    char            type[5];     // we only use 4, 5th is for 0-termination
    // fields valid if type is INSTALLER_PART_FILE
    uint32_t        fileSize;    // size of the file
    uint32_t        fileOffset;  // offset in the executable of the file start
    char *          fileName;    // name of the file
};

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
    ::MessageBoxA(gHwndFrame, errorMsg, "Installer failed", MB_OK | MB_ICONEXCLAMATION);
    free(errorMsg);
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

void NotifyFailed(char *msg)
{
    ::MessageBoxA(gHwndFrame, msg, "Installer failed",  MB_ICONINFORMATION | MB_OK);
}

/* Load information about parts embedded in the installer.
   The format of the data is:

   For a part that is a file:
     $fileData      - blob
     $fileDataLen   - length of $data, 32-bit unsigned integer, little-endian
     $fileName      - ascii string, name of the file (without terminating zero!)
     $fileNameLne   - length of $fileName, 32-bit unsigned integer, little-endian
     'kifi'         - 4 byte unique header

   For a part that signifies end of parts:
     'kien'         - 4 byte unique header

   Data is laid out so that it can be read sequentially from the end, because
   it's easier for the installer to seek to the end of itself than parse
   PE header to figure out where the data starts. */

EmbeddedPart *LoadEmbeddedPartsInfo() {
    EmbeddedPart *  root = NULL;
    EmbeddedPart *  part;
    DWORD           res;
    char *           msg;

    TCHAR *exePath = GetExePath();
    HANDLE h = ::CreateFile(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
    ::MessageBoxA(gHwndFrame, msg, "Info", MB_ICONINFORMATION | MB_OK);
    free(msg);
#endif

    // at this point we have to be positioned in the file at the beginning of the header
    if (!ReadData(h, (LPVOID)part->type, 4, "Couldn't read the header"))
        goto Error;

    if (str_eqn(part->type, INSTALLER_PART_END, 4)) {
        goto Exit;
    }

    if (str_eqn(part->type, INSTALLER_PART_FILE, 4)) {
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
        ::MessageBoxA(gHwndFrame, msg, "Installer", MB_ICONINFORMATION | MB_OK);
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
    return root;
Error:
    FreeEmbeddedParts(root);
    root = NULL;
    goto Exit;
}

TCHAR *GetInstallationDir()
{
    static TCHAR installationDir[MAX_PATH];
    static BOOL alreadyCalculated = FALSE;
    if (alreadyCalculated)
        return installationDir;
    BOOL ok = SHGetSpecialFolderPath(NULL, installationDir, CSIDL_PROGRAM_FILES, FALSE);
    if (!ok)
        return NULL;
    tstr_cat_s(installationDir, dimof(installationDir), _T("\\"));
    tstr_cat_s(installationDir, dimof(installationDir), TAPP);
    return installationDir;    
}

BOOL CopyFileData(HANDLE hSrc, HANDLE hDst, DWORD size)
{
    BOOL    ok;
    DWORD   bytesTransferred;
    char *  buf[1024*8];
    DWORD   toCopyLeft = size;

    while (0 != toCopyLeft) {
        DWORD toRead = dimof(buf);
        if (toRead > toCopyLeft)
            toRead = toCopyLeft;

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

        toCopyLeft -= toRead;
    }       
    return TRUE;
Error:
    return FALSE;
}

BOOL ExtractPartFile(TCHAR *dir, EmbeddedPart *p)
{
    TCHAR * dstName = NULL, *dstPath = NULL;
    HANDLE  hDst = INVALID_HANDLE_VALUE, hSrc = INVALID_HANDLE_VALUE;
    BOOL    ok = TRUE;

    if (!str_ieq(INSTALLER_PART_FILE, p->type)) // double-check
        return FALSE;

    dstName = utf8_to_tstr(p->fileName);
    dstPath = tstr_cat3(dir, _T("\\"), dstName);
    TCHAR *exePath = GetExePath();

    hSrc = ::CreateFile(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        NotifyFailed("Couldn't open myself for reading");
        goto Error;
    }

    DWORD res = SetFilePointer(hSrc, p->fileOffset, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res) {
        ShowLastError("Couldn't seek to file part");
        goto Error;
    }

    hDst = ::CreateFile(dstPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        // leaks but we don't care
        ShowLastError(str_printf("Couldn't open %s for writing", tstr_to_utf8(dstPath)));
        goto Error;
    }

    ok = CopyFileData(hSrc, hDst, p->fileSize);

Exit:
    CloseHandle(hDst);
    CloseHandle(hSrc);
    free(dstPath);
    free(dstName);

    return ok;
Error:
    ok = FALSE;
    goto Exit;
}

void InstallCopyFiles(EmbeddedPart *root)
{
    TCHAR *installDir = GetInstallationDir();
    EmbeddedPart *p = root;
    while (p) {
        EmbeddedPart *next = p->next;
        if (str_ieq(INSTALLER_PART_FILE, p->type))
            ExtractPartFile(installDir, p);
        p = next;
    }

}

void CreateUninstaller()
{
    TCHAR *installDir = GetInstallationDir();
    TCHAR *uninstallerPath = tstr_cat3(installDir, _T("\\"), _T("uninstall.exe"));

    // TODO: finish me:
    // copy myself (just the template part) to uninstallerPath
    // append INSTALLER_PART_END and INSTALLER_PART_UNINSTALLER markers

    free(uninstallerPath);
}

BOOL IsUninstaller(EmbeddedPart *root)
{
    EmbeddedPart *p = root;
    while (p) {
        EmbeddedPart *next = p->next;
        if (str_ieq(p->type, INSTALLER_PART_UNINSTALLER))
            return TRUE;
        p = next;
    }
    return FALSE;
}

void OnButtonInstall()
{
    EmbeddedPart *parts = LoadEmbeddedPartsInfo();
    if (NULL == parts)
        return;

    if (IsUninstaller(parts)) {
        // shouldn't happen
        return;
    }

    /* if the app is running, we have to kill it so that we can over-write the executable */
    KillProcess(EXE, TRUE);

    InstallCopyFiles(parts);
    CreateUninstaller();

    /* TODO:
        - set necessary registry settings
        - launch the program
    */
}

void OnUninstall()
{
    /* if the app is running, we have to kill it to delete the files */
    KillProcess(EXE, TRUE);

}

inline void SetFont(HWND hwnd, HFONT font)
{
    ::SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

inline int RectDx(RECT *r)
{
    return r->right - r->left;
}

inline int RectDy(RECT *r)
{
    return r->bottom - r->top;
}

#if 0
void ResizeClientArea(HWND hwnd, int dx, int dy)
{
    RECT rwin, rcln;
    ::GetClientRect(hwnd, &rwin);
}
#endif

static HFONT CreateDefaultGuiFont()
{
    if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &gNonClientMetrics, 0))
    {
        // fonts: lfMenuFont, lfStatusFont, lfMessageFont, lfCaptionFont
        return ::CreateFontIndirect(&gNonClientMetrics.lfMessageFont);
    }
    return NULL;
}

void DrawMain(HWND hwnd, HDC hdc, RECT *rect)
{
    HBRUSH brushBg = ::CreateSolidBrush(ABOUT_BG_COLOR);

/*
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontVersionTxt = Win32_Font_GetSimple(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt);
    */

    ::SetBkMode(hdc, TRANSPARENT);

    RECT rc;
    ::GetClientRect(hwnd, &rc);
    rc.bottom -= 48;
    ::FillRect(hdc, &rc, brushBg);

    Rect ellipseRect(gBallX-5, gBallY-5, 10, 10);
    Graphics g(hdc);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    SolidBrush blackBrush(Color(255, 0, 0, 0));
    g.FillEllipse(&blackBrush, ellipseRect);

    ::DeleteObject(brushBg);
}

void OnPaintMain(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC hdc = ::BeginPaint(hwnd, &ps);
    DrawMain(hwnd, hdc, &rc);
    ::EndPaint(hwnd, &ps);
}

void OnMouseMove(HWND hwnd, int x, int y)
{
    gBallX = x;
    gBallY = y;
    ::InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    RECT        r;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            ::GetClientRect(hwnd, &r);
            x = RectDx(&r) - 120 - 8;
            y = RectDy(&r) - 22 - 8;
            gHwndButtonInstall = ::CreateWindow(WC_BUTTON, _T("Install SumatraPDF"),
                                BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                                x, y, 120, 22, hwnd, (HMENU)ID_BUTTON_INSTALL, ghinst, NULL);
            ::SetFont(gHwndButtonInstall, gFontDefault);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintMain(hwnd);
            break;

        case WM_COMMAND:
            wmId    = LOWORD(wParam);
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
    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcFrame;
    wcex.lpszClassName  = INSATLLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    ATOM atom = RegisterClassEx(&wcex);
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

    gHwndFrame = CreateWindow(
            INSATLLER_FRAME_CLASS_NAME, _T("SumatraPDF Installer"),
            //WS_OVERLAPPEDWINDOW,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            -1, -1, 320, 480,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndFrame)
        return FALSE;
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG                 msg = {0};

    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);

    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    CoInitialize(NULL);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

Exit:
    GdiplusShutdown(gGdiplusToken);
    CoUninitialize();

    return msg.wParam;
}
