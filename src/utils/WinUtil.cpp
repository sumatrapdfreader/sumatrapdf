/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include <shlwapi.h>
#include <shlobj.h>
#include <io.h>
#include <fcntl.h>

#include "BaseUtil.h"
#include "FileUtil.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "Scopes.h"

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

void SeeLastError(DWORD err)
{
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
    if (ERROR_SUCCESS != res && ERROR_FILE_NOT_FOUND != res)
        SeeLastError(res);
    return val;
}

bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value)
{
    LSTATUS res = SHSetValue(keySub, keyName, valName, REG_SZ, (const VOID *)value, (DWORD)(str::Len(value) + 1) * sizeof(TCHAR));
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

bool CreateRegKey(HKEY keySub, const TCHAR *keyName)
{
    HKEY hKey;
    if (RegCreateKeyEx(keySub, keyName, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return false;
    RegCloseKey(hKey);
    return true;
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

void EnableNx()
{
    _NtSetInformationProcess ntsip;
    DWORD dep_mode = 13; /* ENABLE | DISABLE_ATL | PERMANENT */

    ntsip = (_NtSetInformationProcess)LoadDllFunc(_T("ntdll.dll"), "NtSetInformationProcess");
    if (ntsip)
        ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS, &dep_mode, sizeof(dep_mode));
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

static ULARGE_INTEGER FileTimeToLargeInteger(FILETIME& ft)
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

#ifndef _UNICODE
    hr = file->Save(ScopedMem<WCHAR>(str::conv::ToWStr(shortcutPath)), TRUE);
#else
    hr = file->Save(shortcutPath, TRUE);
#endif
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

HANDLE LaunchProcess(TCHAR *cmdLine, DWORD flags)
{
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    // per msdn, cmdLine has to be writeable
    if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi)) {
        SeeLastError();
        return NULL;
    }

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
    return rcWnd.Inside(PointI(pt.x, pt.y));
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

        if (!SetClipboardData(CF_T_TEXT, handle))
            SeeLastError();
    }

    if (!appendOnly)
        CloseClipboard();

    return handle != NULL;
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

IStream *CreateStreamFromData(void *data, size_t len)
{
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

HRESULT GetDataFromStream(IStream *stream, void **data, size_t *len)
{
    if (!data || !len) return E_INVALIDARG;

    STATSTG stat;
    HRESULT res = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;
    assert(0 == stat.cbSize.HighPart);
    if (stat.cbSize.HighPart > 0 || stat.cbSize.LowPart > UINT_MAX - 2)
        return E_OUTOFMEMORY;

    // zero terminate the stream's content, so that it could be
    // used directly as either a char* or a WCHAR* string
    *len = stat.cbSize.LowPart;
    *data = malloc(*len + 2);
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

void InvertBitmapColors(HBITMAP hbmp)
{
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
    unsigned char *bmpData = (unsigned char *)malloc(bmpBytes);
    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        for (int i = 0; i < bmpBytes; i++) {
            // don't affect the alpha channel
            if ((i + 1) % 4)
                bmpData[i] = 255 - bmpData[i];
        }
        SetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS);
    }

    free(bmpData);
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
