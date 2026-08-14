/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/BitManip.h"
#include "base/File.h"
#include "base/WinDynCalls.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include <aclapi.h>
#include <bitset>
#if COMPILER_MINGW
#include <cpuid.h>
#endif
#include <float.h> // for _clearfp / _controlfp_s in MaskFpExceptions
#include <mlang.h>
#ifdef __GNUC__
// mingw needs explicit UUID declaration for IMultiLanguage2
__CRT_UUID_DECL(IMultiLanguage2, 0xDCCFC164, 0x2B38, 0x11D2, 0xB7, 0xEC, 0x00, 0xC0, 0x4F, 0x8F, 0x5D, 0x9A)
#endif

//--- subclass ids

static AtomicInt gSubclassId = 0;

UINT_PTR NextSubclassId() {
    int res = AtomicIntInc(&gSubclassId);
    return (UINT_PTR)res;
}

//--- bool / BOOL

bool ToBool(BOOL b) {
    return b ? true : false;
}

//--- GDI: bitmaps / pixmaps (RenderedBitmap)

Size RenderedBitmap::GetSize() {
    return size;
}

// approximate size, we assume 4 bytes per pixel and don't count stride
i64 RenderedBitmapByteSize(RenderedBitmap* bmp) {
    if (!bmp) {
        return 0;
    }
    Size s = bmp->GetSize();
    i64 res = i64(s.dx) * i64(s.dy) * i64(4);
    return res;
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

//--- edit control

void EditSelectAll(HWND hwnd) {
    Edit_SetSel(hwnd, 0, -1);
}

//--- list box

void ListBox_AppendString_NoSort(HWND hwnd, WStr txt) {
    LbInsertString(hwnd, -1, txt);
}

void LbResetContent(HWND hwnd) {
    SendMessageW(hwnd, LB_RESETCONTENT, 0, 0);
}

int LbAddString(HWND hwnd, WStr text) {
    return (int)SendMessageW(hwnd, LB_ADDSTRING, 0, (LPARAM)CWStrTemp(text));
}

int LbAddString(HWND hwnd, Str text) {
    return LbAddString(hwnd, ToWStrTemp(text));
}

int LbInsertString(HWND hwnd, int idx, WStr text) {
    return (int)SendMessageW(hwnd, LB_INSERTSTRING, (WPARAM)idx, (LPARAM)CWStrTemp(text));
}

int LbInsertString(HWND hwnd, int idx, Str text) {
    return LbInsertString(hwnd, idx, ToWStrTemp(text));
}

int LbGetCount(HWND hwnd) {
    return (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
}

int LbGetCurrentSelection(HWND hwnd) {
    return (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
}

bool LbSetCurrentSelection(HWND hwnd, int idx) {
    LRESULT res = SendMessageW(hwnd, LB_SETCURSEL, (WPARAM)idx, 0);
    return idx < 0 || res != LB_ERR;
}

TempWStr LbGetTextTemp(HWND hwnd, int idx) {
    int len = (int)SendMessageW(hwnd, LB_GETTEXTLEN, (WPARAM)idx, 0);
    if (len == LB_ERR) {
        return {};
    }
    TempWStr text = AllocArrayTemp<WCHAR>(len + 1);
    LRESULT res = SendMessageW(hwnd, LB_GETTEXT, (WPARAM)idx, (LPARAM)text.s);
    if (res == LB_ERR) {
        return {};
    }
    text.len = (int)res;
    return text;
}

int LbGetItemHeight(HWND hwnd, int idx) {
    return (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, (WPARAM)idx, 0);
}

void LbSetItemHeight(HWND hwnd, int idx, int height) {
    SendMessageW(hwnd, LB_SETITEMHEIGHT, (WPARAM)idx, (LPARAM)height);
}

Rect LbGetItemRect(HWND hwnd, int idx) {
    RECT rect{};
    LRESULT res = SendMessageW(hwnd, LB_GETITEMRECT, (WPARAM)idx, (LPARAM)&rect);
    if (res == LB_ERR) {
        return {};
    }
    return {rect};
}

int LbItemFromPoint(HWND hwnd, Point point, bool* outside) {
    LRESULT res = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y));
    *outside = HIWORD(res) != 0;
    return (int)LOWORD(res);
}

int LbGetTopIndex(HWND hwnd) {
    return (int)SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0);
}

bool LbSetTopIndex(HWND hwnd, int idx) {
    LRESULT res = SendMessageW(hwnd, LB_SETTOPINDEX, (WPARAM)idx, 0);
    return res != LB_ERR;
}

void LbInitStorage(HWND hwnd, int count) {
    SendMessageW(hwnd, LB_INITSTORAGE, (WPARAM)count, 0);
}

//--- list view

int LvGetItemCount(HWND hwnd) {
    return (int)SendMessageW(hwnd, LVM_GETITEMCOUNT, 0, 0);
}

int LvGetNextItem(HWND hwnd, int start, UINT flags) {
    return (int)SendMessageW(hwnd, LVM_GETNEXTITEM, (WPARAM)start, MAKELPARAM(flags, 0));
}

void LvSetItemState(HWND hwnd, int i, UINT state, UINT mask) {
    LVITEMW item = {};
    item.stateMask = mask;
    item.state = state;
    SendMessageW(hwnd, LVM_SETITEMSTATE, (WPARAM)i, (LPARAM)&item);
}

UINT LvGetItemState(HWND hwnd, int i, UINT mask) {
    return (UINT)SendMessageW(hwnd, LVM_GETITEMSTATE, (WPARAM)i, (LPARAM)mask);
}

void LvEnsureVisible(HWND hwnd, int i, bool partialOk) {
    SendMessageW(hwnd, LVM_ENSUREVISIBLE, (WPARAM)i, (LPARAM)(partialOk ? TRUE : FALSE));
}

HWND LvGetEditControl(HWND hwnd) {
    return (HWND)SendMessageW(hwnd, LVM_GETEDITCONTROL, 0, 0);
}

int LvInsertItem(HWND hwnd, const LVITEMW* item) {
    return (int)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)item);
}

bool LvEditLabel(HWND hwnd, int i) {
    return SendMessageW(hwnd, LVM_EDITLABELW, (WPARAM)i, 0) != 0;
}

void LvDeleteItem(HWND hwnd, int i) {
    SendMessageW(hwnd, LVM_DELETEITEM, (WPARAM)i, 0);
}

void LvDeleteAllItems(HWND hwnd) {
    SendMessageW(hwnd, LVM_DELETEALLITEMS, 0, 0);
}

// empty Rect on failure
Rect LvGetItemRect(HWND hwnd, int i, int code) {
    RECT rc = {};
    rc.left = code;
    if (!SendMessageW(hwnd, LVM_GETITEMRECT, (WPARAM)i, (LPARAM)&rc)) {
        return {};
    }
    return {rc};
}

Rect LvGetSubItemRect(HWND hwnd, int iItem, int iSub, int code) {
    RECT rc = {};
    rc.top = iSub;
    rc.left = code;
    if (!SendMessageW(hwnd, LVM_GETSUBITEMRECT, (WPARAM)iItem, (LPARAM)&rc)) {
        return {};
    }
    return {rc};
}

void LvSetColumnWidth(HWND hwnd, int iCol, int cx) {
    SendMessageW(hwnd, LVM_SETCOLUMNWIDTH, (WPARAM)iCol, MAKELPARAM(cx, 0));
}

void LvSetItemText(HWND hwnd, int i, int iSub, WStr text) {
    LVITEMW item = {};
    item.iSubItem = iSub;
    item.pszText = CWStrTemp(text);
    SendMessageW(hwnd, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&item);
}

void LvSetItemText(HWND hwnd, int i, int iSub, Str text) {
    LvSetItemText(hwnd, i, iSub, ToWStrTemp(text));
}

TempWStr LvGetItemTextTemp(HWND hwnd, int i, int iSub) {
    // LVM_GETITEMTEXT needs a buffer; grow until it fits
    int cch = 256;
    for (;;) {
        TempWStr text = AllocArrayTemp<WCHAR>(cch);
        LVITEMW item = {};
        item.iSubItem = iSub;
        item.pszText = text.s;
        item.cchTextMax = cch;
        int n = (int)SendMessageW(hwnd, LVM_GETITEMTEXTW, (WPARAM)i, (LPARAM)&item);
        if (n + 1 < cch || cch >= 32 * 1024) {
            text.len = n;
            return text;
        }
        cch *= 2;
    }
}

// client coords; flagsOut optional (LVHT_*)
int LvHitTest(HWND hwnd, Point pt, UINT* flagsOut) {
    LVHITTESTINFO info = {};
    info.pt.x = pt.x;
    info.pt.y = pt.y;
    int i = (int)SendMessageW(hwnd, LVM_HITTEST, 0, (LPARAM)&info);
    if (flagsOut) {
        *flagsOut = info.flags;
    }
    return i;
}

DWORD LvSetExtendedStyle(HWND hwnd, DWORD ex) {
    return (DWORD)SendMessageW(hwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)ex);
}

int LvInsertColumn(HWND hwnd, int iCol, const LVCOLUMNW* col) {
    return (int)SendMessageW(hwnd, LVM_INSERTCOLUMNW, (WPARAM)iCol, (LPARAM)col);
}

//--- resources / instance / common controls

void InitAllCommonControls() {
    INITCOMMONCONTROLSEX cex{};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&cex);
}

void FillWndClassEx(WNDCLASSEX& wcex, WStr clsName, WNDPROC wndproc) {
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hCursor = GetCachedCursor(IDC_ARROW);
    wcex.lpszClassName = clsName.s;
    wcex.lpfnWndProc = wndproc;
}

//--- HWND: geometry

Rect HwndClientRect(HWND hwnd) {
    RECT rc{};
    ::GetClientRect(hwnd, &rc);
    return {rc};
}

Rect HwndWindowRect(HWND hwnd) {
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    return {rc};
}

void HwndInvalidateRect(HWND hwnd, Rect rect, bool erase) {
    if (rect.IsEmpty()) {
        return;
    }
    RECT r = ToRECT(rect);
    InvalidateRect(hwnd, &r, toBOOL(erase));
}

void HwndInvalidate(HWND hwnd, bool erase) {
    InvalidateRect(hwnd, nullptr, toBOOL(erase));
}

Rect HwndMapRectToWindow(Rect rect, HWND hwndFrom, HWND hwndTo) {
    RECT rc = ToRECT(rect);
    ::MapWindowPoints(hwndFrom, hwndTo, (LPPOINT)&rc, 2);
    return ToRect(rc);
}

// map client coords where x=0 is the physical left edge (even on WS_EX_LAYOUTRTL windows)
Rect HwndMapLtrClientRectToScreen(HWND hwnd, Rect r) {
    if (HwndIsRtl(hwnd)) {
        int w = HwndClientRect(hwnd).dx;
        r.x = w - r.x - r.dx;
    }
    return HwndMapRectToWindow(r, hwnd, nullptr);
}

// for SetWindowPos on a WS_EX_LAYOUTRTL parent: child x as offset from physical left
int HwndMapChildXForRtlParent(HWND parent, int ltrX, int childDx) {
    if (!HwndIsRtl(parent)) {
        return ltrX;
    }
    return HwndClientRect(parent).dx - ltrX - childDx;
}

//--- HWND: coordinates

Point HwndMapWindowPoint(HWND hwndFrom, HWND hwndTo, Point p) {
    POINT pt = ToPOINT(p);
    ::MapWindowPoints(hwndFrom, hwndTo, &pt, 1);
    return {pt.x, pt.y};
}

Point HwndClientToScreen(HWND hwnd, Point p) {
    POINT pt = ToPOINT(p);
    ClientToScreen(hwnd, &pt);
    return {pt.x, pt.y};
}

Point HwndScreenToClient(HWND hwnd, Point p) {
    POINT pt = ToPOINT(p);
    ScreenToClient(hwnd, &pt);
    return {pt.x, pt.y};
}

HWND HwndWindowFromPoint(Point p) {
    return WindowFromPoint(ToPOINT(p));
}

Point GetCursorPosition() {
    POINT pt{};
    GetCursorPos(&pt);
    return {pt.x, pt.y};
}

//--- HWND: focus / visibility / Z-order

// move window to top of Z order (i.e. make it visible to the user)
// but without activation (i.e. capturing focus)
void HwndShowWithoutActivate(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

//--- HWND: geometry (move)

void HwndMoveWindow(HWND hwnd, Rect* r) {
    MoveWindow(hwnd, r->x, r->y, r->dx, r->dy, TRUE);
}

//--- OS / process / CPU

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
        return fmt("10.%d", buildNo);
    }

    // either a newer or an older NT version, neither of which we support
    return fmt("NT %u.%u", ver.dwMajorVersion, ver.dwMinorVersion);
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
        return str::DupTemp("unknown");
    }
    return OsNameFromVerTemp(ver);
}

// returns nullptr if not set
TempStr GetEnvVariableTemp(Str name) {
    WCHAR bufStatic[256];
    WCHAR* buf = &bufStatic[0];
    DWORD cchBufSize = dimof(bufStatic);
    WCHAR* nameW = CWStrTemp(name);
    DWORD res = GetEnvironmentVariableW(nameW, buf, cchBufSize);
    if (res == 0) {
        // env variable doesn't exist
        return {};
    }
    if (res >= cchBufSize) {
        // buffer was too small
        cchBufSize = res + 4; // +4 jic
        buf = AllocArrayTemp<WCHAR>((int)cchBufSize);
        res = GetEnvironmentVariableW(nameW, buf, cchBufSize);
        ReportIf(res == 0 || res > cchBufSize);
    }
    return ToUtf8Temp(buf);
}

bool IsProcess64() {
    return 8 == sizeof(void*);
}

bool IsProcess32() {
    return 4 == sizeof(void*);
}

// https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-iswow64process
bool IsRunningInWow64() {
    if (IsProcess64()) {
        // only 32-bit build can run under wow
        return false;
    }
    BOOL isWow = FALSE;
    if (IsWow64Process(GetCurrentProcess(), &isWow)) {
        return isWow == TRUE;
    }
    return false;
}

bool IsRunningOnWine() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    bool isWine = false;
    // Canonical Wine detection: Wine's ntdll.dll exports wine_get_version() and
    // siblings. This works regardless of the graphics backend and is what Wine
    // itself documents. We probe several exports because some configs hide only
    // wine_get_version (e.g. staging's "hide Wine version" option).
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        const char* wineExports[] = {
            "wine_get_version",
            "wine_get_host_version",
            "wine_get_build_id",
            "wine_nt_to_unix_file_name",
        };
        for (const char* fn : wineExports) {
            if (GetProcAddress(hNtdll, fn)) {
                isWine = true;
                break;
            }
        }
    }
    // Fallback: Wine creates a Software\Wine registry key. Cheap, independent of
    // the graphics backend, available from process start, and present even when
    // the ntdll wine_* exports are hidden.
    if (!isWine &&
        (RegKeyExists(HKEY_CURRENT_USER, R"(Software\Wine)") || RegKeyExists(HKEY_LOCAL_MACHINE, R"(Software\Wine)"))) {
        isWine = true;
    }
    // Last resort: scan loaded modules for a Wine graphics driver. Covers the X11
    // (winex11.drv) and Wayland (winewayland.drv) backends. Misses headless Wine
    // and the early-startup window before a driver is loaded, hence the checks
    // above run first.
    if (isWine) {
        cached = 1;
        return true;
    }
    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        cached = 0;
        return false;
    }
    MODULEENTRY32 mod{};
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        auto nameA = ToUtf8Temp(mod.szModule);
        if (str::EqI(nameA, StrL("winex11.drv")) || str::EqI(nameA, StrL("winewayland.drv"))) {
            isWine = true;
            break;
        }
        cont = Module32Next(snap, &mod);
    }
    cached = isWine ? 1 : 0;
    return isWine;
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

// number of logical processors available to the process
int CpuCoreCount() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors;
    return n < 1 ? 1 : n;
}

// return true if OS and our process have the same arch (i.e. both are 32bit
// or both are 64bit)
bool IsProcessAndOsArchSame() {
    return IsProcess64() == IsOs64();
}

TempStr GetLastErrorStrTemp(DWORD& err) {
    if (err == 0) {
        err = GetLastError();
    }
    if (err == 0) {
        return StrL("");
    }
    if (err == ERROR_INTERNET_EXTENDED_ERROR) {
        WCHAR buf[4096]{};
        DWORD bufSize = dimof(buf) - 1;
        // ignoring a case where buffer is too small. 4 kB should be enough for everybody
        InternetGetLastResponseInfoW(&err, buf, &bufSize);
        buf[dimof(buf) - 1] = 0;
        return ToUtf8Temp(buf);
    }
    WCHAR* msgBuf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD ferr = FormatMessageW(flags, nullptr, err, lang, (LPWSTR)&msgBuf, 0, nullptr);
    if (!ferr || !msgBuf) {
        return StrL("");
    }
    TempStr res = ToUtf8Temp(msgBuf);
    LocalFree(msgBuf);
    return res;
}

void LogLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    if (str::IsNull(msg)) {
        msg = StrL("");
    }
    str::TrimWSInPlace(msg, str::TrimOpt::Both);
    logf("LogLastError: 0x%x (%d) '%s'\n", (int)err, (int)err, msg);
}

void DbgOutLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    OutputDebugStringA(msg.s);
}

//--- registry

// return true if a given registry key (path) exists
bool RegKeyExists(HKEY keySub, Str keyName) {
    HKEY hKey;
    WCHAR* keyNameW = CWStrTemp(keyName);
    LONG res = RegOpenKeyW(keySub, keyNameW, &hKey);
    if (ERROR_SUCCESS == res) {
        RegCloseKey(hKey);
        return true;
    }

    // return true for key that exists even if it's not
    // accessible by us
    return ERROR_ACCESS_DENIED == res;
}

TempStr ReadRegStrTemp(HKEY keySub, Str keyName, Str valName) {
    if (!keySub) {
        return nullptr;
    }
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    WStr val;
    REGSAM access = KEY_READ;
    HKEY hKey;
TryAgainWOW64:
    LONG res = RegOpenKeyEx(keySub, keyNameW, 0, access, &hKey);
    if (ERROR_SUCCESS == res) {
        DWORD valLen;
        res = RegQueryValueEx(hKey, valNameW, nullptr, nullptr, nullptr, &valLen);
        if (ERROR_SUCCESS == res) {
            val = WStr(AllocArray<WCHAR>((int)(valLen / sizeof(WCHAR)) + 1));
            res = RegQueryValueEx(hKey, valNameW, nullptr, nullptr, (LPBYTE)val.s, &valLen);
            if (ERROR_SUCCESS != res) {
                wstr::FreePtr(&val);
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
    TempStr resv = ToUtf8Temp(val.s);
    wstr::Free(val);
    return resv;
}

TempStr LoggedReadRegStrTemp(HKEY keySub, Str keyName, Str valName) {
    auto res = ReadRegStrTemp(keySub, keyName, valName);
    logf("ReadRegStrTemp(%s, %s, %s) => '%s'\n", RegKeyNameTemp(keySub), keyName, valName, res);
    return res;
}

TempStr ReadRegStr2Temp(Str keyName, Str valName) {
    TempStr res = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, valName);
    if (!res) {
        res = ReadRegStrTemp(HKEY_CURRENT_USER, keyName, valName);
    }
    return res;
}

TempStr LoggedReadRegStr2Temp(Str keyName, Str valName) {
    TempStr res = LoggedReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, valName);
    if (!res) {
        res = LoggedReadRegStrTemp(HKEY_CURRENT_USER, keyName, valName);
    }
    return res;
}

bool WriteRegStr(HKEY keySub, Str keyName, Str valName, Str value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    int cch;
    WCHAR* valueW = CWStrTemp(value, cch);
    DWORD cbData = (DWORD)(cch + 1) * sizeof(WCHAR);
    LSTATUS res = SHSetValueW(keySub, keyNameW, valNameW, REG_SZ, (const void*)valueW, cbData);
    return ERROR_SUCCESS == res;
}

bool LoggedWriteRegStr(HKEY keySub, Str keyName, Str valName, Str value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    int cch;
    WCHAR* valueW = CWStrTemp(value, cch);
    DWORD cbData = (DWORD)(cch + 1) * sizeof(WCHAR);
    LSTATUS res = SHSetValueW(keySub, keyNameW, valNameW, REG_SZ, (const void*)valueW, cbData);
    if (res != ERROR_SUCCESS) {
        logf("WriteRegStr(%s, %s, %s, %s) failed with '%d'\n", RegKeyNameTemp(keySub), keyName, valName, value, res);
        LogLastError();
        return false;
    }
    logf("WriteRegStr(%s, %s, %s, %s) ok!\n", RegKeyNameTemp(keySub), keyName, valName, value);
    return true;
}

bool ReadRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD& value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    DWORD size = sizeof(DWORD);
    LSTATUS res = SHGetValue(keySub, keyNameW, valNameW, nullptr, &value, &size);
    return ERROR_SUCCESS == res && sizeof(DWORD) == size;
}

bool WriteRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    LSTATUS res = SHSetValueW(keySub, keyNameW, valNameW, REG_DWORD, (const void*)&value, sizeof(DWORD));
    return ERROR_SUCCESS == res;
}

bool LoggedWriteRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD value) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valNameW = CWStrTemp(valName);
    LSTATUS res = SHSetValueW(keySub, keyNameW, valNameW, REG_DWORD, (const void*)&value, sizeof(DWORD));
    if (res != ERROR_SUCCESS) {
        logf("WriteRegDWORD(%s, %s, %s, %d) failed with '%d'\n", RegKeyNameTemp(keySub), keyName, valName, (int)value,
             res);
        LogLastError();
        return false;
    }
    logf("WriteRegDWORD(%s, %s, %s, %d) => ok'\n", RegKeyNameTemp(keySub), keyName, valName, (int)value);
    return true;
}

bool LoggedWriteRegNone(HKEY hkey, Str key, Str valName) {
    WCHAR* keyW = CWStrTemp(key);
    WCHAR* valNameW = CWStrTemp(valName);
    LSTATUS res = SHSetValueW(hkey, keyW, valNameW, REG_NONE, nullptr, 0);
    logf("LoggedWriteRegNone(%s, %s, %s) => '%d'\n", RegKeyNameTemp(hkey), key, valName, res);
    return (ERROR_SUCCESS == res);
}

bool CreateRegKey(HKEY keySub, Str keyName) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    HKEY hKey;
    LSTATUS res = RegCreateKeyExW(keySub, keyNameW, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(hKey);
    return true;
}

TempStr RegKeyNameTemp(HKEY key) {
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

static TempStr RegKeyNameWTemp(HKEY key) {
    auto k = RegKeyNameTemp(key);
    return str::Dup(k);
}

// Open a registry key's DACL so we can delete protected uninstall/keys.
// Uses an explicit Everyone FULL_CONTROL ACL (not a NULL DACL, which CodeQL flags).
static void ResetRegKeyAcl(HKEY hkey, Str keyName) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    HKEY hKey;
    LONG res = RegOpenKeyEx(hkey, keyNameW, 0, WRITE_DAC, &hKey);
    if (ERROR_SUCCESS != res) {
        return;
    }

    PSID everyoneSid = nullptr;
    PACL dacl = nullptr;
    SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
    if (!AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyoneSid)) {
        RegCloseKey(hKey);
        return;
    }

    EXPLICIT_ACCESSW ea{};
    ea.grfAccessPermissions = KEY_ALL_ACCESS;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPWSTR)everyoneSid;

    if (SetEntriesInAclW(1, &ea, nullptr, &dacl) != ERROR_SUCCESS) {
        FreeSid(everyoneSid);
        RegCloseKey(hKey);
        return;
    }

    SECURITY_DESCRIPTOR secdesc;
    InitializeSecurityDescriptor(&secdesc, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&secdesc, TRUE, dacl, FALSE);
    RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &secdesc);

    LocalFree(dacl);
    FreeSid(everyoneSid);
    RegCloseKey(hKey);
}

bool DeleteRegKey(HKEY keySub, Str keyName, bool resetACLFirst) {
    if (resetACLFirst) {
        ResetRegKeyAcl(keySub, keyName);
    }
    WCHAR* keyNameW = CWStrTemp(keyName);
    LSTATUS res = SHDeleteKeyW(keySub, keyNameW);
    return ERROR_SUCCESS == res || ERROR_FILE_NOT_FOUND == res;
}

bool LoggedDeleteRegKey(HKEY keySub, Str keyName, bool resetACLFirst) {
    if (resetACLFirst) {
        ResetRegKeyAcl(keySub, keyName);
    }
    WCHAR* keyNameW = CWStrTemp(keyName);
    LSTATUS res = SHDeleteKeyW(keySub, keyNameW);
    logf("LoggedDeleteRegKey(%s, %s, %d) => %d\n", RegKeyNameWTemp(keySub), keyName, resetACLFirst, res);
    bool ok = (ERROR_SUCCESS == res) || (ERROR_FILE_NOT_FOUND == res);
    if (!ok) {
        LogLastError(res);
    }
    return ok;
}

bool DeleteRegValue(HKEY keySub, Str keyName, Str val) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valW = CWStrTemp(val);

    auto res = SHDeleteValueW(keySub, keyNameW, valW);
    return res == ERROR_SUCCESS;
}

bool LoggedDeleteRegValue(HKEY keySub, Str keyName, Str val) {
    WCHAR* keyNameW = CWStrTemp(keyName);
    WCHAR* valW = CWStrTemp(val);

    auto res = SHDeleteValueW(keySub, keyNameW, valW);
    bool ok = (ERROR_SUCCESS == res) || (ERROR_FILE_NOT_FOUND == res);
    logf("LoggedDeleteRegValue(%s, %s, %s) => %d\n", RegKeyNameWTemp(keySub), keyName, val, res);
    if (!ok) {
        LogLastError(res);
    }
    return ok;
}

HRESULT CLSIDFromString(Str lpsz, LPCLSID pclsid) {
    WCHAR* ws = CWStrTemp(lpsz);
    return CLSIDFromString(ws, pclsid);
}

//--- environment / errors / paths

TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing) {
    if (createIfMissing) {
        csidl = csidl | CSIDL_FLAG_CREATE;
    }
    WCHAR path[MAX_PATH]{};
    HRESULT res = SHGetFolderPathW(nullptr, csidl, nullptr, 0, path);
    if (S_OK != res) {
        return {};
    }
    return ToUtf8Temp(path);
}

// temp directory
TempStr GetTempDirTemp() {
    WCHAR dir[MAX_PATH] = {};
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
    ReportIf(cch >= dimof(dir));
    return ToUtf8Temp(WStr(dir, (int)cch));
}

//--- OS / process (misc)

void DisableDataExecution() {
    // Win7+; 32-bit only (fails with ERROR_NOT_SUPPORTED on 64-bit processes)
    SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
}

enum class ConsoleState {
    Uninitialized,
    NoConsole,
    StdoutRedirected,
    AttachedToParent,
    AllocatedNew,
};

static ConsoleState gConsoleState = ConsoleState::Uninitialized;
static HANDLE gOriginalStdout = INVALID_HANDLE_VALUE;
static HANDLE gOriginalStderr = INVALID_HANDLE_VALUE;
static HWND gStartupForegroundWindow = nullptr;
static bool gLoggedToConsole = false;

static void InitConsoleState() {
    if (gConsoleState != ConsoleState::Uninitialized) {
        return;
    }

    gStartupForegroundWindow = GetForegroundWindow();
    gOriginalStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    gOriginalStderr = GetStdHandle(STD_ERROR_HANDLE);
    if (gOriginalStdout != INVALID_HANDLE_VALUE && gOriginalStdout != nullptr) {
        DWORD fileType = GetFileType(gOriginalStdout);
        if (fileType == FILE_TYPE_DISK) {
            gConsoleState = ConsoleState::StdoutRedirected;
            return;
        }
        // PowerShell pipe redirection breaks WriteFile from GUI apps; attach to console instead.
        if (fileType == FILE_TYPE_PIPE && !WasLaunchedByPowershellWithPipeRedirect()) {
            gConsoleState = ConsoleState::StdoutRedirected;
            return;
        }
    }

    gConsoleState = ConsoleState::NoConsole;
}

static bool StdoutRedirected() {
    InitConsoleState();
    return gConsoleState == ConsoleState::StdoutRedirected;
}

// https://www.tillett.info/2013/05/13/how-to-create-a-windows-program-that-works-as-both-as-a-gui-and-console-application/
// TODO: see if https://github.com/apenwarr/fixconsole/blob/master/fixconsole_windows.go would improve things
// a stream whose parent-provided handle is a file or pipe was redirected by the
// parent (`> out.txt`, `| more`) and must keep receiving CRT output even after
// we attach to a console for logging
static bool IsFileOrPipe(HANDLE h) {
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD fileType = GetFileType(h);
    return fileType == FILE_TYPE_DISK || fileType == FILE_TYPE_PIPE;
}

static void RedirectStdioToConsole(bool redirectStdin = false) {
    FILE* con{nullptr};
    if (!IsFileOrPipe(gOriginalStdout)) {
        freopen_s(&con, "CONOUT$", "w", stdout);
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
    if (!IsFileOrPipe(gOriginalStderr)) {
        freopen_s(&con, "CONOUT$", "w", stderr);
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    if (redirectStdin) {
        freopen_s(&con, "CONIN$", "r", stdin);
        setvbuf(stdin, nullptr, _IONBF, 0);
    }
}

static bool AttachToParentConsole() {
    InitConsoleState();
    if (gConsoleState == ConsoleState::AttachedToParent || gConsoleState == ConsoleState::AllocatedNew) {
        return true;
    }
    if (StdoutRedirected()) {
        return true;
    }
    if (gConsoleState != ConsoleState::NoConsole) {
        return false;
    }

    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return false;
    }
    gConsoleState = ConsoleState::AttachedToParent;
    RedirectStdioToConsole(true);
    return true;
}

// returns true if a new console window was allocated
static bool AttachOrAllocateConsole() {
    InitConsoleState();
    if (gConsoleState == ConsoleState::AllocatedNew) {
        return true;
    }
    if (gConsoleState == ConsoleState::AttachedToParent) {
        return false;
    }
    if (StdoutRedirected()) {
        return false;
    }
    if (gConsoleState != ConsoleState::NoConsole) {
        return false;
    }

    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        gConsoleState = ConsoleState::AttachedToParent;
        RedirectStdioToConsole(true);
        return false;
    }

    AllocConsole();
    gConsoleState = ConsoleState::AllocatedNew;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = 500;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
    RedirectStdioToConsole(true);
    return true;
}

bool RedirectIOToExistingConsole() {
    return AttachToParentConsole();
}

// returns true if had to allocate new console (i.e. show console window)
// false if redirected to existing console, which means it was launched from a shell
//--- console

bool RedirectIOToConsole() {
    return AttachOrAllocateConsole();
}

static void SendEnterToParentConsole(HWND foregroundWnd) {
    if (foregroundWnd && IsWindow(foregroundWnd)) {
        SetForegroundWindow(foregroundWnd);
    }
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void HandleRedirectedConsoleOnShutdown() {
    InitConsoleState();
    if (gConsoleState == ConsoleState::AllocatedNew) {
        system("pause");
    } else if (gConsoleState == ConsoleState::AttachedToParent) {
        SendEnterToParentConsole(nullptr);
    }
}

void InitConsoleOutput() {
    InitConsoleState();
}

void LogConsole(Str s) {
    if (s.len <= 0) {
        return;
    }

    InitConsoleState();
    if (StdoutRedirected()) {
        if (gOriginalStdout != INVALID_HANDLE_VALUE) {
            DWORD written;
            BOOL ok = WriteFile(gOriginalStdout, s.s, s.len, &written, nullptr);
            if (!ok) {
                logf("error: %s\n", GetLastErrorAsStr(GetTempArena()));
            }
        }
        return;
    }

    // passive by design: write only to a console that already exists (inherited
    // or explicitly set up via RedirectIOToConsole / RedirectIOToExistingConsole).
    // never attach to the parent console or allocate one here: logging from a GUI
    // process launched by a script would spray log lines over the terminal of
    // whatever shell happens to be the ancestor
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == nullptr || hConsole == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written;
    // fails harmlessly if the handle is not a console (e.g. a pipe we chose not to write to)
    if (WriteConsoleA(hConsole, s.s, s.len, &written, nullptr)) {
        gLoggedToConsole = true;
    }
}

void SendEnterIfLoggedToConsole() {
    InitConsoleState();
    if (!gLoggedToConsole) {
        return;
    }
    if (gConsoleState != ConsoleState::AttachedToParent) {
        return;
    }
    if (!gStartupForegroundWindow) {
        return;
    }
    SendEnterToParentConsole(gStartupForegroundWindow);
}

void WaitForConsoleClose() {
    SendEnterIfLoggedToConsole();
    InitConsoleState();
    if (gConsoleState != ConsoleState::AllocatedNew) {
        return;
    }

    const char* msg = "press Enter to exit";
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, msg, (DWORD)strlen(msg), &written, nullptr);
    }

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(hInput);
        char c;
        DWORD read;
        ReadConsoleA(hInput, &c, 1, &read, nullptr);
    }
}

//--- environment / paths (shell helpers)

void ChangeCurrDirToDocuments() {
    TempStr dir = GetSpecialFolderTemp(CSIDL_MYDOCUMENTS);
    WCHAR* dirW = CWStrTemp(dir);
    SetCurrentDirectoryW(dirW);
}

static ULARGE_INTEGER FileTimeToLargeInteger(const FILETIME& ft) {
    ULARGE_INTEGER res;
    res.LowPart = ft.dwLowDateTime;
    res.HighPart = ft.dwHighDateTime;
    return res;
}

TempStr ResolveLnkTemp(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    if (!pathW.s) {
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

    HRESULT hRes = file->Load(pathW.s, STGM_READ);
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

bool CreateShortcut(Str shortcutPath, Str exePath, Str args, Str description, int iconIndex) {
    TempWStr ws;
    ScopedCom com;

    ScopedComPtr<IShellLink> lnk;
    if (!lnk.Create(CLSID_ShellLink)) {
        return false;
    }

    ScopedComQIPtr<IPersistFile> file(lnk);
    if (!file) {
        return false;
    }

    ws = ToWStrTemp(exePath);
    HRESULT hr = lnk->SetPath(ws.s);
    if (FAILED(hr)) {
        return false;
    }

    lnk->SetWorkingDirectory(path::GetDirTemp(ws).s);
    // lnk->SetShowCmd(SW_SHOWNORMAL);
    // lnk->SetHotkey(0);
    lnk->SetIconLocation(ws.s, iconIndex);
    if (args) {
        ws = ToWStrTemp(args);
        lnk->SetArguments(ws.s);
    }
    if (description) {
        ws = ToWStrTemp(description);
        lnk->SetDescription(ws.s);
    }

    ws = ToWStrTemp(shortcutPath);
    hr = file->Save(ws.s, TRUE);
    return SUCCEEDED(hr);
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
IDataObject* GetDataObjectForFile(Str filePath, HWND hwnd) {
    ScopedComPtr<IShellFolder> pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr)) {
        return nullptr;
    }

    WCHAR* lpWPath = CWStrTemp(filePath);
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

//--- keyboard state

bool IsKeyPressed(int key) {
    SHORT state = GetKeyState(key);
    SHORT isDown = (SHORT)(state & 0x8000);
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
        uint n;
        if (VerQueryValue(versionInfo, L"\\", (LPVOID*)&fileInfo, &n)) {
            fileVersion = fileInfo->dwFileVersionMS;
        }
    }

    return fileVersion;
}
#endif

bool LaunchFileShell(Str path, Str params, Str verb, bool hidden) {
    if (len(path) == 0) {
        return false;
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = CWStrTemp(verb);
    sei.lpFile = CWStrTemp(path);
    sei.lpParameters = CWStrTemp(params);
    sei.nShow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    BOOL ok = ShellExecuteExW(&sei);
    if (!ok) {
        DWORD err = GetLastError();
        logf("LaunchFile: ShellExecuteExW path: '%s' params: '%s' verb: '%s'\n", path, params, verb);
        LogLastError(err);
        return false;
    }
    logf("LaunchFileShell: launched '%s'\n", path);
    return true;
}

bool LaunchBrowser(Str url) {
    return LaunchFileShell(url, Str(), StrL("open"));
}

void OpenPathInDefaultFileManager(Str path) {
    if (len(path) == 0) {
        return;
    }

    // strip \\?\ prefix — shell APIs (ILCreateFromPath, explorer.exe) don't understand it
    str::TrimPrefix(path, StrL("\\\\?\\"));

    // Use SHOpenFolderAndSelectItems which respects the default file manager
    // (e.g. Directory Opus) instead of hardcoding explorer.exe
    WCHAR* pathW = CWStrTemp(path);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(pathW);
    if (pidl) {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        return;
    }

    // fallback to using explorer.exe
    WCHAR winDir[MAX_PATH]{};
    UINT n = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    TempStr explorer = ToUtf8Temp(winDir);
    explorer = path::JoinTemp(explorer, StrL("explorer.exe"));
    if (file::Exists(explorer)) return;
    TempStr args = fmt("/select,\"%s\"", path);
    CreateProcessHelper(explorer, args);
}

HANDLE LaunchProcessWithCmdLine(Str exe, Str cmdLine) {
    PROCESS_INFORMATION pi = {nullptr};
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    // first cmd-line argument should be the exe name
    TempStr cmd = fmt("\"%s\" %s", exe, cmdLine);
    WCHAR* cmdLineW = CWStrTemp(cmd);

    WCHAR* exeW = CWStrTemp(exe);
    // note: cmdLineW is modified by CreateProcessW so must be writeable
    BOOL ok = CreateProcessW(exeW, cmdLineW, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// cmdLine must contain quoted exe path as first argument
HANDLE LaunchProcessInDir(Str cmdLine, Str currDir, DWORD flags) {
    PROCESS_INFORMATION pi = {nullptr};
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    // CreateProcess() might modify cmd line argument, so make a copy
    // in case caller provides a read-only string
    WCHAR* cmdLineW = CWStrTemp(cmdLine);
    // lpCurrentDirectory must be nullptr (inherit caller's dir) when no dir is
    // given. CWStrTemp() of an empty Str returns a non-null L"" which
    // CreateProcessW rejects (ERROR_DIRECTORY), so map empty -> nullptr.
    WCHAR* dirW = len(currDir) == 0 ? nullptr : CWStrTemp(currDir);
    if (!CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, FALSE, flags, nullptr, dirW, &si, &pi)) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool CreateProcessHelper(Str exe, Str args) {
    if (!args) {
        args = "";
    }
    TempStr cmd = fmt("\"%s\" %s", exe, args);
    AutoCloseHandle process = LaunchProcessInDir(cmd);
    return process != nullptr;
}

bool IsProcessRunningElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, size, &size);
    CloseHandle(token);
    if (!ok) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

#if 0
// return true if the app is running in elevated (as admin)
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
#endif

// returns the exe path of the parent process, or nullptr on failure
// if pidOut is not nullptr, it receives the parent process ID
TempStr GetParentProcessPath(DWORD* pidOut) {
    if (pidOut) {
        *pidOut = 0;
    }
    DWORD pid = GetCurrentProcessId();
    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return {};
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD parentPid = 0;
    if (!Process32FirstW(snap, &pe)) {
        return {};
    }
    do {
        if (pe.th32ProcessID == pid) {
            parentPid = pe.th32ParentProcessID;
            break;
        }
    } while (Process32NextW(snap, &pe));
    if (parentPid == 0) {
        return {};
    }
    if (pidOut) {
        *pidOut = parentPid;
    }
    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hProc.IsValid()) {
        return {};
    }
    WCHAR path[MAX_PATH]{};
    DWORD pathLen = MAX_PATH;
    if (!QueryFullProcessImageNameW(hProc, 0, path, &pathLen)) {
        return {};
    }
    return ToUtf8Temp(path);
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
    TOKEN_GROUPS* ptg = nullptr;
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
        if (!GetTokenInformation(hToken, TokenGroups, nullptr, 0, &cbTokenGroups) &&
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
        for (DWORD groupID : groupsToCheck) {
            // Create a SID for the local group and then check if it exists in our token
            DWORD sub1 = SECURITY_BUILTIN_DOMAIN_RID;
            ok = AllocateAndInitializeSid(&systemSid, 2, sub1, groupID, 0, 0, 0, 0, 0, 0, &psid);
            if (!ok) {
                continue;
            }

            if (checkTokenForGroupDeny) {
                CheckTokenMembership(nullptr, psid, &isMember);
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

bool LaunchElevated(Str path, Str cmdline) {
    return LaunchFileShell(path, cmdline, "runas");
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
//--- HWND: screen / work area / placement

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
Size HwndLimitSizeToScreen(HWND hwnd, Size size) {
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    BOOL ok = GetMonitorInfo(hmon, &mi);
    if (!ok) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    int dx = RectDx(mi.rcWork);
    size.dx = std::min(size.dx, dx);
    int dy = RectDy(mi.rcWork);
    size.dy = std::min(size.dy, dy);
    return size;
}

// If the window is off-screen (e.g. a monitor was disconnected),
// move it to the nearest visible monitor's work area.
void HwndEnsureOnScreen(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    Rect rect = HwndWindowRect(hwnd);
    if (rect.IsEmpty()) {
        return;
    }
    Rect shifted = ShiftRectToWorkArea(rect, nullptr, false);
    if (IsZoomed(hwnd)) {
        // for maximized windows, check if the window's non-maximized position
        // would be on a visible monitor; if not, move to primary and re-maximize
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (GetWindowPlacement(hwnd, &wp)) {
            Rect normal = ToRect(wp.rcNormalPosition);
            Rect normalShifted = ShiftRectToWorkArea(normal);
            if (normal != normalShifted) {
                wp.rcNormalPosition = ToRECT(normalShifted);
                SetWindowPlacement(hwnd, &wp);
            }
        }
        return;
    }
    if (rect == shifted) {
        return;
    }
    HwndMoveWindow(hwnd, &shifted);
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
Rect HwndGetFullscreenRect(HWND hwnd) {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        return ToRect(mi.rcMonitor);
    }
    // fall back to the primary monitor
    return {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

static BOOL CALLBACK GetMonitorRectProc(HMONITOR /*hMonitor*/, HDC /*hdc*/, LPRECT rcMonitor, LPARAM data) {
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

//--- GDI: draw / measure (primitives)

void HdcDrawRect(HDC hdc, const Rect& rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y);
    LineTo(hdc, rect.x + rect.dx - 1, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y + rect.dy - 1);
    LineTo(hdc, rect.x, rect.y);
}

void HdcFillRect(HDC hdc, const Rect& rect, HBRUSH br) {
    RECT r = ToRECT(rect);
    ::FillRect(hdc, &r, br);
}

void HdcFillRect(HDC hdc, const Rect& rect, Color col) {
    AutoDeleteBrush br(CreateSolidBrush(col));
    RECT r = ToRECT(rect);
    ::FillRect(hdc, &r, br);
}

void HdcDrawLine(HDC hdc, const Rect& rect) {
    MoveToEx(hdc, rect.x, rect.y, nullptr);
    LineTo(hdc, rect.x + rect.dx, rect.y + rect.dy);
}

// returns previously focused window
//--- HWND: focus / identity / cursor

HWND HwndSetFocus(HWND hwnd) {
    return SetFocus(hwnd);
}

bool HwndIsFocused(HWND hwnd) {
    return GetFocus() == hwnd;
}

bool HwndIsCursorOverWindow(HWND hwnd) {
    Point pt = GetCursorPosition();
    Rect rcWnd = HwndWindowRect(hwnd);
    return rcWnd.Contains(pt);
}

HWND HwndGetParent(HWND hwnd) {
    return ::GetParent(hwnd);
}

TempStr HwndGetClassName(HWND hwnd) {
    WCHAR buf[512] = {};
    int n = GetClassNameW(hwnd, buf, dimof(buf));
    ReportIf(n == 0);
    return ToUtf8Temp(buf);
}

Point HwndGetCursorPos(HWND hwnd) {
    return HwndScreenToClient(hwnd, GetCursorPosition());
}

Point& UnmirrorRtl(HWND hwnd, Point& p) {
    if (!HwndIsRtl(hwnd)) return p;
    p.x = HwndClientRect(hwnd).dx - 1 - p.x;
    return p;
}

bool HwndIsMouseOverRect(HWND hwnd, const Rect& r) {
    Point curPos = HwndGetCursorPos(hwnd);
    return r.Contains(curPos);
}

void HwndCenterDialog(HWND hDlg, HWND hParent) {
    if (!hParent) {
        hParent = GetParent(hDlg);
    }

    Rect rcDialog = HwndWindowRect(hDlg);
    rcDialog.Offset(-rcDialog.x, -rcDialog.y);
    Rect rcOwner = HwndWindowRect(hParent ? hParent : GetDesktopWindow());
    Rect rcRect = rcOwner;
    rcRect.Offset(-rcRect.x, -rcRect.y);

    // center dialog on its parent window
    rcDialog.Offset(rcOwner.x + ((rcRect.x - rcDialog.x + rcRect.dx - rcDialog.dx) / 2),
                    rcOwner.y + ((rcRect.y - rcDialog.y + rcRect.dy - rcDialog.dy) / 2));
    // ensure that the dialog is fully visible on one monitor
    rcDialog = ShiftRectToWorkArea(rcDialog, hParent, true);

    SetWindowPos(hDlg, nullptr, rcDialog.x, rcDialog.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

// Get the name of default printer or nullptr if not exists.
TempStr GetDefaultPrinterNameTemp() {
    WCHAR buf[512] = {};
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize)) {
        return ToUtf8Temp(buf);
    }
    return nullptr;
}

static HWND gClipboardOwnerWnd = nullptr;

static HWND GetClipboardOwnerWnd() {
    if (gClipboardOwnerWnd && IsWindow(gClipboardOwnerWnd)) {
        return gClipboardOwnerWnd;
    }
    static bool registered = false;
    static WCHAR className[] = L"SumatraPDFClipboardOwner";
    if (!registered) {
        WNDCLASSEX wcex{};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.lpfnWndProc = DefWindowProcW;
        wcex.hInstance = GetModuleHandle(nullptr);
        wcex.lpszClassName = className;
        RegisterClassExW(&wcex);
        registered = true;
    }
    gClipboardOwnerWnd =
        CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    return gClipboardOwnerWnd;
}

//--- clipboard

bool OpenClipboardForUpdate() {
    HWND owner = GetClipboardOwnerWnd();
    if (!owner || !OpenClipboard(owner)) {
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }
    return true;
}

void CloseClipboardAfterUpdate() {
    CloseClipboard();
}

static bool CopyOrAppendTextToClipboard(WStr text, bool appendOnly) {
    if (!text) {
        return false;
    }

    if (!appendOnly) {
        if (!OpenClipboardForUpdate()) {
            return false;
        }
    }

    int n = text.len + 1;
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(WCHAR));
    if (!handle) {
        if (!appendOnly) {
            CloseClipboardAfterUpdate();
        }
        return false;
    }

    WCHAR* globalText = (WCHAR*)GlobalLock(handle);
    if (!globalText) {
        GlobalFree(handle);
        if (!appendOnly) {
            CloseClipboardAfterUpdate();
        }
        return false;
    }
    wstr::BufSet(WStr(globalText, n), text);
    GlobalUnlock(handle);

    if (!SetClipboardData(CF_UNICODETEXT, handle)) {
        GlobalFree(handle);
        if (!appendOnly) {
            CloseClipboardAfterUpdate();
        }
        return false;
    }
    // SetClipboardData owns the handle now.

    if (!appendOnly) {
        CloseClipboardAfterUpdate();
    }

    return true;
}

bool CopyTextToClipboard(Str s) {
    return CopyOrAppendTextToClipboard(ToWStrTemp(s), false);
}

bool AppendTextToClipboard(Str s) {
    return CopyOrAppendTextToClipboard(ToWStrTemp(s), true);
}

static bool SetClipboardImage(HBITMAP hbmp) {
    if (!hbmp) {
        return false;
    }
    BITMAP bmpInfo;
    if (!GetObject(hbmp, sizeof(BITMAP), &bmpInfo)) {
        return false;
    }
    // Give the clipboard its own bitmap. SetClipboardData owns clipBmp on success.
    HBITMAP clipBmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, bmpInfo.bmWidth, bmpInfo.bmHeight, 0);
    if (!clipBmp) {
        return false;
    }
    if (!SetClipboardData(CF_BITMAP, clipBmp)) {
        DeleteObject(clipBmp);
        return false;
    }
    return true;
}

bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly) {
    if (!appendOnly) {
        if (!OpenClipboardForUpdate()) {
            return false;
        }
    }

    bool ok = SetClipboardImage(hbmp);

    if (!appendOnly) {
        CloseClipboardAfterUpdate();
    }

    return ok;
}

//--- HWND: styles / RTL

static void HwndSetWindowStyle(HWND hwnd, DWORD flags, bool enable, int type) {
    DWORD style = GetWindowLongW(hwnd, type);
    DWORD newStyle;
    if (enable) {
        newStyle = style | flags;
    } else {
        newStyle = style & ~flags;
    }
    if (newStyle != style) {
        SetWindowLongW(hwnd, type, (LONG)newStyle);
    }
}

bool HwndIsWindowStyleSet(HWND hwnd, DWORD flags) {
    DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
    return bit::IsMaskSet<DWORD>(style, flags);
}

bool HwndIsWindowStyleExSet(HWND hwnd, DWORD flags) {
    DWORD style = GetWindowLongW(hwnd, GWL_EXSTYLE);
    return (style != flags) != 0;
}

bool HwndIsRtl(HWND hwnd) {
    DWORD style = GetWindowLongW(hwnd, GWL_EXSTYLE);
    return bit::IsMaskSet<DWORD>(style, WS_EX_LAYOUTRTL);
}

void HwndSetRtl(HWND hwnd, bool isRtl) {
    HwndSetWindowExStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRtl);
}

void HwndSetWindowStyle(HWND hwnd, DWORD flags, bool enable) {
    HwndSetWindowStyle(hwnd, flags, enable, GWL_STYLE);
}

void HwndSetWindowExStyle(HWND hwnd, DWORD flags, bool enable) {
    HwndSetWindowStyle(hwnd, flags, enable, GWL_EXSTYLE);
}

//--- HWND: geometry (relative)

Rect ChildPosWithinParent(HWND hwnd) {
    Point pt = HwndClientToScreen(GetParent(hwnd), Point());
    Rect rc = HwndWindowRect(hwnd);
    rc.Offset(-pt.x, -pt.y);
    return rc;
}

int GetSizeOfDefaultGuiFont() {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int res = std::abs(ncm.lfMessageFont.lfHeight);
    return res;
}

// fills ncm with non-client metrics (incl. font sizes) scaled for the given
// dpi, so UI fonts can be sized for the monitor a window is on and not just
// the system dpi. Uses SystemParametersInfoForDpi() (Win 10 1607+) when
// available, otherwise scales the system-dpi metrics manually.
bool GetNonClientMetricsForDpi(int dpi, NONCLIENTMETRICS* ncm) {
    ncm->cbSize = sizeof(*ncm);
    if (DynSystemParametersInfoForDpi &&
        DynSystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(*ncm), ncm, 0, (UINT)dpi)) {
        return true;
    }
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(*ncm), ncm, 0)) {
        return false;
    }
    int sysDpi = DpiGetForHwnd(nullptr);
    if (sysDpi <= 0 || sysDpi == dpi) {
        return true;
    }
    auto scaleLf = [sysDpi, dpi](LOGFONTW& lf) {
        int h = (int)std::abs(lf.lfHeight);
        lf.lfHeight = -MulDiv(h, dpi, sysDpi);
    };
    scaleLf(ncm->lfMessageFont);
    scaleLf(ncm->lfMenuFont);
    scaleLf(ncm->lfStatusFont);
    scaleLf(ncm->lfCaptionFont);
    scaleLf(ncm->lfSmCaptionFont);
    return true;
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
    ReportIf(hdc == hdcBuffer);
    if (hdcBuffer) {
        BitBlt(hdc, rect.x, rect.y, rect.dx, rect.dy, hdcBuffer, 0, 0, SRCCOPY);
    }
}

DeferWinPosHelper::DeferWinPosHelper() : hdwp(::BeginDeferWindowPos(32)) {}

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
    // SWP_NOCOPYBITS: a sibling that grows into another's old screen rect
    // (canvas into a shrinking TOC) otherwise inherits those pixels. WebView2
    // is transparent, so that leftover TOC flash is visible until it composites.
    uint uFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
    if (!bRepaint) {
        uFlags |= SWP_NOREDRAW;
    }
    this->SetWindowPos(hWnd, nullptr, x, y, cx, cy, uFlags);
}

void DeferWinPosHelper::MoveWindow(HWND hWnd, Rect r) {
    this->MoveWindow(hWnd, r.x, r.y, r.dx, r.dy);
}

void MenuSetChecked(HMENU m, int id, bool isChecked) {
    ReportIf(id < 0);
    if (!m || id < 0) {
        return;
    }
    // CheckMenuItem(MF_BYCOMMAND) only hits the first item with that id. The
    // same command can appear twice (e.g. File and Settings "Use SumatraPDF
    // File Picker"), so walk the whole menu tree and update every match.
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_STATE | MIIM_FTYPE;
        if (!GetMenuItemInfoW(m, (UINT)i, TRUE, &mii)) {
            continue;
        }
        if (mii.hSubMenu) {
            MenuSetChecked(mii.hSubMenu, id, isChecked);
            continue;
        }
        if ((int)mii.wID != id) {
            continue;
        }
        mii.fMask = MIIM_STATE;
        if (isChecked) {
            mii.fState |= MFS_CHECKED;
        } else {
            mii.fState &= ~MFS_CHECKED;
        }
        SetMenuItemInfoW(m, (UINT)i, TRUE, &mii);
    }
}

bool MenuSetEnabled(HMENU m, int id, bool isEnabled) {
    ReportIf(id < 0);
    BOOL ret = EnableMenuItem(m, (UINT)id, MF_BYCOMMAND | (isEnabled ? MF_ENABLED : MF_GRAYED));
    return ret != -1;
}

void MenuRemove(HMENU m, int id) {
    ReportIf(id < 0);
    RemoveMenu(m, (UINT)id, MF_BYCOMMAND);
}

// TODO: this doesn't recognize enum Cmd, why?
// void Remove(HMENU m, enum Cmd id);
void MenuEmpty(HMENU m) {
    while (RemoveMenu(m, 0, MF_BYPOSITION)) {
        // no-op
    }
}

void MenuSetText(HMENU m, int id, WStr s) {
    ReportIf(id < 0);
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MFT_STRING;
    mii.dwTypeData = s.s;
    mii.cch = (uint)s.len;
    BOOL ok = SetMenuItemInfoW(m, id, FALSE, &mii);
    if (!ok) {
        // setting text on a menu item that isn't present is benign (e.g. the
        // item was filtered out by command visibility): log it, don't assert
        TempStr tmp = len(s) == 0 ? StrL("(null)") : ToUtf8Temp(s);
        logf("MenuSetText(): id=%d, s='%s'\n", id, tmp);
        LogLastError();
    }
}

void MenuSetText(HMENU m, int id, Str s) {
    TempWStr ws = ToWStrTemp(s);
    MenuSetText(m, id, ws);
}

/* Make a string safe to be displayed as a menu item
   (preserving all & so that they don't get swallowed)
   if no change is needed, the string is returned as is,
   else it's also saved in newResult for automatic freeing */
TempStr MenuToSafeStringTemp(Str s) {
    TempStr safe = str::ReplaceTemp(s, StrL("&"), StrL("&&"));
    return safe;
}

IStream* CreateStreamFromData(const Str& d) {
    // d is binary bytes; formats like JP2/JXL/TGA legitimately start with a 0 byte
    if (len(d) == 0) {
        return nullptr;
    }

    const void* data = (u8*)d.s;
    size_t dataLen = (size_t)d.len;
    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        return nullptr;
    }

    ULONG n;
    if (FAILED(stream->Write(data, (ULONG)dataLen, &n)) || n != dataLen) {
        return nullptr;
    }

    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    stream->AddRef();
    return stream;
}

Str ReadIStream(IStream* stream) {
    if (!stream) {
        return {};
    }

    STATSTG stat;
    HRESULT res = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res)) {
        return {};
    }
    if (stat.cbSize.QuadPart > INT_MAX - sizeof(WCHAR)) {
        return {};
    }

    int n = (int)stat.cbSize.QuadPart;
    char* d = AllocArray<char>(n + sizeofi(WCHAR));
    if (!d) {
        return {};
    }

    LARGE_INTEGER zero{};
    res = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    if (FAILED(res)) {
        free(d);
        return {};
    }

    int total = 0;
    while (total < n) {
        ULONG read = 0;
        ULONG toRead = (ULONG)(n - total);
        res = stream->Read(d + total, toRead, &read);
        if (FAILED(res) || read == 0) {
            free(d);
            return {};
        }
        total += (int)read;
    }
    d[n] = 0;
    d[n + 1] = 0;
    return Str(d, n);
}

uint GuessTextCodepage(Str data, uint defVal) {
    // try to guess the codepage
    ScopedComPtr<IMultiLanguage2> pMLang;
    if (!pMLang.Create(CLSID_CMultiLanguage)) {
        return defVal;
    }

    int ilen = std::min(data.len, INT_MAX);
    int count = 1;
    DetectEncodingInfo info{};
    HRESULT hr = pMLang->DetectInputCodepage(MLDETECTCP_NONE, CP_ACP, data.s, &ilen, &info, &count);
    if (FAILED(hr) || count != 1) {
        return defVal;
    }
    return info.nCodePage;
}

TempStr NormalizeString(Str strA, int /* NORM_FORM */ form) {
    TempWStr str = ToWStrTemp(strA);
    // ::NormalizeString is Win32 (normaliz.dll); this function is our UTF-8 wrapper
    int sizeEst = ::NormalizeString((NORM_FORM)form, str.s, str.len, nullptr, 0);
    if (sizeEst <= 0) {
        return nullptr;
    }
    // according to MSDN the estimate may be off somewhat:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd319093(v=vs.85).aspx
    sizeEst = sizeEst * 2;
    WCHAR* res = AllocArrayTemp<WCHAR>(sizeEst);
    sizeEst = ::NormalizeString((NORM_FORM)form, str.s, str.len, res, sizeEst);
    if (sizeEst <= 0) {
        return nullptr;
    }
    return ToUtf8Temp(WStr(res));
}

bool RegisterOrUnregisterServerDLL(Str dllPath, bool install, Str args) {
    if (FAILED(OleInitialize(nullptr))) {
        return false;
    }

    // make sure that the DLL can find any DLLs it depends on and
    // which reside in the same directory (in this case: libsumatrapdf.dll)
    TempStr dllDir = path::GetDirTemp(dllPath);
    SetDllDirectoryW(CWStrTemp(dllDir));

    defer {
        SetDllDirectoryW(L"");
        OleUninitialize();
    };

    HMODULE lib = LoadLibraryA(dllPath.s);
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
            WCHAR* argsW = CWStrTemp(args);
            ok = SUCCEEDED(DllInstall(install, argsW));
        } else {
            args = nullptr;
        }
    }

    if (!args) {
        Str func = install ? StrL("DllRegisterServer") : StrL("DllUnregisterServer");
        DllRegUnregProc DllRegUnreg = (DllRegUnregProc)GetProcAddress(lib, func.s);
        if (DllRegUnreg) {
            ok = SUCCEEDED(DllRegUnreg());
        }
    }
    return ok;
}

bool RegisterServerDLL(Str dllPath, Str args) {
    return RegisterOrUnregisterServerDLL(dllPath, true, args);
}

bool UnRegisterServerDLL(Str dllPath, Str args) {
    if (!file::Exists(dllPath)) {
        return true;
    }
    return RegisterOrUnregisterServerDLL(dllPath, false, args);
}

//--- HWND: text / visibility / chrome / Z-order

void HwndToForeground(HWND hwnd) {
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
}

int HwndGetTextLen(HWND hwnd) {
    return (int)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

// return text of window or edit control, nullptr in case of an error
TempWStr HwndGetTextWTemp(HWND hwnd) {
    int cch = HwndGetTextLen(hwnd);
    WCHAR* buf = AllocArrayTemp<WCHAR>(cch + 2); // +2 for extra room
    if (!buf) {
        return {};
    }
    LRESULT copied = SendMessageW(hwnd, WM_GETTEXT, cch + 1, (LPARAM)buf);
    return WStr(buf, (int)copied);
}

// return text of window or edit control, nullptr in case of an error
TempStr HwndGetTextTemp(HWND hwnd) {
    int cch = HwndGetTextLen(hwnd);
    WCHAR* buf = AllocArrayTemp<WCHAR>(cch + 2); // +2 jic
    if (!buf) {
        return {};
    }
    LRESULT copied = SendMessageW(hwnd, WM_GETTEXT, cch + 1, (LPARAM)buf);
    WStr txt(buf, (int)copied);
    return ToUtf8Temp(txt);
}

bool HwndHasFrameThickness(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_THICKFRAME);
}

bool HwndHasCaption(HWND hwnd) {
    return bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), WS_CAPTION);
}

bool HwndIsVisible(HWND hwnd) {
    return ::IsWindowVisible(hwnd);
}

void HwndSetVisible(HWND hwnd, bool visible) {
    if (HwndIsVisible(hwnd) == visible) {
        return;
    }
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

void HwndShow(HWND hwnd) {
    HwndSetVisible(hwnd, true);
}

void HwndHide(HWND hwnd) {
    HwndSetVisible(hwnd, false);
}

//--- GDI: bitmaps / pixmaps

// cf. fz_mul255 in fitz.h
static inline int mul255(int a, int b) {
    int x = (a * b) + 128;
    x += x >> 8;
    return x >> 8;
}

// Recolor a rendered page bitmap: map black->textColor and white->bgColor
// (proportionally in between). When linkColor is non-zero, pixels that look
// like link text (blue-ish) are set to linkColor instead. Pixels inside
// skipRects keep their original colors (dark-mode image preservation).
void UpdateBitmapColors(HBITMAP hbmp, Color textColor, Color bgColor, Color linkColor, Vec<Rect>* skipRects) {
    if (!hbmp) {
        return;
    }
    if ((textColor & 0xFFFFFF) == kColBlack && (bgColor & 0xFFFFFF) == kColWhite && !linkColor && !skipRects) {
        return;
    }

    byte linkR = 0, linkG = 0, linkB = 0;
    bool recolorLinks = linkColor != 0;
    if (recolorLinks) {
        UnpackColor(linkColor, linkR, linkG, linkB);
    }

    auto isLikelyLinkPixel = [](u8 r, u8 g, u8 b) -> bool {
        int maxRG = r > g ? r : g;
        if (b < maxRG + 25) {
            return false;
        }
        if (b < 72) {
            return false;
        }
        int lum = (int(r) + g + b) / 3;
        if (lum > 230) {
            return false;
        }
        return true;
    };

    auto setLinkPixel = [&](u8* px) {
        px[0] = linkB;
        px[1] = linkG;
        px[2] = linkR;
    };

    // color order in DIB is blue-green-red-alpha
    byte rt, gt, bt;
    UnpackColor(textColor, rt, gt, bt);
    const int base[4] = {bt, gt, rt, 0};
    byte rb, gb, bb;
    UnpackColor(bgColor, rb, gb, bb);
    int const diff[4] = {(int)bb - base[0], (int)gb - base[1], (int)rb - base[2], 255};

    DIBSECTION info{};
    int ret = GetObject(hbmp, sizeof(info), &info);
    ReportIf(ret < sizeof(info.dsBm));
    Size size(info.dsBm.bmWidth, info.dsBm.bmHeight);

    auto skipPixel = [&](int x, int y) -> bool {
        if (!skipRects) {
            return false;
        }
        for (Rect& sr : *skipRects) {
            if (sr.Contains(x, y)) {
                return true;
            }
        }
        return false;
    };

    // for mapped 32-bit DI bitmaps: directly access the pixel data
    if (ret >= sizeof(info.dsBm) && info.dsBm.bmBits && 32 == info.dsBm.bmBitsPixel &&
        size.dx * 4 == info.dsBm.bmWidthBytes) {
        int bmpBytes = size.dx * size.dy * 4;
        u8* bmpData = (u8*)info.dsBm.bmBits;
        for (int i = 0; i < bmpBytes; i += 4) {
            int x = (i / 4) % size.dx;
            int y = (i / 4) / size.dx;
            u8 b = bmpData[i];
            u8 g = bmpData[i + 1];
            u8 r = bmpData[i + 2];
            if (skipPixel(x, y)) {
                continue;
            }
            if (recolorLinks && isLikelyLinkPixel(r, g, b)) {
                setLinkPixel(&bmpData[i]);
                continue;
            }
            for (int k = 0; k < 4; k++) {
                bmpData[i + k] = (u8)(base[k] + mul255(bmpData[i + k], diff[k]));
            }
        }
        return;
    }

    // for mapped 24-bit DI bitmaps: directly access the pixel data
    if (ret >= sizeof(info.dsBm) && info.dsBm.bmBits && 24 == info.dsBm.bmBitsPixel &&
        info.dsBm.bmWidthBytes >= size.dx * 3) {
        u8* bmpData = (u8*)info.dsBm.bmBits;
        for (int y = 0; y < size.dy; y++) {
            for (int x = 0; x < size.dx; x++) {
                u8* px = bmpData + ((size_t)y * info.dsBm.bmWidthBytes) + ((size_t)x * 3);
                u8 b = px[0];
                u8 g = px[1];
                u8 r = px[2];
                if (skipPixel(x, y)) {
                    continue;
                }
                if (recolorLinks && isLikelyLinkPixel(r, g, b)) {
                    setLinkPixel(px);
                    continue;
                }
                for (int k = 0; k < 3; k++) {
                    px[k] = (u8)(base[k] + mul255(px[k], diff[k]));
                }
            }
        }
        return;
    }

    // for paletted DI bitmaps: only update the color palette
    if (sizeof(info) == ret && info.dsBmih.biBitCount && info.dsBmih.biBitCount <= 8) {
        ReportIf(info.dsBmih.biBitCount != 8);
        RGBQUAD palette[256];
        HDC hDC = CreateCompatibleDC(nullptr);
        DeleteObject(SelectObject(hDC, hbmp));
        uint num = GetDIBColorTable(hDC, 0, dimof(palette), palette);
        for (uint i = 0; i < num; i++) {
            u8 r = palette[i].rgbRed;
            u8 g = palette[i].rgbGreen;
            u8 b = palette[i].rgbBlue;
            if (recolorLinks && isLikelyLinkPixel(r, g, b)) {
                palette[i].rgbRed = linkR;
                palette[i].rgbGreen = linkG;
                palette[i].rgbBlue = linkB;
                continue;
            }
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
    ReportIf(!bmpData);

    if (GetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        for (int i = 0; i < bmpBytes; i++) {
            int k = i % 4;
            bmpData[i] = (u8)(base[k] + mul255(bmpData[i], diff[k]));
        }
        SetDIBits(hDC, hbmp, 0, size.dy, bmpData, &bmi, DIB_RGB_COLORS);
    }

    DeleteDC(hDC);
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
    bool ok = StretchBlt(hdc, x, y, tdx, tdy, bmpDC, 0, 0, dx, dy, SRCCOPY) ? true : false;
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

// close handle returned by FindFirstFile()
bool SafeFindClose(HANDLE* hPtr) {
    HANDLE h = *hPtr;
    if (!IsValidHandle(h)) {
        *hPtr = nullptr;
        return false;
    }
    BOOL ok = FindClose(h);
    *hPtr = nullptr;
    return !!ok;
}

// This is just to satisfy /analyze. CloseHandle(nullptr) works perfectly fine
// but /analyze complains anyway
bool SafeCloseHandle(HANDLE* hPtr) {
    HANDLE h = *hPtr;
    if (!IsValidHandle(h)) {
        *hPtr = nullptr;
        return false;
    }
    BOOL ok = CloseHandle(h);
    *hPtr = nullptr;
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
void RunNonElevated(Str exePath) {
    if (!file::Exists(exePath)) {
        logf("RunNonElevated: file '%s' doesn't exist\n", exePath);
        return;
    }
    logf("RunNonElevated: '%s'\n", exePath);
    TempStr cmd;
    TempStr explorerPath;
    WCHAR buf[MAX_PATH] = {};
    uint res = GetWindowsDirectoryW(buf, dimof(buf));
    if (0 == res || res >= dimof(buf)) {
        goto Run;
    }
    explorerPath = ToUtf8Temp(buf);
    explorerPath = path::JoinTemp(explorerPath, StrL("explorer.exe"));
    if (!file::Exists(explorerPath)) {
        goto Run;
    }
    cmd = fmt("\"%s\" \"%s\"", explorerPath, exePath);
Run:
    HANDLE h = LaunchProcessInDir(len(cmd) == 0 ? exePath : cmd);
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
    Rect rc = HwndWindowRect(hwnd);
    if (dx == -1) {
        dx = rc.dx;
    }
    if (dy == -1) {
        dy = rc.dy;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, dx, dy, SWP_NOMOVE | SWP_NOZORDER);
}

void MessageBoxWarningSimple(HWND hwnd, WStr msg, WStr title) {
    uint type = MB_OK | MB_ICONEXCLAMATION;
    if (len(title) == 0) {
        title = WStrL(L"Warning");
    }
    MessageBoxW(hwnd, msg.s, title.s, type);
}

void MessageBoxNYI(HWND hwnd) {
    MessageBoxWarningSimple(hwnd, L"Not Yet Implemented!", L"NYI");
}

void VariantInitBstr(VARIANT& urlVar, WStr s) {
    VariantInit(&urlVar);
    urlVar.vt = VT_BSTR;
    urlVar.bstrVal = SysAllocStringLen(s.s, s.len);
}

static HDDEDATA CALLBACK DdeCallback(UINT /*type*/, UINT /*fmt*/, HCONV /*hconv*/, HSZ /*hsz1*/, HSZ /*hsz2*/,
                                     HDDEDATA /*hdata*/, ULONG_PTR /*data1*/, ULONG_PTR /*data2*/) {
    return nullptr;
}

bool DDEExecute(WStr server, WStr topic, WStr command) {
    DWORD inst = 0;
    HSZ hszServer = nullptr, hszTopic = nullptr;
    HCONV hconv = nullptr;
    bool ok = false;
    uint result = 0;
    DWORD cbLen = 0;
    HDDEDATA answer;

    ReportIf(command.len >= INT_MAX - 1);
    if (command.len >= INT_MAX - 1) {
        return false;
    }

    result = DdeInitializeW(&inst, DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        return false;
    }

    hszServer = DdeCreateStringHandleW(inst, server.s, CP_WINNEUTRAL);
    if (!hszServer) {
        goto Exit;
    }
    hszTopic = DdeCreateStringHandleW(inst, topic.s, CP_WINNEUTRAL);
    if (!hszTopic) {
        goto Exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, nullptr);
    if (!hconv) {
        goto Exit;
    }

    cbLen = ((DWORD)command.len + 1) * sizeof(WCHAR);
    answer =
        DdeClientTransaction((BYTE*)command.s, cbLen, hconv, nullptr, CF_UNICODETEXT, XTYP_EXECUTE, 10000, nullptr);
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

static LPWSTR knownCursorIds[] = {IDC_ARROW,  IDC_IBEAM,    IDC_HAND,     IDC_SIZEALL, IDC_SIZEWE,
                                  IDC_SIZENS, IDC_SIZENWSE, IDC_SIZENESW, IDC_NO,      IDC_CROSS};

static HCURSOR cachedCursors[dimof(knownCursorIds)]{};

static int GetCursorIndex(LPWSTR cursorId) {
    int n = dimofi(knownCursorIds);
    for (int i = 0; i < n; i++) {
        if (cursorId == knownCursorIds[i]) {
            return i;
        }
    }
    return -1;
}

#if 0
static const char* cursorNames =
    "IDC_ARROW\0IDC_BEAM\0IDC_HAND\0IDC_SIZEALL\0IDC_SIZEWE\0IDC_SIZENS\0IDC_SIZENWSE\0IDC_SIZENESW\0IDC_NO\0IDC_CROSS\0";

static const char* GetCursorName(LPWSTR cursorId) {
    int i = GetCursorIndex(cursorId);
    if (i == -1) {
        return "unknown";
    }
    return SeqStrByIndex(cursorNames, i);
}

static void LogCursor(LPWSTR cursorId) {
    static int n = 0;
    const char* name = GetCursorName(cursorId);
    logf("SetCursor %s 0x%x %d\n", name, (int)(intptr_t)cursorId, n);
    n++;
}
#else
static void LogCursor(LPWSTR /*cursorId*/) {
    // no-op
}
#endif

HCURSOR GetCachedCursor(LPWSTR cursorId) {
    int i = GetCursorIndex(cursorId);
    ReportIf(i < 0);
    if (i < 0) {
        return nullptr;
    }
    if (nullptr == cachedCursors[i]) {
        cachedCursors[i] = LoadCursor(nullptr, cursorId);
        ReportIf(cachedCursors[i] == nullptr);
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
    for (HCURSOR& cur : cachedCursors) {
        if (cur) {
            DestroyCursor(cur);
            cur = nullptr;
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

// http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
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
    int xPadding = DpiScale(8 * 2);
    int yPadding = DpiScale(2 * 2);
    s.cx += xPadding;
    s.cy += yPadding;
    Size res = {s.cx, s.cy};
    return res;
}

constexpr int kResourceNotFound = -1;

bool LockDataResource(int resId, LoadedDataResource* res) {
    if (res->dataSize != 0) {
        return res->dataSize != kResourceNotFound;
    }

    auto* h = GetModuleHandleW(nullptr);
    WCHAR* name = MAKEINTRESOURCEW(resId);
    HRSRC resSrc = FindResourceW(h, name, RT_RCDATA);
    if (!resSrc) {
        res->dataSize = kResourceNotFound;
        return false;
    }
    HGLOBAL hres = LoadResource(nullptr, resSrc);
    if (!hres) {
        res->dataSize = kResourceNotFound;
        return false;
    }
    res->data = (const u8*)LockResource(hres);
    res->dataSize = (int)SizeofResource(nullptr, resSrc);
    return true;
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

//--- HWND: text / font / icon / paint / position / messages

void HwndSetText(HWND hwnd, Str sv) {
    // can be called before a window is created
    if (!hwnd) {
        return;
    }
    if (len(sv) == 0) {
        sv = Str();
    }
    // WM_SETTEXT unconditionally invalidates and repaints the control (and, for
    // edit controls, fires EN_CHANGE and resets the caret/selection). Skip it
    // when the text is unchanged so callers don't cause needless repaints /
    // flicker (e.g. the toolbar page box on Back when the page doesn't change).
    // Every caller is fine with this: those that need a re-search/notification on
    // unchanged text trigger it explicitly, not via the EN_CHANGE side effect.
    TempStr current = HwndGetTextTemp(hwnd);
    if (current && str::Eq(current, sv)) {
        return;
    }
    WCHAR* ws = CWStrTemp(sv);
    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)ws);
}

void HwndSetDlgItemText(HWND hDlg, int itemID, Str s) {
    WCHAR* ws = CWStrTemp(s);
    SetDlgItemTextW(hDlg, itemID, ws);
}

// hwnd should be Combo Box control
void CbAddString(HWND hwnd, Str s) {
    WCHAR* ws = CWStrTemp(s);
    SendMessageW(hwnd, CB_ADDSTRING, 0, (LPARAM)ws);
}

// hwnd should be Combo Box control
void CbSetCurrentSelection(HWND hwnd, int selIdx) {
    SendMessageW(hwnd, CB_SETCURSEL, (WPARAM)selIdx, 0);
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

// schedule WM_PAINT at window's leasure
void HwndScheduleRepaint(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd)) {
        return;
    }
    HwndInvalidate(hwnd);
}

// do WM_PAINT immediately
void HwndRepaintNow(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd)) {
        return;
    }
    HwndInvalidate(hwnd);
    // send WM_PAINT right away (normally would wait for empty msg queue)
    UpdateWindow(hwnd);
}

void HwndSetFont(HWND hwnd, HFONT font) {
    if (!hwnd || !font) {
        return;
    }
    SetWindowFont(hwnd, font, TRUE);
}

static BOOL CALLBACK SetFontChildProc(HWND hwnd, LPARAM lp) {
    SetWindowFont(hwnd, (HFONT)lp, TRUE);
    return TRUE;
}

// Set the font on hwnd and every descendant. Handy after a DPI change, when the
// whole dialog has to move to a font scaled for the new DPI.
void HwndSetFontForWindowAndItsChildren(HWND hwnd, HFONT font) {
    if (!hwnd || !font) {
        return;
    }
    SetWindowFont(hwnd, font, TRUE);
    EnumChildWindows(hwnd, SetFontChildProc, (LPARAM)font);
}

void HwndSetTreeFontForDpi(HWND hwndTree, HFONT font, int dpi) {
    if (!hwndTree || !font) {
        return;
    }
    if (dpi <= 0) {
        dpi = RoundUp(DpiGetForHwnd(hwndTree), 4);
    }
    HwndSetFont(hwndTree, font);
    HDC dc = GetDC(hwndTree);
    if (!dc) {
        return;
    }
    {
        ScopedSelectFont selectFont(dc, font);
        TEXTMETRICW tm{};
        if (GetTextMetricsW(dc, &tm)) {
            int itemH = tm.tmHeight + tm.tmExternalLeading + MulDiv(4, dpi, 96);
            SendMessageW(hwndTree, TVM_SETITEMHEIGHT, (WPARAM)itemH, 0);
        }
    }
    ReleaseDC(hwndTree, dc);
}

// change size of the window to have a given client size
void HwndResizeClientSize(HWND hwnd, int dx, int dy) {
    Rect rc = HwndWindowRect(hwnd);
    int x = rc.x;
    int y = rc.y;
    DWORD style = GetWindowStyle(hwnd);
    DWORD exStyle = GetWindowExStyle(hwnd);
    RECT r = {x, y, x + dx, y + dy};
    BOOL ok = AdjustWindowRectEx(&r, style, false, exStyle);
    ReportIf(!ok);
    int dx2 = RectDx(r);
    int dy2 = RectDy(r);
    ok = SetWindowPos(hwnd, nullptr, 0, 0, dx2, dy2, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREPOSITION);
    ReportIf(!ok);
}

// position hwnd on the right of hwndRelative
void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative) {
    Rect rHwnd = HwndWindowRect(hwnd);
    Rect rHwndRelative = HwndWindowRect(hwndRelative);
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
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);

    Rect r2 = {x, y, r.dx, r.dy};
    r = ShiftRectToWorkArea(r2, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void HwndSendCommand(HWND hwnd, int cmdId, LPARAM lp) {
    SendMessageW(hwnd, WM_COMMAND, (WPARAM)cmdId, lp);
}

void HwndPostCommand(HWND hwnd, int cmdId, LPARAM lp) {
    PostMessageW(hwnd, WM_COMMAND, (WPARAM)cmdId, lp);
}

void HwndDestroyWindowSafe(HWND* hwndPtr) {
    auto* hwnd = *hwndPtr;
    *hwndPtr = nullptr;

    if (!hwnd || !::IsWindow(hwnd)) {
        return;
    }
    ::DestroyWindow(hwnd);
}

//--- toolbar / GDI handles / tree view / HGLOBAL / timing

void TbSetButtonStructSize(HWND hwnd, int size) {
    SendMessageW(hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)size, 0);
}

void TbAddButtons(HWND hwnd, int count, const TBBUTTON* buttons) {
    auto res = SendMessageW(hwnd, TB_ADDBUTTONS, count, (LPARAM)buttons);
    ReportDebugIf(0 == res);
}

void TbAutoSize(HWND hwnd) {
    SendMessageW(hwnd, TB_AUTOSIZE, 0, 0);
}

int TbGetButtonCount(HWND hwnd) {
    return (int)SendMessageW(hwnd, TB_BUTTONCOUNT, 0, 0);
}

DWORD TbGetExtendedStyle(HWND hwnd) {
    return (DWORD)SendMessageW(hwnd, TB_GETEXTENDEDSTYLE, 0, 0);
}

void TbSetExtendedStyle(HWND hwnd, DWORD style) {
    SendMessageW(hwnd, TB_SETEXTENDEDSTYLE, 0, style);
}

Rect TbGetItemRect(HWND hwnd, int buttonIdx) {
    if (!hwnd) {
        return {};
    }
    RECT rc{};
    auto res = SendMessageW(hwnd, TB_GETITEMRECT, buttonIdx, (LPARAM)&rc);
    if (res == 0) {
        logf("TbGetItemRect: hwnd=0x%p, buttonIdx: %d\n", hwnd, buttonIdx);
        LogLastError();
        ReportIf(res == 0);
    }
    return {rc};
}

bool DeleteObjectSafe(HGDIOBJ* h) {
    if (!h || !*h) {
        return false;
    }
    auto res = ::DeleteObject(*h);
    *h = nullptr;
    return ToBool(res);
}

bool DeleteBrushSafe(HBRUSH* br) {
    return DeleteObjectSafe((HGDIOBJ*)br);
}

bool DestroyIconSafe(HICON* h) {
    if (!h || !*h) {
        return false;
    }
    auto res = ::DestroyIcon(*h);
    *h = nullptr;
    return ToBool(res);
}

//--- GDI: draw / measure (text)

int HdcDrawText(HDC hdc, WStr s, const Rect& r, uint format, HFONT font) {
    ReportIf(format & DT_CALCRECT);
    if (len(s) == 0) {
        return 0;
    }
    ScopedSelectFont f(hdc, font);
    RECT r2 = ToRECT(r);
    return DrawTextW(hdc, s.s, s.len, &r2, format);
}

int HdcDrawText(HDC hdc, Str s, const Rect& r, uint format, HFONT font) {
    return HdcDrawText(hdc, ToWStrTemp(s), r, format, font);
}

int HdcDrawText(HDC hdc, Str s, const Point& pos, uint format, HFONT font) {
    Rect r = {pos.x, pos.y, 0, 0};
    return HdcDrawText(hdc, s, r, format, font);
}

int HdcDrawText(HDC hdc, WStr s, const Point& pos, uint format, HFONT font) {
    Rect r = {pos.x, pos.y, 0, 0};
    return HdcDrawText(hdc, s, r, format, font);
}

static Rect HdcMeasureWithDrawText(HDC hdc, WStr s, Rect r, uint format, HFONT font) {
    if (len(s) == 0) {
        return r;
    }
    ScopedSelectFont f(hdc, font);
    RECT r2 = ToRECT(r);
    DrawTextW(hdc, s.s, s.len, &r2, format | DT_CALCRECT);
    return ToRect(r2);
}

static Rect HdcMeasureWithDrawText(HDC hdc, Str s, Rect r, uint format, HFONT font) {
    return HdcMeasureWithDrawText(hdc, ToWStrTemp(s), r, format, font);
}

bool HdcExTextOut(HDC hdc, Point pos, uint options, const Rect& rect, WStr text) {
    RECT r = ToRECT(rect);
    RECT* rectPtr = rect.IsEmpty() ? nullptr : &r;
    return ExtTextOutW(hdc, pos.x, pos.y, options, rectPtr, text.s, (uint)text.len, nullptr) != 0;
}

bool HdcExTextOut(HDC hdc, Point pos, uint options, const Rect& rect, Str text) {
    return HdcExTextOut(hdc, pos, options, rect, ToWStrTemp(text));
}

void HdcFillRectWithBkColor(HDC hdc, const Rect& rect) {
    RECT r = ToRECT(rect);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &r, nullptr, 0, nullptr);
}

// uses the same logic as HdcDrawText
// maxDx limits the width, used when measuring text wrapped with DT_WORDBREAK
Size HdcMeasureText(HDC hdc, Str s, int maxDx, uint format, HFONT font) {
    if (len(s) == 0) {
        return {};
    }
    Rect bounds = {0, 0, maxDx, 4096};
    Rect measured = HdcMeasureWithDrawText(hdc, s, bounds, format, font);
    return {measured.dx, measured.dy};
}

void HdcDrawCenteredText(HDC hdc, Rect r, Str txt, bool isRTL) {
    int prevMode = SetBkMode(hdc, TRANSPARENT);
    uint format = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    if (isRTL) {
        format |= DT_RTLREADING;
    }
    HdcDrawText(hdc, txt, r, format);
    if (prevMode != 0) {
        SetBkMode(hdc, prevMode);
    }
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

// SHAddToRecentDocs can block on network paths (shell resolves / writes Recent).
// Run those off the UI thread with COM initialized and a heap-owned path.
static void AddPathToRecentDocsOnThread(WCHAR* pathW) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitedByUs = SUCCEEDED(hr);
    if (!comInitedByUs && hr != RPC_E_CHANGED_MODE) {
        logf("AddPathToRecentDocsOnThread: CoInitializeEx failed hr=0x%08x\n", (unsigned)hr);
        free(pathW);
        return;
    }

    if (pathW) {
        SHAddToRecentDocs(SHARD_PATH, pathW);
    }
    free(pathW);

    if (comInitedByUs) {
        CoUninitialize();
    }
}

void AddPathToRecentDocs(Str path) {
    if (!path) {
        return;
    }
    if (!path::IsOnNetworkDrive(path)) {
        WCHAR* pathW = CWStrTemp(path);
        SHAddToRecentDocs(SHARD_PATH, pathW);
        return;
    }

    WCHAR* owned = ToWStr(path).s; // heap; thread frees
    if (!owned) {
        return;
    }
    RunAsync(MkFunc0(AddPathToRecentDocsOnThread, owned), StrL("AddPathToRecentDocs"));
}

TempStr HGLOBALToStrTemp(HGLOBAL h, bool isUnicode) {
    void* mem = GlobalLock(h);
    if (!mem) {
        return {};
    }

    TempStr res;
    if (isUnicode) {
        res = ToUtf8Temp(WStr((WCHAR*)mem));
    } else {
        res = str::DupTemp(Str((char*)mem));
    }
    GlobalUnlock(h);
    return res;
}

HGLOBAL MemToHGLOBAL(void* src, int n, UINT flags) {
    HGLOBAL h = GlobalAlloc(flags, n);
    if (!h) {
        return nullptr;
    }
    void* d = GlobalLock(h);
    if (d) {
        memcpy(d, src, n);
    }
    GlobalUnlock(h);
    return h;
}

HGLOBAL StrToHGLOBAL(Str s, UINT flags) {
    int cb = len(s) + 1;
    return MemToHGLOBAL((void*)s.s, cb, flags);
}

TempStr AtomToStrTemp(ATOM a) {
    WCHAR buf[1024];
    UINT cch = GlobalGetAtomNameW(a, buf, dimofi(buf));
    if (cch == 0) {
        return {};
    }
    return ToUtf8Temp(WStr(buf, (int)cch));
}

int MsgBox(HWND hwnd, Str text, Str caption, UINT flags) {
    WCHAR* textW = CWStrTemp(text);
    WCHAR* captionW = CWStrTemp(caption);
    return MessageBoxW(hwnd, textW, captionW, flags);
}

// Some 3rd-party DLLs loaded into our process (e.g. ffmpeg-based WIC codecs
// like CopyTrans HEIC, printer drivers, shell extensions) unmask floating-point
// exceptions in the per-thread FPU/MXCSR control word and don't restore it.
// We (and mupdf) rely on the default environment where FP exceptions are masked
// e.g. comparing against NaN must not trap (EXCEPTION_FLT_INVALID_OPERATION).
// Call this after code paths that might run such DLLs.
void MaskFpExceptions() {
    _clearfp();
    uint unused;
    _controlfp_s(&unused, _MCW_EM, _MCW_EM);
}

//--- OS / process / CPU (SIMD)

u32 CpuID() {
#if IS_ARM_64
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent
    u32 res = 0;
    if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE)) {
        res |= kCpuNEON;
    }
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)) {
        res |= kCpuArmCrypto;
    }
    if (IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)) {
        res |= kCpuArmAtomics;
    }
    if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) {
        res |= kCpuArmDotProd;
    }
    return res;
#else
    // https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
    std::bitset<32> f_1_ECX_;
    std::bitset<32> f_1_EDX_;
    std::bitset<32> f_7_EBX_;
    std::bitset<32> f_7_ECX_;

    u32 res = 0;
    int cpuInfo[4]{};
#if COMPILER_MINGW
    __cpuid(0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#else
    __cpuid(cpuInfo, 0);
#endif
    int nIds = cpuInfo[0];
    if (nIds >= 1) {
#if COMPILER_MINGW
        __cpuid(1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#else
        __cpuid(cpuInfo, 1);
#endif
        f_1_ECX_ = cpuInfo[2];
        f_1_EDX_ = cpuInfo[3];
    }
    if (nIds >= 7) {
#if COMPILER_MINGW
        __cpuid_count(7, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#else
        __cpuid(cpuInfo, 7);
#endif
        f_7_EBX_ = cpuInfo[1];
        f_7_ECX_ = cpuInfo[2];
    }

    if (f_1_EDX_[23]) {
        res = res | kCpuMMX;
    }
    if (f_1_EDX_[25]) {
        res = res | kCpuSSE;
    }
    if (f_1_EDX_[26]) {
        res = res | kCpuSSE2;
    }
    if (f_1_ECX_[0]) {
        res = res | kCpuSSE3;
    }
    if (f_1_ECX_[9]) {
        res = res | kCpuSSE3;
    }
    if (f_1_ECX_[19]) {
        res = res | kCpuSSE41;
    }
    if (f_1_ECX_[20]) {
        res = res | kCpuSSE42;
    }
    if (f_1_ECX_[28]) {
        res = res | kCpuAVX;
    }
    if (f_7_EBX_[5]) {
        res = res | kCpuAVX2;
    }
    return res;
#endif
}

Str LatestSupportedSIMD() {
    u32 id = CpuID();
    // x86/x64
    if (id & kCpuAVX2) {
        return StrL("avx2");
    }
    if (id & kCpuAVX) {
        return StrL("avx");
    }
    if (id & kCpuSSE42) {
        return StrL("sse42");
    }
    if (id & kCpuSSE41) {
        return StrL("sse41");
    }
    if (id & kCpuSSE3) {
        return StrL("sse3");
    }
    if (id & kCpuSSE2) {
        return StrL("sse2");
    }
    if (id & kCpuSSE) {
        return StrL("sse");
    }
    // ARM
    if (id & kCpuArmDotProd) {
        return StrL("dotprod");
    }
    if (id & kCpuNEON) {
        return StrL("neon");
    }
    return StrL("none");
}

//--- timing

LARGE_INTEGER TimeNow() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now;
}

double TimeDiffSecs(const LARGE_INTEGER& start, const LARGE_INTEGER& end) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    auto diff = end.QuadPart - start.QuadPart;
    double res = (double)(diff) / (double)(freq.QuadPart);
    return res;
}

double TimeDiffMs(const LARGE_INTEGER& start, const LARGE_INTEGER& end) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    auto diff = end.QuadPart - start.QuadPart;
    double res = (double)(diff) / (double)(freq.QuadPart);
    return res * 1000;
}

//--- GDI: draw (misc) / DC state

void HdcPaintCheckerboard(HDC hdc, int x, int y, int w, int h) {
    constexpr int kCheckerSize = 8;
    Color lightColor = kColWhite;
    Color darkColor = MkRgb(204, 204, 204);
    HBRUSH lightBrush = CreateSolidBrush(lightColor);
    HBRUSH darkBrush = CreateSolidBrush(darkColor);

    for (int cy = 0; cy < h; cy += kCheckerSize) {
        for (int cx = 0; cx < w; cx += kCheckerSize) {
            int cellW = std::min(kCheckerSize, w - cx);
            int cellH = std::min(kCheckerSize, h - cy);
            RECT rc = {x + cx, y + cy, x + cx + cellW, y + cy + cellH};
            bool isDark = ((cx / kCheckerSize) + (cy / kCheckerSize)) % 2 != 0;
            HdcFillRect(hdc, ToRect(rc), isDark ? darkBrush : lightBrush);
        }
    }

    DeleteObject(lightBrush);
    DeleteObject(darkBrush);
}

// --- begin: merged from former src/common/win_util.cpp ---

//--- DC state

SavedDCState SaveDCState(HWND hwnd) {
    SavedDCState state = {};
    state.hwnd = hwnd;
    state.hdc = GetDC(hwnd);
    HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    if (hFont) {
        state.oldFont = (HFONT)SelectObject(state.hdc, hFont);
    }
    return state;
}

void RestoreDCState(SavedDCState* state) {
    if (state->oldFont) {
        SelectObject(state->hdc, state->oldFont);
    }
    ReleaseDC(state->hwnd, state->hdc);
}

Size HdcGetTextExtentPoint32(HDC hdc, WStr str) {
    SIZE size{};
    GetTextExtentPoint32W(hdc, str.s, str.len, &size);
    return {(int)size.cx, (int)size.cy};
}

Size HdcGetTextExtentPoint32(HDC hdc, Str str) {
    return HdcGetTextExtentPoint32(hdc, ToWStrTemp(str));
}

int HdcMeasureStringWidth(HDC hdc, WStr str) {
    return HdcGetTextExtentPoint32(hdc, str).dx;
}

Str GetLastErrorAsStr(Arena* arena) {
    DWORD err = GetLastError();
    if (!err) {
        return str::Dup(arena, StrL("no error"));
    }
    wchar_t* msgBuf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msgBuf, 0, nullptr);
    if (!msgBuf) {
        return str::Dup(arena, StrL("FormatMessageW() failed"));
    }
    auto ws = WStr(msgBuf);
    Str temp = ToUtf8(GetTempArena(), WStr(msgBuf));
    temp = str::TrimSuffixWhitespace(temp);
    Str result = fmt("0x%08lX '%s'", err, temp);
    LocalFree(msgBuf);
    return str::Dup(arena, result);
}

// Check if we were launched by PowerShell with stdout redirected to a pipe.
// PowerShell's pipe redirection has known issues with GUI apps using WriteFile.
bool WasLaunchedByPowershellWithPipeRedirect() {
    // Check if stdout is a pipe
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE || hStdout == nullptr) {
        return false;
    }
    if (GetFileType(hStdout) != FILE_TYPE_PIPE) {
        return false;
    }

    // Get our parent process ID
    DWORD parentPid = 0;
    DWORD myPid = GetCurrentProcessId();

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == myPid) {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    if (parentPid == 0) {
        CloseHandle(hSnapshot);
        return false;
    }

    // Find parent process name
    Str parentName;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == parentPid) {
                parentName = ToUtf8Temp(WStr(pe.szExeFile));
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    return str::StartsWithI(parentName, StrL("pwsh.exe")) || str::StartsWithI(parentName, StrL("powershell"));
}

Str GetAppLocalDataDirTemp() {
    wchar_t* path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
    if (FAILED(hr) || !path) {
        return {};
    }
    Str result = ToUtf8Temp(WStr(path));
    CoTaskMemFree(path);
    return result;
}
// --- end: merged from former src/common/win_util.cpp ---
