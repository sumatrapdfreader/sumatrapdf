/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinUtil.h"
#include "Touch.h"

namespace Touch {

typedef BOOL (WINAPI * GetGestureInfoPtr)(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
typedef BOOL (WINAPI * CloseGestureInfoHandlePtr)(HGESTUREINFO hGestureInfo);
typedef BOOL (WINAPI * SetGestureConfigPtr)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

static GetGestureInfoPtr         g_pGetGestureInfo = NULL;
static CloseGestureInfoHandlePtr g_pCloseGestureInfoHandle = NULL;
static SetGestureConfigPtr       g_pSetGestureConfig = NULL;

static void InitializeGestures()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    HMODULE hLib = SafeLoadLibrary(L"user32.dll");
    if (!hLib)
        return;

#define Load(func) g_p ## func = (func ## Ptr)GetProcAddress(hLib, #func)
    Load(GetGestureInfo);
    Load(CloseGestureInfoHandle);
    Load(SetGestureConfig);
#undef Load
}

bool SupportsGestures()
{
    InitializeGestures();
    return g_pGetGestureInfo && g_pCloseGestureInfoHandle;
}

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo)
{
    if (!g_pGetGestureInfo)
        return FALSE;
    return g_pGetGestureInfo(hGestureInfo, pGestureInfo);
}

BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo)
{
    if (!g_pCloseGestureInfoHandle)
        return FALSE;
    return g_pCloseGestureInfoHandle(hGestureInfo);
}

BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize)
{
    if (!g_pSetGestureConfig)
        return FALSE;
    return g_pSetGestureConfig(hwnd, dwReserved, cIDs, pGestureConfig, cbSize);
}

}
