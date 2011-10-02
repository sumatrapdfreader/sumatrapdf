/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>

#include "Touch.h"

// Static initializers
Touch::GetGestureInfoPtr Touch::g_pGetGestureInfo;
Touch::CloseGestureInfoHandlePtr Touch::g_pCloseGestureInfoHandle;
Touch::SetGestureConfigPtr Touch::g_pSetGestureConfig;

bool Touch::SupportsGestures()
{	
    OSVERSIONINFOEX ver;
    ZeroMemory(&ver, sizeof(ver));
    ver.dwOSVersionInfoSize = sizeof(ver);
    BOOL ok = GetVersionEx((OSVERSIONINFO*)&ver);
    if (!ok)
        return false;

	// For future versions of Windows
	if ( ver.dwMajorVersion > 6 )
		return true;

	// Greater than or equal to Windows 7 (0x0601)
	return ver.dwMajorVersion >= 6 && ver.dwMinorVersion >= 1;
}

static HMODULE s_hLib = NULL;

void Touch::InitializeGestures()
{	
	if (g_pGetGestureInfo || s_hLib) 
	{
		return;
	}

	s_hLib = ::LoadLibrary(L"user32.dll");

	// gesture interfaces
	if (s_hLib) 
	{
		g_pGetGestureInfo = (GetGestureInfoPtr)GetProcAddress(s_hLib, "GetGestureInfo");
		g_pCloseGestureInfoHandle = (CloseGestureInfoHandlePtr)GetProcAddress(s_hLib, "CloseGestureInfoHandle");
		g_pSetGestureConfig = (SetGestureConfigPtr)GetProcAddress(s_hLib, "SetGestureConfig");
	}
}