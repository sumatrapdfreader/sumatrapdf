/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include <mlang.h>

#include "utils/Log.h"

static HFONT gDefaultGuiFont = nullptr;
static HFONT gDefaultGuiFontBold = nullptr;
static HFONT gDefaultGuiFontItalic = nullptr;
static HFONT gDefaultGuiFontBoldItalic = nullptr;

bool ToBool(BOOL b) {
    return b ? true : false;
}

Size BlittableBitmap::GetSize() {
    return size;
}

RenderedBitmap::RenderedBitmap(HBITMAP hbmp, Size size, HANDLE hMap) {
    this->hbmp = hbmp;
    this->hMap = hMap;
    this->size = size;
}

RenderedBitmap::~RenderedBitmap() {
    if (IsValidHandle(hbmp)) {
        DeleteObject(hbmp);
    }
    if (IsValidHandle(hMap)) {
        CloseHandle(hMap);
    }
}

RenderedBitmap* RenderedBitmap::Clone() const {
    HBITMAP hbmp2 = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, size.dx, size.dy, 0);
    return new RenderedBitmap(hbmp2, size);
}

bool RenderedBitmap::IsValid() {
    return hbmp != nullptr;
}

// render the bitmap into the target rectangle (streching and skewing as requird)
bool RenderedBitmap::Blit(HDC hdc, Rect target) {
    return BlitHBITMAP(hbmp, hdc, target);
}

// callers must not delete this (use Clone if you have to modify it)
HBITMAP RenderedBitmap::GetBitmap() const {
    return hbmp;
}

void EditSelectAll(HWND hwnd) {
    Edit_SetSel(hwnd, 0, -1);
}

int EditIdealDy(HWND hwnd, bool hasBorder, int lines) {
    CrashIf(lines < 1);
    CrashIf(lines > 256);

    HFONT hfont = HwndGetFont(hwnd);
    Size s1 = HwndMeasureText(hwnd, "Minimal", hfont);
    // logf("Edit::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    char* txt = HwndGetTextTemp(hwnd);
    Size s2 = HwndMeasureText(hwnd, txt, hfont);
    int dy = std::min(s1.dy, s2.dy);
    if (dy == 0) {
        dy = std::max(s1.dy, s2.dy);
    }
    dy = dy * lines;
    if (hasBorder) {
        dy += DpiScale(hwnd, 8);
    }
    // logf("Edit::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return dy;
}

// HWND should be Edit control
// Should be called after user types Ctrl + Backspace to
// delete word backwards from current cursor position
void EditImplementCtrlBack(HWND hwnd) {
    WCHAR* text = HwndGetTextWTemp(hwnd);
    int selStart = LOWORD(Edit_GetSel(hwnd)), selEnd = selStart;
    // remove the rectangle produced by Ctrl+Backspace
    if (selStart > 0 && text[selStart - 1] == '\x7F') {
        memmove(text + selStart - 1, text + selStart, str::Len(text + selStart - 1) * sizeof(WCHAR));
        HwndSetText(hwnd, text);
        selStart = selEnd = selStart - 1;
    }
    // remove the previous word (and any spacing after it)
    for (; selStart > 0 && str::IsWs(text[selStart - 1]); selStart--) {
        ;
    }
    for (; selStart > 0 && !str::IsWs(text[selStart - 1]); selStart--) {
        ;
    }
    Edit_SetSel(hwnd, selStart, selEnd);
    SendMessageW(hwnd, WM_CLEAR, 0, 0); // delete selected text
}

void ListBox_AppendString_NoSort(HWND hwnd, const WCHAR* txt) {
    ListBox_InsertString(hwnd, -1, txt);
}

// https://learn.microsoft.com/en-us/windows/win32/controls/lb-gettopindex
int ListBoxGetTopIndex(HWND hwnd) {
    auto res = ListBox_GetTopIndex(hwnd);
    return res;
}

// https://learn.microsoft.com/en-us/windows/win32/controls/lb-settopindex
bool ListBoxSetTopIndex(HWND hwnd, int idx) {
    auto res = ListBox_SetTopIndex(hwnd, idx);
    return res != LB_ERR;
}

void InitAllCommonControls() {
    INITCOMMONCONTROLSEX cex{};
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

RECT ClientRECT(HWND hwnd) {
    RECT r;
    ::GetClientRect(hwnd, &r);
    return r;
}

Rect ClientRect(HWND hwnd) {
    RECT rc{};
    ::GetClientRect(hwnd, &rc);
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
    return ToRect(rc);
}

int MapWindowPoints(HWND hwndFrom, HWND hwndTo, Point* points, int nPoints) {
    CrashIf(nPoints > 64);
    POINT pnts[64];
    for (int i = 0; i < nPoints; i++) {
        pnts[i].x = points[i].x;
        pnts[i].y = points[i].y;
    }
    int res = MapWindowPoints(hwndFrom, hwndTo, &pnts[0], (uint)nPoints);
    for (int i = 0; i < nPoints; i++) {
        points[i].x = pnts[i].x;
        points[i].y = pnts[i].y;
    }
    return res;
}

void HwndScreenToClient(HWND hwnd, Point& p) {
    POINT pt = {p.x, p.y};
    ScreenToClient(hwnd, &pt);
    p.x = pt.x;
    p.y = pt.y;
}

// move window to top of Z order (i.e. make it visible to the user)
// but without activation (i.e. capturing focus)
void HwndMakeVisible(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void MoveWindow(HWND hwnd, Rect rect) {
    MoveWindow(hwnd, rect.x, rect.y, rect.dx, rect.dy, TRUE);
}

void MoveWindow(HWND hwnd, RECT* r) {
    MoveWindow(hwnd, r->left, r->top, RectDx(*r), RectDy(*r), TRUE);
}

bool GetOsVersion(OSVERSIONINFOEX& ver) {
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
    return !!ok;
}

TempStr OsNameFromVerTemp(const OSVERSIONINFOEX& ver) {
    if (VER_PLATFORM_WIN32_NT != ver.dwPlatformId) {
        return str::DupTemp("9x");
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 3) {
        return str::DupTemp("8.1"); // or Server 2012 R2
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 2) {
        return str::DupTemp("8"); // or Server 2012
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1) {
        return str::DupTemp("7"); // or Server 2008 R2
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0) {
        return str::DupTemp("Vista"); // or Server 2008
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2) {
        return str::DupTemp("Server 2003");
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1) {
        return str::DupTemp("XP");
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0) {
        return str::DupTemp("2000");
    }
    if (ver.dwMajorVersion == 10) {
        // ver.dwMinorVersion seems to always be 0
        int buildNo = (int)(ver.dwBuildNumber & 0xFFFF);
        AutoFreeStr s = str::Format("10.%d", buildNo);
        return str::DupTemp(s.Get());
    }

    // either a newer or an older NT version, neither of which we support
    AutoFreeStr s = str::Format("NT %u.%u", ver.dwMajorVersion, ver.dwMinorVersion);
    return str::DupTemp(s.Get());
}

TempStr GetWindowsVerTemp() {
    OSVERSIONINFOEX ver{};
    ver.dwOSVersionInfoSize = sizeof(ver);
#pragma warning(push)
#pragma warning(disable : 4996)  // 'GetVersionEx': was declared deprecated
#pragma warning(disable : 28159) // Consider using 'IsWindows*' instead of 'GetVersionExW'
    // see: https://msdn.microsoft.com/en-us/library/windows/desktop/dn424972(v=vs.85).aspx
    // starting with Windows 8.1, GetVersionEx will report a wrong version number
    // unless the OS's GUID has been explicitly added to the compatibility manifest
    BOOL ok = GetVersionExW((OSVERSIONINFO*)&ver); // NOLINT
#pragma warning(pop)
    if (!ok) {
        return str::DupTemp("uknown");
    }
    return OsNameFromVerTemp(ver);
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

bool IsArmBuild() {
    return IS_ARM_64 == 1;
}

// return true if OS and our process have the same arch (i.e. both are 32bit
// or both are 64bit)
bool IsProcessAndOsArchSame() {
    return IsProcess64() == IsOs64();
}

TempStr GetLastErrorStrTemp(DWORD err) {
    err = (err == 0) ? GetLastError() : err;
    if (err == 0) {
        return str::DupTemp("");
    }
    if (err == ERROR_INTERNET_EXTENDED_ERROR) {
        char buf[4096] = {0};
        DWORD bufSize = dimof(buf);
        // TODO: ignoring a case where buffer is too small. 4 kB should be enough for everybody
        InternetGetLastResponseInfoA(&err, buf, &bufSize);
        buf[4095] = 0;
        return str::DupTemp(buf);
    }
    char* msgBuf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD ferr = FormatMessageA(flags, nullptr, err, lang, (LPSTR)&msgBuf, 0, nullptr);
    if (!ferr || !msgBuf) {
        return str::DupTemp("");
    }
    auto res = str::DupTemp(msgBuf);
    LocalFree(msgBuf);
    return res;
}

void LogLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    if (str::Len(msg) > 0) {
        logf("LogLastError: %s\n", msg);
    }
}

void DbgOutLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    OutputDebugStringA(msg);
}

// return true if a given registry key (path) exists
bool RegKeyExists(HKEY hkey, const char* keyName) {
    HKEY hKey;
    WCHAR* keyNameW = ToWstrTemp(keyName);
    LONG res = RegOpenKeyW(hkey, keyNameW, &hKey);
    if (ERROR_SUCCESS == res) {
        RegCloseKey(hKey);
        return true;
    }

    // return true for key that exists even if it's not
    // accessible by us
    return ERROR_ACCESS_DENIED == res;
}

char* ReadRegStrTemp(HKEY hkey, const char* keyName, const char* valName) {
    if (!hkey) {
        return nullptr;
    }
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valNameW = ToWstrTemp(valName);
    WCHAR* val = nullptr;
    REGSAM access = KEY_READ;
    HKEY hKey;
TryAgainWOW64:
    LONG res = RegOpenKeyEx(hkey, keyNameW, 0, access, &hKey);
    if (ERROR_SUCCESS == res) {
        DWORD valLen;
        res = RegQueryValueEx(hKey, valNameW, nullptr, nullptr, nullptr, &valLen);
        if (ERROR_SUCCESS == res) {
            val = AllocArray<WCHAR>(valLen / sizeof(WCHAR) + 1);
            res = RegQueryValueEx(hKey, valNameW, nullptr, nullptr, (LPBYTE)val, &valLen);
            if (ERROR_SUCCESS != res) {
                str::ReplaceWithCopy(&val, nullptr);
            }
        }
        RegCloseKey(hKey);
    }
    if (ERROR_FILE_NOT_FOUND == res && HKEY_LOCAL_MACHINE == hkey && KEY_READ == access) {
// try the (non-)64-bit key as well, as HKLM\Software is not shared between 32-bit and
// 64-bit applications per http://msdn.microsoft.com/en-us/library/aa384253(v=vs.85).aspx
#ifdef _WIN64
        access = KEY_READ | KEY_WOW64_32KEY;
#else
        access = KEY_READ | KEY_WOW64_64KEY;
#endif
        goto TryAgainWOW64;
    }
    char* resv = ToUtf8Temp(val);
    str::Free(val);
    return resv;
}

char* LoggedReadRegStrTemp(HKEY hkey, const char* keyName, const char* valName) {
    auto res = ReadRegStrTemp(hkey, keyName, valName);
    logf("ReadRegStrTemp(%s, %s, %s) => '%s'\n", RegKeyNameWTemp(hkey), keyName, valName, res);
    return res;
}

char* ReadRegStr2Temp(const char* keyName, const char* valName) {
    char* res = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, valName);
    if (!res) {
        res = ReadRegStrTemp(HKEY_CURRENT_USER, keyName, valName);
    }
    return res;
}

char* LoggedReadRegStr2Temp(const char* keyName, const char* valName) {
    char* res = LoggedReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, valName);
    if (!res) {
        res = LoggedReadRegStrTemp(HKEY_CURRENT_USER, keyName, valName);
    }
    return res;
}

bool WriteRegStr(HKEY hkey, const char* keyName, const char* valName, const char* value) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valNameW = ToWstrTemp(valName);
    WCHAR* valueW = ToWstrTemp(value);

    DWORD cbData = (DWORD)(str::Len(valueW) + 1) * sizeof(WCHAR);
    LSTATUS res = SHSetValueW(hkey, keyNameW, valNameW, REG_SZ, (const void*)valueW, cbData);
    return ERROR_SUCCESS == res;
}

bool LoggedWriteRegStr(HKEY hkey, const char* keyName, const char* valName, const char* value) {
    auto res = WriteRegStr(hkey, keyName, valName, value);
    logf("WriteRegStr(%s, %s, %s, %s) => '%d'\n", RegKeyNameWTemp(hkey), keyName, valName, value, res);
    return res;
}

bool ReadRegDWORD(HKEY hkey, const char* keyName, const char* valName, DWORD& value) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valNameW = ToWstrTemp(valName);
    DWORD size = sizeof(DWORD);
    LSTATUS res = SHGetValue(hkey, keyNameW, valNameW, nullptr, &value, &size);
    return ERROR_SUCCESS == res && sizeof(DWORD) == size;
}

bool WriteRegDWORD(HKEY hkey, const char* keyName, const char* valName, DWORD value) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valNameW = ToWstrTemp(valName);
    LSTATUS res = SHSetValueW(hkey, keyNameW, valNameW, REG_DWORD, (const void*)&value, sizeof(DWORD));
    return ERROR_SUCCESS == res;
}

bool LoggedWriteRegDWORD(HKEY hkey, const char* key, const char* valName, DWORD value) {
    auto res = WriteRegDWORD(hkey, key, valName, value);
    logf("WriteRegDWORD(%s, %s, %s, %d) => '%d'\n", RegKeyNameWTemp(hkey), key, valName, (int)value, res);
    return res;
}

bool LoggedWriteRegNone(HKEY hkey, const char* key, const char* valName) {
    WCHAR* keyW = ToWstrTemp(key);
    WCHAR* valNameW = ToWstrTemp(valName);
    LSTATUS res = SHSetValueW(hkey, keyW, valNameW, REG_NONE, nullptr, 0);
    logf("LoggedWriteRegNone(%s, %s, %s) => '%d'\n", RegKeyNameWTemp(hkey), key, valName, res);
    return (ERROR_SUCCESS == res);
}

bool CreateRegKey(HKEY hkey, const char* keyName) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    HKEY hKey;
    LSTATUS res = RegCreateKeyExW(hkey, keyNameW, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(hKey);
    return true;
}

const char* RegKeyNameTemp(HKEY key) {
    if (key == HKEY_LOCAL_MACHINE) {
        return "HKEY_LOCAL_MACHINE";
    }
    if (key == HKEY_CURRENT_USER) {
        return "HKEY_CURRENT_USER";
    }
    if (key == HKEY_CLASSES_ROOT) {
        return "HKEY_CLASSES_ROOT";
    }
    return "RegKeyName: unknown key";
}

const char* RegKeyNameWTemp(HKEY key) {
    auto k = RegKeyNameTemp(key);
    return str::Dup(k);
}

static void ResetRegKeyAcl(HKEY hkey, const char* keyName) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    HKEY hKey;
    LONG res = RegOpenKeyEx(hkey, keyNameW, 0, WRITE_DAC, &hKey);
    if (ERROR_SUCCESS != res) {
        return;
    }
    SECURITY_DESCRIPTOR secdesc;
    InitializeSecurityDescriptor(&secdesc, SECURITY_DESCRIPTOR_REVISION);

#pragma warning(push)
#pragma warning(disable : 6248)
    // "Setting a SECURITY_DESCRIPTOR's DACL to nullptr will result in an unprotected object"
    // https://docs.microsoft.com/en-us/cpp/code-quality/c6248?view=msvc-170
    SetSecurityDescriptorDacl(&secdesc, TRUE, nullptr, TRUE);
#pragma warning(pop)

    RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &secdesc);
    RegCloseKey(hKey);
}

bool DeleteRegKey(HKEY hkey, const char* keyName, bool resetACLFirst) {
    if (resetACLFirst) {
        ResetRegKeyAcl(hkey, keyName);
    }
    WCHAR* keyNameW = ToWstrTemp(keyName);
    LSTATUS res = SHDeleteKeyW(hkey, keyNameW);
    return ERROR_SUCCESS == res || ERROR_FILE_NOT_FOUND == res;
}

bool LoggedDeleteRegKey(HKEY hkey, const char* keyName, bool resetACLFirst) {
    if (resetACLFirst) {
        ResetRegKeyAcl(hkey, keyName);
    }
    WCHAR* keyNameW = ToWstrTemp(keyName);
    LSTATUS res = SHDeleteKeyW(hkey, keyNameW);
    logf("LoggedDeleteRegKey(%s, %s, %d) => %d\n", RegKeyNameWTemp(hkey), keyName, resetACLFirst, res);
    bool ok = (ERROR_SUCCESS == res) || (ERROR_FILE_NOT_FOUND == res);
    if (!ok) {
        LogLastError(res);
    }
    return ok;
}

bool DeleteRegValue(HKEY hkey, const char* keyName, const char* value) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valueW = ToWstrTemp(value);

    auto res = SHDeleteValueW(hkey, keyNameW, valueW);
    return res == ERROR_SUCCESS;
}

bool LoggedDeleteRegValue(HKEY hkey, const char* keyName, const char* valName) {
    WCHAR* keyNameW = ToWstrTemp(keyName);
    WCHAR* valNameW = ToWstrTemp(valName);

    auto res = SHDeleteValueW(hkey, keyNameW, valNameW);
    bool ok = (ERROR_SUCCESS == res) || (ERROR_FILE_NOT_FOUND == res);
    logf("LoggedDeleteRegValue(%s, %s, %s) => %d\n", RegKeyNameWTemp(hkey), keyName, valName, res);
    if (!ok) {
        LogLastError(res);
    }
    return ok;
}

HRESULT CLSIDFromString(const char* lpsz, LPCLSID pclsid) {
    WCHAR* ws = ToWstrTemp(lpsz);
    return CLSIDFromString(ws, pclsid);
}

TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing) {
    if (createIfMissing) {
        csidl = csidl | CSIDL_FLAG_CREATE;
    }
    WCHAR path[MAX_PATH]{};
    HRESULT res = SHGetFolderPathW(nullptr, csidl, nullptr, 0, path);
    if (S_OK != res) {
        return nullptr;
    }
    return ToUtf8Temp(path);
}

// temp directory
TempStr GetTempDirTemp() {
    WCHAR dir[MAX_PATH] = {0};
#if 0 // TODO: only available in 20348, not yet present in SDK
    DWORD cch = 0;
    if (DynGetTempPath2W) {
        cch = DynGetTempPath2W(dimof(dir), dir);
    }
    if (cch == 0) {
        cch = GetTempPathW(dimof(dir), dir);
    }
#else
    DWORD cch = GetTempPathW(dimof(dir), dir);
#endif
    if (cch == 0) {
        return {};
    }
    // TODO: should handle this
    CrashIf(cch >= dimof(dir));
    return ToUtf8Temp(dir, cch);
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

enum class ConsoleRedirectStatus {
    NotRedirected,
    RedirectedToExistingConsole,
    RedirectedToAllocatedConsole,
};

static ConsoleRedirectStatus gConsoleRedirectStatus{ConsoleRedirectStatus::NotRedirected};

// https://www.tillett.info/2013/05/13/how-to-create-a-windows-program-that-works-as-both-as-a-gui-and-console-application/
// TODO: see if https://github.com/apenwarr/fixconsole/blob/master/fixconsole_windows.go would improve things
static void redirectIOToConsole() {
    FILE* con{nullptr};
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        freopen_s(&con, "CONOUT$", "w", stdout);
        // make them unbuffered
        setvbuf(stdin, nullptr, _IONBF, 0);
    }
    h = GetStdHandle(STD_ERROR_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        freopen_s(&con, "CONOUT$", "w", stderr);
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
#if 0 // probably don't need stdin
    freopen_s(&con, "CONIN$", "r", stdin);
    setvbuf(stdout, nullptr, _IONBF, 0);
#endif
}

bool RedirectIOToExistingConsole() {
    if (gConsoleRedirectStatus != ConsoleRedirectStatus::NotRedirected) {
        return true;
    }
    BOOL ok = AttachConsole(ATTACH_PARENT_PROCESS);
    if (!ok) {
        return false;
    }
    gConsoleRedirectStatus = ConsoleRedirectStatus::RedirectedToExistingConsole;
    redirectIOToConsole();
    return true;
}
// returns true if had to allocate new console (i.e. show console window)
// false if redirected to existing console, which means it was launched from a shell
// TODO: also detect redirected i/o as described in
// https://github.com/apenwarr/fixconsole/blob/master/fixconsole_windows.go
bool RedirectIOToConsole() {
    if (gConsoleRedirectStatus != ConsoleRedirectStatus::NotRedirected) {
        return gConsoleRedirectStatus == ConsoleRedirectStatus::RedirectedToAllocatedConsole;
    }

    // first we try to attach to the console of the parent process
    // which could be a cmd shell. If that succeeds, we'll print to
    // shell's console like non-gui program
    // if that fails, assume we were not launched from a shell and
    // will allocate a console of our own
    // TODO: this is not perfect because after Sumatra finishes,
    // the cursor is not at end of text. Could be unsolvable
    gConsoleRedirectStatus = ConsoleRedirectStatus::RedirectedToExistingConsole;
    BOOL ok = AttachConsole(ATTACH_PARENT_PROCESS);
    if (!ok) {
        AllocConsole();
        gConsoleRedirectStatus = ConsoleRedirectStatus::RedirectedToAllocatedConsole;
        // make buffer big enough to allow scrolling
        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        coninfo.dwSize.Y = 500;
        SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
    }
    redirectIOToConsole();
    return gConsoleRedirectStatus == ConsoleRedirectStatus::RedirectedToAllocatedConsole;
}

static void SendEnterKeyToConsole() {
    INPUT ip;
    // Set up a generic keyboard event.
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0; // hardware scan code for key
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;

    // Send the "Enter" key
    ip.ki.wVk = 0x0D;  // virtual-key code for the "Enter" key
    ip.ki.dwFlags = 0; // 0 for key press
    SendInput(1, &ip, sizeof(INPUT));

    // Release the "Enter" key
    ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
    SendInput(1, &ip, sizeof(INPUT));
}

void HandleRedirectedConsoleOnShutdown() {
    switch (gConsoleRedirectStatus) {
        case ConsoleRedirectStatus::NotRedirected:
            return;
        case ConsoleRedirectStatus::RedirectedToAllocatedConsole:
            // wait for user to press any key to close the console window
            system("pause");
            break;
        case ConsoleRedirectStatus::RedirectedToExistingConsole:
            // simulate releasing console. the cursor still doesn't show up
            // at the end of output, but it's better than nothing
            SendEnterKeyToConsole();
            break;
    }
}

// Return the full exe path of my own executable
TempStr GetExePathTemp() {
    WCHAR buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, dimof(buf) - 1);
    return ToUtf8Temp(buf);
}

// Return directory where our executable is located
TempStr GetExeDirTemp() {
    auto path = GetExePathTemp();
    return path::GetDirTemp(path);
}

void ChangeCurrDirToDocuments() {
    char* dir = GetSpecialFolderTemp(CSIDL_MYDOCUMENTS);
    WCHAR* dirW = ToWstrTemp(dir);
    SetCurrentDirectoryW(dirW);
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

char* ResolveLnkTemp(const char* path) {
    WCHAR* pathW = ToWstr(path);
    ScopedMem<OLECHAR> olePath(pathW);
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

    WCHAR newPath[MAX_PATH]{};
    hRes = lnk->GetPath(newPath, MAX_PATH, nullptr, 0);
    if (FAILED(hRes)) {
        return nullptr;
    }

    return ToUtf8Temp(newPath);
}

bool CreateShortcut(const char* shortcutPathA, const char* exePathA, const char* argsA, const char* descriptionA,
                    int iconIndex) {
    ScopedCom com;

    WCHAR* shortcutPath = ToWstrTemp(shortcutPathA);
    WCHAR* exePath = ToWstrTemp(exePathA);
    WCHAR* args = ToWstrTemp(argsA);
    WCHAR* description = ToWstrTemp(descriptionA);

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

    lnk->SetWorkingDirectory(path::GetDirTemp(exePath));
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
IDataObject* GetDataObjectForFile(const char* filePath, HWND hwnd) {
    ScopedComPtr<IShellFolder> pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr)) {
        return nullptr;
    }

    WCHAR* lpWPath = ToWstrTemp(filePath);
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(nullptr, nullptr, lpWPath, nullptr, &pidl, nullptr);
    if (FAILED(hr)) {
        return nullptr;
    }
    ScopedComPtr<IShellFolder> pShellFolder;
    LPCITEMIDLIST pidlChild;
    hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
    CoTaskMemFree(pidl);
    if (FAILED(hr)) {
        return nullptr;
    }
    IDataObject* pDataObject = nullptr;
    hr = pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, nullptr, (void**)&pDataObject);
    if (FAILED(hr)) {
        return nullptr;
    }
    return pDataObject;
}

bool IsKeyPressed(int key) {
    SHORT state = GetKeyState(key);
    SHORT isDown = state & 0x8000;
    return isDown != 0;
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

#if 0
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
#endif

bool LaunchFile(const char* path, const char* params, const char* verb, bool hidden) {
    if (str::IsEmpty(path)) {
        return false;
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = ToWstrTemp(verb);
    sei.lpFile = ToWstrTemp(path);
    sei.lpParameters = ToWstrTemp(params);
    sei.nShow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    BOOL ok = ShellExecuteExW(&sei);
    if (!ok) {
        DWORD err = GetLastError();
        logf("LaunchFile: ShellExecuteExW path: '%s' params: '%s' verb: '%s'\n", path, params, verb);
        LogLastError(err);
        return false;
    }
    return true;
}

bool LaunchBrowser(const char* url) {
    return LaunchFile(url, nullptr, "open");
}

HANDLE LaunchProces(const char* exe, const char* cmdLine) {
    PROCESS_INFORMATION pi = {nullptr};
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    WCHAR* exeW = ToWstrTemp(exe);
    // CreateProcess() might modify cmd line argument, so make a copy
    // in case caller provides a read-only string
    WCHAR* cmdLineW = ToWstrTemp(cmdLine);
    BOOL ok = CreateProcessW(exeW, cmdLineW, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// TODO: not sure why I decided to not use lpAplicationName arg to CreateProcessW()
HANDLE LaunchProcess(const char* cmdLine, const char* currDir, DWORD flags) {
    PROCESS_INFORMATION pi = {nullptr};
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    // CreateProcess() might modify cmd line argument, so make a copy
    // in case caller provides a read-only string
    WCHAR* cmdLineW = ToWstrTemp(cmdLine);
    WCHAR* dirW = ToWstrTemp(currDir);
    if (!CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, FALSE, flags, nullptr, dirW, &si, &pi)) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool CreateProcessHelper(const char* exe, const char* args) {
    if (!args) {
        args = "";
    }
    AutoFreeStr cmd = str::Format("\"%s\" %s", exe, args);
    AutoCloseHandle process = LaunchProcess(cmd);
    return process != nullptr;
}

// return true if the app is running in elevated (as admin)
// TODO: on Vista+ use GetTokenInformation:
// https://social.msdn.microsoft.com/Forums/vstudio/en-US/f64ff4cb-d21b-4d72-b513-fb8eb39f4a3a/how-to-determine-if-a-user-that-created-a-process-doesnt-belong-to-administrators-group
bool IsProcessRunningElevated() {
    PSID adminsGroup = nullptr;

    // Allocate and initialize a SID of the administrators group
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    DWORD sub1 = SECURITY_BUILTIN_DOMAIN_RID;
    DWORD sub2 = DOMAIN_ALIAS_RID_ADMINS;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, sub1, sub2, 0, 0, 0, 0, 0, 0, &adminsGroup)) {
        return false;
    }

    // Determine whether the SID of administrators group is enabled in
    // the primary access token of the process
    BOOL isAdmin = FALSE;
    CheckTokenMembership(nullptr, adminsGroup, &isAdmin);
    FreeSid(adminsGroup);
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

static const DWORD groupsToCheck[] = {
    DOMAIN_ALIAS_RID_USERS,
    // every user belongs to the users group, hence users come before guests
    DOMAIN_ALIAS_RID_GUESTS,
    DOMAIN_ALIAS_RID_POWER_USERS,
    DOMAIN_ALIAS_RID_ADMINS,
};

// return true if the account has admin privileges
// https://github.com/kichik/nsis/blob/de09827b5b651b1d467c17be17bc7e5a98dae70f/Contrib/UserInfo/UserInfo.c#L70
static DWORD GetAccountTypeHelper(bool checkTokenForGroupDeny) {
    HANDLE hToken = nullptr;
    BOOL isMember = FALSE;
    DWORD highestGroup = 0;
    BOOL validTokenGroups = FALSE;
    TOKEN_GROUPS* ptg = NULL;
    DWORD cbTokenGroups;

    BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &hToken) ||
              OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);
    if (!ok) {
        return false;
    }

    // Use "old school" membership check?
    if (!checkTokenForGroupDeny) {
        // We must query the size of the group information associated with
        // the token. Note that we expect a FALSE result from GetTokenInformation
        // because we've given it a NULL buffer. On exit cbTokenGroups will tell
        // the size of the group information.
        if (!GetTokenInformation(hToken, TokenGroups, NULL, 0, &cbTokenGroups) &&
            GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            // Allocate buffer and ask for the group information again.
            // This may fail if an administrator has added this account
            // to an additional group between our first call to
            // GetTokenInformation and this one.
            ptg = (TOKEN_GROUPS*)GlobalAlloc(GPTR, cbTokenGroups);
            if (ptg && GetTokenInformation(hToken, TokenGroups, ptg, cbTokenGroups, &cbTokenGroups)) {
                validTokenGroups = TRUE;
            }
        }
    }

    SID_IDENTIFIER_AUTHORITY systemSid = {SECURITY_NT_AUTHORITY};
    if (validTokenGroups || checkTokenForGroupDeny) {
        PSID psid = nullptr;
        for (size_t i = 0; i < dimof(groupsToCheck); i++) {
            // Create a SID for the local group and then check if it exists in our token
            DWORD sub1 = SECURITY_BUILTIN_DOMAIN_RID;
            DWORD groupID = groupsToCheck[i];
            ok = AllocateAndInitializeSid(&systemSid, 2, sub1, groupID, 0, 0, 0, 0, 0, 0, &psid);
            if (!ok) {
                continue;
            }

            if (checkTokenForGroupDeny) {
                CheckTokenMembership(0, psid, &isMember);
            } else if (validTokenGroups) {
                isMember = FALSE;
                for (DWORD j = 0; !isMember && (j < ptg->GroupCount); j++) {
                    if (EqualSid(ptg->Groups[j].Sid, psid)) {
                        isMember = TRUE;
                    }
                }
            }
            if (isMember) {
                highestGroup = groupID;
            }
            FreeSid(psid);
        }
    }

    if (ptg) {
        GlobalFree(ptg);
    }
    CloseHandle(hToken);
    return highestGroup;
}

// Get highest account type based on current elevation level i.e. the
// account is admin (DOMAIN_ALIAS_RID_ADMINS) only if the user is running elevated
// see groupsToCheck for possible values
DWORD GetAccountType() {
    return GetAccountTypeHelper(true);
}

// Get highest account type of the user. Can return admin (DOMAIN_ALIAS_RID_ADMINS)
// when not running elevated but the account belong to administrator group
// see groupsToCheck for possible values
DWORD GetOriginalAccountType() {
    return GetAccountTypeHelper(false);
}

bool LaunchElevated(const char* path, const char* cmdline) {
    return LaunchFile(path, cmdline, "runas");
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
Rect ShiftRectToWorkArea(Rect rect, HWND hwnd, bool bFully) {
    Rect monitor = GetWorkAreaRect(rect, hwnd);

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
    MONITORINFO mi{};
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
Rect GetWorkAreaRect(Rect rect, HWND hwnd) {
    RECT tmpRect = ToRECT(rect);
    HMONITOR hmon = MonitorFromRect(&tmpRect, MONITOR_DEFAULTTONEAREST);
    if (hwnd) {
        hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    BOOL ok = GetMonitorInfo(hmon, &mi);
    if (!ok) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    return ToRect(mi.rcWork);
}

// returns the dimensions the given window has to have in order to be a fullscreen window
Rect GetFullscreenRect(HWND hwnd) {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        return ToRect(mi.rcMonitor);
    }
    // fall back to the primary monitor
    return Rect(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

static BOOL CALLBACK GetMonitorRectProc(HMONITOR, HDC, LPRECT rcMonitor, LPARAM data) {
    Rect* rcAll = (Rect*)data;
    *rcAll = rcAll->Union(ToRect(*rcMonitor));
    return TRUE;
}

// returns the smallest rectangle that covers the entire virtual screen (all monitors)
Rect GetVirtualScreenRect() {
    Rect result(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    EnumDisplayMonitors(nullptr, nullptr, GetMonitorRectProc, (LPARAM)&result);
    return result;
}

void DrawRect(HDC hdc, const Rect rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y);
}

void DrawLine(HDC hdc, const Rect rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx, rect.y + rect.dy);
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

Point HwndGetCursorPos(HWND hwnd) {
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return {};
    }
    if (!ScreenToClient(hwnd, &pt)) {
        return {};
    }
    return {pt.x, pt.y};
}

bool IsMouseOverRect(HWND hwnd, const Rect& r) {
    Point curPos = HwndGetCursorPos(hwnd);
    return r.Contains(curPos);
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
    rcDialog = ShiftRectToWorkArea(rcDialog, hParent, true);

    SetWindowPos(hDlg, nullptr, rcDialog.x, rcDialog.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

// Get the name of default printer or nullptr if not exists.
char* GetDefaultPrinterNameTemp() {
    WCHAR buf[512] = {0};
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize)) {
        return ToUtf8Temp(buf);
    }
    return nullptr;
}

static bool CopyOrAppendTextToClipboard(const WCHAR* text, bool appendOnly) {
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

static bool CopyOrAppendTextToClipboard(const char* s, bool appendOnly) {
    WCHAR* ws = ToWstrTemp(s);
    return CopyOrAppendTextToClipboard(ws, appendOnly);
}

bool CopyTextToClipboard(const char* s) {
    return CopyOrAppendTextToClipboard(s, false);
}

bool AppendTextToClipboard(const char* s) {
    return CopyOrAppendTextToClipboard(s, true);
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

HFONT GetDefaultGuiFontOfSize(int size) {
    NONCLIENTMETRICS ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -size;
    HFONT fnt = CreateFontIndirectW(&ncm.lfMessageFont);
    return fnt;
}

HFONT GetUserGuiFont(int size, int weightOffset, char* fontName) {
    NONCLIENTMETRICS ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -size;
    if (fontName && !str::EqI(fontName, "automatic")) {
        TempWstr fontNameW = ToWstrTemp(fontName);
        WCHAR* dest = ncm.lfMessageFont.lfFaceName;
        int cchDestBufSize = dimof(ncm.lfMessageFont.lfFaceName);
        StrCatBuffW(dest, fontNameW, cchDestBufSize);
    }
    ncm.lfMessageFont.lfWeight += weightOffset;
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
    NONCLIENTMETRICS ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    if (bold) {
        ncm.lfMessageFont.lfWeight = FW_BOLD;
    }
    if (italic) {
        ncm.lfMessageFont.lfItalic = true;
    }
    *dest = CreateFontIndirectW(&ncm.lfMessageFont);
    return *dest;
}

int GetSizeOfDefaultGuiFont() {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int res = -ncm.lfMessageFont.lfHeight;
    CrashIf(res <= 0);
    return res;
}

DoubleBuffer::DoubleBuffer(HWND hwnd, Rect rect) : hTarget(hwnd), hdcCanvas(::GetDC(hwnd)), rect(rect) {
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

void DoubleBuffer::Flush(HDC hdc) const {
    CrashIf(hdc == hdcBuffer);
    if (hdcBuffer) {
        BitBlt(hdc, rect.x, rect.y, rect.dx, rect.dy, hdcBuffer, 0, 0, SRCCOPY);
    }
}

DeferWinPosHelper::DeferWinPosHelper() : hdwp(::BeginDeferWindowPos(32)) {
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
    this->SetWindowPos(hWnd, nullptr, x, y, cx, cy, uFlags);
}

void DeferWinPosHelper::MoveWindow(HWND hWnd, Rect r) {
    this->MoveWindow(hWnd, r.x, r.y, r.dx, r.dy);
}

void MenuSetChecked(HMENU m, int id, bool isChecked) {
    CrashIf(id < 0);
    CheckMenuItem(m, (UINT)id, MF_BYCOMMAND | (isChecked ? MF_CHECKED : MF_UNCHECKED));
}

bool MenuSetEnabled(HMENU m, int id, bool isEnabled) {
    CrashIf(id < 0);
    BOOL ret = EnableMenuItem(m, (UINT)id, MF_BYCOMMAND | (isEnabled ? MF_ENABLED : MF_GRAYED));
    return ret != -1;
}

void MenuRemove(HMENU m, int id) {
    CrashIf(id < 0);
    RemoveMenu(m, (UINT)id, MF_BYCOMMAND);
}

void MenuEmpty(HMENU m) {
    while (RemoveMenu(m, 0, MF_BYPOSITION)) {
        // no-op
    }
}

void MenuSetText(HMENU m, int id, const WCHAR* s) {
    CrashIf(id < 0);
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MFT_STRING;
    mii.dwTypeData = (WCHAR*)s;
    mii.cch = (uint)str::Len(s);
    BOOL ok = SetMenuItemInfoW(m, id, FALSE, &mii);
    if (!ok) {
        const char* tmp = s ? ToUtf8Temp(s) : "(null)";
        logf("MenuSetText(): id=%d, s='%s'\n", id, tmp);
        LogLastError();
        ReportIf(true);
    }
}

void MenuSetText(HMENU m, int id, const char* s) {
    WCHAR* ws = ToWstrTemp(s);
    MenuSetText(m, id, ws);
}

/* Make a string safe to be displayed as a menu item
   (preserving all & so that they don't get swallowed)
   if no change is needed, the string is returned as is,
   else it's also saved in newResult for automatic freeing */
TempStr MenuToSafeStringTemp(const char* s) {
    auto str = str::DupTemp(s);
    if (!str::FindChar(str, '&')) {
        return str;
    }
    TempStr safe = str::ReplaceTemp(str, "&", "&&");
    return safe;
}

HFONT CreateSimpleFont(HDC hdc, const char* fontName, int fontSize) {
    WCHAR* fontNameW = ToWstrTemp(fontName);
    LOGFONTW lf{};

    lf.lfWidth = 0;
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;
    str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), fontNameW);
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;

    return CreateFontIndirectW(&lf);
}

// https://www.gamedev.net/forums/topic/683205-c-win32-can-i-change-the-menu-font/
// affects size of menu font, system wide
void SetMenuFontSize(int fontSize) {
    NONCLIENTMETRICS m = {0};
    uint sz = sizeof(NONCLIENTMETRICS);
    m.cbSize = sz;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sz, (void*)&m, 0);
    logf("font size: %d\n", m.lfMenuFont.lfHeight);
    m.lfMenuFont.lfHeight = -fontSize;
    SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sz, (void*)&m, 0);
    // TODO: hangs, maybe needs to use SystemParametersInfoForDpi as doc say
    // for per-monitor dpi aware apps
    // CrashIf(true);
}

IStream* CreateStreamFromData(const ByteSlice& d) {
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

    LARGE_INTEGER zero{};
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
    LARGE_INTEGER zero{};
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

ByteSlice GetDataFromStream(IStream* stream, HRESULT* resOpt) {
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

ByteSlice GetStreamOrFileData(IStream* stream, const char* filePath) {
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
    DetectEncodingInfo info{};
    HRESULT hr = pMLang->DetectInputCodepage(MLDETECTCP_NONE, CP_ACP, (char*)data, &ilen, &info, &count);
    if (FAILED(hr) || count != 1) {
        return defVal;
    }
    return info.nCodePage;
}

char* NormalizeString(const char* strA, int /* NORM_FORM */ form) {
    if (!DynNormalizeString) {
        return nullptr;
    }
    WCHAR* str = ToWstrTemp(strA);
    int sizeEst = DynNormalizeString(form, str, -1, nullptr, 0);
    if (sizeEst <= 0) {
        return nullptr;
    }
    // according to MSDN the estimate may be off somewhat:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd319093(v=vs.85).aspx
    sizeEst = sizeEst * 2;
    AutoFreeWstr res(AllocArray<WCHAR>(sizeEst));
    sizeEst = DynNormalizeString(form, str, -1, res, sizeEst);
    if (sizeEst <= 0) {
        return nullptr;
    }
    return ToUtf8(res.Get());
}

bool RegisterOrUnregisterServerDLL(const char* dllPath, bool install, const char* args) {
    if (FAILED(OleInitialize(nullptr))) {
        return false;
    }

    // make sure that the DLL can find any DLLs it depends on and
    // which reside in the same directory (in this case: libmupdf.dll)
    if (DynSetDllDirectoryW) {
        char* dllDir = path::GetDirTemp(dllPath);
        WCHAR* dllDirW = ToWstrTemp(dllDir);
        DynSetDllDirectoryW(dllDirW);
    }

    defer {
        if (DynSetDllDirectoryW) {
            DynSetDllDirectoryW(L"");
        }
        OleUninitialize();
    };

    HMODULE lib = LoadLibraryA(dllPath);
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
            WCHAR* argsW = ToWstrTemp(args);
            ok = SUCCEEDED(DllInstall(install, argsW));
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

bool RegisterServerDLL(const char* dllPath, const char* args) {
    return RegisterOrUnregisterServerDLL(dllPath, true, args);
}

bool UnRegisterServerDLL(const char* dllPath, const char* args) {
    if (!file::Exists(dllPath)) {
        return true;
    }
    return RegisterOrUnregisterServerDLL(dllPath, false, args);
}

void HwndToForeground(HWND hwnd) {
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
}

size_t HwndGetTextLen(HWND hwnd) {
    return (size_t)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

// return text of window or edit control, nullptr in case of an error
TempWstr HwndGetTextWTemp(HWND hwnd) {
    size_t cch = HwndGetTextLen(hwnd);
    size_t nBytes = (cch + 2) * sizeof(WCHAR); // +2 for extra room
    WCHAR* txt = (WCHAR*)Allocator::AllocZero(GetTempAllocator(), nBytes);
    if (nullptr == txt) {
        return nullptr;
    }
    SendMessageW(hwnd, WM_GETTEXT, cch + 1, (LPARAM)txt);
    return txt;
}

// return text of window or edit control, nullptr in case of an error
TempStr HwndGetTextTemp(HWND hwnd) {
    size_t cch = HwndGetTextLen(hwnd);
    size_t nBytes = (cch + 2) * sizeof(WCHAR); // +2 for extra room
    WCHAR* txt = (WCHAR*)Allocator::AllocZero(GetTempAllocator(), nBytes);
    if (nullptr == txt) {
        return nullptr;
    }
    SendMessageW(hwnd, WM_GETTEXT, cch + 1, (LPARAM)txt);
    return ToUtf8Temp(txt);
}

bool HwndHasFrameThickness(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_THICKFRAME);
}

bool HwndHasCaption(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_CAPTION);
}

void HwndSetVisibility(HWND hwnd, bool visible) {
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

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

    DIBSECTION info{};
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

    BITMAPINFO bmi{};
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
    UnpackColor(textColor, rt, gt, bt);
    const int base[4] = {bt, gt, rt, 0};
    byte rb, gb, bb;
    UnpackColor(bgColor, rb, gb, bb);
    int const diff[4] = {(int)bb - base[0], (int)gb - base[1], (int)rb - base[2], 255};

    DIBSECTION info{};
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

    BITMAPINFO bmi{};
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
ByteSlice SerializeBitmap(HBITMAP hbmp) {
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
    BITMAPINFO bmi{};
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

bool IsValidHandle(HANDLE h) {
    return !(h == nullptr || h == INVALID_HANDLE_VALUE);
}

// This is just to satisfy /analyze. CloseHandle(nullptr) works perfectly fine
// but /analyze complains anyway
bool SafeCloseHandle(HANDLE* h) {
    if (!*h) {
        return true;
    }
    BOOL ok = CloseHandle(*h);
    *h = nullptr;
    return !!ok;
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
void RunNonElevated(const char* exePath) {
    AutoFreeStr cmd;
    char* explorerPath;
    char* bufA;
    WCHAR buf[MAX_PATH]{};
    uint res = GetWindowsDirectoryW(buf, dimof(buf));
    if (0 == res || res >= dimof(buf)) {
        goto Run;
    }
    bufA = ToUtf8Temp(buf);
    explorerPath = path::JoinTemp(bufA, "explorer.exe");
    if (!file::Exists(explorerPath)) {
        goto Run;
    }
    cmd.Set(str::Format("\"%s\" \"%s\"", explorerPath, exePath));
Run:
    HANDLE h = LaunchProcess(cmd ? cmd.Get() : exePath);
    SafeCloseHandle(&h);
}

void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu) {
    WINDOWINFO wi{};
    wi.cbSize = sizeof(wi);
    ::GetWindowInfo(hwnd, &wi);

    RECT r{};
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

void HwndScheduleRepaint(HWND hwnd) {
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

ByteSlice LoadDataResource(int resId) {
    HRSRC resSrc = FindResourceW(nullptr, MAKEINTRESOURCE(resId), RT_RCDATA);
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
    char* s = str::Dup(resData, size);
    UnlockResource(res);
    return {(u8*)s, size};
}

static HDDEDATA CALLBACK DdeCallback(UINT, UINT, HCONV, HSZ, HSZ, HDDEDATA, ULONG_PTR, ULONG_PTR) {
    return nullptr;
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

    result = DdeInitializeW(&inst, DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        return false;
    }

    hszServer = DdeCreateStringHandleW(inst, server, CP_WINNEUTRAL);
    if (!hszServer) {
        goto Exit;
    }
    hszTopic = DdeCreateStringHandleW(inst, topic, CP_WINNEUTRAL);
    if (!hszTopic) {
        goto Exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, nullptr);
    if (!hconv) {
        goto Exit;
    }

    cbLen = ((DWORD)str::Len(command) + 1) * sizeof(WCHAR);
    answer = DdeClientTransaction((BYTE*)command, cbLen, hconv, nullptr, CF_UNICODETEXT, XTYP_EXECUTE, 10000, nullptr);
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

static HCURSOR cachedCursors[dimof(knownCursorIds)]{};

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
    logf("SetCursor %s 0x%x %d\n", name, (int)(intptr_t)cursorId, n);
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
            cachedCursors[i] = nullptr;
        }
    }
}

// 0 - metric (centimeters etc.)
// 1 - imperial (inches etc.)
// this triggers drmemory. Force no inlining so that it's easy to write a
// localized suppression
__declspec(noinline) int GetMeasurementSystem() {
    WCHAR unitSystem[2]{};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    if (unitSystem[0] == '0') {
        return 0;
    }
    return 1;
}

// ask for getting WM_MOUSELEAVE for the window
// returns true if started tracking
bool TrackMouseLeave(HWND hwnd) {
    TRACKMOUSEEVENT tme{};
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

// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// A convenient way to grab the same value as HINSTANCE passed to WinMain
HINSTANCE GetInstance() {
    return (HINSTANCE)&__ImageBase;
}

Size ButtonGetIdealSize(HWND hwnd) {
    // adjust to real size and position to the right
    SIZE s{};
    Button_GetIdealSize(hwnd, &s);
    // add padding
    int xPadding = DpiScale(hwnd, 8 * 2);
    int yPadding = DpiScale(hwnd, 2 * 2);
    s.cx += xPadding;
    s.cy += yPadding;
    Size res = {s.cx, s.cy};
    return res;
}

ByteSlice LockDataResource(int id) {
    auto h = GetModuleHandleW(nullptr);
    WCHAR* name = MAKEINTRESOURCEW(id);
    HRSRC resSrc = FindResourceW(h, name, RT_RCDATA);
    if (!resSrc) {
        return {};
    }
    HGLOBAL res = LoadResource(nullptr, resSrc);
    if (!res) {
        return {};
    }
    const u8* data = (const u8*)LockResource(res);
    DWORD dataSize = SizeofResource(nullptr, resSrc);
    return {data, dataSize};
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

void HwndSetText(HWND hwnd, const WCHAR* s) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (!s) {
        SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)L"");
        return;
    }
    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)s);
}

void HwndSetText(HWND hwnd, const char* sv) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (str::IsEmpty(sv)) {
        SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)L"");
        return;
    }
    WCHAR* ws = ToWstrTemp(sv);
    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)ws);
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

// change size of the window to have a given client size
void HwndResizeClientSize(HWND hwnd, int dx, int dy) {
    Rect rc = WindowRect(hwnd);
    int x = rc.x;
    int y = rc.y;
    DWORD style = GetWindowStyle(hwnd);
    DWORD exStyle = GetWindowExStyle(hwnd);
    RECT r = {x, y, x + dx, y + dy};
    BOOL ok = AdjustWindowRectEx(&r, style, false, exStyle);
    CrashIf(!ok);
    int dx2 = RectDx(r);
    int dy2 = RectDy(r);
    ok = SetWindowPos(hwnd, nullptr, 0, 0, dx2, dy2, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREPOSITION);
    CrashIf(!ok);
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
    Rect r = ShiftRectToWorkArea(rHwnd, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void HwndPositionInCenterOf(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = WindowRect(hwndRelative);
    Rect r = WindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);

    Rect r2 = {x, y, r.dx, r.dy};
    r = ShiftRectToWorkArea(r2, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void HwndSendCommand(HWND hwnd, int cmdId) {
    SendMessageW(hwnd, WM_COMMAND, (WPARAM)cmdId, 0);
}

void HwndDestroyWindowSafe(HWND* hwndPtr) {
    auto hwnd = *hwndPtr;
    *hwndPtr = nullptr;

    if (!hwnd || !::IsWindow(hwnd)) {
        return;
    }
    ::DestroyWindow(hwnd);
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

bool DeleteObjectSafe(HGDIOBJ* h) {
    if (!h || !*h) {
        return false;
    }
    auto res = ::DeleteObject(*h);
    *h = nullptr;
    return ToBool(res);
}

bool DeleteFontSafe(HFONT* h) {
    return DeleteObjectSafe((HGDIOBJ*)h);
}

bool DestroyIconSafe(HICON* h) {
    if (!h || !*h) {
        return false;
    }
    auto res = ::DestroyIcon(*h);
    *h = nullptr;
    return ToBool(res);
}

bool TextOutUtf8(HDC hdc, int x, int y, const char* s, int sLen) {
    if (!s) {
        return false;
    }
    if (sLen <= 0) {
        sLen = (int)str::Len(s);
    }
    WCHAR* ws = ToWstrTemp(s, (size_t)sLen);
    if (!ws) {
        return false;
    }
    sLen = (int)str::Len(ws); // TODO: can this be different after converting to WCHAR?
    return TextOutW(hdc, x, y, ws, (int)sLen);
}

bool GetTextExtentPoint32Utf8(HDC hdc, const char* s, int sLen, LPSIZE psizl) {
    *psizl = SIZE{};
    if (!s) {
        return true;
    }
    if (sLen <= 0) {
        sLen = (int)str::Len(s);
    }
    WCHAR* ws = ToWstrTemp(s, sLen);
    if (!ws) {
        return false;
    }
    sLen = (int)str::Len(ws); // TODO: can this be different after converting to WCHAR?
    return GetTextExtentPoint32W(hdc, ws, sLen, psizl);
}

int HdcDrawText(HDC hdc, const char* s, int sLen, RECT* r, UINT format) {
    if (!s) {
        return 0;
    }
    if (sLen <= 0) {
        sLen = (int)str::Len(s);
    }
    WCHAR* ws = ToWstrTemp(s, (size_t)sLen);
    if (!ws) {
        return 0;
    }
    sLen = (int)str::Len(ws);
    return DrawTextW(hdc, ws, sLen, r, format);
}

// uses the same logic as HdcDrawText
Size HdcMeasureText(HDC hdc, const char* s, UINT format) {
    format |= DT_CALCRECT;
    WCHAR* ws = ToWstrTemp(s);
    if (!ws) {
        return {};
    }
    int sLen = (int)str::Len(ws);
    // pick a very large area
    // TODO: allow limiting by dx
    RECT rc{0, 0, 4096, 4096};
    int dy = DrawTextW(hdc, ws, sLen, &rc, format);
    if (0 == dy) {
        return {};
    }
    int dx = RectDx(rc);
    int dy2 = RectDy(rc);
    if (dy2 > dy) {
        dy = dy2;
    }
    return Size(dx, dy);
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

void DrawCenteredText(HDC hdc, const Rect r, const char* txt, bool isRTL) {
    TempWstr ws = ToWstrTemp(txt);
    DrawCenteredText(hdc, r, ws, isRTL);
}

void DrawCenteredText(HDC hdc, const RECT& r, const WCHAR* txt, bool isRTL) {
    Rect rc = ToRect(r);
    DrawCenteredText(hdc, rc, txt, isRTL);
}

// Return size of a text <txt> in a given <hwnd>, taking into account its font
Size TextSizeInHwnd(HWND hwnd, const char* txt, HFONT font) {
    if (!txt || !*txt) {
        return Size{};
    }
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);
    SIZE sz{};
    GetTextExtentPoint32Utf8(dc, txt, (int)txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return Size(sz.cx, sz.cy);
}

// Return size of a text <txt> in a given <hwnd>, taking into account its font
Size TextSizeInHwnd(HWND hwnd, const WCHAR* txt, HFONT font) {
    if (!txt || !*txt) {
        return Size{};
    }
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);
    SIZE sz{};
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

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
Size HwndMeasureText(HWND hwnd, const char* txt, HFONT font) {
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
    HdcDrawText(dc, txt, (int)txtLen, &r, fmt);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    int dx = RectDx(r);
    int dy = RectDy(r);
    return {dx, dy};
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

/* Return size of a text <txt> in a given <hdc>, taking into account its font */
Size TextSizeInDC(HDC hdc, const WCHAR* txt) {
    SIZE sz;
    size_t txtLen = str::Len(txt);
    GetTextExtentPoint32(hdc, txt, (int)txtLen, &sz);
    return Size(sz.cx, sz.cy);
}

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, uint flag, bool subtree) {
    while (hItem) {
        TreeView_Expand(hTree, hItem, flag);
        HTREEITEM child = TreeView_GetChild(hTree, hItem);
        if (child) {
            TreeViewExpandRecursively(hTree, child, flag, false);
        }
        if (subtree) {
            break;
        }
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

void AddPathToRecentDocs(const char* path) {
    WCHAR* pathW = ToWstrTemp(path);
    SHAddToRecentDocs(SHARD_PATH, pathW);
}
