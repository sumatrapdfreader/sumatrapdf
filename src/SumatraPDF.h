/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraPDF_h
#define SumatraPDF_h

#include "BaseUtil.h"
#include "AppPrefs.h"
#include "TextSearch.h"

#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

// all defined in SumatraPDF.cpp
extern HINSTANCE                ghinst;
extern SerializableGlobalPrefs  gGlobalPrefs;
extern HCURSOR                  gCursorHand;
extern HBRUSH                   gBrushNoDocBg;
extern HBRUSH                   gBrushAboutBg;
extern bool                     gRestrictedUse;
extern bool                     gPluginMode;

class WindowInfo;

void LaunchBrowser(const TCHAR *url);
void AssociateExeWithPdfExtension();
void FindTextOnThread(WindowInfo* win, TextSearchDirection direction=FIND_FORWARD);
void DeleteWindowInfo(WindowInfo *win);

#endif
