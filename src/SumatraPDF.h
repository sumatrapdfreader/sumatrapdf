/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraPDF_h
#define SumatraPDF_h

#include "BaseUtil.h"
#include "Vec.h"
#include "AppPrefs.h"

#define APP_NAME_STR            _T("SumatraPDF")
#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

#define COL_BLUE_LINK           RGB(0,0x20,0xa0)

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO (612.0/792.0)

#define SUMATRA_WINDOW_TITLE    _T("SumatraPDF")
#define CANVAS_CLASS_NAME       _T("SUMATRA_PDF_CANVAS")

#define DEFAULT_ROTATION        0

class WindowInfo;
class DisplayState;
class FileHistory;

// all defined in SumatraPDF.cpp
extern HINSTANCE                ghinst;
extern Vec<WindowInfo*>         gWindows;
extern HBRUSH                   gBrushBg;
extern SerializableGlobalPrefs  gGlobalPrefs;
extern FileHistory              gFileHistory;
extern HCURSOR                  gCursorHand;
extern bool                     gRestrictedUse;


HMENU BuildMenu(HWND hWnd);
void CreateToolbar(WindowInfo *win, HINSTANCE hInst);
void UpdateToolbarAndScrollbarsForAllWindows();
void WindowInfo_EnterFullscreen(WindowInfo *win, bool presentation=false);
void CheckPositionAndSize(DisplayState *ds);
void UpdateToolbarPageText(WindowInfo *win, int pageCount);

#endif
