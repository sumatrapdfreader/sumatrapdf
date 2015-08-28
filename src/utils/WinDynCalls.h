/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetProcessDEPPolicy(), we define
a bool flag HasSetProcessDEPPolicy and function pointer DynSetProcessDEPPolicy()
(with a signature matching SetProcessDEPPolicy()).

The intent is to simplify by (potentially) loading all functions at once instead
of scattering those calls 
*/

#include <dwmapi.h>
#include <vssym32.h>

// kernel32.dll
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x2
#endif
typedef BOOL(WINAPI *Sig_SetProcessDEPPolicy)(DWORD dwFlags);

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

#define KERNEL32_APIS_LIST(V) \
    V(SetProcessDEPPolicy)

#define UXTHEME_APIS_LIST(V) \
    V(IsAppThemed) \
    V(OpenThemeData) \
    V(CloseThemeData) \
    V(DrawThemeBackground) \
    V(IsThemeActive) \
    V(IsThemeBackgroundPartiallyTransparent) \
    V(GetThemeColor)

#define API_DECLARATION(name) \
extern bool  Has##name; \
extern Sig_##name Dyn##name;

KERNEL32_APIS_LIST(API_DECLARATION)
UXTHEME_APIS_LIST(API_DECLARATION)

#undef API_DECLARATION

HMODULE SafeLoadLibrary(const WCHAR *dllName);
void InitDynCalls();

// convenience wrappers
namespace vss {
    HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
    HRESULT CloseThemeData(HTHEME hTheme);
    HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
    BOOL IsThemeActive();
    BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId);
    HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);
};

