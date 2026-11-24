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

// kernel32.dll — only APIs not guaranteed on stock Windows 7
#define KERNEL32_API_LIST(V) V(SetThreadDescription)

KERNEL32_API_LIST(API_DECLARATION2)

// not declared in SDK headers with _WIN32_WINNT=0x0601, define manually
typedef UINT(WINAPI* Sig_GetDpiForWindow)(HWND);
typedef BOOL(WINAPI* Sig_SystemParametersInfoForDpi)(UINT, UINT, PVOID, UINT, UINT);
typedef int(WINAPI* Sig_GetSystemMetricsForDpi)(int, UINT);
extern Sig_GetDpiForWindow DynGetDpiForWindow;
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
    V(SymGetLineFromAddr64)

DBGHELP_API_LIST(API_DECLARATION2)

#undef API_DECLARATION2

void NoDllHijacking();
HMODULE SafeLoadLibrary(Str dllName);

#endif
