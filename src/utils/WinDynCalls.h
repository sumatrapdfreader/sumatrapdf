/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

// dbghelp.h is included here so that warning C4091 can be disabled in a single location
#pragma warning(push)
// VS2015: 'typedef ': ignored on left of '' when no variable is declared
#pragma warning(disable : 4091)
#include <dbghelp.h>
#pragma warning(pop)
#include <tlhelp32.h>

typedef decltype(SetProcessDEPPolicy)* Sig_SetProcessDEPPolicy;
// typedef BOOL(WINAPI* Sig_SetProcessDEPPolicy)(DWORD dwFlags);

typedef decltype(IsWow64Process)* Sig_IsWow64Process;
// typedef BOOL(WINAPI* Sig_IsWow64Process)(HANDLE, PBOOL);

typedef decltype(SetDllDirectoryW)* Sig_SetDllDirectoryW;
// typedef BOOL(WINAPI* Sig_SetDllDirectoryW)(LPCWSTR);

// TODO: not available in 32bit XP SDK
// typedef decltype(SetDefaultDllDirectories)* Sig_SetDefaultDllDirectories;
typedef BOOL(WINAPI* Sig_SetDefaultDllDirectories)(DWORD);

// TODO: not available in 32bit XP SDK
// typedef decltype(SetProcessMitigationPolicy)* Sig_SetProcessMitigationPolicy;
typedef BOOL(WINAPI* Sig_SetProcessMitigationPolicy)(int, PVOID, SIZE_T);

typedef decltype(RtlCaptureContext)* Sig_RtlCaptureContext;
// typedef void(WINAPI* Sig_RtlCaptureContext)(PCONTEXT);

#define KERNEL32_API_LIST(V)    \
    V(SetProcessDEPPolicy)      \
    V(IsWow64Process)           \
    V(SetDllDirectoryW)         \
    V(SetDefaultDllDirectories) \
    V(RtlCaptureContext)        \
    V(SetProcessMitigationPolicy)

// ntdll.dll
#define PROCESS_EXECUTE_FLAGS 0x22
#define MEM_EXECUTE_OPTION_DISABLE 0x1
#define MEM_EXECUTE_OPTION_ENABLE 0x2
#define MEM_EXECUTE_OPTION_PERMANENT 0x8
#define MEM_EXECUTE_OPTION_DISABLE_ATL 0x4

/* enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4 */
typedef HRESULT(WINAPI* Sig_NtSetInformationProcess)(HANDLE ProcessHandle, UINT ProcessInformationClass,
                                                     PVOID ProcessInformation, ULONG ProcessInformationLength);

#define NTDLL_API_LIST(V) V(NtSetInformationProcess)

// uxtheme.dll
typedef decltype(IsAppThemed)* Sig_IsAppThemed;
// typedef BOOL(WINAPI* Sig_IsAppThemed)();

typedef decltype(OpenThemeData)* Sig_OpenThemeData;
// typedef HTHEME(WINAPI* Sig_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList);

typedef decltype(CloseThemeData)* Sig_CloseThemeData;
// typedef HRESULT(WINAPI* Sig_CloseThemeData)(HTHEME hTheme);

typedef decltype(DrawThemeBackground)* Sig_DrawThemeBackground;
// typedef HRESULT(WINAPI* Sig_DrawThemeBackground)(HTHEME, HDC, int, int, LPCRECT, LPCRECT);

typedef decltype(IsThemeActive)* Sig_IsThemeActive;
// typedef BOOL(WINAPI* Sig_IsThemeActive)(void);

typedef decltype(IsThemeBackgroundPartiallyTransparent)* Sig_IsThemeBackgroundPartiallyTransparent;
// typedef BOOL(WINAPI* Sig_IsThemeBackgroundPartiallyTransparent)(HTHEME, int, int);

typedef decltype(GetThemeColor)* Sig_GetThemeColor;
// typedef HRESULT(WINAPI* Sig_GetThemeColor)(HTHEME, int, int, int, COLORREF*);

typedef decltype(SetWindowTheme)* Sig_SetWindowTheme;
// typedef HRESULT(WINAPI* Sig_SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);

#define UXTHEME_API_LIST(V)                  \
    V(IsAppThemed)                           \
    V(OpenThemeData)                         \
    V(CloseThemeData)                        \
    V(DrawThemeBackground)                   \
    V(IsThemeActive)                         \
    V(IsThemeBackgroundPartiallyTransparent) \
    V(SetWindowTheme)                        \
    V(GetThemeColor)

/// dwmapi.dll
typedef HRESULT(WINAPI* Sig_DwmIsCompositionEnabled)(BOOL* pfEnabled);
typedef HRESULT(WINAPI* Sig_DwmExtendFrameIntoClientArea)(HWND hwnd, const MARGINS* pMarInset);
typedef BOOL(WINAPI* Sig_DwmDefWindowProc)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);
typedef HRESULT(WINAPI* Sig_DwmGetWindowAttribute)(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute);

#define DWMAPI_API_LIST(V)          \
    V(DwmIsCompositionEnabled)      \
    V(DwmExtendFrameIntoClientArea) \
    V(DwmDefWindowProc)             \
    V(DwmGetWindowAttribute)

// normaliz.dll
typedef int(WINAPI* Sig_NormalizeString)(int, LPCWSTR, int, LPWSTR, int);

#define NORMALIZ_API_LIST(V) V(NormalizeString)

// uiautomationcore.dll, not available under Win2000
typedef LRESULT(WINAPI* Sig_UiaReturnRawElementProvider)(HWND hwnd, WPARAM wParam, LPARAM lParam,
                                                         IRawElementProviderSimple* el);
typedef HRESULT(WINAPI* Sig_UiaHostProviderFromHwnd)(HWND hwnd, IRawElementProviderSimple** pProvider);
typedef HRESULT(WINAPI* Sig_UiaRaiseAutomationEvent)(IRawElementProviderSimple* pProvider, EVENTID id);
typedef HRESULT(WINAPI* Sig_UiaRaiseStructureChangedEvent)(IRawElementProviderSimple* pProvider,
                                                           StructureChangeType structureChangeType, int* pRuntimeId,
                                                           int cRuntimeIdLen);
typedef HRESULT(WINAPI* Sig_UiaGetReservedNotSupportedValue)(IUnknown** punkNotSupportedValue);

#define UIA_API_LIST(V)              \
    V(UiaReturnRawElementProvider)   \
    V(UiaHostProviderFromHwnd)       \
    V(UiaRaiseAutomationEvent)       \
    V(UiaRaiseStructureChangedEvent) \
    V(UiaGetReservedNotSupportedValue)

// user32.dll

// TODO: this shold be defined somewhere since msdn docs
// mention HIDWORD https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-gestureinfo
#if 1
#ifndef _QWORD_DEFINED
#define _QWORD_DEFINED
typedef unsigned __int64 QWORD, *LPQWORD;
#endif

#ifndef MAKEQWORD
#define MAKEQWORD(a, b) ((QWORD)(((QWORD)((DWORD)(a))) << 32 | ((DWORD)(b))))
#define LODWORD(l) ((DWORD)((l)&0xFFFFFFFF))
#define HIDWORD(l) ((DWORD)(((QWORD)(l) >> 32) & 0xFFFFFFFF))
#endif
#endif

// typedef decltype(GetGestureInfo) Sig_GetGestureInfo;
typedef BOOL(WINAPI* Sig_GetGestureInfo)(HGESTUREINFO, PGESTUREINFO);

// typedef decltype(CloseGestureInfoHandle) Sig_CloseGestureInfoHandle;
typedef BOOL(WINAPI* Sig_CloseGestureInfoHandle)(HGESTUREINFO);

// typedef decltype(SetGestureConfig) Sig_GetGestSig_SetGestureConfigureInfo;
typedef BOOL(WINAPI* Sig_SetGestureConfig)(HWND, DWORD, UINT, PGESTURECONFIG, UINT);

// typedef decltype(GetThreadDpiAwarenessContext) Sig_GetThreadDpiAwarenessContext;
typedef DPI_AWARENESS_CONTEXT(WINAPI* Sig_GetThreadDpiAwarenessContext)();

// typedef decltype(SetThreadDpiAwarenessContext) Sig_SetThreadDpiAwarenessContext;
typedef DPI_AWARENESS_CONTEXT(WINAPI* Sig_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

typedef DPI_AWARENESS(WINAPI* Sig_GetAwarenessFromDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

// typedef decltype(GetDpiForWindow)* Sig_GetDpiForWindow;
typedef UINT(WINAPI* Sig_GetDpiForWindow)(HWND);

#define USER32_API_LIST(V)                 \
    V(GetDpiForWindow)                     \
    V(GetThreadDpiAwarenessContext)        \
    V(GetAwarenessFromDpiAwarenessContext) \
    V(SetThreadDpiAwarenessContext)        \
    V(SetGestureConfig)                    \
    V(GetGestureInfo)                      \
    V(CloseGestureInfoHandle)

// dbghelp.dll,  may not be available under Win2000
typedef decltype(MiniDumpWriteDump)* Sig_MiniDumpWriteDump;
// typedef BOOL(WINAPI* Sig_MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, LONG, PMINIDUMP_EXCEPTION_INFORMATION,
//                                            PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

typedef BOOL(WINAPI* Sig_SymInitializeW)(HANDLE, PCWSTR, BOOL);
typedef BOOL(WINAPI* Sig_SymInitialize)(HANDLE, PCSTR, BOOL);
typedef BOOL(WINAPI* Sig_SymCleanup)(HANDLE);
typedef DWORD(WINAPI* Sig_SymGetOptions)();
typedef DWORD(WINAPI* Sig_SymSetOptions)(DWORD);

typedef BOOL(WINAPI* Sig_StackWalk64)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME64 StackFrame,
                                      PVOID ContextRecord, PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
                                      PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
                                      PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
                                      PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);

typedef BOOL(WINAPI* Sig_SymFromAddr)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);

typedef PVOID(WINAPI* Sig_SymFunctionTableAccess64)(HANDLE hProcess, DWORD64 AddrBase);

typedef DWORD64(WINAPI* Sig_SymGetModuleBase64)(HANDLE hProcess, DWORD64 qwAddr);

typedef BOOL(WINAPI* Sig_SymSetSearchPathW)(HANDLE hProcess, PCWSTR SearchPath);

typedef BOOL(WINAPI* Sig_SymSetSearchPath)(HANDLE hProcess, PCSTR SearchPath);

typedef BOOL(WINAPI* Sig_SymRefreshModuleList)(HANDLE hProcess);

typedef BOOL(WINAPI* Sig_SymGetLineFromAddr64)(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement,
                                               PIMAGEHLP_LINE64 Line);

#define DBGHELP_API_LIST(V)     \
    V(MiniDumpWriteDump)        \
    V(SymInitializeW)           \
    V(SymInitialize)            \
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

#define API_DECLARATION(name) extern Sig_##name Dyn##name;

KERNEL32_API_LIST(API_DECLARATION)
NTDLL_API_LIST(API_DECLARATION)
UXTHEME_API_LIST(API_DECLARATION)
DWMAPI_API_LIST(API_DECLARATION)
NORMALIZ_API_LIST(API_DECLARATION)
USER32_API_LIST(API_DECLARATION)
UIA_API_LIST(API_DECLARATION)
DBGHELP_API_LIST(API_DECLARATION)
#undef API_DECLARATION

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
BOOL DefWindowProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);
HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute);
}; // namespace dwm

// Touch Gesture API, only available in Windows 7
namespace touch {

bool SupportsGestures();

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
} // namespace touch

namespace uia {

LRESULT ReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple*);
HRESULT HostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple** pProvider);
HRESULT RaiseAutomationEvent(IRawElementProviderSimple* pProvider, EVENTID id);
HRESULT RaiseStructureChangedEvent(IRawElementProviderSimple* pProvider, StructureChangeType structureChangeType,
                                   int* pRuntimeId, int cRuntimeIdLen);
HRESULT GetReservedNotSupportedValue(IUnknown** punkNotSupportedValue);
}; // namespace uia

void NoDllHijacking();
void PrioritizeSystemDirectoriesForDllLoad();
