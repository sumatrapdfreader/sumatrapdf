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

struct MenuDef {
    const char *title;
    int         id;
    int         flags;
};

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

class WindowInfo;
class Favorites;

// all defined in SumatraPDF.cpp
extern HINSTANCE                ghinst;
extern SerializableGlobalPrefs  gGlobalPrefs;
extern HCURSOR                  gCursorHand;
extern HBRUSH                   gBrushNoDocBg;
extern HBRUSH                   gBrushAboutBg;
extern bool                     gPluginMode;
extern TCHAR *                  gPluginURL;
extern Vec<WindowInfo*>         gWindows;
extern Favorites *              gFavorites;
extern FileHistory              gFileHistory;
extern HFONT                    gDefaultGuiFont;
extern WNDPROC                  DefWndProcCloseButton;
extern MenuDef                  menuDefFavorites[];

LRESULT CALLBACK WndProcCloseButton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool  HasPermission(int permission);
bool  IsUIRightToLeft();
bool  LaunchBrowser(const TCHAR *url);
void  AssociateExeWithPdfExtension();
void  FindTextOnThread(WindowInfo* win, TextSearchDirection direction=FIND_FORWARD);
void  CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose=false);
void  SetSidebarVisibility(WindowInfo *win, bool tocVisible, bool favVisible);
void  RememberFavTreeExpansionState(WindowInfo *win);
void  LayoutTreeContainer(HWND hwndContainer, int id);
void  DrawCloseButton(DRAWITEMSTRUCT *dis);
void  AdvanceFocus(WindowInfo* win);
bool  WindowInfoStillValid(WindowInfo *win);
bool  SavePrefs();
HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu);
#endif
