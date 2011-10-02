/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>

#include "Touch.h"

Touch::GetGestureInfoPtr Touch::g_pGetGestureInfo;
Touch::CloseGestureInfoHandlePtr Touch::g_pCloseGestureInfoHandle;
Touch::SetGestureConfigPtr Touch::g_pSetGestureConfig;

bool Touch::SupportsGestures()
{
    return g_pGetGestureInfo != NULL;
}

static HMODULE s_hLib = NULL;

void Touch::InitializeGestures()
{
    assert(NULL == g_pGetGestureInfo && NULL == s_hLib); // don't call twice

    if (g_pGetGestureInfo || s_hLib) 
        return;

    s_hLib = ::LoadLibrary(L"user32.dll");
    if (!s_hLib)
        return;

    g_pGetGestureInfo = (GetGestureInfoPtr)GetProcAddress(s_hLib, "GetGestureInfo");
    g_pCloseGestureInfoHandle = (CloseGestureInfoHandlePtr)GetProcAddress(s_hLib, "CloseGestureInfoHandle");
    g_pSetGestureConfig = (SetGestureConfigPtr)GetProcAddress(s_hLib, "SetGestureConfig");
}