/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/WinUtil.h"

#define API_DECLARATION(name) Sig_##name Dyn##name = nullptr;

KERNEL32_API_LIST(API_DECLARATION)
NTDLL_API_LIST(API_DECLARATION)
UXTHEME_API_LIST(API_DECLARATION)
NORMALIZ_API_LIST(API_DECLARATION)
USER32_API_LIST(API_DECLARATION)
DWMAPI_API_LIST(API_DECLARATION)
UIA_API_LIST(API_DECLARATION)
DBGHELP_API_LIST(API_DECLARATION)

#undef API_DECLARATION

#define API_LOAD(name) Dyn##name = (Sig_##name)GetProcAddress(h, #name);

// Loads a DLL explicitly from the system's library collection
HMODULE SafeLoadLibrary(const WCHAR* dllName) {
    WCHAR dllPath[MAX_PATH];
    UINT res = GetSystemDirectoryW(dllPath, dimof(dllPath));
    if (!res || res >= dimof(dllPath)) {
        return nullptr;
    }
    BOOL ok = PathAppendW(dllPath, dllName);
    if (!ok) {
        return nullptr;
    }
    return LoadLibraryW(dllPath);
}

void InitDynCalls() {
    HMODULE h = SafeLoadLibrary(L"kernel32.dll");
    CrashAlwaysIf(!h);
    KERNEL32_API_LIST(API_LOAD);

    h = SafeLoadLibrary(L"ntdll.dll");
    CrashAlwaysIf(!h);
    NTDLL_API_LIST(API_LOAD);

    h = SafeLoadLibrary(L"user32.dll");
    CrashAlwaysIf(!h);
    USER32_API_LIST(API_LOAD);

    h = SafeLoadLibrary(L"uxtheme.dll");
    if (h) {
        UXTHEME_API_LIST(API_LOAD);
    }

    h = SafeLoadLibrary(L"dwmapi.dll");
    if (h) {
        DWMAPI_API_LIST(API_LOAD);
    }

    h = SafeLoadLibrary(L"normaliz.dll");
    if (h) {
        NORMALIZ_API_LIST(API_LOAD);
    }

    h = SafeLoadLibrary(L"uiautomationcore.dll");
    if (h) {
        UIA_API_LIST(API_LOAD)
    }

#if 0
    WCHAR *dbghelpPath = L"C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Team Tools\\Performance Tools\\dbghelp.dll";
    h = LoadLibrary(dbghelpPath);
#else
    h = SafeLoadLibrary(L"dbghelp.dll");
#endif
    if (h) {
        DBGHELP_API_LIST(API_LOAD)
    }
}

#undef API_LOAD

namespace touch {

bool SupportsGestures() {
    return DynGetGestureInfo && DynCloseGestureInfoHandle;
}

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo) {
    if (!DynGetGestureInfo) {
        return FALSE;
    }
    return DynGetGestureInfo(hGestureInfo, pGestureInfo);
}

BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo) {
    if (!DynCloseGestureInfoHandle) {
        return FALSE;
    }
    return DynCloseGestureInfoHandle(hGestureInfo);
}

BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize) {
    if (!DynSetGestureConfig) {
        return FALSE;
    }
    return DynSetGestureConfig(hwnd, dwReserved, cIDs, pGestureConfig, cbSize);
}
} // namespace touch

namespace theme {

bool IsAppThemed() {
    if (DynIsAppThemed && DynIsAppThemed()) {
        return true;
    }
    return false;
}

HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList) {
    if (DynOpenThemeData) {
        return DynOpenThemeData(hwnd, pszClassList);
    }
    return nullptr;
}

HRESULT CloseThemeData(HTHEME hTheme) {
    if (DynCloseThemeData) {
        return DynCloseThemeData(hTheme);
    }
    return E_NOTIMPL;
}

HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect) {
    if (DynDrawThemeBackground) {
        return DynDrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
    }
    return E_NOTIMPL;
}

BOOL IsThemeActive() {
    if (DynIsThemeActive) {
        return DynIsThemeActive();
    }
    return FALSE;
}

BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId) {
    if (DynIsThemeBackgroundPartiallyTransparent) {
        return DynIsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId);
    }
    return FALSE;
}

HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF* pColor) {
    if (DynGetThemeColor) {
        return DynGetThemeColor(hTheme, iPartId, iStateId, iPropId, pColor);
    }
    return E_NOTIMPL;
}
}; // namespace theme

namespace dwm {

BOOL IsCompositionEnabled() {
    if (!DynDwmIsCompositionEnabled) {
        return FALSE;
    }
    BOOL isEnabled;
    if (SUCCEEDED(DynDwmIsCompositionEnabled(&isEnabled))) {
        return isEnabled;
    }
    return FALSE;
}

HRESULT ExtendFrameIntoClientArea(HWND hwnd, const MARGINS* pMarInset) {
    if (!DynDwmExtendFrameIntoClientArea) {
        return E_NOTIMPL;
    }
    return DynDwmExtendFrameIntoClientArea(hwnd, pMarInset);
}

BOOL DefWindowProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) {
    if (!DynDwmDefWindowProc) {
        return FALSE;
    }
    return DynDwmDefWindowProc(hwnd, msg, wParam, lParam, plResult);
}

HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute) {
    if (!DynDwmGetWindowAttribute) {
        return E_NOTIMPL;
    }
    return DynDwmGetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
}
}; // namespace dwm

namespace uia {

LRESULT ReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple* provider) {
    if (!DynUiaReturnRawElementProvider) {
        return 0;
    }
    return DynUiaReturnRawElementProvider(hwnd, wParam, lParam, provider);
}

HRESULT HostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple** pProvider) {
    if (!DynUiaHostProviderFromHwnd) {
        return E_NOTIMPL;
    }
    return DynUiaHostProviderFromHwnd(hwnd, pProvider);
}

HRESULT RaiseAutomationEvent(IRawElementProviderSimple* pProvider, EVENTID id) {
    if (!DynUiaRaiseAutomationEvent) {
        return E_NOTIMPL;
    }
    return DynUiaRaiseAutomationEvent(pProvider, id);
}

HRESULT RaiseStructureChangedEvent(IRawElementProviderSimple* pProvider, StructureChangeType structureChangeType,
                                   int* pRuntimeId, int cRuntimeIdLen) {
    if (!DynUiaRaiseStructureChangedEvent) {
        return E_NOTIMPL;
    }
    return DynUiaRaiseStructureChangedEvent(pProvider, structureChangeType, pRuntimeId, cRuntimeIdLen);
}

HRESULT GetReservedNotSupportedValue(IUnknown** punkNotSupportedValue) {
    if (!DynUiaRaiseStructureChangedEvent) {
        return E_NOTIMPL;
    }
    return DynUiaGetReservedNotSupportedValue(punkNotSupportedValue);
}
}; // namespace uia

static const WCHAR* dllsToPreload =
    L"gdiplus.dll\0msimg32.dll\0shlwapi.dll\0urlmon.dll\0version.dll\0windowscodecs.dll\0wininet.dll\0\0";

// try to mitigate dll hijacking by pre-loading all the dlls that we delay load or might
// be loaded indirectly
void NoDllHijacking() {
    const WCHAR* dll = dllsToPreload;
    while (*dll) {
        SafeLoadLibrary(dll);
        seqstrings::SkipStr(dll);
    }
}

// As of the Windows 10.0.17134.0 SDK, this struct and ProcessImageLoadPolicy are defined in winnt.h.
#if !defined(NTDDI_WIN10_RS4)
#pragma warning(push)
//  C4201: nonstandard extension used: nameless struct/union
#pragma warning(disable : 4201)
// https://msdn.microsoft.com/en-us/library/windows/desktop/mt706245(v=vs.85).aspx
typedef struct _PROCESS_MITIGATION_IMAGE_LOAD_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD NoRemoteImages : 1;
            DWORD NoLowMandatoryLabelImages : 1;
            DWORD PreferSystem32Images : 1;
            DWORD ReservedFlags : 29;
        };
    };
} PROCESS_MITIGATION_IMAGE_LOAD_POLICY, *PPROCESS_MITIGATION_IMAGE_LOAD_POLICY;
#pragma warning(pop)

// https://msdn.microsoft.com/en-us/library/windows/desktop/hh871470(v=vs.85).aspx
constexpr int ProcessImageLoadPolicy = 10;
#endif

// https://github.com/videolan/vlc/blob/8663561d3f71595ebf116f17279a495b67cac713/bin/winvlc.c#L84
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh769088(v=vs.85).aspx
// Note: dlls we explicitly link to (like version.dll) get loaded before main is called
// so this only works for explicit LoadLibrary calls or delay loaded libraries
void PrioritizeSystemDirectoriesForDllLoad() {
    if (!DynSetProcessMitigationPolicy) {
        return;
    }
    // Only supported since Win 10
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY m = {0};
    m.PreferSystem32Images = 1;
    DynSetProcessMitigationPolicy(ProcessImageLoadPolicy, &m, sizeof(m));
    DbgOutLastError();
}
