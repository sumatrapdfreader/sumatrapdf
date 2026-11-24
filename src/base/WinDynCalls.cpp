/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if OS_WIN
#include "base/Win.h"
#include "base/WinDynCalls.h"

#include <delayimp.h>

#define API_DECLARATION(name) Sig_##name Dyn##name = nullptr;

KERNEL32_API_LIST(API_DECLARATION)
DBGHELP_API_LIST(API_DECLARATION)

#undef API_DECLARATION

// manual definitions for functions not in API lists
Sig_GetDpiForWindow DynGetDpiForWindow = nullptr;
Sig_SystemParametersInfoForDpi DynSystemParametersInfoForDpi = nullptr;
Sig_GetSystemMetricsForDpi DynGetSystemMetricsForDpi = nullptr;
Sig_GetDpiForMonitor DynGetDpiForMonitor = nullptr;

#define API_LOAD(name) Dyn##name = (Sig_##name)GetProcAddress(h, #name);

// Loads a DLL explicitly from the system's library collection
HMODULE SafeLoadLibrary(Str dllName) {
    WCHAR dllPath[MAX_PATH];
    uint res = GetSystemDirectoryW(dllPath, dimof(dllPath));
    if (!res || res >= dimof(dllPath)) {
        return nullptr;
    }
    WCHAR* dllNameW = CWStrTemp(dllName);
    BOOL ok = PathAppendW(dllPath, dllNameW);
    if (!ok) {
        return nullptr;
    }
    return LoadLibraryW(dllPath);
}

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetThreadDescription(), we define
a function pointer DynSetThreadDescription() (with a signature matching
SetThreadDescription()).

You can test if a function is available with if (DynSetThreadDescription).

APIs available on our minimum OS (Windows 7) are called directly, not via Dyn*.
*/
void InitDynCalls() {
    HMODULE h = SafeLoadLibrary(StrL("kernel32.dll"));
    ReportIf(!h);
    KERNEL32_API_LIST(API_LOAD);

    h = SafeLoadLibrary(StrL("user32.dll"));
    ReportIf(!h);
    DynGetDpiForWindow = (Sig_GetDpiForWindow)GetProcAddress(h, "GetDpiForWindow");
    DynSystemParametersInfoForDpi = (Sig_SystemParametersInfoForDpi)GetProcAddress(h, "SystemParametersInfoForDpi");
    DynGetSystemMetricsForDpi = (Sig_GetSystemMetricsForDpi)GetProcAddress(h, "GetSystemMetricsForDpi");

    h = SafeLoadLibrary(StrL("shcore.dll"));
    if (h) {
        DynGetDpiForMonitor = (Sig_GetDpiForMonitor)GetProcAddress(h, "GetDpiForMonitor");
    }

    h = SafeLoadLibrary(StrL("dbghelp.dll"));
    if (h) {
        DBGHELP_API_LIST(API_LOAD)
    }
}

#undef API_LOAD

// System dlls that other system dlls load by name, which can look in the exe's
// dir first. Not KnownDLLs (always from the system dir, e.g. gdiplus, shlwapi)
// nor our delay-loads (SafeDelayLoadHook).
static const char* dllsToPreload = "msimg32.dll\0version.dll\0windowscodecs.dll\0";

// try to mitigate dll hijacking: a loaded dll is reused by name instead of
// being searched for again
void NoDllHijacking() {
    for (Str dll = SeqStrFirst(dllsToPreload); len(dll) > 0; dll = SeqStrNext(dll)) {
        SafeLoadLibrary(dll);
    }
}

// our own delay-loaded dll, next to the exe; every other one is the system's
static constexpr Str kAppDelayLoadDll = StrL("libsumatrapdf.dll");

// A delay-loaded dll is otherwise looked up on the default search path, where a
// planted copy could win. Loading the system's from the system dir means a
// delay-loaded dll (urlmon.dll, for CHM) needn't be preloaded at startup.
// Allocation free: it can run before anything is set up.
static FARPROC WINAPI SafeDelayLoadHook(unsigned notify, PDelayLoadInfo info) {
    if (notify != dliNotePreLoadLibrary || !info || !info->szDll) {
        return nullptr;
    }
    if (_stricmp(info->szDll, kAppDelayLoadDll.s) == 0) {
        return nullptr;
    }
    WCHAR path[MAX_PATH];
    uint n = GetSystemDirectoryW(path, dimof(path));
    if (n == 0 || n >= dimof(path) - 1) {
        return nullptr;
    }
    path[n++] = L'\\';
    int nName = MultiByteToWideChar(CP_ACP, 0, info->szDll, -1, path + n, (int)dimof(path) - (int)n);
    if (nName <= 0) {
        return nullptr;
    }
    return (FARPROC)LoadLibraryW(path);
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = SafeDelayLoadHook;

#endif // OS_WIN
