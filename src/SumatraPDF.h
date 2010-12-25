/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRAPDF_H_
#define SUMATRAPDF_H_

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
#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")

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
