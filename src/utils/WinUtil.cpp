/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include <io.h>
#include <fcntl.h>
#include <mlang.h>

#include "DebugLog.h"

// Loads a DLL explicitly from the system's library collection
HMODULE SafeLoadLibrary(const TCHAR *dllName)
{
    TCHAR dllPath[MAX_PATH];
    GetSystemDirectory(dllPath, dimof(dllPath));
    PathAppend(dllPath, dllName);
    return LoadLibrary(dllPath);
}

FARPROC LoadDllFunc(TCHAR *dllName, const char *funcName)
{
    HMODULE h = SafeLoadLibrary(dllName);
    if (!h)
        return NULL;
    return GetProcAddress(h, funcName);

    // Note: we don't unload the dll. It's harmless for those that would stay
    // loaded anyway but we would crash trying to call a function that
    // was grabbed from a dll that was unloaded in the meantime
}

void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = { 0 };
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);
}

void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance)
{
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize     = sizeof(WNDCLASSEX);
    wcex.style      = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance  = hInstance;
    wcex.hCursor    = LoadCursor(NULL, IDC_ARROW);
}

// Return true if application is themed. Wrapper around IsAppThemed() in uxtheme.dll
// that is compatible with earlier windows versions.
bool IsAppThemed()
{
    FARPROC pIsAppThemed = LoadDllFunc(_T("uxtheme.dll"), "IsAppThemed");
    if (!pIsAppThemed)
        return false;
    if (pIsAppThemed())
        return true;
    return false;
}

WORD GetWindowsVersion()
{
    DWORD ver = GetVersion();
    return MAKEWORD(HIBYTE(ver), LOBYTE(ver));
}

bool IsRunningInWow64()
{
#ifndef _WIN64
    typedef BOOL (WINAPI *IsWow64ProcessProc)(HANDLE, PBOOL);
    IsWow64ProcessProc _IsWow64Process = (IsWow64ProcessProc)LoadDllFunc(_T("kernel32.dll"), "IsWow64Process");
    BOOL isWow = FALSE;
    if (_IsWow64Process)
        _IsWow64Process(GetCurrentProcess(), &isWow);
    return isWow;
#else
    return false;
#endif
}

void LogLastError(DWORD err)
{
    // allow to set a breakpoint in release builds
    if (0 == err)
        err = GetLastError();
    char *msgBuf = NULL;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang =  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    FormatMessageA(flags, NULL, err, lang, (LPSTR)&msgBuf, 0, NULL);
    if (!msgBuf) return;
    plogf("LogLastError: %s", msgBuf);
    LocalFree(msgBuf);
}

// return true if a given registry key (path) exists
bool RegKeyExists(HKEY keySub, const TCHAR *keyName)
{
    HKEY hKey;
    LONG res = RegOpenKey(keySub, keyName, &hKey);
    if (ERROR_SUCCESS == res) {
        RegCloseKey(hKey);
        return true;
    }

    // return true for key that exists even if it's not
    // accessible by us
    return ERROR_ACCESS_DENIED == res;
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
            if (ERROR_SUCCESS != res)
                str::ReplacePtr(&val, NULL);
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
    return val;
}

bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value)
{
    LSTATUS res = SHSetValue(keySub, keyName, valName, REG_SZ, (const VOID *)value, (DWORD)(str::Len(value) + 1) * sizeof(TCHAR));
    return ERROR_SUCCESS == res;
}

bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value)
{
    LSTATUS res = SHSetValue(keySub, keyName, valName, REG_DWORD, (const VOID *)&value, sizeof(DWORD));
    return ERROR_SUCCESS == res;
}

bool CreateRegKey(HKEY keySub, const TCHAR *keyName)
{
    HKEY hKey;
    if (RegCreateKeyEx(keySub, keyName, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return false;
    RegCloseKey(hKey);
    return true;
}

// try to remove any access restrictions on the key
// by granting everybody all access to this key (NULL DACL)
static void ResetRegKeyAcl(HKEY keySub, const TCHAR *keyName)
{
    HKEY hKey;
    LONG res = RegOpenKeyEx(keySub, keyName, 0, WRITE_DAC, &hKey);
    if (ERROR_SUCCESS != res)
        return;
    SECURITY_DESCRIPTOR secdesc;
    InitializeSecurityDescriptor(&secdesc, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&secdesc, TRUE, NULL, TRUE);
    RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &secdesc);
    RegCloseKey(hKey);
}

bool DeleteRegKey(HKEY keySub, const TCHAR *keyName, bool resetACLFirst)
{
    if (resetACLFirst)
        ResetRegKeyAcl(keySub, keyName);

    LSTATUS res = SHDeleteKey(keySub, keyName);
    return ERROR_SUCCESS == res || ERROR_FILE_NOT_FOUND == res;
}

TCHAR *ReadIniString(const TCHAR *iniPath, const TCHAR *section, const TCHAR *key)
{
    DWORD bufCch = 64*512; // so max memory use is 64k
    TCHAR *value = (TCHAR*)malloc(bufCch*sizeof(TCHAR));
    if (value)
        GetPrivateProfileString(section, key, NULL, value, bufCch-1, iniPath);
    return value;
}

#define PROCESS_EXECUTE_FLAGS 0x22

/* enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4 */
typedef HRESULT (WINAPI *_NtSetInformationProcess)(HANDLE ProcessHandle, UINT ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);

#define MEM_EXECUTE_OPTION_DISABLE 0x1
#define MEM_EXECUTE_OPTION_ENABLE 0x2
#define MEM_EXECUTE_OPTION_PERMANENT 0x8
#define MEM_EXECUTE_OPTION_DISABLE_ATL 0x4

typedef BOOL (WINAPI* SetProcessDEPPolicyFunc)(DWORD dwFlags);
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION     0x2
#endif
 
void EnableDataExecution()
{
    // first try the documented SetProcessDEPPolicy
    SetProcessDEPPolicyFunc spdp;
    spdp = (SetProcessDEPPolicyFunc) LoadDllFunc(_T("kernel32.dll"), "SetProcessDEPPolicy");
    if (spdp) {
        spdp(0);
        return;
    }

    // now try undocumented NtSetInformationProcess
    _NtSetInformationProcess ntsip;
    DWORD depMode = MEM_EXECUTE_OPTION_ENABLE | MEM_EXECUTE_OPTION_DISABLE_ATL;

    ntsip = (_NtSetInformationProcess)LoadDllFunc(_T("ntdll.dll"), "NtSetInformationProcess");
    if (ntsip)
        ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS, &depMode, sizeof(depMode));
}

void DisableDataExecution()
{
    // first try the documented SetProcessDEPPolicy
    SetProcessDEPPolicyFunc spdp;
    spdp = (SetProcessDEPPolicyFunc) LoadDllFunc(_T("kernel32.dll"), "SetProcessDEPPolicy");
    if (spdp) {
        spdp(PROCESS_DEP_ENABLE);
        return;
    }

    // now try undocumented NtSetInformationProcess
    _NtSetInformationProcess ntsip;
    DWORD depMode = MEM_EXECUTE_OPTION_DISABLE | MEM_EXECUTE_OPTION_DISABLE_ATL;

    ntsip = (_NtSetInformationProcess)LoadDllFunc(_T("ntdll.dll"), "NtSetInformationProcess");
    if (ntsip)
        ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS, &depMode, sizeof(depMode));
}

// Code from http://www.halcyon.com/~ast/dload/guicon.htm
void RedirectIOToConsole()
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
    hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stderr, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *(FILE *)_fdopen(hConHandle, "r");
    setvbuf(stdin, NULL, _IONBF, 0);
}

/* Return the full exe path of my own executable.
   Caller needs to free() the result. */
TCHAR *GetExePath()
{
    TCHAR buf[MAX_PATH];
    buf[0] = 0;
    GetModuleFileName(NULL, buf, dimof(buf));
    // TODO: is normalization needed here at all?
    return path::Normalize(buf);
}

static ULARGE_INTEGER FileTimeToLargeInteger(const FILETIME& ft)
{
    ULARGE_INTEGER res;
    res.LowPart = ft.dwLowDateTime;
    res.HighPart = ft.dwHighDateTime;
    return res;
}

/* Return <ft1> - <ft2> in seconds */
int FileTimeDiffInSecs(FILETIME& ft1, FILETIME& ft2)
{
    ULARGE_INTEGER t1 = FileTimeToLargeInteger(ft1);
    ULARGE_INTEGER t2 = FileTimeToLargeInteger(ft2);
    // diff is in 100 nanoseconds
    LONGLONG diff = t1.QuadPart - t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (int)diff;
}

TCHAR *ResolveLnk(const TCHAR * path)
{
    ScopedMem<OLECHAR> olePath(str::conv::ToWStr(path));
    if (!olePath)
        return NULL;

    ScopedComPtr<IShellLink> lnk;
    HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hRes))
        return NULL;

    ScopedComQIPtr<IPersistFile> file(lnk);
    if (!file)
        return NULL;

    hRes = file->Load(olePath, STGM_READ);
    if (FAILED(hRes))
        return NULL;

    hRes = lnk->Resolve(NULL, SLR_UPDATE);
    if (FAILED(hRes))
        return NULL;

    TCHAR newPath[MAX_PATH];
    hRes = lnk->GetPath(newPath, MAX_PATH, NULL, 0);
    if (FAILED(hRes))
        return NULL;

    return str::Dup(newPath);
}

bool CreateShortcut(const TCHAR *shortcutPath, const TCHAR *exePath,
                    const TCHAR *args, const TCHAR *description, int iconIndex)
{
    ScopedCom com;
    ScopedComPtr<IShellLink> lnk;

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hr))
        return false;

    ScopedComQIPtr<IPersistFile> file(lnk);
    if (!file)
        return false;

    hr = lnk->SetPath(exePath);
    if (FAILED(hr))
        return false;

    lnk->SetWorkingDirectory(ScopedMem<TCHAR>(path::GetDir(exePath)));
    // lnk->SetShowCmd(SW_SHOWNORMAL);
    // lnk->SetHotkey(0);
    lnk->SetIconLocation(exePath, iconIndex);
    if (args)
        lnk->SetArguments(args);
    if (description)
        lnk->SetDescription(description);

    hr = file->Save(AsWStrQ(shortcutPath), TRUE);
    return SUCCEEDED(hr);
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
IDataObject* GetDataObjectForFile(LPCTSTR filePath, HWND hwnd)
{
    ScopedComPtr<IShellFolder> pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr))
        return NULL;

    IDataObject* pDataObject = NULL;
    ScopedMem<WCHAR> lpWPath(str::conv::ToWStr(filePath));
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(NULL, NULL, lpWPath, NULL, &pidl, NULL);
    if (SUCCEEDED(hr)) {
        ScopedComPtr<IShellFolder> pShellFolder;
        LPCITEMIDLIST pidlChild;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
        if (SUCCEEDED(hr))
            pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, NULL, (void **)&pDataObject);
        CoTaskMemFree(pidl);
    }

    return pDataObject;
}

// The result value contains major and minor version in the high resp. the low WORD
DWORD GetFileVersion(TCHAR *path)
{
    DWORD fileVersion = 0;
    DWORD handle;
    DWORD size = GetFileVersionInfoSize(path, &handle);
    ScopedMem<void> versionInfo(malloc(size));

    if (versionInfo && GetFileVersionInfo(path, handle, size, versionInfo)) {
        VS_FIXEDFILEINFO *fileInfo;
        UINT len;
        if (VerQueryValue(versionInfo, _T("\\"), (LPVOID *)&fileInfo, &len))
            fileVersion = fileInfo->dwFileVersionMS;
    }

    return fileVersion;
}

bool LaunchFile(const TCHAR *path, const TCHAR *params, const TCHAR *verb, bool hidden)
{
    if (!path)
        return false;

    SHELLEXECUTEINFO sei = { 0 };
    sei.cbSize  = sizeof(sei);
    sei.fMask   = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb  = verb;
    sei.lpFile  = path;
    sei.lpParameters = params;
    sei.nShow   = hidden ? SW_HIDE : SW_SHOWNORMAL;
    return ShellExecuteEx(&sei);
}

HANDLE LaunchProcess(TCHAR *cmdLine, const TCHAR *currDir, DWORD flags)
{
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    // per msdn, cmdLine has to be writeable
    if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, flags, NULL, currDir, &si, &pi))
        return NULL;

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
RectI ShiftRectToWorkArea(RectI rect, bool bFully)
{
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof mi;
    GetMonitorInfo(MonitorFromRect(&rect.ToRECT(), MONITOR_DEFAULTTONEAREST), &mi);
    RectI monitor = RectI::FromRECT(mi.rcWork);

    if (rect.y + rect.dy <= monitor.y || bFully && rect.y < monitor.y)
        /* Rectangle is too far above work area */
        rect.Offset(0, monitor.y - rect.y);
    else if (rect.y >= monitor.y + monitor.dy || bFully && rect.y + rect.dy > monitor.y + monitor.dy)
        /* Rectangle is too far below */
        rect.Offset(0, monitor.y - rect.y + monitor.dy - rect.dy);

    if (rect.x + rect.dx <= monitor.x || bFully && rect.x < monitor.x)
        /* Too far left */
        rect.Offset(monitor.x - rect.x, 0);
    else if (rect.x >= monitor.x + monitor.dx || bFully && rect.x + rect.dx > monitor.x + monitor.dx)
        /* Too far right */
        rect.Offset(monitor.x - rect.x + monitor.dx - rect.dx, 0);

    return rect;
}

// returns the dimensions the given window has to have in order to be a fullscreen window
RectI GetFullscreenRect(HWND hwnd)
{
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return RectI::FromRECT(mi.rcMonitor);
    // fall back to the primary monitor
    return RectI(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

static BOOL CALLBACK GetMonitorRectProc(HMONITOR hMonitor, HDC hdc, LPRECT rcMonitor, LPARAM data)
{
    RectI *rcAll = (RectI *)data;
    *rcAll = rcAll->Union(RectI::FromRECT(*rcMonitor));
    return TRUE;
}

// returns the smallest rectangle that covers the entire virtual screen (all monitors)
RectI GetVirtualScreenRect()
{
    RectI result(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    EnumDisplayMonitors(NULL, NULL, GetMonitorRectProc, (LPARAM)&result);
    return result;
}

void PaintRect(HDC hdc, RectI& rect)
{
    MoveToEx(hdc, rect.x, rect.y, NULL);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y);
}

void PaintLine(HDC hdc, RectI& rect)
{
    MoveToEx(hdc, rect.x, rect.y, NULL);
    LineTo(hdc, rect.x + rect.dx, rect.y + rect.dy);
}

void DrawCenteredText(HDC hdc, RectI& r, const TCHAR *txt, bool isRTL)
{
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, -1, &r.ToRECT(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | (isRTL ? DT_RTLREADING : 0));
}

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SizeI TextSizeInHwnd(HWND hwnd, const TCHAR *txt)
{
    SIZE sz;
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ prev = SelectObject(dc, f);
    GetTextExtentPoint32(dc, txt, (int)txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return SizeI(sz.cx, sz.cy);
}

bool IsCursorOverWindow(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    WindowRect rcWnd(hwnd);
    return rcWnd.Contains(PointI(pt.x, pt.y));
}

bool GetCursorPosInHwnd(HWND hwnd, POINT& posOut)
{
    if (!GetCursorPos(&posOut))
        return false;
    if (!ScreenToClient(hwnd, &posOut))
        return false;
    return true;
}

void CenterDialog(HWND hDlg, HWND hParent)
{
    if (!hParent)
        hParent = GetParent(hDlg);

    RectI rcDialog = WindowRect(hDlg);
    rcDialog.Offset(-rcDialog.x, -rcDialog.y);
    RectI rcOwner = WindowRect(hParent ? hParent : GetDesktopWindow());
    RectI rcRect = rcOwner;
    rcRect.Offset(-rcRect.x, -rcRect.y);

    // center dialog on its parent window
    rcDialog.Offset(rcOwner.x + (rcRect.x - rcDialog.x + rcRect.dx - rcDialog.dx) / 2,
                    rcOwner.y + (rcRect.y - rcDialog.y + rcRect.dy - rcDialog.dy) / 2);
    // ensure that the dialog is fully visible on one monitor
    rcDialog = ShiftRectToWorkArea(rcDialog, true);

    SetWindowPos(hDlg, 0, rcDialog.x, rcDialog.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

/* Get the name of default printer or NULL if not exists.
   The caller needs to free() the result */
TCHAR *GetDefaultPrinterName()
{
    TCHAR buf[512];
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize))
        return str::Dup(buf);
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

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (str::Len(text) + 1) * sizeof(TCHAR));
    if (handle) {
        TCHAR *globalText = (TCHAR *)GlobalLock(handle);
        lstrcpy(globalText, text);
        GlobalUnlock(handle);

        SetClipboardData(CF_T_TEXT, handle);
    }

    if (!appendOnly)
        CloseClipboard();

    return handle != NULL;
}

bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly)
{
    if (!appendOnly) {
        if (!OpenClipboard(NULL))
            return false;
        EmptyClipboard();
    }

    bool ok = false;
    if (hbmp) {
        BITMAP bmpInfo;
        GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
        if (bmpInfo.bmBits != NULL) {
            // GDI+ produced HBITMAPs are DIBs instead of DDBs which
            // aren't correctly handled by the clipboard, so create a
            // clipboard-safe clone
            ScopedGdiObj<HBITMAP> ddbBmp((HBITMAP)CopyImage(hbmp,
                IMAGE_BITMAP, bmpInfo.bmWidth, bmpInfo.bmHeight, 0));
            ok = SetClipboardData(CF_BITMAP, ddbBmp) != NULL;
        }
        else
            ok = SetClipboardData(CF_BITMAP, hbmp) != NULL;
    }

    if (!appendOnly)
        CloseClipboard();

    return ok;
}

void ToggleWindowStyle(HWND hwnd, DWORD flag, bool enable, int type)
{
    DWORD style = GetWindowLong(hwnd, type);
    if (enable)
        style = style | flag;
    else
        style = style & ~flag;
    SetWindowLong(hwnd, type, style);
}

DoubleBuffer::DoubleBuffer(HWND hwnd, RectI rect) :
    hTarget(hwnd), rect(rect), hdcBuffer(NULL), doubleBuffer(NULL)
{
    hdcCanvas = ::GetDC(hwnd);

    if (rect.IsEmpty())
        return;

    doubleBuffer = CreateCompatibleBitmap(hdcCanvas, rect.dx, rect.dy);
    if (!doubleBuffer)
        return;

    hdcBuffer = CreateCompatibleDC(hdcCanvas);
    if (!hdcBuffer)
        return;

    if (rect.x != 0 || rect.y != 0) {
        SetGraphicsMode(hdcBuffer, GM_ADVANCED);
        XFORM ctm = { 1.0, 0, 0, 1.0, (float)-rect.x, (float)-rect.y };
        SetWorldTransform(hdcBuffer, &ctm);
    }
    DeleteObject(SelectObject(hdcBuffer, doubleBuffer));
}

DoubleBuffer::~DoubleBuffer()
{
    DeleteObject(doubleBuffer);
    DeleteDC(hdcBuffer);
    ReleaseDC(hTarget, hdcCanvas);
}

void DoubleBuffer::Flush(HDC hdc)
{
    assert(hdc != hdcBuffer);
    if (hdcBuffer)
        BitBlt(hdc, rect.x, rect.y, rect.dx, rect.dy, hdcBuffer, 0, 0, SRCCOPY);
}

namespace win {
    namespace menu {

void SetText(HMENU m, UINT id, TCHAR *s)
{
    MENUITEMINFO mii = { 0 };
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MFT_STRING;
    mii.dwTypeData = s;
    mii.cch = (UINT)str::Len(s);
    SetMenuItemInfo(m, id, FALSE, &mii);
}

/* Make a string safe to be displayed as a menu item
   (preserving all & so that they don't get swallowed)
   Caller needs to free() the result. */
TCHAR *ToSafeString(const TCHAR *str)
{
    if (!str::FindChar(str, '&'))
        return str::Dup(str);

    StrVec ampSplitter;
    ampSplitter.Split(str, _T("&"));
    return ampSplitter.Join(_T("&&"));
}

    }
}

HFONT GetSimpleFont(HDC hdc, TCHAR *fontName, int fontSize)
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
    str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), fontName);
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;

    return CreateFontIndirect(&lf);
}

IStream *CreateStreamFromData(const void *data, size_t len)
{
    if (!data)
        return NULL;

    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream)))
        return NULL;

    ULONG written;
    if (FAILED(stream->Write(data, (ULONG)len, &written)) || written != len)
        return NULL;

    LARGE_INTEGER zero = { 0 };
    stream->Seek(zero, STREAM_SEEK_SET, NULL);

    stream->AddRef();
    return stream;
}

static HRESULT GetDataFromStream(IStream *stream, void **data, ULONG *len)
{
    if (!stream)
        return E_INVALIDARG;

    STATSTG stat;
    HRESULT res = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;
    assert(0 == stat.cbSize.HighPart);
    if (stat.cbSize.HighPart > 0 || stat.cbSize.LowPart > UINT_MAX - sizeof(WCHAR))
        return E_OUTOFMEMORY;

    // zero-terminate the stream's content, so that it could be
    // used directly as either a char* or a WCHAR* string
    *len = stat.cbSize.LowPart;
    *data = malloc(*len + sizeof(WCHAR));
    if (!*data)
        return E_OUTOFMEMORY;

    ULONG read;
    LARGE_INTEGER zero = { 0 };
    stream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = stream->Read(*data, stat.cbSize.LowPart, &read);
    if (FAILED(res) || read != *len) {
        free(*data);
        return res;
    }

    ((char *)*data)[*len] = '\0';
    ((char *)*data)[*len + 1] = '\0';

    return S_OK;
}

void *GetDataFromStream(IStream *stream, size_t *len, HRESULT *res_opt)
{
    void *data;
    ULONG size;
    HRESULT res = GetDataFromStream(stream, &data, &size);
    if (len)
        *len = size;
    if (res_opt)
        *res_opt = res;
    if (FAILED(res))
        return NULL;
    return data;
}

bool ReadDataFromStream(IStream *stream, void *buffer, size_t len, size_t offset)
{
    LARGE_INTEGER off;
    off.QuadPart = offset;
    HRESULT res = stream->Seek(off, STREAM_SEEK_SET, NULL);
    if (FAILED(res))
        return false;
    ULONG read;
    res = stream->Read(buffer, len, &read);
    if (read < len)
        ((char *)buffer)[read] = '\0';
    return SUCCEEDED(res);
}

UINT GuessTextCodepage(const char *data, size_t len, UINT default)
{
    // try to guess the codepage
    ScopedComPtr<IMultiLanguage2> pMLang;
    HRESULT hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_ALL,
                                  IID_IMultiLanguage2, (void **)&pMLang);
    if (FAILED(hr))
        return default;

    int ilen = (int)min(len, INT_MAX);
    int count = 1;
    DetectEncodingInfo info = { 0 };
    hr = pMLang->DetectInputCodepage(MLDETECTCP_NONE, CP_ACP, (char *)data,
                                     &ilen, &info, &count);
    if (FAILED(hr) || count != 1)
        return default;
    return info.nCodePage;
}

namespace win {

TCHAR *GetText(HWND hwnd)
{
    size_t  cchTxtLen = GetTextLen(hwnd);
    TCHAR * txt = (TCHAR*)calloc(cchTxtLen + 1, sizeof(TCHAR));
    if (NULL == txt)
        return NULL;
    SendMessage(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

int GetHwndDpi(HWND hwnd, float *uiDPIFactor)
{
    HDC dc = GetDC(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    if (uiDPIFactor)
        *uiDPIFactor = ceil(dpi * 4.0f / USER_DEFAULT_SCREEN_DPI) / 4.0f;
    ReleaseDC(hwnd, dc);
    return dpi;
}

}

SizeI GetBitmapSize(HBITMAP hbmp)
{
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    return SizeI(bmpInfo.bmWidth, bmpInfo.bmHeight);
}

// cf. fz_mul255 in fitz.h
inline int mul255(int a, int b)
{
    int x = a * b + 128;
    x += x >> 8;
    return x >> 8;
}

void UpdateBitmapColorRange(HBITMAP hbmp, COLORREF range[2])
{
    if ((range[0] & 0xFFFFFF) == WIN_COL_BLACK &&
        (range[1] & 0xFFFFFF) == WIN_COL_WHITE)
        return;

    // color order in DIB is blue-green-red-alpha
    int base[4] = { GetBValue(range[0]), GetGValue(range[0]), GetRValue(range[0]), 0 };
    int diff[4] = {
        GetBValue(range[1]) - base[0],
        GetGValue(range[1]) - base[1],
        GetRValue(range[1]) - base[2],
        255
    };

    HDC hDC = GetDC(NULL);
    BITMAPINFO bmi = { 0 };
    SizeI size = GetBitmapSize(hbmp);

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size.dx;
    bmi.bmiHeader.biHeight = size.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int bmpBytes = size.dx * size.dy * 4;
    ScopedMem<unsigned char> bmpData((unsigned char *)malloc(bmpBytes));
    CrashIf(!bmpData);
    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        for (int i = 0; i < bmpBytes; i++) {
            int k = i % 4;
            bmpData[i] = base[k] + mul255(bmpData[i], diff[k]);
        }
        SetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS);
    }

    ReleaseDC(NULL, hDC);
}

// create data for a .bmp file from this bitmap (if saved to disk, the HBITMAP
// can be deserialized with LoadImage(NULL, ..., LD_LOADFROMFILE) and its
// dimensions determined again with GetBitmapSize(...))
unsigned char *SerializeBitmap(HBITMAP hbmp, size_t *bmpBytesOut)
{
    SizeI size = GetBitmapSize(hbmp);
    DWORD bmpHeaderLen = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO);
    DWORD bmpBytes = ((size.dx * 3 + 3) / 4) * 4 * size.dy + bmpHeaderLen;
    unsigned char *bmpData = SAZA(unsigned char, bmpBytes);
    if (!bmpData)
        return NULL;

    BITMAPINFO *bmi = (BITMAPINFO *)(bmpData + sizeof(BITMAPFILEHEADER));
    bmi->bmiHeader.biSize = sizeof(bmi->bmiHeader);
    bmi->bmiHeader.biWidth = size.dx;
    bmi->bmiHeader.biHeight = size.dy;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 24;
    bmi->bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(NULL);
    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData + bmpHeaderLen, bmi, DIB_RGB_COLORS)) {
        BITMAPFILEHEADER *bmpfh = (BITMAPFILEHEADER *)bmpData;
        bmpfh->bfType = MAKEWORD('B', 'M');
        bmpfh->bfOffBits = bmpHeaderLen;
        bmpfh->bfSize = bmpBytes;
    } else {
        free(bmpData);
        bmpData = NULL;
    }
    ReleaseDC(NULL, hDC);

    if (bmpBytesOut)
        *bmpBytesOut = bmpBytes;
    return bmpData;
}

// This is meant to measure program startup time from the user perspective.
// One place to measure it is at the beginning of WinMain().
// Another place is on the first run of WM_PAINT of the message loop of main window.
double GetProcessRunningTime()
{
    FILETIME currTime, startTime, d1, d2, d3;
    GetSystemTimeAsFileTime(&currTime);
    HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    double timeInMs = 0;
    if (!hproc)
        return 0;
    if (GetProcessTimes(hproc, &startTime, &d1, &d2, &d3)) {
        ULARGE_INTEGER start = FileTimeToLargeInteger(startTime);
        ULARGE_INTEGER curr = FileTimeToLargeInteger(currTime);
        ULONGLONG diff = curr.QuadPart - start.QuadPart;
        // FILETIME is in 100 ns chunks
        timeInMs = ((double)(diff * 100)) / (double)1000000;
    }
    CloseHandle(hproc);
    return timeInMs;
}

typedef BOOL WINAPI SaferCreateLevelProc(DWORD dwScopeId, DWORD dwLevelId, DWORD OpenFlags, SAFER_LEVEL_HANDLE *pLevelHandle, LPVOID lpReserved);
typedef BOOL WINAPI SaferComputeTokenFromLevelProc(SAFER_LEVEL_HANDLE LevelHandle, HANDLE InAccessToken, PHANDLE OutAccessToken, DWORD dwFlags, LPVOID lpReserved);
typedef BOOL WINAPI SaferCloseLevelProc(SAFER_LEVEL_HANDLE hLevelHandle);

// note: the intended purpose of this code was to launch non-elevated process
// from elevated process, but it doesn't seem to do that (we use RunAsUser()
// instead)
HANDLE CreateProcessAtLevel(const TCHAR *exe, const TCHAR *args, DWORD level)
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
    LogLastError();
    _SaferCloseLevel(slh);
    return NULL;
}

// This is just to satisfy /analyze. CloseHandle(NULL) works perfectly fine
// but /analyze complains anyway
BOOL SafeCloseHandle(HANDLE h)
{
    if (!h)
        return TRUE;
    return CloseHandle(h);
}

// This is just to satisfy /analyze. DestroyWindow(NULL) works perfectly fine
// but /analyze complains anyway
BOOL SafeDestroyWindow(HWND hwnd)
{
    if (!hwnd)
        return TRUE;
    return DestroyWindow(hwnd);
}

BOOL SafeDestroyWindow(HWND *hwnd)
{
    if (!hwnd || !*hwnd)
        return TRUE;
    BOOL ok = DestroyWindow(*hwnd);
    *hwnd = NULL;
    return ok;
}

// CreateProcessWithTokenW() only available since Vista
typedef BOOL WINAPI CreateProcessWithTokenWProc(HANDLE hToken, DWORD dwLogonFlags,
    LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags,
    LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);

// Run a given *.exe as a non-elevated (non-admin) process.
// based on http://stackoverflow.com/questions/3298611/run-my-program-asuser
bool RunAsUser(TCHAR *cmd)
{
    CreateProcessWithTokenWProc *_CreateProcessWithTokenW;
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    HANDLE hProcessToken = 0;
    HANDLE hShellProcess = 0;
    HANDLE hShellProcessToken = 0;
    HANDLE hPrimaryToken = 0;
    DWORD retLength, pid;
    TOKEN_PRIVILEGES tkp = { 0 };
    bool ret = false;

    _CreateProcessWithTokenW = (CreateProcessWithTokenWProc*)LoadDllFunc(_T("Advapi32.lib"), "CreateProcessWithTokenW");
    if (!_CreateProcessWithTokenW)
        return false;

    // Enable SeIncreaseQuotaPrivilege in this process (won't work if current process is not elevated)
    BOOL ok = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hProcessToken);
    if (!ok) {
        lf("RunAsUser(): OpenProcessToken() failed");
        goto Error;
    }

    tkp.PrivilegeCount = 1;
    ok = LookupPrivilegeValueW(NULL, SE_INCREASE_QUOTA_NAME, &tkp.Privileges[0].Luid);
    if (!ok) {
        lf("RunAsUser(): LookupPrivilegeValue() failed");
        goto Error;
    }
   
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ok = AdjustTokenPrivileges(hProcessToken, FALSE, &tkp, 0, NULL, &retLength);
    if (!ok || (ERROR_SUCCESS != GetLastError())) {
        lf("RunAsUser(): AdjustTokenPrivileges() failed");
        goto Error;
    }

    // Get an HWND representing the desktop shell.
    // Note: this will fail if the shell is not running (crashed or terminated),
    // or the default shell has been replaced with a custom shell. This also won't
    // return what you probably want if Explorer has been terminated and
    // restarted elevated.b
    HWND hwnd = GetShellWindow();
    if (NULL == hwnd) {
        lf("RunAsUser(): GetShellWindow() failed");
        goto Error;
    }

    // Get the PID of the desktop shell process.
    GetWindowThreadProcessId(hwnd, &pid);
    if (0 == pid) {
        lf("RunAsUser(): GetWindowThreadProcessId() failed");
        goto Error;
    }

    // Open the desktop shell process in order to query it (get its token)
    hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (0 == hShellProcess) {
        lf("RunAsUser(): OpenProcess() failed");
        goto Error;
    }

    // Get the process token of the desktop shell.
    ok = OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellProcessToken);
    if (!ok) {
        lf("RunAsUser(): OpenProcessToken() failed");
        goto Error;
    }

    // Duplicate the shell's process token to get a primary token.
    // Based on experimentation, this is the minimal set of rights required 
    // for CreateProcessWithTokenW (contrary to current documentation).
    //DWORD tokenRights = TOKEN_QUERY || TOKEN_ASSIGN_PRIMARY || TOKEN_DUPLICATE || TOKEN_ADJUST_DEFAULT || TOKEN_ADJUST_SESSIONID || TOKEN_IMPERSONATE;
    // TODO: tokenRights could probably be trimmed but the one above is not enough
    DWORD tokenRights = TOKEN_ALL_ACCESS;
    ok = DuplicateTokenEx(hShellProcessToken, tokenRights, NULL, SecurityImpersonation, TokenPrimary, &hPrimaryToken);
    if (!ok) {
        lf("RunAsUser(): DuplicateTokenEx() failed");
        goto Error;
    }

    si.cb = sizeof(si);
    si.wShowWindow = SW_SHOWNORMAL;
    si.dwFlags = STARTF_USESHOWWINDOW;

    ok = _CreateProcessWithTokenW(hPrimaryToken, 0, NULL, AsWStrQ(cmd), 0, NULL, NULL, &si, &pi);
    if (!ok) {
        lf("RunAsUser(): CreateProcessWithTokenW() failed");
        goto Error;
    }

    ret = true;
Exit:
    CloseHandle(hProcessToken);
    CloseHandle(pi.hProcess);
    SafeCloseHandle(hShellProcessToken);
    SafeCloseHandle(hPrimaryToken);
    SafeCloseHandle(hShellProcess);
    return ret;
Error:
    LogLastError();
    goto Exit;
}

// Note: MS_ENH_RSA_AES_PROV_XP isn't defined in the SDK shipping with VS2008
#ifndef MS_ENH_RSA_AES_PROV_XP
#define MS_ENH_RSA_AES_PROV_XP _T("Microsoft Enhanced RSA and AES Cryptographic Provider (Prototype)")
#endif
#ifndef PROV_RSA_AES
#define PROV_RSA_AES 24
#endif

// MD5 digest that uses Windows' CryptoAPI. It's good for code that doesn't already
// have MD5 code (smaller code) and it's probably faster than most other implementations
// TODO: could try to use CryptoNG available starting in Vista. But then again, would that be worth it?
void CalcMD5DigestWin(const void *data, size_t byteCount, unsigned char digest[16])
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    // http://stackoverflow.com/questions/9794745/ms-cryptoapi-doesnt-work-on-windows-xp-with-cryptacquirecontext
    BOOL ok = CryptAcquireContext(&hProv, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (!ok)
        ok = CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV_XP, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);

    CrashAlwaysIf(!ok);
    ok = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    CrashAlwaysIf(!ok);
    ok = CryptHashData(hHash, (const BYTE*)data, byteCount, 0);
    CrashAlwaysIf(!ok);

    DWORD hashLen;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(16 != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(16 != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv,0);
}

// SHA1 digest that uses Windows' CryptoAPI. It's good for code that doesn't already
// have SHA1 code (smaller code) and it's probably faster than most other implementations
// TODO: hasn't been tested for corectness
void CalcSha1DigestWin(void *data, size_t byteCount, unsigned char digest[32])
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    BOOL ok = CryptAcquireContext(&hProv, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (!ok) {
        // TODO: test this on XP
        ok = CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV_XP, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    }
    CrashAlwaysIf(!ok);
    ok = CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    CrashAlwaysIf(!ok);
    ok = CryptHashData(hHash, (const BYTE*)data, byteCount, 0);
    CrashAlwaysIf(!ok);

    DWORD hashLen;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(32 != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(32 != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv,0);
}
