/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define CANVAS_CLASS_NAME       L"SUMATRA_PDF_CANVAS"
#define FRAME_CLASS_NAME        L"SUMATRA_PDF_FRAME"
#define SUMATRA_WINDOW_TITLE    L"SumatraPDF"

#define WEBSITE_MAIN_URL         L"http://www.sumatrapdfreader.org/"
#define WEBSITE_MANUAL_URL       L"http://www.sumatrapdfreader.org/manual.html"
#define WEBSITE_TRANSLATIONS_URL L"https://github.com/sumatrapdfreader/sumatrapdf/wiki/Contribute-translation"

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL         L"http://www.sumatrapdfreader.org/develop.html"
#endif

// Background color comparison:
// Adobe Reader X   0x565656 without any frame border
// Foxit Reader 5   0x9C9C9C with a pronounced frame shadow
// PDF-XChange      0xACA899 with a 1px frame and a gradient shadow
// Google Chrome    0xCCCCCC with a symmetric gradient shadow
// Evince           0xD7D1CB with a pronounced frame shadow
#ifdef DRAW_PAGE_SHADOWS
// SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow
#define COL_WINDOW_BG           RGB(0xCC, 0xCC, 0xCC)
#define COL_PAGE_FRAME          RGB(0x88, 0x88, 0x88)
#define COL_PAGE_SHADOW         RGB(0x40, 0x40, 0x40)
#else
// SumatraPDF       0x999999 without any frame border
#define COL_WINDOW_BG           RGB(0x99, 0x99, 0x99)
#endif

// scrolls half a page down/up (needed for Shift+Up/Down)
#define SB_HPAGEUP   (WM_USER + 1)
#define SB_HPAGEDOWN (WM_USER + 2)

#define HIDE_CURSOR_TIMER_ID        3
#define HIDE_CURSOR_DELAY_IN_MS     3000

#define REPAINT_TIMER_ID            1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define AUTO_RELOAD_TIMER_ID        5
#define AUTO_RELOAD_DELAY_IN_MS     100

#define EBOOK_LAYOUT_TIMER_ID       7

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
class NotificationWnd;
class RenderCache;
class TabInfo;
struct LabelWithCloseWnd;

// all defined in SumatraPDF.cpp
extern bool                     gDebugShowLinks;
extern bool                     gShowFrameRate;
extern bool                     gUseGdiRenderer;
extern int                      gPolicyRestrictions;
extern WStrVec                  gAllowedLinkProtocols;
extern WStrVec                  gAllowedFileTypes;

extern WCHAR *                  gPluginURL;
extern Vec<WindowInfo*>         gWindows;
extern Favorites                gFavorites;
extern FileHistory              gFileHistory;
extern WNDPROC                  DefWndProcCloseButton;
extern RenderCache              gRenderCache;
extern bool                     gShowFrameRate;
extern bool                     gSuppressAltKey;
extern HBITMAP                  gBitmapReloadingCue;
extern HCURSOR                  gCursorDrag;
extern bool                     gCrashOnOpen;

#define gPluginMode             (gPluginURL != NULL)

bool  HasPermission(int permission);
bool  IsUIRightToLeft();
bool  LaunchBrowser(const WCHAR *url);
bool  OpenFileExternally(const WCHAR *path);
void  AssociateExeWithPdfExtension();
void  CloseTab(WindowInfo *win, bool quitIfLast=false);
bool  MayCloseWindow(WindowInfo *win);
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
void  UpdateTabFileDisplayStateForWin(WindowInfo *win, TabInfo *td);
bool  FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lparam, bool inTextfield=false);
void  SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode, bool keepContinuous=false);
void  ReloadDocument(WindowInfo *win, bool autorefresh=false);
bool  CanSendAsEmailAttachment(WindowInfo *win=NULL);
void  OnMenuViewFullscreen(WindowInfo* win, bool presentation=false);

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
void UpdateCursorPositionHelper(WindowInfo *win, PointI pos, NotificationWnd *wnd);
bool DocumentPathExists(const WCHAR *path);
void EnterFullScreen(WindowInfo* win, bool presentation=false);
void ExitFullScreen(WindowInfo* win);
void SetCurrentLang(const char *langCode);
void UpdateUITextForLanguage();
void GetFixedPageUiColors(COLORREF& text, COLORREF& bg);
void RebuildMenuBarForWindow(WindowInfo *win);
void UpdateCheckAsync(WindowInfo *win, bool autoCheck);
void DeleteWindowInfo(WindowInfo *win);
