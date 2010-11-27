/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRAPDF_H_
#define SUMATRAPDF_H_

/* TODO: those should be set from the makefile */
// Modify the following defines if you have to target a platform prior to the ones specified below.
// Their meaning: http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx
// and http://blogs.msdn.com/oldnewthing/archive/2007/04/11/2079137.aspx
// We set the features uniformly to Win 2000 or later.
#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT 
#define _WIN32_WINNT 0x0500
// the following is only defined for _WIN32_WINNT >= 0x0600
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0500
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#ifdef DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <direct.h>

#include "win_util.h"

#include "Resource.h"

#define APP_NAME_STR            _T("SumatraPDF")

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

#define COL_BLUE_LINK           RGB(0,0x20,0xa0)

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize);
void Win32_Font_Delete(HFONT font);

extern HCURSOR gCursorHand;
extern bool gRestrictedUse;
extern HINSTANCE ghinst;

#endif
