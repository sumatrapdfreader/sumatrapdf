/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#include <shlwapi.h>
#include <shlobj.h>
#include <io.h>
#include <fcntl.h>

#include "BaseUtil.h"
#include "FileUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"

#define DONT_INHERIT_HANDLES FALSE

// Return true if application is themed. Wrapper around IsAppThemed() in uxtheme.dll
// that is compatible with earlier windows versions.
bool IsAppThemed() {
    WinLibrary lib(_T("uxtheme.dll"));
    FARPROC pIsAppThemed = lib.GetProcAddr("IsAppThemed");
    if (!pIsAppThemed) 
        return false;
    if (pIsAppThemed())
        return true;
    return false;
}

// Loads a DLL explicitly from the system's library collection
HMODULE WinLibrary::_LoadSystemLibrary(const TCHAR *libName) {
    TCHAR dllPath[MAX_PATH];
    GetSystemDirectory(dllPath, dimof(dllPath));
    PathAppend(dllPath, libName);
    return LoadLibrary(dllPath);
}

static int WindowsVerMajor()
{
    DWORD version = GetVersion();
    return LOBYTE(version);
}

static int WindowsVerMinor()
{
    DWORD version = GetVersion();
    return HIBYTE(version);
}

bool WindowsVerVistaOrGreater()
{
    if (WindowsVerMajor() >= 6)
        return true;
    return false;
}

void SeeLastError(DWORD err) {
    TCHAR *msgBuf = NULL;
    if (err == 0)
        err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msgBuf, 0, NULL);
    if (!msgBuf) return;
    DBG_OUT("SeeLastError(): %s\n", msgBuf);
    LocalFree(msgBuf);
}

// called needs to free() the result
TCHAR *ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName)
{
    TCHAR *val = NULL;
    REGSAM access = KEY_READ;
    HKEY hKey;
TryAgainWOW64:
    LONG res = RegOpenKeyEx(keySub, keyName, 0, access, &hKey);
    if (ERROR_SUCCESS == res) {
        DWORD valLen;
        res = RegQueryValueEx(hKey, valName, NULL, NULL, NULL, &valLen);
        if (ERROR_SUCCESS == res) {
            val = SAZA(TCHAR, valLen / sizeof(TCHAR) + 1);
            res = RegQueryValueEx(hKey, valName, NULL, NULL, (LPBYTE)val, &valLen);
            if (ERROR_SUCCESS != res) {
                free(val);
                val = NULL;
            }
        }
        RegCloseKey(hKey);
    }
    if (ERROR_FILE_NOT_FOUND == res && HKEY_LOCAL_MACHINE == keySub && KEY_READ == access) {
        // try the (non-)64-bit key as well, as HKLM\Software is not shared between 32-bit and
        // 64-bit applications per http://msdn.microsoft.com/en-us/library/aa384253(v=vs.85).aspx
#ifdef _WIN64
        access = KEY_READ | KEY_WOW64_32KEY;
#else
        access = KEY_READ | KEY_WOW64_64KEY;
#endif
        goto TryAgainWOW64;
    }
    if (ERROR_SUCCESS != res && ERROR_FILE_NOT_FOUND != res)
        SeeLastError(res);
    return val;
}

bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value)
{
    LSTATUS res = SHSetValue(keySub, keyName, valName, REG_SZ, (const VOID *)value, (DWORD)(Str::Len(value) + 1) * sizeof(TCHAR));
    if (ERROR_SUCCESS != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res;
}

bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value)
{
    LSTATUS res = SHSetValue(keySub, keyName, valName, REG_DWORD, (const VOID *)&value, sizeof(DWORD));
    if (ERROR_SUCCESS != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res;
}

bool DeleteRegKey(HKEY keySub, const TCHAR *keyName, bool resetACLFirst)
{
    if (resetACLFirst) {
        // try to remove any access restrictions on the key to delete
        // by first granting everybody all access to this key (NULL DACL)
        HKEY hKey;
        LONG res = RegOpenKeyEx(keySub, keyName, 0, WRITE_DAC, &hKey);
        if (ERROR_SUCCESS == res) {
            SECURITY_DESCRIPTOR secdesc;
            InitializeSecurityDescriptor(&secdesc, SECURITY_DESCRIPTOR_REVISION);
            SetSecurityDescriptorDacl(&secdesc, TRUE, NULL, TRUE);
            RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &secdesc);
            RegCloseKey(hKey);
        }
    }

    LSTATUS res = SHDeleteKey(keySub, keyName);
    if (ERROR_SUCCESS != res && ERROR_FILE_NOT_FOUND != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res || ERROR_FILE_NOT_FOUND == res;
}

#define PROCESS_EXECUTE_FLAGS 0x22

/*
 * enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4
 */
typedef HRESULT (WINAPI *_NtSetInformationProcess)(
   HANDLE  ProcessHandle,
   UINT    ProcessInformationClass,
   PVOID   ProcessInformation,
   ULONG   ProcessInformationLength
   );

void EnableNx(void)
{
    WinLibrary lib(_T("ntdll.dll"));
    _NtSetInformationProcess ntsip;
    DWORD dep_mode = 13; /* ENABLE | DISABLE_ATL | PERMANENT */

    ntsip = (_NtSetInformationProcess)lib.GetProcAddr("NtSetInformationProcess");
    if (ntsip)
        ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS, &dep_mode, sizeof(dep_mode));
}

// Code from http://www.halcyon.com/~ast/dload/guicon.htm
void RedirectIOToConsole(void)
{
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    int hConHandle;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = 500;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stderr, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *(FILE *)_fdopen(hConHandle, "r");
    setvbuf(stdin, NULL, _IONBF, 0);
}

TCHAR *ResolveLnk(const TCHAR * path)
{
    IShellLink *lnk = NULL;
    IPersistFile *file = NULL;
    TCHAR *resolvedPath = NULL;

    ScopedMem<OLECHAR> olePath(Str::Conv::ToWStr(path));
    if (!olePath)
        return NULL;

    HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->QueryInterface(IID_IPersistFile, (LPVOID *)&file);
    if (FAILED(hRes))
        goto Exit;

    hRes = file->Load(olePath, STGM_READ);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->Resolve(NULL, SLR_UPDATE);
    if (FAILED(hRes))
        goto Exit;

    TCHAR newPath[MAX_PATH];
    hRes = lnk->GetPath(newPath, MAX_PATH, NULL, 0);
    if (FAILED(hRes))
        goto Exit;

    resolvedPath = Str::Dup(newPath);

Exit:
    if (file)
        file->Release();
    if (lnk)
        lnk->Release();

    return resolvedPath;
}

bool CreateShortcut(const TCHAR *shortcutPath, const TCHAR *exePath,
                    const TCHAR *args, const TCHAR *description, int iconIndex)
{
    IShellLink *lnk = NULL;
    IPersistFile *file = NULL;
    bool ok = false;

    ScopedCom com;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hr)) 
        goto Exit;

    hr = lnk->QueryInterface(IID_IPersistFile, (LPVOID *)&file);
    if (FAILED(hr))
        goto Exit;

    hr = lnk->SetPath(exePath);
    if (FAILED(hr))
        goto Exit;

    lnk->SetWorkingDirectory(ScopedMem<TCHAR>(Path::GetDir(exePath)));
    // lnk->SetShowCmd(SW_SHOWNORMAL);
    // lnk->SetHotkey(0);
    lnk->SetIconLocation(exePath, iconIndex);
    if (args)
        lnk->SetArguments(args);
    if (description)
        lnk->SetDescription(description);

#ifndef _UNICODE
    hr = file->Save(ScopedMem<WCHAR>(Str::Conv::ToWStr(shortcutPath)), TRUE);
#else
    hr = file->Save(shortcutPath, TRUE);
#endif
    ok = SUCCEEDED(hr);

Exit:
    if (file)
        file->Release();
    if (lnk)
        lnk->Release();
    if (FAILED(hr))
        SeeLastError();

    return ok;
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
IDataObject* GetDataObjectForFile(LPCTSTR filePath, HWND hwnd)
{
    IDataObject* pDataObject = NULL;
    IShellFolder *pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr))
        return NULL;

    LPWSTR lpWPath = Str::Conv::ToWStr(filePath);
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(NULL, NULL, lpWPath, NULL, &pidl, NULL);
    if (SUCCEEDED(hr)) {
        IShellFolder *pShellFolder;
        LPCITEMIDLIST pidlChild;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
        if (SUCCEEDED(hr)) {
            pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, NULL, (void **)&pDataObject);
            pShellFolder->Release();
        }
        CoTaskMemFree(pidl);
    }
    pDesktopFolder->Release();

    free(lpWPath);
    return pDataObject;
}

// The result value contains major and minor version in the high resp. the low WORD
DWORD GetFileVersion(TCHAR *path)
{
    DWORD fileVersion = 0;
    DWORD handle;
    DWORD size = GetFileVersionInfoSize(path, &handle);
    LPVOID versionInfo = malloc(size);

    if (GetFileVersionInfo(path, handle, size, versionInfo)) {
        VS_FIXEDFILEINFO *fileInfo;
        UINT len;
        if (VerQueryValue(versionInfo, _T("\\"), (LPVOID *)&fileInfo, &len))
            fileVersion = fileInfo->dwFileVersionMS;
    }

    free(versionInfo);
    return fileVersion;
}

// used to be in win_util.cpp
void launch_url(const TCHAR *url)
{
    if (!url)
        return;

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize  = sizeof(sei);
    sei.fMask   = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb  = TEXT("open");
    sei.lpFile  = url;
    sei.nShow   = SW_SHOWNORMAL;

    ShellExecuteEx(&sei);
    return;
}

void exec_with_params(const TCHAR *exe, const TCHAR *params, bool hidden)
{
    if (!exe)
        return;

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize  = sizeof(sei);
    sei.fMask   = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb  = NULL;
    sei.lpFile  = exe;
    sei.lpParameters = params;
    if (hidden)
        sei.nShow = SW_HIDE;
    else
        sei.nShow   = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

/* On windows those are defined as:
#define CSIDL_PROGRAMS           0x0002
#define CSIDL_PERSONAL           0x0005
#define CSIDL_APPDATA            0x001a
 see shlobj.h for more */

#ifdef CSIDL_APPDATA
/* this doesn't seem to be defined on sm 2002 */
#define SPECIAL_FOLDER_PATH CSIDL_APPDATA
#endif

#ifdef CSIDL_PERSONAL
/* this is defined on sm 2002 and goes to "\My Documents".
   Not sure if I should use it */
 #ifndef SPECIAL_FOLDER_PATH
  #define SPECIAL_FOLDER_PATH CSIDL_PERSONAL
 #endif
#endif

int screen_get_dx(void)
{
    return GetSystemMetrics(SM_CXSCREEN);
}

int screen_get_dy(void)
{
    return GetSystemMetrics(SM_CYSCREEN);
}

int screen_get_menu_dy(void)
{
    return GetSystemMetrics(SM_CYMENU);
}

int screen_get_caption_dy(void)
{
    return GetSystemMetrics(SM_CYCAPTION);
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
void rect_shift_to_work_area(RECT *rect, bool bFully)
{
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof mi;
    GetMonitorInfo(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &mi);
    
    if (rect->bottom <= mi.rcWork.top || bFully && rect->top < mi.rcWork.top)
        /* Rectangle is too far above work area */
        OffsetRect(rect, 0, mi.rcWork.top - rect->top);
    else if (rect->top >= mi.rcWork.bottom || bFully && rect->bottom > mi.rcWork.bottom)
        /* Rectangle is too far below */
        OffsetRect(rect, 0, mi.rcWork.bottom - rect->bottom);
    
    if (rect->right <= mi.rcWork.left || bFully && rect->left < mi.rcWork.left)
        /* Too far left */
        OffsetRect(rect, mi.rcWork.left - rect->left, 0);
    else if (rect->left >= mi.rcWork.right || bFully && rect->right > mi.rcWork.right)
        /* Right */
        OffsetRect(rect, mi.rcWork.right - rect->right, 0);
}

static void rect_client_to_screen(RECT *r, HWND hwnd)
{
    POINT   p1 = {r->left, r->top};
    POINT   p2 = {r->right, r->bottom};
    ClientToScreen(hwnd, &p1);
    ClientToScreen(hwnd, &p2);
    r->left = p1.x;
    r->top = p1.y;
    r->right = p2.x;
    r->bottom = p2.y;
}

void paint_round_rect_around_hwnd(HDC hdc, HWND hwnd_edit_parent, HWND hwnd_edit, COLORREF col)
{
    RECT    r;
    HBRUSH  br;
    HGDIOBJ br_prev;
    HGDIOBJ pen;
    HGDIOBJ pen_prev;
    GetClientRect(hwnd_edit, &r);
    br = CreateSolidBrush(col);
    if (!br) return;
    pen = CreatePen(PS_SOLID, 1, col);
    pen_prev = SelectObject(hdc, pen);
    br_prev = SelectObject(hdc, br);
    rect_client_to_screen(&r, hwnd_edit_parent);
    /* TODO: the roundness value should probably be calculated from the dy of the rect */
    /* TODO: total hack: I manually adjust rectangle to values that fit g_hwnd_edit, as
       found by experimentation. My mapping of coordinates isn't right (I think I need
       mapping from window to window but even then it wouldn't explain -3 for y axis */
    RoundRect(hdc, r.left+4, r.top-3, r.right+12, r.bottom-3, 8, 8);
    if (br_prev)
        SelectObject(hdc, br_prev);
    if (pen_prev)
        SelectObject(hdc, pen_prev);
    DeleteObject(pen);
    DeleteObject(br);
}

void paint_rect(HDC hdc, RECT * rect)
{
    MoveToEx(hdc, rect->left, rect->top, NULL);
    LineTo(hdc, rect->right - 1, rect->top);
    LineTo(hdc, rect->right - 1, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->top);
}

void DrawCenteredText(HDC hdc, RectI r, const TCHAR *txt)
{    
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, -1, &r.ToRECT(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

bool IsCursorOverWindow(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    WindowRect rcWnd(hwnd);
    return rcWnd.Inside(PointI(pt.x, pt.y));
}

void CenterDialog(HWND hDlg)
{
    RECT rcDialog, rcOwner, rcRect;
    HWND hParent = GetParent(hDlg);

    GetWindowRect(hDlg, &rcDialog);
    OffsetRect(&rcDialog, -rcDialog.left, -rcDialog.top);
    GetWindowRect(hParent ? hParent : GetDesktopWindow(), &rcOwner);
    CopyRect(&rcRect, &rcOwner);
    OffsetRect(&rcRect, -rcRect.left, -rcRect.top);

    // center dialog on its parent window
    OffsetRect(&rcDialog, rcOwner.left + (rcRect.right - rcDialog.right) / 2, rcOwner.top + (rcRect.bottom - rcDialog.bottom) / 2);
    // ensure that the dialog is fully visible on one monitor
    rect_shift_to_work_area(&rcDialog, TRUE);

    SetWindowPos(hDlg, 0, rcDialog.left, rcDialog.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

/* Get the name of default printer or NULL if not exists.
   The caller needs to free() the result */
TCHAR *GetDefaultPrinterName()
{
    TCHAR buf[512];
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize))
        return Str::Dup(buf);
    return NULL;
}

bool CopyTextToClipboard(const TCHAR *text, bool appendOnly)
{
    assert(text);
    if (!text) return false;

    if (!appendOnly) {
        if (!OpenClipboard(NULL))
            return false;
        EmptyClipboard();
    }

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (Str::Len(text) + 1) * sizeof(TCHAR));
    if (handle) {
        TCHAR *globalText = (TCHAR *)GlobalLock(handle);
        lstrcpy(globalText, text);
        GlobalUnlock(handle);

        if (!SetClipboardData(CF_T_TEXT, handle))
            SeeLastError();
    }

    if (!appendOnly)
        CloseClipboard();

    return handle != NULL;
}

namespace Win {
namespace Font {

#ifndef USER_DEFAULT_SCREEN_DPI
// the following is only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#define USER_DEFAULT_SCREEN_DPI 96
#endif

HFONT GetSimple(HDC hdc, TCHAR *fontName, int fontSize)
{
    LOGFONT lf = { 0 };

    lf.lfWidth = 0;
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;    
    Str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), fontName);
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;

    return CreateFontIndirect(&lf);
}


}
}

