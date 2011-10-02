/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>

#include "Touch.h"
#include "WinUtil.h"
#include <assert.h>

namespace Touch {

typedef BOOL (WINAPI * GetGestureInfoPtr)(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
typedef BOOL (WINAPI * CloseGestureInfoHandlePtr)(HGESTUREINFO hGestureInfo);
typedef BOOL (WINAPI * SetGestureConfigPtr)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

static GetGestureInfoPtr         g_pGetGestureInfo;
static CloseGestureInfoHandlePtr g_pCloseGestureInfoHandle;
static SetGestureConfigPtr       g_pSetGestureConfig;

static HMODULE g_hLib = NULL;

void InitializeGestures()
{
    assert(NULL == g_pGetGestureInfo && NULL == g_hLib); // don't call twice

    if (g_pGetGestureInfo || g_hLib) 
        return;

    g_hLib = SafeLoadLibrary(_T("user32.dll"));
    if (!g_hLib)
        return;

    g_pGetGestureInfo = (GetGestureInfoPtr)GetProcAddress(g_hLib, "GetGestureInfo");
    g_pCloseGestureInfoHandle = (CloseGestureInfoHandlePtr)GetProcAddress(g_hLib, "CloseGestureInfoHandle");
    g_pSetGestureConfig = (SetGestureConfigPtr)GetProcAddress(g_hLib, "SetGestureConfig");
}

bool SupportsGestures()
{
    return g_pGetGestureInfo != NULL;
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

