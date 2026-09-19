/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "base/AutoWin.h"
#include "base/Win.h"
#include "base/WinDynCalls.h"

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
static HMODULE SafeLoadLibrary(Str dllName) {
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

static const char* dllsToPreload =
    "gdiplus.dll\0msimg32.dll\0shlwapi.dll\0urlmon.dll\0version.dll\0windowscodecs.dll\0wininet.dll\0";

// try to mitigate dll hijacking by pre-loading all the dlls that we delay load or might
// be loaded indirectly
void NoDllHijacking() {
    for (Str dll = SeqStrFirst(dllsToPreload); len(dll) > 0; dll = SeqStrNext(dll)) {
        SafeLoadLibrary(dll);
    }
}
