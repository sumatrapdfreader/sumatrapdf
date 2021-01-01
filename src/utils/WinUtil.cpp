/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include <mlang.h>

#include "utils/BitManip.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/WinUtil.h"
// TODO: move function that need Cmd to ResourceIds.h
#include "Commands.h"

#include "utils/Log.h"
#include "utils/LogDbg.h"

static HFONT gDefaultGuiFont = nullptr;
static HFONT gDefaultGuiFontBold = nullptr;
static HFONT gDefaultGuiFontItalic = nullptr;
static HFONT gDefaultGuiFontBoldItalic = nullptr;

int RectDx(const RECT& r) {
    return r.right - r.left;
}
int RectDy(const RECT& r) {
    return r.bottom - r.top;
}

POINT MakePoint(long x, long y) {
    POINT p = {x, y};
    return p;
}

SIZE MakeSize(long dx, long dy) {
    SIZE sz = {dx, dy};
    return sz;
}

RECT MakeRect(long x, long y, long dx, long dy) {
    RECT r;
    r.left = x;
    r.right = x + dx;
    r.top = y;
    r.bottom = y + dy;
    return r;
}

void Edit_SelectAll(HWND hwnd) {
    Edit_SetSel(hwnd, 0, -1);
}

void ListBox_AppendString_NoSort(HWND hwnd, WCHAR* txt) {
    ListBox_InsertString(hwnd, -1, txt);
}

void InitAllCommonControls() {
    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&cex);
}

void FillWndClassEx(WNDCLASSEX& wcex, const WCHAR* clsName, WNDPROC wndproc) {
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hCursor = GetCachedCursor(IDC_ARROW);
    wcex.lpszClassName = clsName;
    wcex.lpfnWndProc = wndproc;
}

RECT GetClientRect(HWND hwnd) {
    RECT r;
    GetClientRect(hwnd, &r);
    return r;
}

Rect ClientRect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    return Rect(rc);
}

Rect WindowRect(HWND hwnd) {
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    return Rect(rc);
}

Rect MapRectToWindow(Rect rect, HWND hwndFrom, HWND hwndTo) {
    RECT rc = ToRECT(rect);
    MapWindowPoints(hwndFrom, hwndTo, (LPPOINT)&rc, 2);
    return Rect::FromRECT(rc);
}

void MoveWindow(HWND hwnd, Rect rect) {
    MoveWindow(hwnd, rect.x, rect.y, rect.dx, rect.dy, TRUE);
}

void MoveWindow(HWND hwnd, RECT* r) {
    MoveWindow(hwnd, r->left, r->top, RectDx(*r), RectDy(*r), TRUE);
}

void GetOsVersion(OSVERSIONINFOEX& ver) {
    ZeroMemory(&ver, sizeof(ver));
    ver.dwOSVersionInfoSize = sizeof(ver);
#pragma warning(push)
#pragma warning(disable : 4996)  // 'GetVersionEx': was declared deprecated
#pragma warning(disable : 28159) // Consider using 'IsWindows*' instead of 'GetVersionExW'
    // see: https://msdn.microsoft.com/en-us/library/windows/desktop/dn424972(v=vs.85).aspx
    // starting with Windows 8.1, GetVersionEx will report a wrong version number
    // unless the OS's GUID has been explicitly added to the compatibility manifest
    BOOL ok = GetVersionEx((OSVERSIONINFO*)&ver); // NOLINT
#pragma warning(pop)
    CrashIf(!ok);
}

// For more versions see OsNameFromVer() in CrashHandler.cpp
bool IsWin10() {
    OSVERSIONINFOEX ver;
    GetOsVersion(ver);
    return ver.dwMajorVersion == 10;
}

bool IsWin7() {
    OSVERSIONINFOEX ver;
    GetOsVersion(ver);
    return ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1;
}

/* Vista is major: 6, minor: 0 */
bool IsVistaOrGreater() {
    OSVERSIONINFOEX osver = {0};
    ULONGLONG condMask = 0;
    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osver.dwMajorVersion = 6;
    VER_SET_CONDITION(condMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    return VerifyVersionInfo(&osver, VER_MAJORVERSION, condMask);
}

bool IsRunningInWow64() {
#ifndef _WIN64
    BOOL isWow = FALSE;
    if (DynIsWow64Process && DynIsWow64Process(GetCurrentProcess(), &isWow)) {
        return isWow == TRUE;
    }
#endif
    return false;
}

// return true if this is 64-bit executable
bool IsProcess64() {
    return 8 == sizeof(void*);
}

// return true if running on a 64-bit OS
bool IsOs64() {
    // 64-bit processes can only run on a 64-bit OS,
    // 32-bit processes run on a 64-bit OS under WOW64
    return IsProcess64() || IsRunningInWow64();
}

// return true if OS and our process have the same arch (i.e. both are 32bit
// or both are 64bit)
bool IsProcessAndOsArchSame() {
    return IsProcess64() == IsOs64();
}

void LogLastError(DWORD err) {
    // allow to set a breakpoint in release builds
    if (0 == err) {
        err = GetLastError();
    }
    char* msgBuf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD res = FormatMessageA(flags, nullptr, err, lang, (LPSTR)&msgBuf, 0, nullptr);
    if (!res || !msgBuf) {
        return;
    }
    logf("LogLastError: %s\n", msgBuf);
    LocalFree(msgBuf);
}

void DbgOutLastError(DWORD err) {
    if (0 == err) {
        err = GetLastError();
    }
    if (0 == err) {
        return;
    }
    char* msgBuf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD res = FormatMessageA(flags, nullptr, err, lang, (LPSTR)&msgBuf, 0, nullptr);
    if (!res || !msgBuf) {
        return;
    }
    OutputDebugStringA(msgBuf);
    LocalFree(msgBuf);
}

// return true if a given registry key (path) exists
bool RegKeyExists(HKEY keySub, const WCHAR* keyName) {
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
WCHAR* ReadRegStr(HKEY keySub, const WCHAR* keyName, const WCHAR* valName) {
    if (!keySub) {
        return nullptr;
    }
    WCHAR* val = nullptr;
    REGSAM access = KEY_READ;
    HKEY hKey;
TryAgainWOW64:
    LONG res = RegOpenKeyEx(keySub, keyName, 0, access, &hKey);
    if (ERROR_SUCCESS == res) {
        DWORD valLen;
        res = RegQueryValueEx(hKey, valName, nullptr, nullptr, nullptr, &valLen);
        if (ERROR_SUCCESS == res) {
            val = AllocArray<WCHAR>(valLen / sizeof(WCHAR) + 1);
            res = RegQueryValueEx(hKey, valName, nullptr, nullptr, (LPBYTE)val, &valLen);
            if (ERROR_SUCCESS != res) {
                str::ReplacePtr(&val, nullptr);
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
    return val;
}

// called needs to free() the result
char* ReadRegStrUtf8(HKEY keySub, const WCHAR* keyName, const WCHAR* valName) {
    WCHAR* ws = ReadRegStr(keySub, keyName, valName);
    if (!ws) {
        return nullptr;
    }
    auto s = strconv::WstrToUtf8(ws);
    str::Free(ws);
    return (char*)s.data();
}

WCHAR* ReadRegStr2(const WCHAR* keyName, const WCHAR* valName) {
    HKEY keySub1 = HKEY_LOCAL_MACHINE;
    HKEY keySub2 = HKEY_CURRENT_USER;
    WCHAR* res = ReadRegStr(keySub1, keyName, valName);
    if (!res) {
        res = ReadRegStr(keySub2, keyName, valName);
    }
    return res;
}

bool WriteRegStr(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, const WCHAR* value) {
    DWORD cbData = (DWORD)(str::Len(value) + 1) * sizeof(WCHAR);
    LSTATUS res = SHSetValueW(keySub, keyName, valName, REG_SZ, (const void*)value, cbData);
    return ERROR_SUCCESS == res;
}

bool ReadRegDWORD(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, DWORD& value) {
    DWORD size = sizeof(DWORD);
    LSTATUS res = SHGetValue(keySub, keyName, valName, nullptr, &value, &size);
    return ERROR_SUCCESS == res && sizeof(DWORD) == size;
}

bool WriteRegDWORD(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, DWORD value) {
    LSTATUS res = SHSetValueW(keySub, keyName, valName, REG_DWORD, (const void*)&value, sizeof(DWORD));
    return ERROR_SUCCESS == res;
}

bool CreateRegKey(HKEY keySub, const WCHAR* keyName) {
    HKEY hKey;
    LSTATUS res = RegCreateKeyEx(keySub, keyName, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(hKey);
    return true;
}

#pragma warning(push)
#pragma warning(disable : 6248) // "Setting a SECURITY_DESCRIPTOR's DACL to nullptr will result in
                                // an unprotected object"
// try to remove any access restrictions on the key
// by granting everybody all access to this key (nullptr DACL)
static void ResetRegKeyAcl(HKEY keySub, const WCHAR* keyName) {
    HKEY hKey;
    LONG res = RegOpenKeyEx(keySub, keyName, 0, WRITE_DAC, &hKey);
    if (ERROR_SUCCESS != res) {
        return;
    }
    SECURITY_DESCRIPTOR secdesc;
    InitializeSecurityDescriptor(&secdesc, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&secdesc, TRUE, nullptr, TRUE);
    RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &secdesc);
    RegCloseKey(hKey);
}
#pragma warning(pop)

bool DeleteRegKey(HKEY keySub, const WCHAR* keyName, bool resetACLFirst) {
    if (resetACLFirst) {
        ResetRegKeyAcl(keySub, keyName);
    }

    LSTATUS res = SHDeleteKeyW(keySub, keyName);
    return ERROR_SUCCESS == res || ERROR_FILE_NOT_FOUND == res;
}

WCHAR* GetSpecialFolder(int csidl, bool createIfMissing) {
    if (createIfMissing) {
        csidl = csidl | CSIDL_FLAG_CREATE;
    }
    WCHAR path[MAX_PATH] = {0};
    HRESULT res = SHGetFolderPath(nullptr, csidl, nullptr, 0, path);
    if (S_OK != res) {
        return nullptr;
    }
    return str::Dup(path);
}

void DisableDataExecution() {
    // first try the documented SetProcessDEPPolicy
    if (DynSetProcessDEPPolicy) {
        DynSetProcessDEPPolicy(PROCESS_DEP_ENABLE);
        return;
    }

    // now try undocumented NtSetInformationProcess
    if (DynNtSetInformationProcess) {
        DWORD depMode = MEM_EXECUTE_OPTION_DISABLE | MEM_EXECUTE_OPTION_DISABLE_ATL;
        HANDLE p = GetCurrentProcess();
        DynNtSetInformationProcess(p, PROCESS_EXECUTE_FLAGS, &depMode, sizeof(depMode));
    }
}

// Code from http://www.halcyon.com/~ast/dload/guicon.htm
// See https://github.com/benvanik/xenia/issues/228 for the VS2015 fix
void RedirectIOToConsole() {
    CONSOLE_SCREEN_BUFFER_INFO coninfo;

    AllocConsole();

    // make buffer big enough to allow scrolling
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = 500;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

// redirect STDIN, STDOUT and STDERR to the console
#if _MSC_VER < 1900
    int hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *_fdopen(hConHandle, "w");

    hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *_fdopen(hConHandle, "w");

    hConHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *_fdopen(hConHandle, "r");
#else
    FILE* con;
    freopen_s(&con, "CONOUT$", "w", stdout);
    freopen_s(&con, "CONOUT$", "w", stderr);
    freopen_s(&con, "CONIN$", "r", stdin);
#endif

    // make them unbuffered
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

/* Return the full exe path of my own executable.
   Caller needs to free() the result. */
WCHAR* GetExePath() {
    WCHAR buf[MAX_PATH] = {0};
    GetModuleFileName(nullptr, buf, dimof(buf));
    // TODO: is normalization needed here at all?
    return path::Normalize(buf);
}

/* Return directory where this executable is located.
Caller needs to free()
*/
WCHAR* GetExeDir() {
    AutoFreeWstr path = GetExePath();
    WCHAR* dir = path::GetDir(path);
    return dir;
}

/*
Returns ${SystemRoot}\system32 directory.
Caller has to free() the result.
*/
WCHAR* GetSystem32Dir() {
    WCHAR buf[1024] = {0};
    DWORD n = GetEnvironmentVariableW(L"SystemRoot", &buf[0], dimof(buf));
    if ((n == 0) || (n >= dimof(buf))) {
        CrashIf(true);
        return str::Dup(L"c:\\windows\\system32");
    }
    return path::Join(buf, L"system32");
}

/*
Returns current directory.
Caller has to free() the result.
*/
WCHAR* GetCurrentDir() {
    DWORD n = GetCurrentDirectoryW(0, nullptr);
    if (0 == n) {
        return nullptr;
    }
    WCHAR* buf = AllocArray<WCHAR>(n + 1);
    DWORD res = GetCurrentDirectoryW(n, buf);
    if (0 == res) {
        return nullptr;
    }
    CrashIf(res > n);
    return buf;
}

void ChangeCurrDirToSystem32() {
    auto sysDir = GetSystem32Dir();
    SetCurrentDirectoryW(sysDir);
    free(sysDir);
}

static ULARGE_INTEGER FileTimeToLargeInteger(const FILETIME& ft) {
    ULARGE_INTEGER res;
    res.LowPart = ft.dwLowDateTime;
    res.HighPart = ft.dwHighDateTime;
    return res;
}

/* Return <ft1> - <ft2> in seconds */
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2) {
    ULARGE_INTEGER t1 = FileTimeToLargeInteger(ft1);
    ULARGE_INTEGER t2 = FileTimeToLargeInteger(ft2);
    // diff is in 100 nanoseconds
    LONGLONG diff = t1.QuadPart - t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (int)diff;
}

WCHAR* ResolveLnk(const WCHAR* path) {
    ScopedMem<OLECHAR> olePath(str::Dup(path));
    if (!olePath) {
        return nullptr;
    }

    ScopedComPtr<IShellLink> lnk;
    if (!lnk.Create(CLSID_ShellLink)) {
        return nullptr;
    }

    ScopedComQIPtr<IPersistFile> file(lnk);
    if (!file) {
        return nullptr;
    }

    HRESULT hRes = file->Load(olePath, STGM_READ);
    if (FAILED(hRes)) {
        return nullptr;
    }

    hRes = lnk->Resolve(nullptr, SLR_UPDATE);
    if (FAILED(hRes)) {
        return nullptr;
    }

    WCHAR newPath[MAX_PATH] = {0};
    hRes = lnk->GetPath(newPath, MAX_PATH, nullptr, 0);
    if (FAILED(hRes)) {
        return nullptr;
    }

    return str::Dup(newPath);
}

bool CreateShortcut(const WCHAR* shortcutPath, const WCHAR* exePath, const WCHAR* args, const WCHAR* description,
                    int iconIndex) {
    ScopedCom com;

    ScopedComPtr<IShellLink> lnk;
    if (!lnk.Create(CLSID_ShellLink)) {
        return false;
    }

    ScopedComQIPtr<IPersistFile> file(lnk);
    if (!file) {
        return false;
    }

    HRESULT hr = lnk->SetPath(exePath);
    if (FAILED(hr)) {
        return false;
    }

    lnk->SetWorkingDirectory(AutoFreeWstr(path::GetDir(exePath)));
    // lnk->SetShowCmd(SW_SHOWNORMAL);
    // lnk->SetHotkey(0);
    lnk->SetIconLocation(exePath, iconIndex);
    if (args) {
        lnk->SetArguments(args);
    }
    if (description) {
        lnk->SetDescription(description);
    }

    hr = file->Save(shortcutPath, TRUE);
    return SUCCEEDED(hr);
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
IDataObject* GetDataObjectForFile(const WCHAR* filePath, HWND hwnd) {
    ScopedComPtr<IShellFolder> pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr)) {
        return nullptr;
    }

    IDataObject* pDataObject = nullptr;
    AutoFreeWstr lpWPath(str::Dup(filePath));
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(nullptr, nullptr, lpWPath, nullptr, &pidl, nullptr);
    if (SUCCEEDED(hr)) {
        ScopedComPtr<IShellFolder> pShellFolder;
        LPCITEMIDLIST pidlChild;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
        if (SUCCEEDED(hr)) {
            hr = pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, nullptr, (void**)&pDataObject);
            if (FAILED(hr)) {
                pDataObject = nullptr;
            }
        }
        CoTaskMemFree(pidl);
    }

    return pDataObject;
}

bool IsKeyPressed(int key) {
    SHORT state = GetKeyState(key);
    SHORT isDown = state & 0x8000;
    return isDown ? true : false;
}

bool IsShiftPressed() {
    return IsKeyPressed(VK_SHIFT);
}

bool IsAltPressed() {
    return IsKeyPressed(VK_MENU);
}

bool IsCtrlPressed() {
    return IsKeyPressed(VK_CONTROL);
}

// The result value contains major and minor version in the high resp. the low WORD
DWORD GetFileVersion(const WCHAR* path) {
    DWORD fileVersion = 0;
    DWORD size = GetFileVersionInfoSize(path, nullptr);
    ScopedMem<void> versionInfo(malloc(size));

    if (versionInfo && GetFileVersionInfo(path, 0, size, versionInfo)) {
        VS_FIXEDFILEINFO* fileInfo;
        uint len;
        if (VerQueryValue(versionInfo, L"\\", (LPVOID*)&fileInfo, &len)) {
            fileVersion = fileInfo->dwFileVersionMS;
        }
    }

    return fileVersion;
}

bool LaunchFile(const WCHAR* path, const WCHAR* params, const WCHAR* verb, bool hidden) {
    if (!path) {
        return false;
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = verb;
    sei.lpFile = path;
    sei.lpParameters = params;
    sei.nShow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    return !!ShellExecuteExW(&sei);
}

bool LaunchBrowser(const WCHAR* url) {
    return LaunchFile(url, nullptr, L"open");
}

HANDLE LaunchProcess(const WCHAR* cmdLine, const WCHAR* currDir, DWORD flags) {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);

    // CreateProcess() might modify cmd line argument, so make a copy
    // in case caller provides a read-only string
    AutoFreeWstr cmdLineCopy(str::Dup(cmdLine));
    if (!CreateProcessW(nullptr, cmdLineCopy, nullptr, nullptr, FALSE, flags, nullptr, currDir, &si, &pi)) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool CreateProcessHelper(const WCHAR* exe, const WCHAR* args) {
    if (!args) {
        args = L"";
    }
    AutoFreeWstr cmd = str::Format(L"\"%s\" %s", exe, args);
    AutoCloseHandle process = LaunchProcess(cmd);
    return process != nullptr;
}

// return true if the app is running in elevated (as admin)
// TODO: on Vista+ use GetTokenInformation linked
// https://social.msdn.microsoft.com/Forums/vstudio/en-US/f64ff4cb-d21b-4d72-b513-fb8eb39f4a3a/how-to-determine-if-a-user-that-created-a-process-doesnt-belong-to-administrators-group?forum=windowssecurity
bool IsProcessRunningElevated() {
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    // Allocate and initialize a SID of the administrators group
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    DWORD sub1 = SECURITY_BUILTIN_DOMAIN_RID;
    DWORD sub2 = DOMAIN_ALIAS_RID_ADMINS;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, sub1, sub2, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        goto Cleanup;
    }

    // Determine whether the SID of administrators group is enabled in
    // the primary access token of the process
    if (!CheckTokenMembership(nullptr, administratorsGroup, &isAdmin)) {
        goto Cleanup;
    }

Cleanup:
    if (administratorsGroup) {
        FreeSid(administratorsGroup);
    }
    return tobool(isAdmin);
}

// We assume that if OpenProcess() works, we are at the same or greater
// elevation level
// I tried to run IsProcessRunningElevated() on 2 processes but this didn't
// work if we're not elevated and other process is (because we can't OpenProcess())
bool CanTalkToProcess(DWORD procId) {
    BOOL inheritHandle = FALSE;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, inheritHandle, procId);
    if (hProc) {
        CloseHandle(hProc);
        return true;
    }
    return false;
}

bool LaunchElevated(const WCHAR* path, const WCHAR* cmdline) {
    return LaunchFile(path, cmdline, L"runas");
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
Rect ShiftRectToWorkArea(Rect rect, bool bFully) {
    Rect monitor = GetWorkAreaRect(rect);

    if (rect.y + rect.dy <= monitor.y || bFully && rect.y < monitor.y) {
        /* Rectangle is too far above work area */
        rect.Offset(0, monitor.y - rect.y);
    } else if (rect.y >= monitor.y + monitor.dy || bFully && rect.y + rect.dy > monitor.y + monitor.dy) {
        /* Rectangle is too far below */
        rect.Offset(0, monitor.y - rect.y + monitor.dy - rect.dy);
    }

    if (rect.x + rect.dx <= monitor.x || bFully && rect.x < monitor.x) {
        /* Too far left */
        rect.Offset(monitor.x - rect.x, 0);
    } else if (rect.x >= monitor.x + monitor.dx || bFully && rect.x + rect.dx > monitor.x + monitor.dx) {
        /* Too far right */
        rect.Offset(monitor.x - rect.x + monitor.dx - rect.dx, 0);
    }

    return rect;
}

// Limits size to max available work area (screen size - taskbar)
void LimitWindowSizeToScreen(HWND hwnd, SIZE& size) {
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof mi;
    BOOL ok = GetMonitorInfo(hmon, &mi);
    if (!ok) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    int dx = RectDx(mi.rcWork);
    if (size.cx > dx) {
        size.cx = dx;
    }
    int dy = RectDy(mi.rcWork);
    if (size.cy > dy) {
        size.cy = dy;
    }
}

// returns available area of the screen i.e. screen minus taskbar area
Rect GetWorkAreaRect(Rect rect) {
    RECT tmpRect = ToRECT(rect);
    HMONITOR hmon = MonitorFromRect(&tmpRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof mi;
    BOOL ok = GetMonitorInfo(hmon, &mi);
    if (!ok) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    return Rect::FromRECT(mi.rcWork);
}

// returns the dimensions the given window has to have in order to be a fullscreen window
Rect GetFullscreenRect(HWND hwnd) {
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        return Rect::FromRECT(mi.rcMonitor);
    }
    // fall back to the primary monitor
    return Rect(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

static BOOL CALLBACK GetMonitorRectProc([[maybe_unused]] HMONITOR hMonitor, [[maybe_unused]] HDC hdc, LPRECT rcMonitor,
                                        LPARAM data) {
    Rect* rcAll = (Rect*)data;
    *rcAll = rcAll->Union(Rect::FromRECT(*rcMonitor));
    return TRUE;
}

// returns the smallest rectangle that covers the entire virtual screen (all monitors)
Rect GetVirtualScreenRect() {
    Rect result(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    EnumDisplayMonitors(nullptr, nullptr, GetMonitorRectProc, (LPARAM)&result);
    return result;
}

void PaintRect(HDC hdc, const Rect rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y);
}

void PaintLine(HDC hdc, const Rect rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx, rect.y + rect.dy);
}

void DrawCenteredText(HDC hdc, const Rect r, const WCHAR* txt, bool isRTL) {
    SetBkMode(hdc, TRANSPARENT);
    RECT tmpRect = ToRECT(r);
    uint format = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    if (isRTL) {
        format |= DT_RTLREADING;
    }
    DrawTextW(hdc, txt, -1, &tmpRect, format);
}

void DrawCenteredText(HDC hdc, const RECT& r, const WCHAR* txt, bool isRTL) {
    Rect rc = Rect::FromRECT(r);
    DrawCenteredText(hdc, rc, txt, isRTL);
}

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
Size TextSizeInHwnd(HWND hwnd, const WCHAR* txt, HFONT font) {
    SIZE sz{};
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, txt, (int)txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return Size(sz.cx, sz.cy);
}

// TODO: unify with TextSizeInHwnd
/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SIZE TextSizeInHwnd2(HWND hwnd, const WCHAR* txt, HFONT font) {
    SIZE sz{};
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
    window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, txt, (int)txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return sz;
}

/* Return size of a text <txt> in a given <hdc>, taking into account its font */
Size TextSizeInDC(HDC hdc, const WCHAR* txt) {
    SIZE sz;
    size_t txtLen = str::Len(txt);
    GetTextExtentPoint32(hdc, txt, (int)txtLen, &sz);
    return Size(sz.cx, sz.cy);
}

bool IsFocused(HWND hwnd) {
    return GetFocus() == hwnd;
}

bool IsCursorOverWindow(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    Rect rcWnd = WindowRect(hwnd);
    return rcWnd.Contains({pt.x, pt.y});
}

bool GetCursorPosInHwnd(HWND hwnd, Point& posOut) {
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return false;
    }
    if (!ScreenToClient(hwnd, &pt)) {
        return false;
    }
    posOut = {pt.x, pt.y};
    return true;
}

void CenterDialog(HWND hDlg, HWND hParent) {
    if (!hParent) {
        hParent = GetParent(hDlg);
    }

    Rect rcDialog = WindowRect(hDlg);
    rcDialog.Offset(-rcDialog.x, -rcDialog.y);
    Rect rcOwner = WindowRect(hParent ? hParent : GetDesktopWindow());
    Rect rcRect = rcOwner;
    rcRect.Offset(-rcRect.x, -rcRect.y);

    // center dialog on its parent window
    rcDialog.Offset(rcOwner.x + (rcRect.x - rcDialog.x + rcRect.dx - rcDialog.dx) / 2,
                    rcOwner.y + (rcRect.y - rcDialog.y + rcRect.dy - rcDialog.dy) / 2);
    // ensure that the dialog is fully visible on one monitor
    rcDialog = ShiftRectToWorkArea(rcDialog, true);

    SetWindowPos(hDlg, 0, rcDialog.x, rcDialog.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

/* Get the name of default printer or nullptr if not exists.
   The caller needs to free() the result */
WCHAR* GetDefaultPrinterName() {
    WCHAR buf[512] = {0};
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize)) {
        return str::Dup(buf);
    }
    return nullptr;
}

bool CopyTextToClipboard(const WCHAR* text, bool appendOnly) {
    CrashIf(!text);
    if (!text) {
        return false;
    }

    if (!appendOnly) {
        if (!OpenClipboard(nullptr)) {
            return false;
        }
        EmptyClipboard();
    }

    size_t n = str::Len(text) + 1;
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(WCHAR));
    if (handle) {
        WCHAR* globalText = (WCHAR*)GlobalLock(handle);
        if (globalText) {
            str::BufSet(globalText, n, text);
        }
        GlobalUnlock(handle);

        SetClipboardData(CF_UNICODETEXT, handle);
    }

    if (!appendOnly) {
        CloseClipboard();
    }

    return handle != nullptr;
}

static bool SetClipboardImage(HBITMAP hbmp) {
    if (!hbmp) {
        return false;
    }
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    HANDLE h = nullptr;
    if (bmpInfo.bmBits != nullptr) {
        // GDI+ produced HBITMAPs are DIBs instead of DDBs which
        // aren't correctly handled by the clipboard, so create a
        // clipboard-safe clone
        ScopedGdiObj<HBITMAP> ddbBmp((HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, bmpInfo.bmWidth, bmpInfo.bmHeight, 0));
        h = SetClipboardData(CF_BITMAP, ddbBmp);
    } else {
        h = SetClipboardData(CF_BITMAP, hbmp);
    }
    return h != nullptr;
}

bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly) {
    if (!appendOnly) {
        if (!OpenClipboard(nullptr)) {
            return false;
        }
        EmptyClipboard();
    }

    bool ok = SetClipboardImage(hbmp);

    if (!appendOnly) {
        CloseClipboard();
    }

    return ok;
}

static void SetWindowStyle(HWND hwnd, DWORD flags, bool enable, int type) {
    DWORD style = GetWindowLong(hwnd, type);
    DWORD newStyle;
    if (enable) {
        newStyle = style | flags;
    } else {
        newStyle = style & ~flags;
    }
    if (newStyle != style) {
        SetWindowLong(hwnd, type, newStyle);
    }
}

bool IsWindowStyleSet(HWND hwnd, DWORD flags) {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    return bit::IsMaskSet<DWORD>(style, flags);
}

bool IsWindowStyleExSet(HWND hwnd, DWORD flags) {
    DWORD style = GetWindowLong(hwnd, GWL_EXSTYLE);
    return (style != flags) != 0;
}

bool IsRtl(HWND hwnd) {
    DWORD style = GetWindowLong(hwnd, GWL_EXSTYLE);
    return bit::IsMaskSet<DWORD>(style, WS_EX_LAYOUTRTL);
}

void SetRtl(HWND hwnd, bool isRtl) {
    SetWindowExStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRtl);
}

void SetWindowStyle(HWND hwnd, DWORD flags, bool enable) {
    SetWindowStyle(hwnd, flags, enable, GWL_STYLE);
}

void SetWindowExStyle(HWND hwnd, DWORD flags, bool enable) {
    SetWindowStyle(hwnd, flags, enable, GWL_EXSTYLE);
}

Rect ChildPosWithinParent(HWND hwnd) {
    POINT pt = {0, 0};
    ClientToScreen(GetParent(hwnd), &pt);
    Rect rc = WindowRect(hwnd);
    rc.Offset(-pt.x, -pt.y);
    return rc;
}

// don't delete the font
HFONT GetDefaultGuiFont() {
    if (gDefaultGuiFont) {
        return gDefaultGuiFont;
    }
    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    return gDefaultGuiFont;
}

HFONT GetDefaultGuiFontOfSize(int size) {
    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -size;
    HFONT fnt = CreateFontIndirectW(&ncm.lfMessageFont);
    return fnt;
}

// TODO: lfUnderline? lfStrikeOut?
HFONT GetDefaultGuiFont(bool bold, bool italic) {
    HFONT* dest = &gDefaultGuiFont;
    if (bold && !italic) {
        dest = &gDefaultGuiFontBold;
    } else if (!bold && italic) {
        dest = &gDefaultGuiFontItalic;
    } else if (bold && italic) {
        dest = &gDefaultGuiFontBoldItalic;
    }
    HFONT existing = *dest;
    if (existing != nullptr) {
        return existing;
    }
    NONCLIENTMETRICS ncm{0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    if (bold) {
        ncm.lfMessageFont.lfWeight = FW_BOLD;
    }
    if (italic) {
        ncm.lfMessageFont.lfItalic = true;
    }
    *dest = CreateFontIndirect(&ncm.lfMessageFont);
    return *dest;
}

int GetSizeOfDefaultGuiFont() {
    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int res = -ncm.lfMessageFont.lfHeight;
    CrashIf(res <= 0);
    return res;
}

DoubleBuffer::DoubleBuffer(HWND hwnd, Rect rect) : hTarget(hwnd), rect(rect) {
    hdcCanvas = ::GetDC(hwnd);

    if (rect.IsEmpty()) {
        return;
    }

    doubleBuffer = CreateCompatibleBitmap(hdcCanvas, rect.dx, rect.dy);
    if (!doubleBuffer) {
        return;
    }

    hdcBuffer = CreateCompatibleDC(hdcCanvas);
    if (!hdcBuffer) {
        return;
    }

    if (rect.x != 0 || rect.y != 0) {
        SetGraphicsMode(hdcBuffer, GM_ADVANCED);
        XFORM ctm = {1.0, 0, 0, 1.0, (float)-rect.x, (float)-rect.y};
        SetWorldTransform(hdcBuffer, &ctm);
    }
    DeleteObject(SelectObject(hdcBuffer, doubleBuffer));
}

DoubleBuffer::~DoubleBuffer() {
    DeleteObject(doubleBuffer);
    DeleteDC(hdcBuffer);
    ReleaseDC(hTarget, hdcCanvas);
}

HDC DoubleBuffer::GetDC() const {
    if (hdcBuffer != nullptr) {
        return hdcBuffer;
    }
    return hdcCanvas;
}

void DoubleBuffer::Flush(HDC hdc) {
    CrashIf(hdc == hdcBuffer);
    if (hdcBuffer) {
        BitBlt(hdc, rect.x, rect.y, rect.dx, rect.dy, hdcBuffer, 0, 0, SRCCOPY);
    }
}

DeferWinPosHelper::DeferWinPosHelper() {
    hdwp = ::BeginDeferWindowPos(32);
}

DeferWinPosHelper::~DeferWinPosHelper() {
    End();
}

void DeferWinPosHelper::End() {
    if (hdwp) {
        ::EndDeferWindowPos(hdwp);
        hdwp = nullptr;
    }
}

void DeferWinPosHelper::SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags) {
    hdwp = ::DeferWindowPos(hdwp, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
}

void DeferWinPosHelper::SetWindowPos(HWND hwnd, const Rect rc) {
    uint flags = SWP_NOZORDER;
    hdwp = ::DeferWindowPos(hdwp, hwnd, nullptr, rc.x, rc.y, rc.dx, rc.dy, flags);
}

void DeferWinPosHelper::MoveWindow(HWND hWnd, int x, int y, int cx, int cy, BOOL bRepaint) {
    uint uFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
    if (!bRepaint) {
        uFlags |= SWP_NOREDRAW;
    }
    this->SetWindowPos(hWnd, 0, x, y, cx, cy, uFlags);
}

void DeferWinPosHelper::MoveWindow(HWND hWnd, Rect r) {
    this->MoveWindow(hWnd, r.x, r.y, r.dx, r.dy);
}

namespace win {
namespace menu {

void SetChecked(HMENU m, int id, bool isChecked) {
    CrashIf(id < 0);
    CheckMenuItem(m, (uint)id, MF_BYCOMMAND | (isChecked ? MF_CHECKED : MF_UNCHECKED));
}

bool SetEnabled(HMENU m, int id, bool isEnabled) {
    CrashIf(id < 0);
    BOOL ret = EnableMenuItem(m, (uint)id, MF_BYCOMMAND | (isEnabled ? MF_ENABLED : MF_GRAYED));
    return ret != -1;
}

void Remove(HMENU m, int id) {
    CrashIf(id < 0);
    RemoveMenu(m, (uint)id, MF_BYCOMMAND);
}

void Empty(HMENU m) {
    while (RemoveMenu(m, 0, MF_BYPOSITION)) {
        // no-op
    }
}

void SetText(HMENU m, int id, const WCHAR* s) {
    CrashIf(id < 0);
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MFT_STRING;
    mii.dwTypeData = (WCHAR*)s;
    mii.cch = (uint)str::Len(s);
    BOOL ok = SetMenuItemInfoW(m, id, FALSE, &mii);
    CrashIf(!ok);
}

/* Make a string safe to be displayed as a menu item
   (preserving all & so that they don't get swallowed)
   if no change is needed, the string is returned as is,
   else it's also saved in newResult for automatic freeing */
const WCHAR* ToSafeString(AutoFreeWstr& s) {
    auto str = s.Get();
    if (!str::FindChar(str, '&')) {
        return str;
    }
    s.Set(str::Replace(str, L"&", L"&&"));
    return s.Get();
}
} // namespace menu
} // namespace win

HFONT CreateSimpleFont(HDC hdc, const WCHAR* fontName, int fontSize) {
    LOGFONTW lf = {0};

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

    return CreateFontIndirectW(&lf);
}

IStream* CreateStreamFromData(std::span<u8> d) {
    if (d.empty()) {
        return nullptr;
    }

    const void* data = d.data();
    size_t len = d.size();
    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        return nullptr;
    }

    ULONG n;
    if (FAILED(stream->Write(data, (ULONG)len, &n)) || n != len) {
        return nullptr;
    }

    LARGE_INTEGER zero = {0};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    stream->AddRef();
    return stream;
}

static HRESULT GetDataFromStream(IStream* stream, void** data, ULONG* len) {
    if (!stream) {
        return E_INVALIDARG;
    }

    STATSTG stat;
    HRESULT res = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res)) {
        return res;
    }
    if (stat.cbSize.HighPart > 0 || stat.cbSize.LowPart > UINT_MAX - sizeof(WCHAR) - 1) {
        return E_OUTOFMEMORY;
    }

    ULONG n = stat.cbSize.LowPart;
    // zero-terminate the stream's content, so that it could be
    // used directly as either a char* or a WCHAR* string
    char* d = AllocArray<char>(n + sizeof(WCHAR) + 1);
    if (!d) {
        return E_OUTOFMEMORY;
    }

    ULONG read;
    LARGE_INTEGER zero = {0};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    res = stream->Read(d, stat.cbSize.LowPart, &read);
    if (FAILED(res) || read != n) {
        free(d);
        return res;
    }

    *len = n;
    *data = d;
    return S_OK;
}

std::span<u8> GetDataFromStream(IStream* stream, HRESULT* resOpt) {
    void* data = nullptr;
    ULONG size = 0;
    HRESULT res = GetDataFromStream(stream, &data, &size);
    if (resOpt) {
        *resOpt = res;
    }
    if (FAILED(res)) {
        free(data);
        return {};
    }
    return {(u8*)data, (size_t)size};
}

std::span<u8> GetStreamOrFileData(IStream* stream, const WCHAR* filePath) {
    if (stream) {
        return GetDataFromStream(stream, nullptr);
    }
    if (!filePath) {
        return {};
    }
    return file::ReadFile(filePath);
}

bool ReadDataFromStream(IStream* stream, void* buffer, size_t len, size_t offset) {
    LARGE_INTEGER off;
    off.QuadPart = offset;
    HRESULT res = stream->Seek(off, STREAM_SEEK_SET, nullptr);
    if (FAILED(res)) {
        return false;
    }
    ULONG read;
#ifdef _WIN64
    for (; len > ULONG_MAX; len -= ULONG_MAX) {
        res = stream->Read(buffer, ULONG_MAX, &read);
        if (FAILED(res) || read != ULONG_MAX) {
            return false;
        }
        len -= ULONG_MAX;
        buffer = (char*)buffer + ULONG_MAX;
    }
#endif
    res = stream->Read(buffer, (ULONG)len, &read);
    return SUCCEEDED(res) && read == len;
}

uint GuessTextCodepage(const char* data, size_t len, uint defVal) {
    // try to guess the codepage
    ScopedComPtr<IMultiLanguage2> pMLang;
    if (!pMLang.Create(CLSID_CMultiLanguage)) {
        return defVal;
    }

    int ilen = std::min((int)len, INT_MAX);
    int count = 1;
    DetectEncodingInfo info = {0};
    HRESULT hr = pMLang->DetectInputCodepage(MLDETECTCP_NONE, CP_ACP, (char*)data, &ilen, &info, &count);
    if (FAILED(hr) || count != 1) {
        return defVal;
    }
    return info.nCodePage;
}

WCHAR* NormalizeString(const WCHAR* str, int /* NORM_FORM */ form) {
    if (!DynNormalizeString) {
        return nullptr;
    }
    int sizeEst = DynNormalizeString(form, str, -1, nullptr, 0);
    if (sizeEst <= 0) {
        return nullptr;
    }
    // according to MSDN the estimate may be off somewhat:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd319093(v=vs.85).aspx
    sizeEst = sizeEst * 3 / 2 + 1;
    AutoFreeWstr res(AllocArray<WCHAR>(sizeEst));
    sizeEst = DynNormalizeString(form, str, -1, res, sizeEst);
    if (sizeEst <= 0) {
        return nullptr;
    }
    return res.StealData();
}

// http://support.microsoft.com/default.aspx?scid=kb;en-us;207132
bool RegisterOrUnregisterServerDLL(const WCHAR* dllPath, bool install, const WCHAR* args) {
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

bool RegisterServerDLL(const WCHAR* dllPath, const WCHAR* args) {
    return RegisterOrUnregisterServerDLL(dllPath, true, args);
}

bool UnRegisterServerDLL(const WCHAR* dllPath, const WCHAR* args) {
    if (!file::Exists(dllPath)) {
        return true;
    }
    return RegisterOrUnregisterServerDLL(dllPath, false, args);
}

namespace win {

void ToForeground(HWND hwnd) {
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
}

/* return text of window or edit control, nullptr in case of an error.
caller needs to free() the result */
WCHAR* GetText(HWND hwnd) {
    size_t cchTxtLen = GetTextLen(hwnd);
    WCHAR* txt = AllocArray<WCHAR>(cchTxtLen + 1);
    if (nullptr == txt) {
        return nullptr;
    }
    SendMessageW(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

str::Str GetTextUtf8(HWND hwnd) {
    size_t cchTxtLen = GetTextLen(hwnd);
    WCHAR* txt = AllocArray<WCHAR>(cchTxtLen + 1);
    if (nullptr == txt) {
        return str::Str();
    }
    SendMessageW(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    AutoFreeStr od = strconv::WstrToUtf8(txt, cchTxtLen);
    free(txt);
    return {od.AsView()};
}

size_t GetTextLen(HWND hwnd) {
    return (size_t)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

void SetText(HWND hwnd, const WCHAR* txt) {
    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)txt);
}

void SetVisibility(HWND hwnd, bool visible) {
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

bool HasFrameThickness(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_THICKFRAME);
}

bool HasCaption(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_CAPTION);
}
} // namespace win

Size GetBitmapSize(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    return Size(bmpInfo.bmWidth, bmpInfo.bmHeight);
}

// cf. fz_mul255 in fitz.h
inline int mul255(int a, int b) {
    int x = a * b + 128;
    x += x >> 8;
    return x >> 8;
}

void FinalizeBitmapPixels(BitmapPixels* bitmapPixels) {
    HDC hdc = bitmapPixels->hdc;
    if (hdc) {
        SetDIBits(bitmapPixels->hdc, bitmapPixels->hbmp, 0, bitmapPixels->size.dy, bitmapPixels->pixels,
                  &bitmapPixels->bmi, DIB_RGB_COLORS);
        DeleteDC(hdc);
    }
    free(bitmapPixels);
}

static bool IsPalettedBitmap(DIBSECTION& info, int nBytes) {
    return sizeof(info) == nBytes && info.dsBmih.biBitCount != 0 && info.dsBmih.biBitCount <= 8;
}

COLORREF GetPixel(BitmapPixels* bitmap, int x, int y) {
    CrashIf(x < 0 || x >= bitmap->size.dx);
    CrashIf(y < 0 || y >= bitmap->size.dy);
    u8* pixels = bitmap->pixels;
    u8* pixel = pixels + y * bitmap->nBytesPerRow + x * bitmap->nBytesPerPixel;
    // color order in DIB is blue-green-red-alpha
    COLORREF c = 0;
    if (3 == bitmap->nBytesPerPixel) {
        c = RGB(pixel[2], pixel[1], pixel[0]);
    } else if (4 == bitmap->nBytesPerPixel) {
        c = RGB(pixel[3], pixel[2], pixel[1]);
    } else {
        CrashIf(true);
    }
    return c;
}

BitmapPixels* GetBitmapPixels(HBITMAP hbmp) {
    BitmapPixels* res = AllocStruct<BitmapPixels>();

    DIBSECTION info = {0};
    int nBytes = GetObject(hbmp, sizeof(info), &info);
    CrashIf(nBytes < sizeof(info.dsBm));
    Size size(info.dsBm.bmWidth, info.dsBm.bmHeight);

    res->size = size;
    res->hbmp = hbmp;

    if (nBytes >= sizeof(info.dsBm)) {
        res->pixels = (u8*)info.dsBm.bmBits;
    }

    // for mapped 32-bit DI bitmaps: directly access the pixel data
    if (res->pixels && 32 == info.dsBm.bmBitsPixel && size.dx * 4 == info.dsBm.bmWidthBytes) {
        res->nBytesPerPixel = 4;
        res->nBytesPerRow = info.dsBm.bmWidthBytes;
        res->nBytes = size.dx * size.dy * 4;
        return res;
    }

    // for mapped 24-bit DI bitmaps: directly access the pixel data
    if (res->pixels && 24 == info.dsBm.bmBitsPixel && info.dsBm.bmWidthBytes >= size.dx * 3) {
        res->nBytesPerPixel = 3;
        res->nBytesPerRow = info.dsBm.bmWidthBytes;
        res->nBytes = size.dx * size.dy * 4;
        return res;
    }

    // we don't support paletted DI bitmaps
    if (IsPalettedBitmap(info, nBytes)) {
        FinalizeBitmapPixels(res);
        return nullptr;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size.dx;
    bmi.bmiHeader.biHeight = size.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = CreateCompatibleDC(nullptr);
    int bmpBytes = size.dx * size.dy * 4;
    ScopedMem<u8> bmpData((u8*)malloc(bmpBytes));
    CrashIf(!bmpData);

    if (!GetDIBits(hdc, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        DeleteDC(hdc);
        FinalizeBitmapPixels(res);
        return nullptr;
    }
    res->hdc = hdc;
    return res;
}

void UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor) {
    if ((textColor & 0xFFFFFF) == WIN_COL_BLACK && (bgColor & 0xFFFFFF) == WIN_COL_WHITE) {
        return;
    }

    // color order in DIB is blue-green-red-alpha
    byte rt, gt, bt;
    UnpackRgb(textColor, rt, gt, bt);
    int base[4] = {bt, gt, rt, 0};
    byte rb, gb, bb;
    UnpackRgb(bgColor, rb, gb, bb);
    int diff[4] = {(int)bb - base[0], (int)gb - base[1], (int)rb - base[2], 255};

    DIBSECTION info = {0};
    int ret = GetObject(hbmp, sizeof(info), &info);
    CrashIf(ret < sizeof(info.dsBm));
    Size size(info.dsBm.bmWidth, info.dsBm.bmHeight);

    // for mapped 32-bit DI bitmaps: directly access the pixel data
    if (ret >= sizeof(info.dsBm) && info.dsBm.bmBits && 32 == info.dsBm.bmBitsPixel &&
        size.dx * 4 == info.dsBm.bmWidthBytes) {
        int bmpBytes = size.dx * size.dy * 4;
        u8* bmpData = (u8*)info.dsBm.bmBits;
        for (int i = 0; i < bmpBytes; i++) {
            int k = i % 4;
            bmpData[i] = (u8)(base[k] + mul255(bmpData[i], diff[k]));
        }
        return;
    }

    // for mapped 24-bit DI bitmaps: directly access the pixel data
    if (ret >= sizeof(info.dsBm) && info.dsBm.bmBits && 24 == info.dsBm.bmBitsPixel &&
        info.dsBm.bmWidthBytes >= size.dx * 3) {
        u8* bmpData = (u8*)info.dsBm.bmBits;
        for (int y = 0; y < size.dy; y++) {
            for (int x = 0; x < size.dx * 3; x++) {
                int k = x % 3;
                bmpData[x] = (u8)(base[k] + mul255(bmpData[x], diff[k]));
            }
            bmpData += info.dsBm.bmWidthBytes;
        }
        return;
    }

    // for paletted DI bitmaps: only update the color palette
    if (sizeof(info) == ret && info.dsBmih.biBitCount && info.dsBmih.biBitCount <= 8) {
        CrashIf(info.dsBmih.biBitCount != 8);
        RGBQUAD palette[256];
        HDC hDC = CreateCompatibleDC(nullptr);
        DeleteObject(SelectObject(hDC, hbmp));
        uint num = GetDIBColorTable(hDC, 0, dimof(palette), palette);
        for (uint i = 0; i < num; i++) {
            palette[i].rgbRed = (u8)(base[2] + mul255(palette[i].rgbRed, diff[2]));
            palette[i].rgbGreen = (u8)(base[1] + mul255(palette[i].rgbGreen, diff[1]));
            palette[i].rgbBlue = (u8)(base[0] + mul255(palette[i].rgbBlue, diff[0]));
        }
        if (num > 0) {
            SetDIBColorTable(hDC, 0, num, palette);
        }
        DeleteDC(hDC);
        return;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size.dx;
    bmi.bmiHeader.biHeight = size.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = CreateCompatibleDC(nullptr);
    int bmpBytes = size.dx * size.dy * 4;
    ScopedMem<u8> bmpData((u8*)malloc(bmpBytes));
    CrashIf(!bmpData);

    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        for (int i = 0; i < bmpBytes; i++) {
            int k = i % 4;
            bmpData[i] = (u8)(base[k] + mul255(bmpData[i], diff[k]));
        }
        SetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS);
    }

    DeleteDC(hDC);
}

// create data for a .bmp file from this bitmap (if saved to disk, the HBITMAP
// can be deserialized with LoadImage(nullptr, ..., LD_LOADFROMFILE) and its
// dimensions determined again with GetBitmapSize(...))
std::span<u8> SerializeBitmap(HBITMAP hbmp) {
    Size size = GetBitmapSize(hbmp);
    DWORD bmpHeaderLen = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO);
    DWORD bmpBytes = ((size.dx * 3 + 3) / 4) * 4 * size.dy + bmpHeaderLen;
    u8* bmpData = AllocArray<u8>(bmpBytes);
    if (!bmpData) {
        return {};
    }

    BITMAPINFO* bmi = (BITMAPINFO*)(bmpData + sizeof(BITMAPFILEHEADER));
    bmi->bmiHeader.biSize = sizeof(bmi->bmiHeader);
    bmi->bmiHeader.biWidth = size.dx;
    bmi->bmiHeader.biHeight = size.dy;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 24;
    bmi->bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(nullptr);
    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData + bmpHeaderLen, bmi, DIB_RGB_COLORS)) {
        BITMAPFILEHEADER* bmpfh = (BITMAPFILEHEADER*)bmpData;
        bmpfh->bfType = MAKEWORD('B', 'M');
        bmpfh->bfOffBits = bmpHeaderLen;
        bmpfh->bfSize = bmpBytes;
    } else {
        free(bmpData);
        bmpData = nullptr;
    }
    ReleaseDC(nullptr, hDC);

    return {(u8*)bmpData, bmpBytes};
}

HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size.dx;
    bmi.bmiHeader.biHeight = -size.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    // trading speed for memory (32 bits yields far better performance)
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biSizeImage = size.dx * 4 * size.dy;

    void* data = nullptr;
    if (hDataMapping && !*hDataMapping) {
        *hDataMapping =
            CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmi.bmiHeader.biSizeImage, nullptr);
    }
    return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &data, hDataMapping ? *hDataMapping : nullptr, 0);
}

// render the bitmap into the target rectangle (streching and skewing as requird)
bool BlitHBITMAP(HBITMAP hbmp, HDC hdc, Rect target) {
    HDC bmpDC = CreateCompatibleDC(hdc);
    if (!bmpDC) {
        return false;
    }

    BITMAP bi{};
    GetObject(hbmp, sizeof(BITMAP), &bi);
    int dx = bi.bmWidth;
    int dy = bi.bmHeight;

    HGDIOBJ oldBmp = SelectObject(bmpDC, hbmp);
    if (!oldBmp) {
        DeleteDC(bmpDC);
        return false;
    }
    SetStretchBltMode(hdc, HALFTONE);
    int x = target.x;
    int y = target.y;
    int tdx = target.dx;
    int tdy = target.dy;
    bool ok = StretchBlt(hdc, x, y, tdx, tdy, bmpDC, 0, 0, dx, dy, SRCCOPY);
    SelectObject(bmpDC, oldBmp);
    DeleteDC(bmpDC);
    return ok;
}

// This is meant to measure program startup time from the user perspective.
// One place to measure it is at the beginning of WinMain().
// Another place is on the first run of WM_PAINT of the message loop of main window.
double GetProcessRunningTime() {
    FILETIME currTime, startTime, d1, d2, d3;
    GetSystemTimeAsFileTime(&currTime);
    HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    double timeInMs = 0;
    if (!hproc) {
        return 0;
    }
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

// This is just to satisfy /analyze. CloseHandle(nullptr) works perfectly fine
// but /analyze complains anyway
BOOL SafeCloseHandle(HANDLE* h) {
    if (!*h) {
        return TRUE;
    }
    BOOL ok = CloseHandle(*h);
    *h = nullptr;
    return ok;
}

// based on http://mdb-blog.blogspot.com/2013/01/nsis-lunch-program-as-user-from-uac.html
// uses $WINDIR\explorer.exe to launch cmd
// Other promising approaches:
// - http://blogs.msdn.com/b/oldnewthing/archive/2013/11/18/10468726.aspx
// - http://brandonlive.com/2008/04/27/getting-the-shell-to-run-an-application-for-you-part-2-how/
// - http://www.codeproject.com/Articles/23090/Creating-a-process-with-Medium-Integration-Level-f
// Approaches tried but didn't work:
// - http://stackoverflow.com/questions/3298611/run-my-program-asuser
// - using CreateProcessAsUser() with hand-crafted token
// It'll always run the process, might fail to run non-elevated if fails to find explorer.exe
// Also, if explorer.exe is running elevated, it'll probably run elevated as well.
void RunNonElevated(const WCHAR* exePath) {
    AutoFreeWstr cmd, explorerPath;
    WCHAR buf[MAX_PATH] = {0};
    uint res = GetWindowsDirectory(buf, dimof(buf));
    if (0 == res || res >= dimof(buf)) {
        goto Run;
    }
    explorerPath.Set(path::Join(buf, L"explorer.exe"));
    if (!file::Exists(explorerPath)) {
        goto Run;
    }
    cmd.Set(str::Format(L"\"%s\" \"%s\"", explorerPath.Get(), exePath));
Run:
    HANDLE h = LaunchProcess(cmd ? cmd.Get() : exePath);
    SafeCloseHandle(&h);
}

void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu) {
    WINDOWINFO wi = {0};
    wi.cbSize = sizeof(wi);
    GetWindowInfo(hwnd, &wi);

    RECT r = {0};
    r.right = dx;
    r.bottom = dy;
    DWORD style = wi.dwStyle;
    DWORD exStyle = wi.dwExStyle;
    AdjustWindowRectEx(&r, style, hasMenu, exStyle);
    if ((dx == RectDx(wi.rcClient)) && (dy == RectDy(wi.rcClient))) {
        return;
    }

    dx = RectDx(r);
    dy = RectDy(r);
    int x = wi.rcWindow.left;
    int y = wi.rcWindow.top;
    MoveWindow(hwnd, x, y, dx, dy, TRUE);
}

// -1 to use existing value
void ResizeWindow(HWND hwnd, int dx, int dy) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    if (dx == -1) {
        dx = RectDx(rc);
    }
    if (dy == -1) {
        dy = RectDy(rc);
    }
    SetWindowPos(hwnd, nullptr, 0, 0, dx, dy, SWP_NOMOVE | SWP_NOZORDER);
}

void MessageBoxWarningSimple(HWND hwnd, const WCHAR* msg, const WCHAR* title) {
    uint type = MB_OK | MB_ICONEXCLAMATION;
    if (!title) {
        title = L"Warning";
    }
    MessageBox(hwnd, msg, title, type);
}

void MessageBoxNYI(HWND hwnd) {
    MessageBoxWarningSimple(hwnd, L"Not Yet Implemented!", L"NYI");
}

void ScheduleRepaint(HWND hwnd) {
    InvalidateRect(hwnd, nullptr, FALSE);
}

// do WM_PAINT immediately
void RepaintNow(HWND hwnd) {
    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
}

void VariantInitBstr(VARIANT& urlVar, const WCHAR* s) {
    VariantInit(&urlVar);
    urlVar.vt = VT_BSTR;
    urlVar.bstrVal = SysAllocString(s);
}

std::span<u8> LoadDataResource(int resId) {
    HRSRC resSrc = FindResource(nullptr, MAKEINTRESOURCE(resId), RT_RCDATA);
    CrashIf(!resSrc);
    if (!resSrc) {
        return {};
    }
    HGLOBAL res = LoadResource(nullptr, resSrc);
    CrashIf(!res);
    if (!res) {
        return {};
    }
    DWORD size = SizeofResource(nullptr, resSrc);
    const char* resData = (const char*)LockResource(res);
    CrashIf(!resData);
    if (!resData) {
        return {};
    }
    char* s = str::DupN(resData, size);
    UnlockResource(res);
    return {(u8*)s, size};
}

static HDDEDATA CALLBACK DdeCallback([[maybe_unused]] UINT uType, [[maybe_unused]] UINT uFmt,
                                     [[maybe_unused]] HCONV hconv, [[maybe_unused]] HSZ hsz1, [[maybe_unused]] HSZ hsz2,
                                     [[maybe_unused]] HDDEDATA hdata, [[maybe_unused]] ULONG_PTR dwData1,
                                     [[maybe_unused]] ULONG_PTR dwData2) {
    return 0;
}

bool DDEExecute(const WCHAR* server, const WCHAR* topic, const WCHAR* command) {
    DWORD inst = 0;
    HSZ hszServer = nullptr, hszTopic = nullptr;
    HCONV hconv = nullptr;
    bool ok = false;
    uint result = 0;
    DWORD cbLen = 0;
    HDDEDATA answer;

    CrashIf(str::Len(command) >= INT_MAX - 1);
    if (str::Len(command) >= INT_MAX - 1) {
        return false;
    }

    result = DdeInitialize(&inst, DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        return false;
    }

    hszServer = DdeCreateStringHandle(inst, server, CP_WINNEUTRAL);
    if (!hszServer) {
        goto Exit;
    }
    hszTopic = DdeCreateStringHandle(inst, topic, CP_WINNEUTRAL);
    if (!hszTopic) {
        goto Exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, nullptr);
    if (!hconv) {
        goto Exit;
    }

    cbLen = ((DWORD)str::Len(command) + 1) * sizeof(WCHAR);
    answer = DdeClientTransaction((BYTE*)command, cbLen, hconv, 0, CF_UNICODETEXT, XTYP_EXECUTE, 10000, nullptr);
    if (answer) {
        DdeFreeDataHandle(answer);
        ok = true;
    }

Exit:
    if (hconv) {
        DdeDisconnect(hconv);
    }
    if (hszTopic) {
        DdeFreeStringHandle(inst, hszTopic);
    }
    if (hszServer) {
        DdeFreeStringHandle(inst, hszServer);
    }
    DdeUninitialize(inst);

    return ok;
}

// given r,  sets r1, r2 and r3 so that:
//  [         r       ]
//  [ r1 ][  r2 ][ r3 ]
//        ^     ^
//        y     y+dy
void DivideRectV(const RECT& r, int x, int dx, RECT& r1, RECT& r2, RECT& r3) {
    r1 = r2 = r3 = r;
    r1.right = x;
    r2.left = x;
    r2.right = x + dx;
    r3.left = x + dx + 1;
}

// like DivideRectV
void DivideRectH(const RECT& r, int y, int dy, RECT& r1, RECT& r2, RECT& r3) {
    r1 = r2 = r3 = r;
    r1.bottom = y;
    r2.top = y;
    r2.bottom = y + dy;
    r3.top = y + dy + 1;
}

void RectInflateTB(RECT& r, int top, int bottom) {
    r.top += top;
    r.bottom += bottom;
}

static LPWSTR knownCursorIds[] = {IDC_ARROW,  IDC_IBEAM,  IDC_HAND, IDC_SIZEALL,
                                  IDC_SIZEWE, IDC_SIZENS, IDC_NO,   IDC_CROSS};

static HCURSOR cachedCursors[dimof(knownCursorIds)] = {};

static int GetCursorIndex(LPWSTR cursorId) {
    int n = (int)dimof(knownCursorIds);
    for (int i = 0; i < n; i++) {
        if (cursorId == knownCursorIds[i]) {
            return i;
        }
    }
    return -1;
}

#if 0
static const char* cursorNames =
    "IDC_ARROW\0IDC_BEAM\0IDC_HAND\0IDC_SIZEALL\0IDC_SIZEWE\0IDC_SIZENS\0IDC_NO\0IDC_CROSS\0";

static const char* GetCursorName(LPWSTR cursorId) {
    int i = GetCursorIndex(cursorId);
    if (i == -1) {
        return "unknown";
    }
    return seqstrings::IdxToStr(cursorNames, i);
}

static void LogCursor(LPWSTR cursorId) {
    static int n = 0;
    const char* name = GetCursorName(cursorId);
    dbglogf("SetCursor %s 0x%x %d\n", name, (int)(intptr_t)cursorId, n);
    n++;
}
#else
static void LogCursor(LPWSTR) {
    // no-op
}
#endif

HCURSOR GetCachedCursor(LPWSTR cursorId) {
    int i = GetCursorIndex(cursorId);
    CrashIf(i < 0);
    if (i < 0) {
        return nullptr;
    }
    if (nullptr == cachedCursors[i]) {
        cachedCursors[i] = LoadCursor(nullptr, cursorId);
        CrashIf(cachedCursors[i] == nullptr);
    }
    return cachedCursors[i];
}

void SetCursorCached(LPWSTR cursorId) {
    LogCursor(cursorId);
    HCURSOR c = GetCachedCursor(cursorId);
    HCURSOR prevCursor = GetCursor();
    if (c == prevCursor) {
        return;
    }
    SetCursor(c);
}

void DeleteCachedCursors() {
    for (int i = 0; i < dimof(knownCursorIds); i++) {
        HCURSOR cur = cachedCursors[i];
        if (cur) {
            DestroyCursor(cur);
            cachedCursors[i] = NULL;
        }
    }
}

// 0 - metric (centimeters etc.)
// 1 - imperial (inches etc.)
// this triggers drmemory. Force no inlining so that it's easy to write a
// localized suppression
__declspec(noinline) int GetMeasurementSystem() {
    WCHAR unitSystem[2] = {0};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    if (unitSystem[0] == '0') {
        return 0;
    }
    return 1;
}

// ask for getting WM_MOUSELEAVE for the window
// returns true if started tracking
bool TrackMouseLeave(HWND hwnd) {
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_QUERY;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);
    if (0 != (tme.dwFlags & TME_LEAVE)) {
        // is already tracking
        return false;
    }
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);
    return true;
}

void TriggerRepaint(HWND hwnd) {
    InvalidateRect(hwnd, nullptr, FALSE);
}

POINT GetCursorPosInHwnd(HWND hwnd) {
    POINT pt = {0, 0};
    if (GetCursorPos(&pt)) {
        ScreenToClient(hwnd, &pt);
    }
    return pt;
}

// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// A convenient way to grab the same value as HINSTANCE passed to WinMain
HINSTANCE GetInstance() {
    return (HINSTANCE)&__ImageBase;
}

void HwndDpiAdjust(HWND hwnd, float* x, float* y) {
    auto dpi = DpiGet(hwnd);

    if (x != nullptr) {
        float dpiFactor = (float)dpi / 96.f;
        *x = *x * dpiFactor;
    }

    if (y != nullptr) {
        float dpiFactor = (float)dpi / 96.f;
        *y = *y * dpiFactor;
    }
}

Size ButtonGetIdealSize(HWND hwnd) {
    // adjust to real size and position to the right
    SIZE s{};
    Button_GetIdealSize(hwnd, &s);
    // add padding
    float xPadding = 8 * 2;
    float yPadding = 2 * 2;
    HwndDpiAdjust(hwnd, &xPadding, &yPadding);
    s.cx += (int)xPadding;
    s.cy += (int)yPadding;
    Size res = {s.cx, s.cy};
    return res;
}

std::tuple<const u8*, DWORD, HGLOBAL> LockDataResource(int id) {
    auto h = GetModuleHandleW(nullptr);
    WCHAR* name = MAKEINTRESOURCEW(id);
    HRSRC resSrc = FindResourceW(h, name, RT_RCDATA);
    if (!resSrc) {
        return {nullptr, 0, 0};
    }
    HGLOBAL res = LoadResource(nullptr, resSrc);
    if (!res) {
        return {nullptr, 0, 0};
    }

    auto* data = (const u8*)LockResource(res);
    DWORD dataSize = SizeofResource(nullptr, resSrc);
    return {data, dataSize, res};
}

bool IsValidDelayType(int type) {
    switch (type) {
        case TTDT_AUTOPOP:
        case TTDT_INITIAL:
        case TTDT_RESHOW:
        case TTDT_AUTOMATIC:
            return true;
    }
    return false;
}

void HwndSetText(HWND hwnd, std::string_view sv) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (sv.empty()) {
        SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)L"");
        return;
    }
    AutoFreeWstr ws = strconv::Utf8ToWstr(sv);
    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)ws.Get());
}

// https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-seticon
HICON HwndSetIcon(HWND hwnd, HICON icon) {
    if (!hwnd || !icon) {
        return nullptr;
    }
    HICON res = (HICON)SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    return res;
}

// https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-geticon
HICON HwndGetIcon(HWND hwnd) {
    HICON res = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
    return res;
}

void HwndInvalidate(HWND hwnd) {
    if (hwnd) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void HwndSetFont(HWND hwnd, HFONT font) {
    if (!hwnd || !font) {
        return;
    }
    SetWindowFont(hwnd, font, TRUE);
}

HFONT HwndGetFont(HWND hwnd) {
    if (!hwnd) {
        return nullptr;
    }
    auto res = GetWindowFont(hwnd);
    return res;
}

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
Size HwndMeasureText(HWND hwnd, const WCHAR* txt, HFONT font) {
    SIZE sz{};
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);

    RECT r{};
    uint fmt = DT_CALCRECT | DT_LEFT | DT_NOCLIP | DT_EDITCONTROL;
    DrawTextExW(dc, (WCHAR*)txt, (int)txtLen, &r, fmt, nullptr);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    int dx = RectDx(r);
    int dy = RectDy(r);
    return {dx, dy};
}

// position hwnd on the right of hwndRelative
void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative) {
    Rect rHwnd = WindowRect(hwnd);
    Rect rHwndRelative = WindowRect(hwndRelative);
    rHwnd.x = rHwndRelative.x + rHwndRelative.dx;
    rHwnd.y = rHwndRelative.y;
    // position hwnd vertically in the middle of hwndRelative
    int dyDiff = rHwndRelative.dy - rHwnd.dy;
    if (dyDiff > 0) {
        rHwnd.y += dyDiff / 2;
    }
    Rect r = ShiftRectToWorkArea(rHwnd, true);
    SetWindowPos(hwnd, 0, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void HwndSendCommand(HWND hwnd, int cmdId) {
    SendMessageW(hwnd, WM_COMMAND, (WPARAM)cmdId, 0);
}

void TbSetButtonInfo(HWND hwnd, int buttonId, TBBUTTONINFO* info) {
    auto res = SendMessageW(hwnd, TB_SETBUTTONINFO, buttonId, (LPARAM)info);
    CrashIf(0 == res);
}

void TbGetPadding(HWND hwnd, int* padX, int* padY) {
    DWORD res = (DWORD)SendMessageW(hwnd, TB_GETPADDING, 0, 0);
    *padX = (int)LOWORD(res);
    *padY = (int)HIWORD(res);
}

void TbSetPadding(HWND hwnd, int padX, int padY) {
    LPARAM lp = MAKELPARAM(padX, padY);
    auto res = SendMessageW(hwnd, TB_SETPADDING, 0, lp);
    CrashIf(0 == res);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/tb-getrect
void TbGetRect(HWND hwnd, int buttonId, RECT* rc) {
    auto res = SendMessageW(hwnd, TB_GETRECT, buttonId, (LPARAM)rc);
    CrashIf(res == 0);
}

void TbGetMetrics(HWND hwnd, TBMETRICS* metrics) {
    LPARAM lp = (LPARAM)metrics;
    SendMessageW(hwnd, TB_GETMETRICS, 0, lp);
}

void TbSetMetrics(HWND hwnd, TBMETRICS* metrics) {
    LPARAM lp = (LPARAM)metrics;
    SendMessageW(hwnd, TB_SETMETRICS, 0, lp);
}
