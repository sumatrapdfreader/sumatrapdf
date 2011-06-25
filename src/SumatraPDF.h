/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraPDF_h
#define SumatraPDF_h

#include "BaseUtil.h"
#include "AppPrefs.h"
#include "TextSearch.h"

#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")

// permissions that can be revoked (or explicitly set) through Group Policies
enum {
    // enables Update checks, crash report submitting and hyperlinks
    Perm_InternetAccess     = 1 << 0,
    // enables opening and saving documents and launching external viewers
    Perm_DiskAccess         = 1 << 1,
    // enables persistence of preferences to disk (includes the Frequently Read page and Favorites)
    Perm_SavePreferences    = 1 << 2,
    // enables setting as default viewer
    Perm_RegistryAccess     = 1 << 3,
    // enables printing
    Perm_PrinterAccess      = 1 << 4,
    // enables image/text selections and selection copying (if permitted by the document)
    Perm_CopySelection      = 1 << 5,
    // enables all of the above
    Perm_All                = 0x0FFFFFF,
    // set through the command line (Policies might only apply when in restricted use mode)
    Perm_RestrictedUse      = 0x1000000,
};

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
extern bool                     gPluginMode;
extern TCHAR *                  gPluginURL;

class WindowInfo;

bool HasPermission(int permission);
bool LaunchBrowser(const TCHAR *url);
void AssociateExeWithPdfExtension();
void FindTextOnThread(WindowInfo* win, TextSearchDirection direction=FIND_FORWARD);
void CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose=false);

#endif
