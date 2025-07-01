/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetProcessDEPPolicy(), we define
a  function pointer DynSetProcessDEPPolicy() (with a signature matching SetProcessDEPPolicy()).

You can test if a function is available with if (DynSetProcessDEPPolicy).

The intent is to standardize how we do it.
*/

// as an exception, we include system headers needed for the calls that we dynamically load
#include <Windows.h>
#include <dwmapi.h>
#include <vssym32.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <OleAcc.h>
#include <WinNls.h>
#include <processthreadsapi.h>

// dbghelp.h is included here so that warning C4091 can be disabled in a single location
#pragma warning(push)
// VS2015: 'typedef ': ignored on left of '' when no variable is declared
#pragma warning(disable : 4091)
#include <dbghelp.h>
#pragma warning(pop)

#define API_DECLARATION(name) extern Sig_##name Dyn##name;

#define API_DECLARATION2(name)          \
    typedef decltype(name)* Sig_##name; \
    extern Sig_##name Dyn##name;

// ntdll.dll
#define PROCESS_EXECUTE_FLAGS 0x22
#define MEM_EXECUTE_OPTION_DISABLE 0x1
#define MEM_EXECUTE_OPTION_ENABLE 0x2
#define MEM_EXECUTE_OPTION_PERMANENT 0x8
#define MEM_EXECUTE_OPTION_DISABLE_ATL 0x4

/* enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4 */
typedef HRESULT(WINAPI* Sig_NtSetInformationProcess)(HANDLE ProcessHandle, UINT ProcessInformationClass,
                                                     PVOID ProcessInformation,
                                                     ULONG ProcessInformationLength); // NOLINT

#define NTDLL_API_LIST(V) V(NtSetInformationProcess)

NTDLL_API_LIST(API_DECLARATION)

// normaliz.dll
// TODO: need to rename our NormalizeString so that it doesn't conflict
// typedef decltype(NormalizeString)* Sig_NormalizeString2;
typedef int(WINAPI* Sig_NormalizeString)(int, LPCWSTR, int, LPWSTR, int);

#define NORMALIZ_API_LIST(V) V(NormalizeString)

NORMALIZ_API_LIST(API_DECLARATION)

// kernel32.dll
#define KERNEL32_API_LIST(V)     \
    V(SetProcessDEPPolicy)       \
    V(IsWow64Process)            \
    V(GetProcessInformation)     \
    V(SetDllDirectoryW)          \
    V(SetDefaultDllDirectories)  \
    V(RtlCaptureContext)         \
    V(RtlCaptureStackBackTrace)  \
    V(SetThreadDescription)      \
    V(GetFinalPathNameByHandleW) \
    V(SetProcessMitigationPolicy)

// TODO: only available in 20348, not yet present in SDK?
// V(GetTempPath2W)

KERNEL32_API_LIST(API_DECLARATION2)

// user32.dll
#define USER32_API_LIST(V)                 \
    V(GetDpiForWindow)                     \
    V(GetThreadDpiAwarenessContext)        \
    V(GetAwarenessFromDpiAwarenessContext) \
    V(SetThreadDpiAwarenessContext)        \
    V(SetGestureConfig)                    \
    V(GetGestureInfo)                      \
    V(CloseGestureInfoHandle)

USER32_API_LIST(API_DECLARATION2)

// uxtheme.dll
#define UXTHEME_API_LIST(V)                  \
    V(IsAppThemed)                           \
    V(OpenThemeData)                         \
    V(CloseThemeData)                        \
    V(DrawThemeBackground)                   \
    V(IsThemeActive)                         \
    V(IsThemeBackgroundPartiallyTransparent) \
    V(SetWindowTheme)                        \
    V(GetThemeColor)

UXTHEME_API_LIST(API_DECLARATION2)

/// dwmapi.dll
#define DWMAPI_API_LIST(V)          \
    V(DwmIsCompositionEnabled)      \
    V(DwmExtendFrameIntoClientArea) \
    V(DwmDefWindowProc)             \
    V(DwmGetWindowAttribute)        \
    V(DwmSetWindowAttribute)

DWMAPI_API_LIST(API_DECLARATION2)

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

#undef API_DECLARATION
#undef API_DECLARATION2

void InitDynCalls();

// convenience wrappers
namespace theme {

bool IsAppThemed();
HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
HRESULT CloseThemeData(HTHEME hTheme);
HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
BOOL IsThemeActive();
BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId);
HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF* pColor);
}; // namespace theme

namespace dwm {

BOOL IsCompositionEnabled();
HRESULT ExtendFrameIntoClientArea(HWND hwnd, const MARGINS* pMarInset);
BOOL DefaultWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT* plResult);
HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute);
HRESULT SetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute);
HRESULT SetCaptionColor(HWND hwnd, COLORREF col);

}; // namespace dwm

// Touch Gesture API, only available in Windows 7
namespace touch {

bool SupportsGestures();

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
} // namespace touch

void NoDllHijacking();
void PrioritizeSystemDirectoriesForDllLoad();
