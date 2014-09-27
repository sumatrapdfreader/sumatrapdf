/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraPDF_h
#define SumatraPDF_h

#include "DisplayState.h"

#define FRAME_CLASS_NAME        L"SUMATRA_PDF_FRAME"
#define SUMATRA_WINDOW_TITLE    L"SumatraPDF"

#define WEBSITE_MAIN_URL         L"http://blog.kowalczyk.info/software/sumatrapdf/"
#define WEBSITE_MANUAL_URL       L"http://blog.kowalczyk.info/software/sumatrapdf/manual.html"
#define WEBSITE_TRANSLATIONS_URL L"http://code.google.com/p/sumatrapdf/wiki/HelpTranslateSumatra"

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL         L"http://blog.kowalczyk.info/software/sumatrapdf/develop.html"
#endif

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
    // enables fullscreen and presentation view modes
    Perm_FullscreenAccess   = 1 << 6,
    // enables all of the above
    Perm_All                = 0x0FFFFFF,
    // set through the command line (Policies might only apply when in restricted use mode)
    Perm_RestrictedUse      = 0x1000000,
};

enum MenuToolbarFlags {
    MF_NO_TRANSLATE      = 1 << 0,
    MF_PLUGIN_MODE_ONLY  = 1 << 1,
    MF_NOT_FOR_CHM       = 1 << 2,
    MF_NOT_FOR_EBOOK_UI  = 1 << 3,
    MF_CBX_ONLY          = 1 << 4,
#define PERM_FLAG_OFFSET 5
    MF_REQ_INET_ACCESS   = Perm_InternetAccess << PERM_FLAG_OFFSET,
    MF_REQ_DISK_ACCESS   = Perm_DiskAccess << PERM_FLAG_OFFSET,
    MF_REQ_PREF_ACCESS   = Perm_SavePreferences << PERM_FLAG_OFFSET,
    MF_REQ_PRINTER_ACCESS= Perm_PrinterAccess << PERM_FLAG_OFFSET,
    MF_REQ_ALLOW_COPY    = Perm_CopySelection << PERM_FLAG_OFFSET,
    MF_REQ_FULLSCREEN    = Perm_FullscreenAccess << PERM_FLAG_OFFSET,
};

/* styling for About/Properties windows */

#define LEFT_TXT_FONT           L"Arial"
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          L"Arial Black"
#define RIGHT_TXT_FONT_SIZE     12
// for backward compatibility use a value that older versions will render as yellow
#define ABOUT_BG_COLOR_DEFAULT  (RGB(0xff, 0xf2, 0) - 0x80000000)

class Favorites;
class FileHistory;
class WindowInfo;
struct LabelWithCloseWnd;
struct TabData;

// all defined in SumatraPDF.cpp
extern bool                     gDebugShowLinks;
extern bool                     gShowFrameRate;
extern bool                     gUseGdiRenderer;
extern WCHAR *                  gPluginURL;
extern Vec<WindowInfo*>         gWindows;
extern Favorites                gFavorites;
extern FileHistory              gFileHistory;
extern WNDPROC                  DefWndProcCloseButton;

#define gPluginMode             (gPluginURL != NULL)

bool  HasPermission(int permission);
bool  IsUIRightToLeft();
bool  LaunchBrowser(const WCHAR *url);
bool  OpenFileExternally(const WCHAR *path);
void  AssociateExeWithPdfExtension();
void  CloseTab(WindowInfo *win, bool quitIfLast=false);
void  CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose=false);
void  SetSidebarVisibility(WindowInfo *win, bool tocVisible, bool favVisible);
void  RememberFavTreeExpansionState(WindowInfo *win);
void  LayoutTreeContainer(LabelWithCloseWnd *l, HWND hwndTree);
void  AdvanceFocus(WindowInfo* win);
bool  WindowInfoStillValid(WindowInfo *win);
void  SetCurrentLanguageAndRefreshUi(const char *langCode);
void  ShowOrHideToolbarGlobally();
void  ShowOrHideToolbarForWindow(WindowInfo *win);
void  UpdateDocumentColors();
void  UpdateCurrentFileDisplayStateForWin(WindowInfo *win);
void  UpdateTabFileDisplayStateForWin(WindowInfo *win, TabData *td);
bool  FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lparam, bool inTextfield=false);
void  SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode, bool keepContinuous=false);
void  ReloadDocument(WindowInfo *win, bool autorefresh=false);
bool  CanSendAsEmailAttachment(WindowInfo *win=NULL);

COLORREF GetLogoBgColor();
COLORREF GetAboutBgColor();
COLORREF GetNoDocBgColor();

WindowInfo* FindWindowInfoByHwnd(HWND hwnd);
// note: background tabs are only searched if focusTab is true
WindowInfo* FindWindowInfoByFile(const WCHAR *file, bool focusTab);
WindowInfo* FindWindowInfoBySyncFile(const WCHAR *file, bool focusTab);

// LoadDocument carries a lot of state, this holds them in
// one place
struct LoadArgs
{
    explicit LoadArgs(const WCHAR *fileName, WindowInfo *win=NULL) :
        fileName(fileName), win(win), showWin(true), forceReuse(false),
        isNewWindow(false), allowFailure(true), placeWindow(true) { }

    const WCHAR *fileName;
    WindowInfo *win;
    bool showWin;
    bool forceReuse;
    // for internal use
    bool isNewWindow;
    bool allowFailure;
    bool placeWindow;
};

WindowInfo* LoadDocument(LoadArgs& args);
WindowInfo *CreateAndShowWindowInfo();

UINT MbRtlReadingMaybe();
void MessageBoxWarning(HWND hwnd, const WCHAR *msg, const WCHAR *title = NULL);

#endif
