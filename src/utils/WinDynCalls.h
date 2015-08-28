/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetProcessDEPPolicy(), we define
a  function pointer DynSetProcessDEPPolicy() (with a signature matching SetProcessDEPPolicy()).

You can test if a function is available with if (DynSetProcessDEPPolicy).

The intent is to standardize how we do it.
*/

// as an exception, we include the headers needed for the calls that we dynamically load
#include <dwmapi.h>
#include <vssym32.h>

// kernel32.dll
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x2
#endif
typedef BOOL(WINAPI *Sig_SetProcessDEPPolicy)(DWORD dwFlags);
typedef BOOL(WINAPI *Sig_IsWow64Process)(HANDLE, PBOOL);
typedef BOOL(WINAPI *Sig_SetDllDirectoryW)(LPCWSTR);

// ntdll.dll

#define PROCESS_EXECUTE_FLAGS 0x22
#define MEM_EXECUTE_OPTION_DISABLE 0x1
#define MEM_EXECUTE_OPTION_ENABLE 0x2
#define MEM_EXECUTE_OPTION_PERMANENT 0x8
#define MEM_EXECUTE_OPTION_DISABLE_ATL 0x4

/* enable "NX" execution prevention for XP, 2003
* cf. http://www.uninformed.org/?v=2&a=4 */
typedef HRESULT(WINAPI *Sig_NtSetInformationProcess)(HANDLE ProcessHandle, UINT ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);


// ustheme.dll

/* for win SDKs that don't have this */
#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A
#endif
#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif
#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
#define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif

typedef BOOL(WINAPI *Sig_IsAppThemed)();
typedef HTHEME(WINAPI *Sig_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT(WINAPI *Sig_CloseThemeData)(HTHEME hTheme);
typedef HRESULT(WINAPI *Sig_DrawThemeBackground)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
typedef BOOL(WINAPI *Sig_IsThemeActive)(void);
typedef BOOL(WINAPI *Sig_IsThemeBackgroundPartiallyTransparent)(HTHEME hTheme, int iPartId, int iStateId);
typedef HRESULT(WINAPI *Sig_GetThemeColor)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

// normaliz.dll
typedef int(WINAPI *Sig_NormalizeString)(int, LPCWSTR, int, LPWSTR, int);

#define KERNEL32_API_LIST(V) \
    V(SetProcessDEPPolicy) \
    V(IsWow64Process) \
    V(SetDllDirectoryW)

#define NTDLL_API_LIST(V) \
    V(NtSetInformationProcess)

#define UXTHEME_API_LIST(V) \
    V(IsAppThemed) \
    V(OpenThemeData) \
    V(CloseThemeData) \
    V(DrawThemeBackground) \
    V(IsThemeActive) \
    V(IsThemeBackgroundPartiallyTransparent) \
    V(GetThemeColor)

#define NORMALIZ_API_LIST(V) \
    V(NormalizeString)

#define API_DECLARATION(name) \
extern Sig_##name Dyn##name;

KERNEL32_API_LIST(API_DECLARATION)
NTDLL_API_LIST(API_DECLARATION)
UXTHEME_API_LIST(API_DECLARATION)
NORMALIZ_API_LIST(API_DECLARATION)

#undef API_DECLARATION

HMODULE SafeLoadLibrary(const WCHAR *dllName);
void InitDynCalls();

// convenience wrappers
namespace dyn {

bool IsAppThemed();
HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
HRESULT CloseThemeData(HTHEME hTheme);
HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
BOOL IsThemeActive();
BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId);
HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

};

