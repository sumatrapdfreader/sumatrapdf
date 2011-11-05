/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraPDF_h
#define SumatraPDF_h

#include "BaseUtil.h"
#include "AppPrefs.h"
#include "TextSearch.h"

#define UWM_PREFS_FILE_UPDATED  (WM_USER + 1)

#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")
#define SUMATRA_WINDOW_TITLE    _T("SumatraPDF")

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

enum MenuToolbarFlags {
    MF_NO_TRANSLATE      = 1 << 0,
    MF_PLUGIN_MODE_ONLY  = 1 << 1,
    MF_NOT_FOR_CHM       = 1 << 2,
#define PERM_FLAG_OFFSET 3
    MF_REQ_INET_ACCESS   = Perm_InternetAccess << PERM_FLAG_OFFSET,
    MF_REQ_DISK_ACCESS   = Perm_DiskAccess << PERM_FLAG_OFFSET,
    MF_REQ_PREF_ACCESS   = Perm_SavePreferences << PERM_FLAG_OFFSET,
    MF_REQ_PRINTER_ACCESS= Perm_PrinterAccess << PERM_FLAG_OFFSET,
    MF_REQ_ALLOW_COPY    = Perm_CopySelection << PERM_FLAG_OFFSET,
};

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12
// for backward compatibility use a value that older versions will render as yellow
#define ABOUT_BG_COLOR_DEFAULT  (RGB(0xff, 0xf2, 0) - 0x80000000)

class WindowInfo;
class Favorites;

// all defined in SumatraPDF.cpp
extern HINSTANCE                ghinst;
extern bool                     gDebugShowLinks;
extern bool                     gUseGdiRenderer;
extern HCURSOR                  gCursorHand;
extern HCURSOR                  gCursorArrow;
extern HCURSOR                  gCursorIBeam;
extern HBRUSH                   gBrushNoDocBg;
extern HBRUSH                   gBrushAboutBg;
extern HFONT                    gDefaultGuiFont;
extern TCHAR *                  gPluginURL;
extern Vec<WindowInfo*>         gWindows;
extern Favorites *              gFavorites;
extern FileHistory              gFileHistory;
extern WNDPROC                  DefWndProcCloseButton;

#define gPluginMode             (gPluginURL != NULL)

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
void  ChangeLanguage(const char *langName);
void  ShowOrHideToolbarGlobally();
void  UpdateCurrentFileDisplayStateForWin(WindowInfo* win);
bool  FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lparam, bool inTextfield=false);
void  SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode, bool keepContinuous=false);
void  ReloadDocument(WindowInfo *win, bool autorefresh=false);
bool  CanSendAsEmailAttachment(WindowInfo *win=NULL);
bool  DoCachePageRendering(WindowInfo *win, int pageNo);

WindowInfo* FindWindowInfoByFile(const TCHAR *file);
WindowInfo* FindWindowInfoByHwnd(HWND hwnd);
WindowInfo* FindWindowInfoBySyncFile(const TCHAR *file);
WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win=NULL,
                         bool showWin=true, bool forceReuse=false, bool suppressPwdUI=false);

#endif
