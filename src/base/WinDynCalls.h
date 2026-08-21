/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

void InitDynCalls();

#if OS_WIN

// as an exception, we include system headers needed for the calls that we dynamically load
// (and a few related headers that call sites historically got via this include)
#include <windows.h>
#include <vssym32.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <oleacc.h>
#include <winnls.h>
#include <processthreadsapi.h>

// dbghelp.h is included here so that warning C4091 can be disabled in a single location
#pragma warning(push)
// VS2015: 'typedef ': ignored on left of '' when no variable is declared
#pragma warning(disable : 4091)
#include <dbghelp.h>
#pragma warning(pop)

#define API_DECLARATION2(name)          \
    typedef decltype(name)* Sig_##name; \
    extern Sig_##name Dyn##name;

// mingw-w64 headers before v12 (e.g. Debian's 10.0.0) don't declare
// SetThreadDescription; the decltype in API_DECLARATION2 needs a declaration.
// An identical redeclaration is harmless on newer headers.
#ifdef __MINGW32__
extern "C" WINBASEAPI HRESULT WINAPI SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
#endif

// mingw-w64 headers before v12 (e.g. Ubuntu 24.04's 11.0.1) also predate the
// Windows 11 DWM additions; the dwm:: wrappers pass attributes as DWORD.
#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 12
typedef enum {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3,
} DWM_WINDOW_CORNER_PREFERENCE;
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWMWA_BORDER_COLOR 34
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif

// kernel32.dll — only APIs not guaranteed on stock Windows 7
#define KERNEL32_API_LIST(V)    \
    V(SetDefaultDllDirectories) \
    V(SetThreadDescription)

// TODO: only available in 20348, not yet present in SDK?
// V(GetTempPath2W)

KERNEL32_API_LIST(API_DECLARATION2)

// not declared in SDK headers with _WIN32_WINNT=0x0601, define manually
typedef BOOL(WINAPI* Sig_GetProcessInformation)(HANDLE, int, LPVOID, DWORD);
typedef BOOL(WINAPI* Sig_SetProcessMitigationPolicy)(int, PVOID, SIZE_T);
extern Sig_GetProcessInformation DynGetProcessInformation;
extern Sig_SetProcessMitigationPolicy DynSetProcessMitigationPolicy;

// not declared in SDK headers with _WIN32_WINNT=0x0601, define manually
typedef UINT(WINAPI* Sig_GetDpiForWindow)(HWND);
typedef HANDLE(WINAPI* Sig_GetThreadDpiAwarenessContext)(void);
typedef int(WINAPI* Sig_GetAwarenessFromDpiAwarenessContext)(HANDLE);
typedef HANDLE(WINAPI* Sig_SetThreadDpiAwarenessContext)(HANDLE);
typedef BOOL(WINAPI* Sig_SystemParametersInfoForDpi)(UINT, UINT, PVOID, UINT, UINT);
typedef int(WINAPI* Sig_GetSystemMetricsForDpi)(int, UINT);
extern Sig_GetDpiForWindow DynGetDpiForWindow;
extern Sig_GetThreadDpiAwarenessContext DynGetThreadDpiAwarenessContext;
extern Sig_GetAwarenessFromDpiAwarenessContext DynGetAwarenessFromDpiAwarenessContext;
extern Sig_SetThreadDpiAwarenessContext DynSetThreadDpiAwarenessContext;
extern Sig_SystemParametersInfoForDpi DynSystemParametersInfoForDpi;
extern Sig_GetSystemMetricsForDpi DynGetSystemMetricsForDpi;

// shcore.dll
typedef HRESULT(WINAPI* Sig_GetDpiForMonitor)(HMONITOR, int, UINT*, UINT*);
extern Sig_GetDpiForMonitor DynGetDpiForMonitor;

// dbghelp.dll, there are different versions not sure if I can rely on
// this to be always present on every Windows version
#define DBGHELP_API_LIST(V)     \
    V(MiniDumpWriteDump)        \
    V(SymInitializeW)           \
    V(SymCleanup)               \
    V(SymGetOptions)            \
    V(SymSetOptions)            \
    V(StackWalk64)              \
    V(SymFromAddr)              \
    V(SymFunctionTableAccess64) \
    V(SymGetModuleBase64)       \
    V(SymSetSearchPathW)        \
    V(SymSetSearchPath)         \
    V(SymRefreshModuleList)     \
    V(SymGetLineFromAddr64)

DBGHELP_API_LIST(API_DECLARATION2)

#undef API_DECLARATION2

void NoDllHijacking();
void PrioritizeSystemDirectoriesForDllLoad();

#endif
