/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/WinDynCalls.h"

#define API_DECLARATION(name) Sig_##name Dyn##name = nullptr;

KERNEL32_API_LIST(API_DECLARATION)
NTDLL_API_LIST(API_DECLARATION)
UXTHEME_API_LIST(API_DECLARATION)
NORMALIZ_API_LIST(API_DECLARATION)
USER32_API_LIST(API_DECLARATION)
DWMAPI_API_LIST(API_DECLARATION)
DBGHELP_API_LIST(API_DECLARATION)

#undef API_DECLARATION

#define API_LOAD(name) Dyn##name = (Sig_##name)GetProcAddress(h, #name);

// Loads a DLL explicitly from the system's library collection
static HMODULE SafeLoadLibrary(const char* dllNameA) {
    WCHAR dllPath[MAX_PATH];
    uint res = GetSystemDirectoryW(dllPath, dimof(dllPath));
    if (!res || res >= dimof(dllPath)) {
        return nullptr;
    }
    auto dllName = ToWStrTemp(dllNameA);
    BOOL ok = PathAppendW(dllPath, dllName);
    if (!ok) {
        return nullptr;
    }
    return LoadLibraryW(dllPath);
}

void InitDynCalls() {
    HMODULE h = SafeLoadLibrary("kernel32.dll");
    CrashAlwaysIf(!h);
    KERNEL32_API_LIST(API_LOAD);

    h = SafeLoadLibrary("ntdll.dll");
    CrashAlwaysIf(!h);
    NTDLL_API_LIST(API_LOAD);

    h = SafeLoadLibrary("user32.dll");
    CrashAlwaysIf(!h);
    USER32_API_LIST(API_LOAD);

    h = SafeLoadLibrary("uxtheme.dll");
    if (h) {
        UXTHEME_API_LIST(API_LOAD);
    }

    h = SafeLoadLibrary("dwmapi.dll");
    if (h) {
        DWMAPI_API_LIST(API_LOAD);
    }

    h = SafeLoadLibrary("normaliz.dll");
    if (h) {
        NORMALIZ_API_LIST(API_LOAD);
    }

#if 0
    WCHAR *dbghelpPath = L"C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Team Tools\\Performance Tools\\dbghelp.dll";
    h = LoadLibrary(dbghelpPath);
#else
    h = SafeLoadLibrary("dbghelp.dll");
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
    return DynIsAppThemed && DynIsAppThemed();
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

BOOL DefaultWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT* plResult) {
    if (!DynDwmDefWindowProc) {
        return FALSE;
    }
    return DynDwmDefWindowProc(hwnd, msg, wp, lp, plResult);
}

HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute) {
    if (!DynDwmGetWindowAttribute) {
        return E_NOTIMPL;
    }
    return DynDwmGetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

HRESULT SetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute) {
    if (!DynDwmSetWindowAttribute) {
        return E_NOTIMPL;
    }
    return DynDwmSetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

// https://stackoverflow.com/questions/39261826/change-the-color-of-the-title-bar-caption-of-a-win32-application
HRESULT SetCaptionColor(HWND hwnd, COLORREF col) {
    return SetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR, &col, sizeof(col));
}

}; // namespace dwm

static const char* dllsToPreload =
    "gdiplus.dll\0msimg32.dll\0shlwapi.dll\0urlmon.dll\0version.dll\0windowscodecs.dll\0wininet.dll\0";

// try to mitigate dll hijacking by pre-loading all the dlls that we delay load or might
// be loaded indirectly
void NoDllHijacking() {
    const char* dll = dllsToPreload;
    while (dll) {
        SafeLoadLibrary(dll);
        seqstrings::Next(dll);
    }
}

// https://github.com/videolan/vlc/blob/8663561d3f71595ebf116f17279a495b67cac713/bin/winvlc.c#L84
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh769088(v=vs.85).aspx
// Note: dlls we explicitly link to (like version.dll) get loaded before main is called
// so this only works for explicit LoadLibrary calls or delay loaded libraries
void PrioritizeSystemDirectoriesForDllLoad() {
    if (!DynSetProcessMitigationPolicy) {
        return;
    }
    // Only supported since Win 10
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY m{};
    m.PreferSystem32Images = 1;
    DynSetProcessMitigationPolicy(ProcessImageLoadPolicy, &m, sizeof(m));
    DbgOutLastError();
}
