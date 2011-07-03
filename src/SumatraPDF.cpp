/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include <shlobj.h>
#include <wininet.h>
#include <locale.h>
#include <time.h>

#include "WindowInfo.h"
#include "RenderCache.h"
#include "PdfSync.h"
#include "Resource.h"

#include "AppPrefs.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "SumatraAbout.h"
#include "FileHistory.h"
#include "FileWatch.h"
#include "AppTools.h"
#include "Notifications.h"

#include "WinUtil.h"
#include "Http.h"
#include "FileUtil.h"
#include "CrashHandler.h"
#include "ParseCommandLine.h"
#include "StressTesting.h"

#include "translations.h"
#include "Version.h"

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST           IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST            IDM_VIEW_CONTINUOUS
#define IDM_ZOOM_FIRST                  IDM_ZOOM_FIT_PAGE
#define IDM_ZOOM_LAST                   IDM_ZOOM_CUSTOM
// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       IDM_ZOOM_FIT_PAGE - IDM_ZOOM_CUSTOM must be in a continuous range!
CASSERT(IDM_VIEW_LAYOUT_LAST - IDM_VIEW_LAYOUT_FIRST == 3, view_layout_range);
CASSERT(IDM_ZOOM_LAST - IDM_ZOOM_FIRST == 17, zoom_range);

// Undefine any of these two, if you prefer MuPDF/Fitz to render the whole page
// (using FreeType for fonts) at the expense of higher memory/spooler requirements.
// #define USE_GDI_FOR_RENDERING
#define USE_GDI_FOR_PRINTING

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

/* Define THREAD_BASED_FILEWATCH to use the thread-based implementation of file change detection. */
#define THREAD_BASED_FILEWATCH

/* Define if you want to display an additional debug menu */
#ifdef DEBUG
#define SHOW_DEBUG_MENU_ITEMS
#endif

#define ZOOM_IN_FACTOR      1.2f
#define ZOOM_OUT_FACTOR     1.0f / ZOOM_IN_FACTOR

/* if TRUE, we're in debug mode where we show links as blue rectangle on
   the screen. Makes debugging code related to links easier. */
#ifdef DEBUG
static bool             gDebugShowLinks = true;
#else
static bool             gDebugShowLinks = false;
#endif

/* if true, we're rendering everything with the GDI+ back-end,
   otherwise Fitz is used at least for screen rendering.
   In Debug builds, you can switch between the two by hitting the '$' key */
#ifdef USE_GDI_FOR_RENDERING
static bool             gUseGdiRenderer = true;
#else
static bool             gUseGdiRenderer = false;
#endif

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
bool                    gPluginMode = false;
TCHAR *                 gPluginURL = NULL; // owned by CommandLineInfo in WinMain

/* default UI settings */

#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_ROTATION        0
#define DEFAULT_LANGUAGE        "en"

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO          (612.0/792.0)

#if defined(SVN_PRE_RELEASE_VER) && !defined(BLACK_ON_YELLOW)
#define ABOUT_BG_COLOR          RGB(255,0,0)
#else
#define ABOUT_BG_COLOR          RGB(255,242,0)
#endif
// for backward compatibility use a value that older versions will render as yellow
#define ABOUT_BG_COLOR_DEFAULT  (RGB(255,242,0) - 0x80000000)

#define COL_WINDOW_BG           RGB(0xcc, 0xcc, 0xcc)
#define COL_WINDOW_SHADOW       RGB(0x40, 0x40, 0x40)
#define COL_PAGE_FRAME          RGB(0x88, 0x88, 0x88)
#define COL_FWDSEARCH_BG        RGB(0x65, 0x81 ,0xff)
#define COL_SELECTION_RECT      RGB(0xF5, 0xFC, 0x0C)

#define SUMATRA_WINDOW_TITLE    _T("SumatraPDF")

#define CANVAS_CLASS_NAME       _T("SUMATRA_PDF_CANVAS")
#define SPLITER_CLASS_NAME      _T("Spliter")
#define PREFS_FILE_NAME         _T("sumatrapdfprefs.dat")
#define RESTRICTIONS_FILE_NAME  _T("sumatrapdfrestrict.ini")
#define CRASH_DUMP_FILE_NAME    _T("sumatrapdfcrash.dmp")

#define DEFAULT_LINK_PROTOCOLS  _T("http,https,mailto")

#define SPLITTER_DX  5
#define SIDEBAR_MIN_WIDTH 150

#define REPAINT_TIMER_ID            1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define SMOOTHSCROLL_TIMER_ID       2
#define SMOOTHSCROLL_DELAY_IN_MS    20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

#define HIDE_CURSOR_TIMER_ID        3
#define HIDE_CURSOR_DELAY_IN_MS     3000

#define HIDE_FWDSRCHMARK_TIMER_ID                4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS             400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS     100
#define HIDE_FWDSRCHMARK_STEPS                   5

#define AUTO_RELOAD_TIMER_ID        5
#define AUTO_RELOAD_DELAY_IN_MS     100

#if !defined(THREAD_BASED_FILEWATCH)
#define FILEWATCH_DELAY_IN_MS       1000
#endif

#define UWM_PREFS_FILE_UPDATED  (WM_USER + 1)

#define WS_REBAR (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | \
                  RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)

       HINSTANCE                    ghinst = NULL;

static HCURSOR                      gCursorArrow;
       HCURSOR                      gCursorHand;
static HCURSOR                      gCursorDrag;
static HCURSOR                      gCursorIBeam;
static HCURSOR                      gCursorScroll;
static HCURSOR                      gCursorSizeWE;
static HCURSOR                      gCursorNo;
       HBRUSH                       gBrushNoDocBg;
       HBRUSH                       gBrushAboutBg;
static HBRUSH                       gBrushWhite;
static HBRUSH                       gBrushBlack;
static HBRUSH                       gBrushShadow;
static HFONT                        gDefaultGuiFont;
static HBITMAP                      gBitmapReloadingCue;

static RenderCache                  gRenderCache;
static Vec<WindowInfo*>             gWindows;
static FileHistory                  gFileHistory;
static Favorites *                  gFavorites;
static UIThreadWorkItemQueue        gUIThreadMarshaller;

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static int                          gPolicyRestrictions = Perm_All;
// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm_DiskAccess)
static StrVec                       gAllowedLinkProtocols;

SerializableGlobalPrefs             gGlobalPrefs = {
    false, // bool globalPrefsOnly
    DEFAULT_LANGUAGE, // const char *currentLanguage
    true, // bool showToolbar
    false, // bool pdfAssociateDontAskAgain
    false, // bool pdfAssociateShouldAssociate
    true, // bool enableAutoUpdate
    true, // bool rememberOpenedFiles
    ABOUT_BG_COLOR_DEFAULT, // int bgColor
    false, // bool escToExit
    NULL, // TCHAR *inverseSearchCmdLine
    false, // bool enableTeXEnhancements
    NULL, // TCHAR *versionToSkip
    NULL, // char *lastUpdateTime
    DEFAULT_DISPLAY_MODE, // DisplayMode defaultDisplayMode
    DEFAULT_ZOOM, // float defaultZoom
    WIN_STATE_NORMAL, // int  windowState
    RectI(), // RectI windowPos
    true, // bool showToc
    0, // int  m_tocDx
    0, // int  fwdSearchOffset
    COL_FWDSEARCH_BG, // int  fwdSearchColor
    15, // int  fwdSearchWidth
    0, // bool fwdSearchPermanent
    true, // bool showStartPage
    0, // int openCountWeek
    { 0, 0 }, // FILETIME lastPrefUpdate
};

enum MenuToolbarFlags {
    MF_NO_TRANSLATE      = 1 << 0,
    MF_PLUGIN_MODE_ONLY  = 1 << 1,
#define PERM_FLAG_OFFSET 2
    MF_REQ_INET_ACCESS   = Perm_InternetAccess << PERM_FLAG_OFFSET,
    MF_REQ_DISK_ACCESS   = Perm_DiskAccess << PERM_FLAG_OFFSET,
    MF_REQ_PREF_ACCESS   = Perm_SavePreferences << PERM_FLAG_OFFSET,
    MF_REQ_PRINTER_ACCESS= Perm_PrinterAccess << PERM_FLAG_OFFSET,
    MF_REQ_ALLOW_COPY    = Perm_CopySelection << PERM_FLAG_OFFSET,
};

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    int           bmpIndex;
    int           cmdId;
    const char *  toolTip;
    int           flags;
};

static ToolbarButtonInfo gToolbarButtons[] = {
    { 0,   IDM_OPEN,              _TRN("Open"),           MF_REQ_DISK_ACCESS },
// the Open button is replaced with a Save As button in Plugin mode:
//  { 12,  IDM_SAVEAS,            _TRN("Save As"),        MF_REQ_DISK_ACCESS },
    { 1,   IDM_PRINT,             _TRN("Print"),          MF_REQ_PRINTER_ACCESS },
    { -1,  IDM_GOTO_PAGE,         NULL,                   0 },
    { 2,   IDM_GOTO_PREV_PAGE,    _TRN("Previous Page"),  0 },
    { 3,   IDM_GOTO_NEXT_PAGE,    _TRN("Next Page"),      0 },
    { -1,  0,                     NULL,                   0 },
    { 4,   IDT_VIEW_FIT_WIDTH,    _TRN("Fit Width and Show Pages Continuously"), 0 },
    { 5,   IDT_VIEW_FIT_PAGE,     _TRN("Fit a Single Page"), 0 },
    { 6,   IDT_VIEW_ZOOMOUT,      _TRN("Zoom Out"),       0 },
    { 7,   IDT_VIEW_ZOOMIN,       _TRN("Zoom In"),        0 },
    { -1,  IDM_FIND_FIRST,        NULL,                   0 },
    { 8,   IDM_FIND_PREV,         _TRN("Find Previous"),  0 },
    { 9,   IDM_FIND_NEXT,         _TRN("Find Next"),      0 },
    // TODO: is this button really used often enough?
    { 10,  IDM_FIND_MATCH,        _TRN("Match Case"),     0 },
};

#define TOOLBAR_BUTTONS_COUNT dimof(gToolbarButtons)

static void CreateToolbar(WindowInfo& win);
static void CreateSidebar(WindowInfo* win);
static void ToggleTocBox(WindowInfo *win);
static void ClearTocBox(WindowInfo *win);
static void ToggleFavorites(WindowInfo *win);
static void UpdateToolbarFindText(WindowInfo& win);
static void UpdateToolbarPageText(WindowInfo& win, int pageCount);
static void UpdateToolbarToolText();
static void UpdateToolbarAndScrollbarsForAllWindows();
static void RebuildMenuBar();
static void OnMenuFindMatchCase(WindowInfo& win);
static bool LoadDocIntoWindow(const TCHAR *fileName, WindowInfo& win, 
    const DisplayState *state, bool isNewWindow, bool tryRepair, 
    bool showWin, bool placeWindow);

static void DeleteOldSelectionInfo(WindowInfo& win, bool alsoTextSel=false);
static void ClearSearchResult(WindowInfo& win);
static void EnterFullscreen(WindowInfo& win, bool presentation=false);
static void ExitFullscreen(WindowInfo& win);

bool HasPermission(int permission)
{
    return (permission & gPolicyRestrictions) == permission;
}

static bool CurrLangNameSet(const char *langName)
{
    const char *langCode = Trans::ConfirmLanguage(langName);
    if (!langCode)
        return false;

    gGlobalPrefs.currentLanguage = langCode;

    bool ok = Trans::SetCurrentLanguage(langCode);
    assert(ok);
    return ok;
}

#ifndef SUMATRA_UPDATE_INFO_URL
#ifdef SVN_PRE_RELEASE_VER
#define SUMATRA_UPDATE_INFO_URL _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-prerelease-latest.txt")
#else
#define SUMATRA_UPDATE_INFO_URL _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-latest.txt")
#endif
#endif

#ifndef SVN_UPDATE_LINK
#ifdef SVN_PRE_RELEASE_VER
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf/prerelease.html")
#else
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf")
#endif
#endif

#define SECS_IN_DAY 60*60*24

// lets the shell open a URI for any supported scheme in
// the appropriate application (web browser, mail client, etc.)
bool LaunchBrowser(const TCHAR *url)
{
    if (!HasPermission(Perm_DiskAccess))
        return false;

    // check if this URL's protocol is allowed
    const TCHAR *colon = str::FindChar(url, ':');
    if (!colon)
        return false;
    ScopedMem<TCHAR> protocol(str::DupN(url, colon - url));
    str::ToLower(protocol);
    if (gAllowedLinkProtocols.Find(protocol) == -1)
        return false;

    LaunchFile(url, NULL, _T("open"));
    return true;
}

static bool HasFavorites()
{
    return gFavorites->Count() > 0;
}

static bool CanViewExternally(WindowInfo *win=NULL)
{
    if (!HasPermission(Perm_DiskAccess))
        return false;
    if (!win || win->IsAboutWindow())
        return true;
    return file::Exists(win->loadedFilePath);
}

static bool IsNonPdfDocument(WindowInfo *win=NULL)
{
    if (!win || !win->dm)
        return false;
    return win->dm->engineType != Engine_PDF;
}

static bool CanViewWithFoxit(WindowInfo *win=NULL)
{
    // Requirements: a valid filename and a valid path to Foxit
    if (!CanViewExternally(win) || IsNonPdfDocument(win))
        return false;
    ScopedMem<TCHAR> path(GetFoxitPath());
    return path != NULL;
}

static bool ViewWithFoxit(WindowInfo *win, TCHAR *args=NULL)
{
    if (!CanViewWithFoxit(win))
        return false;

    ScopedMem<TCHAR> exePath(GetFoxitPath());
    if (!exePath)
        return false;
    if (!args)
        args = _T("");

    // Foxit cmd-line format:
    // [PDF filename] [-n <page number>] [-pwd <password>] [-z <zoom>]
    // TODO: Foxit allows passing password and zoom
    ScopedMem<TCHAR> params(str::Format(_T("%s \"%s\" -n %d"), args, win->loadedFilePath, win->dm->currentPageNo()));
    LaunchFile(exePath, params);
    return true;
}

static bool CanViewWithPDFXChange(WindowInfo *win=NULL)
{
    // Requirements: a valid filename and a valid path to PDF X-Change
    if (!CanViewExternally(win) || IsNonPdfDocument(win))
        return false;
    ScopedMem<TCHAR> path(GetPDFXChangePath());
    return path != NULL;
}

static bool ViewWithPDFXChange(WindowInfo *win, TCHAR *args=NULL)
{
    if (!CanViewWithPDFXChange(win))
        return false;

    ScopedMem<TCHAR> exePath(GetPDFXChangePath());
    if (!exePath)
        return false;
    if (!args)
        args = _T("");

    // PDFXChange cmd-line format:
    // [/A "param=value [&param2=value ..."] [PDF filename] 
    // /A params: page=<page number>
    ScopedMem<TCHAR> params(str::Format(_T("%s /A \"page=%d\" \"%s\""), args, win->dm->currentPageNo(), win->loadedFilePath));
    LaunchFile(exePath, params);
    return true;
}

static bool CanViewWithAcrobat(WindowInfo *win=NULL)
{
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (!CanViewExternally(win) || IsNonPdfDocument(win))
        return false;
    ScopedMem<TCHAR> exePath(GetAcrobatPath());
    return exePath != NULL;
}

static bool ViewWithAcrobat(WindowInfo *win, TCHAR *args=NULL)
{
    if (!CanViewWithAcrobat(win))
        return false;

    ScopedMem<TCHAR> exePath(GetAcrobatPath());
    if (!exePath)
        return false;

    if (!args)
        args = _T("");

    ScopedMem<TCHAR> params(NULL);
    // Command line format for version 6 and later:
    //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf
    //   /P <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
    // TODO: Also set zoom factor and scroll to current position?
    if (win->dm && HIWORD(GetFileVersion(exePath)) >= 6)
        params.Set(str::Format(_T("/A \"page=%d\" %s \"%s\""), win->dm->currentPageNo(), args, win->dm->fileName()));
    else
        params.Set(str::Format(_T("%s \"%s\""), args, win->loadedFilePath));
    LaunchFile(exePath, params);

    return true;
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE); 

static bool CanSendAsEmailAttachment(WindowInfo *win=NULL)
{
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (!CanViewExternally(win))
        return false;

    IDropTarget *pDropTarget = NULL;
    if (FAILED(CoCreateInstance(CLSID_SendMail, NULL, CLSCTX_ALL, IID_IDropTarget, (void **)&pDropTarget)))
        return false;
    pDropTarget->Release();
    return true;
}

static bool SendAsEmailAttachment(WindowInfo *win)
{
    if (!CanSendAsEmailAttachment(win))
        return false;

    // We use the SendTo drop target provided by SendMail.dll, which should ship with all
    // commonly used Windows versions, instead of MAPISendMail, which doesn't support
    // Unicode paths and might not be set up on systems not having Microsoft Outlook installed.
    IDataObject *pDataObject = GetDataObjectForFile(win->dm->fileName(), win->hwndFrame);
    if (!pDataObject)
        return false;

    IDropTarget *pDropTarget = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SendMail, NULL, CLSCTX_ALL, IID_IDropTarget, (void **)&pDropTarget);
    if (SUCCEEDED(hr)) {
        POINTL pt = { 0, 0 };
        DWORD dwEffect = 0;
        pDropTarget->DragEnter(pDataObject, MK_LBUTTON, pt, &dwEffect);
        hr = pDropTarget->Drop(pDataObject, MK_LBUTTON, pt, &dwEffect);
        pDropTarget->Release();
    }

    pDataObject->Release();
    return SUCCEEDED(hr);
}

static void MenuUpdateDisplayMode(WindowInfo& win)
{
    bool enabled = false;
    DisplayMode displayMode = gGlobalPrefs.defaultDisplayMode;
    if (win.IsDocLoaded()) {
        enabled = true;
        displayMode = win.dm->displayMode();
    }

    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++)
        win::menu::SetEnabled(win.menu, id, enabled);

    UINT id = 0;
    switch (displayMode) {
        case DM_SINGLE_PAGE: id = IDM_VIEW_SINGLE_PAGE; break;
        case DM_FACING: id = IDM_VIEW_FACING; break;
        case DM_BOOK_VIEW: id = IDM_VIEW_BOOK; break;
        case DM_CONTINUOUS: id = IDM_VIEW_SINGLE_PAGE; break;
        case DM_CONTINUOUS_FACING: id = IDM_VIEW_FACING; break;
        case DM_CONTINUOUS_BOOK_VIEW: id = IDM_VIEW_BOOK; break;
        default: assert(!win.dm && DM_AUTOMATIC == displayMode); break;
    }

    CheckMenuRadioItem(win.menu, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    if (displayModeContinuous(displayMode))
        win::menu::SetChecked(win.menu, IDM_VIEW_CONTINUOUS, true);
}

void WindowInfo::SwitchToDisplayMode(DisplayMode displayMode, bool keepContinuous)
{
    if (!this->IsDocLoaded())
        return;

    if (keepContinuous && displayModeContinuous(this->dm->displayMode())) {
        switch (displayMode) {
            case DM_SINGLE_PAGE: displayMode = DM_CONTINUOUS; break;
            case DM_FACING: displayMode = DM_CONTINUOUS_FACING; break;
            case DM_BOOK_VIEW: displayMode = DM_CONTINUOUS_BOOK_VIEW; break;
        }
    }

    this->dm->changeDisplayMode(displayMode);
    UpdateToolbarState();
}

#define SEP_ITEM "-----"

struct MenuDef {
    const char *title;
    int         id;
    int         flags;
};

MenuDef menuDefFile[] = {
    { _TRN("&Open\tCtrl-O"),                IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl-W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl-S"),          IDM_SAVEAS,                 MF_REQ_DISK_ACCESS },
    { _TRN("&Print...\tCtrl-P"),            IDM_PRINT,                  MF_REQ_PRINTER_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("Save S&hortcut...\tCtrl-Shift-S"), IDM_SAVEAS_BOOKMARK,     MF_REQ_DISK_ACCESS },
    { _TRN("Open in &Adobe Reader"),        IDM_VIEW_WITH_ACROBAT,      MF_REQ_DISK_ACCESS },
    { _TRN("Open in &Foxit Reader"),        IDM_VIEW_WITH_FOXIT,        MF_REQ_DISK_ACCESS },
    { _TRN("Open in PDF-XChange"),          IDM_VIEW_WITH_PDF_XCHANGE,  MF_REQ_DISK_ACCESS },
    { _TRN("Send by &E-mail..."),           IDM_SEND_BY_EMAIL,          MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl-D"),          IDM_PROPERTIES,             0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("E&xit\tCtrl-Q"),                IDM_EXIT,                   0 }
};

MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl-6"),         IDM_VIEW_SINGLE_PAGE,       0  },
    { _TRN("&Facing\tCtrl-7"),              IDM_VIEW_FACING,            0  },
    { _TRN("&Book View\tCtrl-8"),           IDM_VIEW_BOOK,              0  },
    { _TRN("Show &pages continuously"),     IDM_VIEW_CONTINUOUS,        0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Rotate &Left\tCtrl-Shift--"),   IDM_VIEW_ROTATE_LEFT,       0  },
    { _TRN("Rotate &Right\tCtrl-Shift-+"),  IDM_VIEW_ROTATE_RIGHT,      0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Pr&esentation\tCtrl-L"),        IDM_VIEW_PRESENTATION_MODE, 0  },
    { _TRN("F&ullscreen\tCtrl-Shift-L"),    IDM_VIEW_FULLSCREEN,        0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Book&marks\tF12"),              IDM_VIEW_BOOKMARKS,         0  },
    { _TRN("Show &Toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR, 0  },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All\tCtrl-A"),          IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { _TRN("&Copy Selection\tCtrl-C"),      IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
};

MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0  },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0  },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0  },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0  },
    { _TRN("Pa&ge...\tCtrl-G"),             IDM_GOTO_PAGE,              0  },
    { SEP_ITEM,                             0,                          0  },
    { _TRN("&Back\tAlt+Left Arrow"),        IDM_GOTO_NAV_BACK,          0  },
    { _TRN("F&orward\tAlt+Right Arrow"),    IDM_GOTO_NAV_FORWARD,       0  },
    { SEP_ITEM,                             0,                          0  },
    { _TRN("Fin&d...\tCtrl-F"),             IDM_FIND_FIRST,             0  },
};

MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl-0"),            IDM_ZOOM_FIT_PAGE,          0  },
    { _TRN("&Actual Size\tCtrl-1"),         IDM_ZOOM_ACTUAL_SIZE,       0  },
    { _TRN("Fit &Width\tCtrl-2"),           IDM_ZOOM_FIT_WIDTH,         0  },
    { _TRN("Fit &Content\tCtrl-3"),         IDM_ZOOM_FIT_CONTENT,       0  },
    { _TRN("Custom &Zoom...\tCtrl-Y"),      IDM_ZOOM_CUSTOM,            0  },
    { SEP_ITEM },
    { "6400%",                              IDM_ZOOM_6400,              MF_NO_TRANSLATE  },
    { "3200%",                              IDM_ZOOM_3200,              MF_NO_TRANSLATE  },
    { "1600%",                              IDM_ZOOM_1600,              MF_NO_TRANSLATE  },
    { "800%",                               IDM_ZOOM_800,               MF_NO_TRANSLATE  },
    { "400%",                               IDM_ZOOM_400,               MF_NO_TRANSLATE  },
    { "200%",                               IDM_ZOOM_200,               MF_NO_TRANSLATE  },
    { "150%",                               IDM_ZOOM_150,               MF_NO_TRANSLATE  },
    { "125%",                               IDM_ZOOM_125,               MF_NO_TRANSLATE  },
    { "100%",                               IDM_ZOOM_100,               MF_NO_TRANSLATE  },
    { "50%",                                IDM_ZOOM_50,                MF_NO_TRANSLATE  },
    { "25%",                                IDM_ZOOM_25,                MF_NO_TRANSLATE  },
    { "12.5%",                              IDM_ZOOM_12_5,              MF_NO_TRANSLATE  },
    { "8.33%",                              IDM_ZOOM_8_33,              MF_NO_TRANSLATE  },
};

MenuDef menuDefFavorites[] = {
    // TODO: translate when finalized
    { "Add to favorites",                  IDM_FAV_ADD,                MF_NO_TRANSLATE },
    { "Remove from favorites",             IDM_FAV_DEL,                MF_NO_TRANSLATE },
    { "Show Favorites",                    IDM_FAV_TOGGLE,             MF_NO_TRANSLATE },
};

MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
#if 0
    { _TRN("Contribute Translation"),       IDM_CONTRIBUTE_TRANSLATION, MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_REQ_PREF_ACCESS },
};

MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};

#ifdef SHOW_DEBUG_MENU_ITEMS
MenuDef menuDefDebug[] = {
    { "Highlight links",                    IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
    { "Use GDI+ renderer",                  IDM_DEBUG_GDI_RENDERER,     MF_NO_TRANSLATE },
    { SEP_ITEM,                             0,                          0 },
    { "Crash me",                           IDM_DEBUG_CRASH_ME,         MF_NO_TRANSLATE },
};
#endif

MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection"),              IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Link Address"),           IDM_COPY_LINK_TARGET,       MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                IDM_COPY_COMMENT,           MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All"),                  IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_PLUGIN_MODE_ONLY | MF_REQ_ALLOW_COPY },
    { _TRN("&Save As..."),                  IDM_SAVEAS,                 MF_PLUGIN_MODE_ONLY | MF_REQ_DISK_ACCESS },
    { _TRN("&Print..."),                    IDM_PRINT,                  MF_PLUGIN_MODE_ONLY | MF_REQ_PRINTER_ACCESS },
    { _TRN("P&roperties"),                  IDM_PROPERTIES,             MF_PLUGIN_MODE_ONLY },
};

MenuDef menuDefContextStart[] = {
    { _TRN("&Open Document"),               IDM_OPEN_SELECTED_DOCUMENT, 0 },
    { _TRN("&Pin Document"),                IDM_PIN_SELECTED_DOCUMENT,  0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("&Remove Document"),             IDM_FORGET_SELECTED_DOCUMENT, 0 },
};

static void AddFileMenuItem(HMENU menuFile, const TCHAR *filePath, UINT index)
{
    assert(filePath && menuFile);
    if (!filePath || !menuFile) return;

    ScopedMem<TCHAR> menuString(str::Format(_T("&%d) %s"), (index + 1) % 10, path::GetBaseName(filePath)));
    UINT menuId = IDM_FILE_HISTORY_FIRST + index;
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
}

static HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu)
{
    assert(menu);

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        const char *title = md.title;
        if (!HasPermission(md.flags >> PERM_FLAG_OFFSET))
            continue;
        if (!gPluginMode && (md.flags & MF_PLUGIN_MODE_ONLY))
            continue;

        if (str::Eq(title, SEP_ITEM)) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            ScopedMem<TCHAR> tmp(str::conv::FromUtf8(title));
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        } else {
            const TCHAR *tmp = Trans::GetTranslation(title);
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        }
    }

    return menu;
}

static void AppendRecentFilesToMenu(HMENU m)
{
    if (!HasPermission(Perm_SavePreferences)) return;
    if (gFileHistory.IsEmpty()) return;

    for (int index = 0; index < FILE_HISTORY_MAX_RECENT; index++) {
        DisplayState *state = gFileHistory.Get(index);
        if (!state)
            break;
        assert(state->filePath);
        if (state->filePath)
            AddFileMenuItem(m, state->filePath, index);
        if (FILE_HISTORY_MAX_RECENT == index)
            DBG_OUT("  not adding, reached max %d items\n", FILE_HISTORY_MAX_RECENT);
    }

    InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
}

#define ALWAYS_SUBMENUS 1

// Note: those might be too big
#define MAX_FAV_SUBMENUS 10
#define MAX_FAV_MENUS 10

static TCHAR *FavReadableName(FavName *fn)
{
    if (fn->name) {
        return str::Format(_T("%s (page %d)"), fn->name, fn->pageNo);
    } else {
        return str::Format(_T("Page %d"), fn->pageNo);
    }
}

static void AppendFavMenuItems(HMENU m, FileFavs *f, UINT& idx)
{
    size_t items = f->favNames.Count();
    if (items > MAX_FAV_MENUS) {
        items = MAX_FAV_MENUS;
    }
    for (size_t i=0; i<items; i++)
    {
        FavName *fn = f->favNames.At(i);
        fn->menuId = idx++;
        ScopedMem<TCHAR> s(FavReadableName(fn));
        AppendMenu(m, MF_STRING, (UINT_PTR)fn->menuId, s);
    }
}

static int sortByBaseFileName(const void *a, const void *b)
{
    TCHAR *filePathA = *(TCHAR**)a;
    TCHAR *filePathB = *(TCHAR**)b;
    return str::CmpNatural(path::GetBaseName(filePathA), path::GetBaseName(filePathB));
}

// For easy access, we try to show favorites in the menu, similar to a list of
// recently opened files.
// The first menu items are for currently opened file (up to MAX_FAV_MENUS), based
// on the assumption that user is usually interested in navigating current file.
// Then we have a submenu for each file for which there are bookmarks (up to
// MAX_FAV_SUBMENUS), each having up to MAX_FAV_MENUS menu items.
// If not all favorites can be shown, we also enable "Show all favorites" menu which
// will provide a way to see all favorites.
// Note: not sure if that's the best layout. Maybe we should always use submenu and
// put the submenu for current file as the first one (potentially named as "Current file"
// or some such, to make it stand out from other submenus)
static void AppendFavMenus(HMENU m, const TCHAR *currFilePath)
{
    UINT idx = IDM_FAV_FIRST;
    FileFavs *currFileFav = NULL;
    gFavorites->ResetMenuIds();
    size_t submenus = gFavorites->favs.Count();
    bool addedSep = false;
    if (NULL != currFilePath)
    {
        currFileFav = gFavorites->GetFavByFilePath(currFilePath);
        if (currFileFav && !currFileFav->IsEmpty()) {
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
            addedSep = true;
            submenus--; // exclude current file
#if ALWAYS_SUBMENUS
            HMENU sub = CreateMenu();
            AppendFavMenuItems(sub, currFileFav, idx);
            AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, _T("Current file"));
#else
            AppendFavMenuItems(m, currFileFav, idx, needsShowMore);
#endif
        }
    }

    if (submenus > 0 && !addedSep)
        AppendMenu(m, MF_SEPARATOR, 0, NULL);

    // sort the files with favorites by base file name of file path
    Vec<TCHAR*> filePathsSorted;
    for (size_t i = 0; i < gFavorites->favs.Count(); i++)
    {
        FileFavs *f = gFavorites->favs.At(i);
        if (f == currFileFav)
            continue;
        filePathsSorted.Append(f->filePath);
    }
    filePathsSorted.Sort(sortByBaseFileName);

    for (size_t i=0; i<filePathsSorted.Count(); i++)
    {
        TCHAR *filePath = filePathsSorted.At(i);
        FileFavs *f = gFavorites->GetFavByFilePath(filePath);
        HMENU sub = CreateMenu();
        AppendFavMenuItems(sub, f, idx);
        const TCHAR *fileName = path::GetBaseName(filePath);
        AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, fileName);
    }

    win::menu::SetEnabled(m, IDM_FAV_TOGGLE, HasFavorites());
}

// Called when a user opens "Favorites" top-level menu. We need to construct
// the menu:
// - disable add/remove menu items if no document is opened
// - if a document is opened and the page is already bookmarked,
//   disable "add" menu item and enable "remove" menu item
// - if a document is opened and the page is not bookmarked,
//   enable "add" menu item and disable "remove" menu item
static void RebuildFavMenu(WindowInfo *win, HMENU menu)
{
    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefFavorites, dimof(menuDefFavorites), menu);
    if (!win->IsDocLoaded()) {
        win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
        win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
        AppendFavMenus(menu, NULL);
    } else {
        int pageNo = win->currPageNo;
        TCHAR *filePath = win->loadedFilePath;
        bool isBookmarked = gFavorites->IsPageInFavorites(filePath, pageNo);
        if (isBookmarked)
        {
            win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
            // TODO: translate when finalized
            ScopedMem<TCHAR> s(str::Format(_T("Remove page %d from favorites"), pageNo));
            win::menu::SetText(menu, IDM_FAV_DEL, s);
        } else {
            win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
            // TODO: translate when finalized
            ScopedMem<TCHAR> s(str::Format(_T("Add page %d to favorites"), pageNo));
            win::menu::SetText(menu, IDM_FAV_ADD, s);
        }
        AppendFavMenus(menu, filePath);
    }
}

static void RebuildFileMenu(HMENU menu)
{
    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), menu);
    AppendRecentFilesToMenu(menu);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // (don't hide items here that won't always be hidden,
    // do that in MenuUpdateStateForWindow)
    if (!CanViewWithAcrobat())
        win::menu::Remove(menu, IDM_VIEW_WITH_ACROBAT);
    if (!CanViewWithFoxit())
        win::menu::Remove(menu, IDM_VIEW_WITH_FOXIT);
    if (!CanViewWithPDFXChange())
        win::menu::Remove(menu, IDM_VIEW_WITH_PDF_XCHANGE);
    if (!CanSendAsEmailAttachment())
        win::menu::Remove(menu, IDM_SEND_BY_EMAIL);
}

static HMENU BuildMenu(WindowInfo *win)
{
    HMENU mainMenu = CreateMenu();
    HMENU m = CreateMenu();
    RebuildFileMenu(m);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, dimof(menuDefView), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, dimof(menuDefGoTo), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    m = BuildMenuFromMenuDef(menuDefZoom, dimof(menuDefZoom), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));
#ifndef HIDE_FAVORITES_MENU
    if (HasPermission(Perm_SavePreferences)) {
        // I think it makes sense to disable favorites in restricted mode
        // because they wouldn't be persisted, anyway
        m = CreateMenu();
        RebuildFavMenu(win, m);
        // TODO: translate when finalized
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("F&avorites"));
    }
#endif
    m = BuildMenuFromMenuDef(menuDefSettings, dimof(menuDefSettings), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#ifdef SHOW_DEBUG_MENU_ITEMS
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("Debug"));
#endif

    SetMenu(win->hwndFrame, mainMenu);
    return mainMenu;
}

WindowInfo *FindWindowInfoByHwnd(HWND hwnd)
{
    HWND parent = GetParent(hwnd);
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (hwnd == win->hwndFrame      ||
            hwnd == win->hwndProperties ||
            // canvas, toolbar, rebar, tocbox, spliter
            parent == win->hwndFrame    ||
            // infotips, message windows
            parent == win->hwndCanvas   ||
            // page and find labels and boxes
            parent == win->hwndToolbar  ||
            // ToC tree, sidebar title and close button
            parent == win->hwndTocBox ||
            // Favorites tree, title, and close button
            parent == win->hwndFavBox)
        {
            return win;
        }
    }
    return NULL;
}

static bool WindowInfoStillValid(WindowInfo *win)
{
    return gWindows.Find(win) != -1;
}

// Find the first window showing a given PDF file 
WindowInfo* FindWindowInfoByFile(TCHAR *file)
{
    ScopedMem<TCHAR> normFile(path::Normalize(file));
    if (!normFile)
        return NULL;

    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (!win->IsAboutWindow() && path::IsSame(win->loadedFilePath, normFile))
            return win;
    }

    return NULL;
}

// Find the first window that has been produced from <file>
WindowInfo* FindWindowInfoBySyncFile(TCHAR *file)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        Vec<RectI> rects;
        UINT page;
        if (win->pdfsync && win->pdfsync->source_to_pdf(file, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE)
            return win;
    }
    return NULL;
}

/* Get password for a given 'fileName', can be NULL if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
TCHAR *WindowInfo::GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                               unsigned char decryptionKeyOut[32], bool *saveKey)
{
    DisplayState *fileFromHistory = gFileHistory.Find(fileName);
    if (fileFromHistory && fileFromHistory->decryptionKey) {
        ScopedMem<char> fingerprint(str::MemToHex(fileDigest, 16));
        *saveKey = str::StartsWith(fileFromHistory->decryptionKey, fingerprint.Get());
        if (*saveKey && str::HexToMem(fileFromHistory->decryptionKey + 32, decryptionKeyOut, 32))
            return NULL;
    }

    *saveKey = false;
    if (suppressPwdUI)
        return NULL;

    fileName = path::GetBaseName(fileName);
    return Dialog_GetPassword(this->hwndFrame, fileName, gGlobalPrefs.rememberOpenedFiles ? saveKey : NULL);
}

/* Caller needs to free() the result. */
static TCHAR *GetPrefsFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

static struct {
    unsigned short itemId;
    float zoom;
} gZoomMenuIds[] = {
    { IDM_ZOOM_6400,    6400.0 },
    { IDM_ZOOM_3200,    3200.0 },
    { IDM_ZOOM_1600,    1600.0 },
    { IDM_ZOOM_800,     800.0  },
    { IDM_ZOOM_400,     400.0  },
    { IDM_ZOOM_200,     200.0  },
    { IDM_ZOOM_150,     150.0  },
    { IDM_ZOOM_125,     125.0  },
    { IDM_ZOOM_100,     100.0  },
    { IDM_ZOOM_50,      50.0   },
    { IDM_ZOOM_25,      25.0   },
    { IDM_ZOOM_12_5,    12.5   },
    { IDM_ZOOM_8_33,    8.33f  },
    { IDM_ZOOM_CUSTOM,  0      },
    { IDM_ZOOM_FIT_PAGE,    ZOOM_FIT_PAGE    },
    { IDM_ZOOM_FIT_WIDTH,   ZOOM_FIT_WIDTH   },
    { IDM_ZOOM_FIT_CONTENT, ZOOM_FIT_CONTENT },
    { IDM_ZOOM_ACTUAL_SIZE, ZOOM_ACTUAL_SIZE },
};

static UINT MenuIdFromVirtualZoom(float virtualZoom)
{
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (virtualZoom == gZoomMenuIds[i].zoom)
            return gZoomMenuIds[i].itemId;
    }
    return IDM_ZOOM_CUSTOM;
}

static float ZoomMenuItemToZoom(UINT menuItemId)
{
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (menuItemId == gZoomMenuIds[i].itemId)
            return gZoomMenuIds[i].zoom;
    }
    assert(0);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, UINT menuItemId, bool canZoom)
{
    assert(IDM_ZOOM_FIRST <= menuItemId && menuItemId <= IDM_ZOOM_LAST);

    for (int i = 0; i < dimof(gZoomMenuIds); i++)
        win::menu::SetEnabled(m, gZoomMenuIds[i].itemId, canZoom);

    if (IDM_ZOOM_100 == menuItemId)
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;
    CheckMenuRadioItem(m, IDM_ZOOM_FIRST, IDM_ZOOM_LAST, menuItemId, MF_BYCOMMAND);
    if (IDM_ZOOM_ACTUAL_SIZE == menuItemId)
        CheckMenuRadioItem(m, IDM_ZOOM_100, IDM_ZOOM_100, IDM_ZOOM_100, MF_BYCOMMAND);
}

static void MenuUpdateZoom(WindowInfo& win)
{
    float zoomVirtual = gGlobalPrefs.defaultZoom;
    if (win.IsDocLoaded())
        zoomVirtual = win.dm->zoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win.menu, menuId, win.IsDocLoaded());
}

static void RememberWindowPosition(WindowInfo& win)
{
    // update global windowState for next default launch when either
    // no pdf is opened or a document without window dimension information
    if (win.presentation)
        gGlobalPrefs.windowState = win.windowStateBeforePresentation;
    else if (win.fullScreen)
        gGlobalPrefs.windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win.hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win.hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_NORMAL;

    gGlobalPrefs.sidebarDx = WindowRect(win.hwndTocBox).dx;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL == gGlobalPrefs.windowState &&
        !IsIconic(win.hwndFrame) && !win.presentation) {
        // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
        //       the non-maximized dimensions for proper restoration)
        gGlobalPrefs.windowPos = WindowRect(win.hwndFrame);
    }
}

static void UpdateDisplayStateWindowRect(WindowInfo& win, DisplayState& ds, bool updateGlobal=true)
{
    if (updateGlobal)
        RememberWindowPosition(win);

    ds.windowState = gGlobalPrefs.windowState;
    ds.windowPos = gGlobalPrefs.windowPos;
    ds.sidebarDx = gGlobalPrefs.sidebarDx;
}

static void UpdateCurrentFileDisplayStateForWin(WindowInfo& win)
{
    RememberWindowPosition(win);
    if (!win.IsDocLoaded())
        return;

    const TCHAR *fileName = win.dm->fileName();
    assert(fileName);
    if (!fileName)
        return;

    DisplayState *state = gFileHistory.Find(fileName);
    assert(state || !gGlobalPrefs.rememberOpenedFiles);
    if (!state)
        return;

    if (!win.dm->displayStateFromModel(state))
        return;
    state->useGlobalValues = gGlobalPrefs.globalPrefsOnly;
    UpdateDisplayStateWindowRect(win, *state, false);
    win.DisplayStateFromToC(state);
}

static void ShowOrHideToolbarGlobally()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        if (gGlobalPrefs.showToolbar) {
            ShowWindow(win->hwndReBar, SW_SHOW);
        } else {
            // Move the focus out of the toolbar
            if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
                SetFocus(win->hwndFrame);
            ShowWindow(win->hwndReBar, SW_HIDE);
        }
        ClientRect rect(win->hwndFrame);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    }
}

bool IsUIRightToLeft()
{
    int langIx = Trans::GetLanguageIndex(gGlobalPrefs.currentLanguage);
    return Trans::IsLanguageRtL(langIx);
}

// updates the layout for a window to either left-to-right or right-to-left
// depending on the currently used language (cf. IsUIRightToLeft)
static void UpdateWindowRtlLayout(WindowInfo *win)
{
    bool isRTL = IsUIRightToLeft();
    bool wasRTL = (GetWindowLong(win->hwndFrame, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (wasRTL == isRTL)
        return;

    bool showToC = win->tocVisible;
    bool showFav = win->favVisible;
    if (showToC || showFav)
        SetSidebarVisibility(win, false, false);

    // cf. http://www.microsoft.com/middleeast/msdn/mirror.aspx
    ToggleWindowStyle(win->hwndFrame, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->hwndTocBox, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    HWND tocBoxTitle = GetDlgItem(win->hwndTocBox, 0);
    ToggleWindowStyle(tocBoxTitle, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    HWND favBoxTitle = GetDlgItem(win->hwndFavBox, 0);
    ToggleWindowStyle(favBoxTitle, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    ToggleWindowStyle(win->hwndReBar, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->hwndToolbar, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->hwndFindBox, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->hwndFindText, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->hwndPageText, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    // TODO: also update the canvas scrollbars (?)

    // ensure that the ToC sidebar is on the correct side and that its
    // title and close button are also correctly laid out
    if (showToC || showFav) {
        SetSidebarVisibility(win, showToC, showFav);
        if (showToC)
            SendMessage(win->hwndTocBox, WM_SIZE, 0, 0);
        if (showFav)
            SendMessage(win->hwndFavBox, WM_SIZE, 0, 0);
    }
}

static void UpdateRtlLayoutForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateWindowRtlLayout(gWindows[i]);
    }
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
static bool SavePrefs()
{
    // don't save preferences for plugin windows
    if (gPluginMode)
        return false;

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++)
        UpdateCurrentFileDisplayStateForWin(*gWindows[i]);

    ScopedMem<TCHAR> path(GetPrefsFileName());
    bool ok = Prefs::Save(path, gGlobalPrefs, gFileHistory, gFavorites);
    if (ok) {
        // notify all SumatraPDF instances about the updated prefs file
        HWND hwnd = NULL;
        while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL)))
            PostMessage(hwnd, UWM_PREFS_FILE_UPDATED, 0, 0);
    }
    return ok;
}

// refresh the preferences when a different SumatraPDF process saves them
static bool ReloadPrefs()
{
    ScopedMem<TCHAR> path(GetPrefsFileName());

    FILETIME time = file::GetModificationTime(path);
    if (time.dwLowDateTime == gGlobalPrefs.lastPrefUpdate.dwLowDateTime &&
        time.dwHighDateTime == gGlobalPrefs.lastPrefUpdate.dwHighDateTime) {
        return true;
    }

    const char *currLang = gGlobalPrefs.currentLanguage;
    bool showToolbar = gGlobalPrefs.showToolbar;

    FileHistory fileHistory;
    Favorites *favs = NULL;
    Prefs::Load(path, gGlobalPrefs, fileHistory, &favs);

    gFileHistory.Clear();
    gFileHistory.ExtendWith(fileHistory);
    delete gFavorites;
    gFavorites = favs;

    if (gWindows.Count() > 0 && gWindows[0]->IsAboutWindow()) {
        gWindows[0]->DeleteInfotip();
        gWindows[0]->RedrawAll(true);
    }

    // update the current language
    if (!str::Eq(currLang, gGlobalPrefs.currentLanguage)) {
        CurrLangNameSet(gGlobalPrefs.currentLanguage);
        UpdateRtlLayoutForAllWindows();
        RebuildMenuBar();
        UpdateToolbarToolText();
    }
    if (gGlobalPrefs.showToolbar != showToolbar)
        ShowOrHideToolbarGlobally();
    return true;
}

static int GetPolicies(bool isRestricted)
{
    static struct {
        const TCHAR *name;
        int perm;
    } policies[] = {
        { _T("InternetAccess"), Perm_InternetAccess },
        { _T("DiskAccess"),     Perm_DiskAccess },
        { _T("SavePreferences"),Perm_SavePreferences },
        { _T("RegistryAccess"), Perm_RegistryAccess },
        { _T("PrinterAccess"),  Perm_PrinterAccess },
        { _T("CopySelection"),  Perm_CopySelection },
    };

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (cf. ../docs/sumatrapdfrestrict.ini )
    ScopedMem<TCHAR> restrictPath(GetExePath());
    if (restrictPath) {
        restrictPath.Set(path::GetDir(restrictPath));
        restrictPath.Set(path::Join(restrictPath, RESTRICTIONS_FILE_NAME));
    }
    if (file::Exists(restrictPath)) {
        int policy = Perm_RestrictedUse;
        for (size_t i = 0; i < dimof(policies); i++) {
            int check = GetPrivateProfileInt(_T("Policies"), policies[i].name, 0, restrictPath);
            if (check)
                policy = policy | policies[i].perm;
        }

        // determine the list of allowed link protocols
        gAllowedLinkProtocols.Reset();
        if ((policy & Perm_DiskAccess)) {
            TCHAR protocols[200];
            ZeroMemory(protocols, sizeof(protocols));
            GetPrivateProfileString(_T("Policies"), _T("LinkProtocols"), DEFAULT_LINK_PROTOCOLS, protocols, dimof(protocols), restrictPath);
            str::ToLower(protocols);
            str::TransChars(protocols, _T(":; "), _T(",,,"));
            gAllowedLinkProtocols.Split(protocols, _T(","), true);
        }

        return policy;
    }

    gAllowedLinkProtocols.Reset();
    if (!isRestricted)
        gAllowedLinkProtocols.Split(DEFAULT_LINK_PROTOCOLS, _T(","));

    return isRestricted ? Perm_RestrictedUse : Perm_All;
}

void QueueWorkItem(UIThreadWorkItem *wi)
{
    gUIThreadMarshaller.Queue(wi);
}

class ThumbnailRenderingWorkItem : public UIThreadWorkItem, public RenderingCallback
{
    const TCHAR *filePath;
    RenderedBitmap *bmp;

public:
    ThumbnailRenderingWorkItem(WindowInfo *win, const TCHAR *filePath) :
        UIThreadWorkItem(win), filePath(str::Dup(filePath)), bmp(NULL) {
    }
    ~ThumbnailRenderingWorkItem() {
        free((void *)filePath);
        delete bmp;
    }
    
    virtual void Callback(RenderedBitmap *bmp) {
        this->bmp = bmp;
        QueueWorkItem(this);
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win)) {
            DisplayState *state = gFileHistory.Find(filePath);
            if (state) {
                state->thumbnail = bmp;
                bmp = NULL;
                SaveThumbnail(*state);
            }
        }
    }
};

void CreateThumbnailForFile(WindowInfo& win, DisplayState& state)
{
    // don't even create thumbnails for files that won't need them anytime soon
    Vec<DisplayState *> *list = gFileHistory.GetFrequencyOrder();
    int ix = list->Find(&state);
    delete list;
    if (ix < 0 || FILE_HISTORY_MAX_FREQUENT * 2 <= ix)
        return;

    if (HasThumbnail(state))
        return;

    RectD pageRect = win.dm->engine->PageMediabox(1);
    if (pageRect.IsEmpty())
        return;

    pageRect = win.dm->engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = THUMBNAIL_DX / (float)pageRect.dx;
    if (pageRect.dy > (float)THUMBNAIL_DY / zoom)
        pageRect.dy = (float)THUMBNAIL_DY / zoom;
    pageRect = win.dm->engine->Transform(pageRect, 1, 1.0f, 0, true);

    RenderingCallback *callback = new ThumbnailRenderingWorkItem(&win, win.loadedFilePath);
    gRenderCache.Render(win.dm, 1, 0, zoom, pageRect, *callback);
}

void WindowInfo::Reload(bool autorefresh)
{
    DisplayState ds;
    ds.useGlobalValues = gGlobalPrefs.globalPrefsOnly;
    if (!this->IsDocLoaded() || !this->dm->displayStateFromModel(&ds)) {
        if (!autorefresh && !this->IsDocLoaded() && !this->IsAboutWindow())
            LoadDocument(this->loadedFilePath, this);
        return;
    }
    UpdateDisplayStateWindowRect(*this, ds);
    this->DisplayStateFromToC(&ds);
    // Set the windows state based on the actual window's placement
    ds.windowState =  this->fullScreen ? WIN_STATE_FULLSCREEN
                    : IsZoomed(this->hwndFrame) ? WIN_STATE_MAXIMIZED 
                    : IsIconic(this->hwndFrame) ? WIN_STATE_MINIMIZED
                    : WIN_STATE_NORMAL ;

    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken, 
    // we postpone the reload until the next autorefresh event
    bool tryRepair = !autorefresh;
    ScopedMem<TCHAR> path(str::Dup(this->loadedFilePath));
    if (!LoadDocIntoWindow(path, *this, &ds, false, tryRepair, true, false))
        return;

    if (gGlobalPrefs.showStartPage) {
        // refresh the thumbnail for this file
        DisplayState *state = gFileHistory.Find(ds.filePath);
        if (state)
            CreateThumbnailForFile(*this, *state);
    }

    // save a newly remembered password into file history so that
    // we don't ask again at the next refresh
    ScopedMem<char> decryptionKey(this->dm->engine->GetDecryptionKey());
    if (decryptionKey) {
        DisplayState *state = gFileHistory.Find(ds.filePath);
        if (state && !str::Eq(state->decryptionKey, decryptionKey))
            str::ReplacePtr(&state->decryptionKey, decryptionKey);
    }
}

static void UpdateToolbarBg(HWND hwnd, bool enabled)
{
    ToggleWindowStyle(hwnd, SS_WHITERECT, enabled);
}

static void UpdateFindbox(WindowInfo* win)
{
    UpdateToolbarBg(win->hwndFindBg, win->IsDocLoaded());
    UpdateToolbarBg(win->hwndPageBg, win->IsDocLoaded());

    InvalidateRect(win->hwndToolbar, NULL, TRUE);
    UpdateWindow(win->hwndToolbar);

    if (!win->IsDocLoaded()) {  // Avoid focus on Find box
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorArrow);
        HideCaret(NULL);
    } else {
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorIBeam);
        ShowCaret(NULL);
    }
}

static bool FileCloseMenuEnabled()
{
    for (size_t i = 0; i < gWindows.Count(); i++)
        if (!gWindows[i]->IsAboutWindow())
            return true;
    return false;
}

bool TbIsSeparator(ToolbarButtonInfo& tbi)
{
    return tbi.bmpIndex < 0;
}

static BOOL IsVisibleToolbarButton(WindowInfo& win, int buttonNo)
{
    if (!win.dm || !win.dm->engine || !win.dm->engine->IsImageCollection())
        return TRUE;

    int cmdId = gToolbarButtons[buttonNo].cmdId;
    switch (cmdId) {
        case IDM_FIND_FIRST:
        case IDM_FIND_NEXT:
        case IDM_FIND_PREV:
        case IDM_FIND_MATCH:
            return FALSE;
    }
    return TRUE;
}

static LPARAM ToolbarButtonEnabledState(WindowInfo& win, int buttonNo)
{
    const LPARAM enabled = (LPARAM)MAKELONG(1,0);
    const LPARAM disabled = (LPARAM)MAKELONG(0,0);

    int cmdId = gToolbarButtons[buttonNo].cmdId;

    // If restricted, disable
    if (!HasPermission(gToolbarButtons[buttonNo].flags >> PERM_FLAG_OFFSET))
        return disabled;

    // If no file open, only enable open button
    if (!win.IsDocLoaded())
        return IDM_OPEN == cmdId ? enabled : disabled;

    switch (cmdId)
    {
        case IDM_OPEN:
            // opening different files isn't allowed in plugin mode
            if (gPluginMode)
                return disabled;
            break;

        case IDM_FIND_NEXT:
        case IDM_FIND_PREV:
            // TODO: Update on whether there's more to find, not just on whether there is text.
            if (win::GetTextLen(win.hwndFindBox) == 0)
                return disabled;
            break;

        case IDM_GOTO_NEXT_PAGE:
            if (win.dm->currentPageNo() == win.dm->pageCount())
                return disabled;
            break;
        case IDM_GOTO_PREV_PAGE:
            if (win.dm->currentPageNo() == 1)
                return disabled;
            break;
    }

    return enabled;
}

static void ToolbarUpdateStateForWindow(WindowInfo& win) {

    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        BOOL hide = !IsVisibleToolbarButton(win, i);
        SendMessage(win.hwndToolbar, TB_HIDEBUTTON, gToolbarButtons[i].cmdId, hide);

        if (TbIsSeparator(gToolbarButtons[i]))
            continue;

        LPARAM buttonState = ToolbarButtonEnabledState(win, i);
        SendMessage(win.hwndToolbar, TB_ENABLEBUTTON, gToolbarButtons[i].cmdId, buttonState);
    }
}

static void MenuUpdatePrintItem(WindowInfo& win, HMENU menu, bool disableOnly=false) {
    bool filePrintEnabled = win.IsDocLoaded();
    bool filePrintAllowed = !filePrintEnabled || win.dm->engine->IsPrintingAllowed();

    int ix;
    for (ix = 0; ix < dimof(menuDefFile) && menuDefFile[ix].id != IDM_PRINT; ix++);
    assert(ix < dimof(menuDefFile));
    if (ix < dimof(menuDefFile)) {
        const TCHAR *printItem = Trans::GetTranslation(menuDefFile[ix].title);
        if (!filePrintAllowed)
            printItem = _TR("&Print... (denied)");
        if (!filePrintAllowed || !disableOnly)
            ModifyMenu(menu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
    }

    win::menu::SetEnabled(menu, IDM_PRINT, filePrintEnabled && filePrintAllowed);
}

static void MenuUpdateStateForWindow(WindowInfo& win) {
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SAVEAS_BOOKMARK, IDM_SEND_BY_EMAIL,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE, 
        IDM_SELECT_ALL, IDM_COPY_SELECTION, IDM_PROPERTIES, IDM_VIEW_PRESENTATION_MODE };
    static UINT menusToDisableIfNotPdfDocument[] = {
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE
    };
    static UINT menusToDisableIfDirectory[] = {
        IDM_SAVEAS, IDM_SEND_BY_EMAIL
    };

    assert(FileCloseMenuEnabled() == !win.IsAboutWindow()); // TODO: ???
    win::menu::SetEnabled(win.menu, IDM_CLOSE, FileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win.menu);

    bool enabled = win.IsDocLoaded() && win.dm->HasTocTree();
    win::menu::SetEnabled(win.menu, IDM_VIEW_BOOKMARKS, enabled);

    bool documentSpecific = win.IsDocLoaded();
    bool checked = documentSpecific ? win.tocVisible : gGlobalPrefs.showToc;
    win::menu::SetChecked(win.menu, IDM_VIEW_BOOKMARKS, checked);

    win::menu::SetChecked(win.menu, IDM_FAV_TOGGLE, win.favVisible);
    win::menu::SetChecked(win.menu, IDM_VIEW_SHOW_HIDE_TOOLBAR, gGlobalPrefs.showToolbar);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win.IsDocLoaded()) {
        win::menu::SetEnabled(win.menu, IDM_GOTO_NAV_BACK, win.dm->canNavigate(-1));
        win::menu::SetEnabled(win.menu, IDM_GOTO_NAV_FORWARD, win.dm->canNavigate(1));
    }

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        UINT id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win.menu, id, win.IsDocLoaded());
    }

    if (IsNonPdfDocument(&win)) {
        for (int i = 0; i < dimof(menusToDisableIfNotPdfDocument); i++) {
            UINT id = menusToDisableIfNotPdfDocument[i];
            win::menu::SetEnabled(win.menu, id, false);
        }
    }

    if (win.dm && Engine_ImageDir == win.dm->engineType) {
        for (int i = 0; i < dimof(menusToDisableIfDirectory); i++) {
            UINT id = menusToDisableIfDirectory[i];
            win::menu::SetEnabled(win.menu, id, false);
        }
    }

    if (win.dm && win.dm->engine)
        win::menu::SetEnabled(win.menu, IDM_FIND_FIRST, !win.dm->engine->IsImageCollection());

#ifdef SHOW_DEBUG_MENU_ITEMS
    win::menu::SetChecked(win.menu, IDM_DEBUG_SHOW_LINKS, gDebugShowLinks);
    win::menu::SetChecked(win.menu, IDM_DEBUG_GDI_RENDERER, gUseGdiRenderer);
#endif
}

static void UpdateToolbarAndScrollbarsForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        ToolbarUpdateStateForWindow(*win);

        if (!win->IsDocLoaded()) {
            ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
            if (win->IsAboutWindow())
                win::SetText(win->hwndFrame, SUMATRA_WINDOW_TITLE);
        }
    }
}

#define MIN_WIN_DX 50
#define MIN_WIN_DY 50

void EnsureWindowVisibility(RectI& rect)
{
    // adjust to the work-area of the current monitor (not necessarily the primary one)
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(MonitorFromRect(&rect.ToRECT(), MONITOR_DEFAULTTONEAREST), &mi))
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    RectI work = RectI::FromRECT(mi.rcWork);
    // make sure that the window is neither too small nor bigger than the monitor
    if (rect.dx < MIN_WIN_DX || rect.dx > work.dx)
        rect.dx = (int)min(work.dy * DEF_PAGE_RATIO, work.dx);
    if (rect.dy < MIN_WIN_DY || rect.dy > work.dy)
        rect.dy = work.dy;

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    RectI halfCaption(rect.x, rect.y + captionDy / 2, rect.dx, captionDy / 2);
    if (halfCaption.Intersect(work).IsEmpty())
        rect = RectI(work.TL(), rect.Size());
}

static WindowInfo* CreateWindowInfo()
{
    RectI windowPos;
    if (gGlobalPrefs.windowPos.IsEmpty()) {
        // center the window on the primary monitor
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        RectI work = RectI::FromRECT(workArea);
        windowPos.y = work.x;
        windowPos.dy = work.dy;
        windowPos.dx = (int)min(windowPos.dy * DEF_PAGE_RATIO, work.dx);
        windowPos.x = (work.dx - windowPos.dx) / 2;
    }
    else {
        windowPos = gGlobalPrefs.windowPos;
        EnsureWindowVisibility(windowPos);
    }

    HWND hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    assert(NULL == FindWindowInfoByHwnd(hwndFrame));
    WindowInfo *win = new WindowInfo(hwndFrame);

    HWND hwndCanvas = CreateWindowEx(
            WS_EX_STATICEDGE, 
            CANVAS_CLASS_NAME, NULL,
            WS_CHILD | WS_HSCROLL | WS_VSCROLL,
            0, 0, 0, 0, /* position and size determined in OnSize */
            hwndFrame, NULL,
            ghinst, NULL);
    if (!hwndCanvas)
        return NULL;
    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(hwndCanvas, SB_BOTH, FALSE);

    assert(NULL == win->menu);
    win->menu = BuildMenu(win);

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    CreateToolbar(*win);
    CreateSidebar(win);
    UpdateFindbox(win);
    DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    UpdateWindowRtlLayout(win);
    return win;
}

static void DeleteWindowInfo(WindowInfo *win)
{
    assert(win);
    if (!win) return;

    // must DestroyWindow(win->hwndProperties) before removing win from
    // the list of properties beacuse WM_DESTROY handler needs to find
    // WindowInfo for its HWND
    if (win->hwndProperties) {
        DestroyWindow(win->hwndProperties);
        assert(NULL == win->hwndProperties);
    }
    gWindows.Remove(win);

    ImageList_Destroy((HIMAGELIST)SendMessage(win->hwndToolbar, TB_GETIMAGELIST, 0, 0));
    DragAcceptFiles(win->hwndCanvas, FALSE);

    delete win;
}

static bool LoadDocIntoWindow(
    const TCHAR *fileName, // path to the document
    WindowInfo& win,       // destination window
    const DisplayState *state,   // state
    bool isNewWindow,      // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool tryRepair,        // if true then try to repair the document if it is broken
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (isNewWindow=false)
{
    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.globalPrefsOnly || state && state->useGlobalValues))
        state = NULL;

    DisplayMode displayMode = gGlobalPrefs.defaultDisplayMode;
    int startPage = 1;
    ScrollState ss(1, -1, -1);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState;
    int showType = gGlobalPrefs.windowState == WIN_STATE_MAXIMIZED || showAsFullScreen ? SW_MAXIMIZE : SW_NORMAL;

    if (state) {
        startPage = state->pageNo;
        displayMode = state->displayMode;
        showAsFullScreen = WIN_STATE_FULLSCREEN == state->windowState;
        if (state->windowState == WIN_STATE_NORMAL)
            showType = SW_NORMAL;
        else if (state->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen)
            showType = SW_MAXIMIZE;
        else if (state->windowState == WIN_STATE_MINIMIZED)
            showType = SW_MINIMIZE;
    }

    DisplayModel *prevModel = win.dm;
    win.AbortFinding();
    delete win.pdfsync;
    win.pdfsync = NULL;

    str::ReplacePtr(&win.loadedFilePath, fileName);
    win.dm = DisplayModel::CreateFromFileName(&win, fileName, displayMode,
        startPage, win.GetViewPortSize());
    bool needrefresh = !win.dm;

    bool oldTocShow = win.tocVisible;
    // ToC items might hold a reference to an Engine, so make sure to
    // delete them before destroying the whole DisplayModel
    if (win.dm || tryRepair)
        ClearTocBox(&win);

    assert(!win.IsAboutWindow() && win.IsDocLoaded() == (win.dm != NULL));
    if (win.dm) {
        if (prevModel && str::Eq(win.dm->fileName(), prevModel->fileName()))
            gRenderCache.KeepForDisplayModel(prevModel, win.dm);
        delete prevModel;
    }
    else if (tryRepair) {
        DBG_OUT("failed to load file %s\n", fileName);
        delete prevModel;
        ScopedMem<TCHAR> title(str::Format(_T("%s - %s"), path::GetBaseName(fileName), SUMATRA_WINDOW_TITLE));
        win::SetText(win.hwndFrame, title);
        goto Error;
    }
    else {
        // if there is an error while reading the document and a repair is not requested
        // then fallback to the previous state
        DBG_OUT("failed to load file %s, falling back to previous DisplayModel\n", fileName);
        win.dm = prevModel;
    }

    float zoomVirtual = gGlobalPrefs.defaultZoom;
    int rotation = DEFAULT_ROTATION;

    if (state) {
        if (win.dm->validPageNo(startPage)) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else let win.dm->Relayout() scroll to fit the page (again)
        }
        else if (startPage > win.dm->pageCount())
            ss.page = win.dm->pageCount();
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;

        win.tocVisible = state->showToc;
        win.tocState.Reset();
        if (state->tocState) {
            int len = state->tocState[0];
            win.tocState.MakeSpaceAt(0, len);
            memcpy(win.tocState.LendData(), state->tocState + 1, len * sizeof(int));
        }
    }
    else {
        win.tocVisible = gGlobalPrefs.showToc;
    }
    //SidebarDxFromDisplayState(state);

    // Review needed: Is the following block really necessary?
    /*
    // The WM_SIZE message must be sent *after* updating win.showToc
    // otherwise the bookmark window reappear even if state->showToc=false.
    ClientRect rect(win.hwndFrame);
    SendMessage(win.hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    */

    win.dm->Relayout(zoomVirtual, rotation);
    // Only restore the scroll state when everything is visible
    // (otherwise we might have to relayout twice, which can take
    //  a while for longer documents)
    // win.dm->SetScrollState(ss);

    if (!isNewWindow) {
        win.RedrawAll();
        OnMenuFindMatchCase(win);
    }
    UpdateFindbox(&win);

    int pageCount = win.dm->pageCount();
    if (pageCount > 0) {
        UpdateToolbarPageText(win, pageCount);
        UpdateToolbarFindText(win);
    }

    const TCHAR *baseName = path::GetBaseName(win.dm->fileName());
    TCHAR *title = str::Format(_T("%s - %s"), baseName, SUMATRA_WINDOW_TITLE);
#ifdef UNICODE
    if (IsUIRightToLeft()) {
        free(title);
        // explicitly revert the title, so that filenames aren't garbled
        title = str::Format(_T("\u202A%s - %s\u202C"), SUMATRA_WINDOW_TITLE, baseName);
    }
#endif
    if (needrefresh) {
        TCHAR *msg = str::Format(_TR("[Changes detected; refreshing] %s"), title);
        free(title);
        title = msg;
    }
    win::SetText(win.hwndFrame, title);
    free(title);

    if (HasPermission(Perm_DiskAccess) && Engine_PDF == win.dm->engineType) {
        int res = Synchronizer::Create(fileName, win.dm, &win.pdfsync);
        // expose SyncTeX in the UI
        if (PDFSYNCERR_SUCCESS == res)
            gGlobalPrefs.enableTeXEnhancements = true;
    }

Error:
    if (isNewWindow || placeWindow && state) {
        if (isNewWindow && state && !state->windowPos.IsEmpty()) {
            // Make sure it doesn't have a position like outside of the screen etc.
            RectI rect = ShiftRectToWorkArea(state->windowPos);
            // This shouldn't happen until !win.IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win.hwndFrame, rect.x, rect.y, rect.dx, rect.dy, TRUE);
        }
#if 0 // not ready yet
        else {
            IntelligentWindowResize(win);
        }
#endif
        if (showWin) {
            ShowWindow(win.hwndFrame, showType);
        }
        UpdateWindow(win.hwndFrame);
    }
    if (win.IsDocLoaded()) {
        ToggleWindowStyle(win.hwndPageBox, ES_NUMBER, !win.dm->engine || !win.dm->engine->HasPageLabels());
        win.dm->SetScrollState(ss);
        win.UpdateToolbarState();
    }

    bool showToc = win.IsDocLoaded() && win.tocVisible && win.dm->HasTocTree();
    if (showToc) {
        SetSidebarVisibility(&win, true, false);
    } else if (oldTocShow) {
        // Hide the now useless ToC sidebar and force an update afterwards
        SetSidebarVisibility(&win, false, false);
        win.RedrawAll(true);
    }
    UpdateToolbarAndScrollbarsForAllWindows();
    if (!win.IsDocLoaded()) {
        win.RedrawAll();
        return false;
    }
    // This should only happen after everything else is ready
    if ((isNewWindow || placeWindow) && showWin && showAsFullScreen)
        EnterFullscreen(win);
    if (!isNewWindow && win.presentation && win.dm)
        win.dm->setPresentationMode(true);

    return true;
}

class FileChangeCallback : public UIThreadWorkItem, public CallbackFunc
{
public:
    FileChangeCallback(WindowInfo *win) : UIThreadWorkItem(win) { }

    virtual void Callback() {
        // We cannot call win->Reload directly as it could cause race conditions
        // between the watching thread and the main thread (and only pass a copy of this
        // callback to the UIThreadMarshaller, as the object will be deleted after use)
        QueueWorkItem(new FileChangeCallback(win));
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win)) {
            // delay the reload slightly, in case we get another request immediately after this one
            SetTimer(win->hwndCanvas, AUTO_RELOAD_TIMER_ID, AUTO_RELOAD_DELAY_IN_MS, NULL);
        }
    }
};

#ifndef THREAD_BASED_FILEWATCH
static void RefreshUpdatedFiles() {
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        if (win->watcher)
            win->watcher->CheckForChanges();
    }
}
#endif

WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win, bool showWin, bool forceReuse, bool suppressPwdUI)
{
    assert(fileName);
    if (!fileName) return NULL;

    ScopedMem<TCHAR> fullpath(path::Normalize(fileName));
    if (!fullpath)
        return win;

    bool isNewWindow = false;
    if (!win && 1 == gWindows.Count() && gWindows[0]->IsAboutWindow()) {
        win = gWindows[0];
    }
    else if (!win || win->IsDocLoaded() && !forceReuse) {
        win = CreateWindowInfo();
        if (!win)
            return NULL;
        isNewWindow = true;
    }

    DeleteOldSelectionInfo(*win, true);
    win->fwdSearchMark.show = false;
    win->messages->CleanUp(NG_RESPONSE_TO_ACTION);
    win->messages->CleanUp(NG_PAGE_INFO_HELPER);

    DisplayState *ds = gFileHistory.Find(fullpath);
    if (ds) {
        AdjustRemovableDriveLetter(fullpath);
        if (ds->windowPos.IsEmpty())
            ds->windowPos = gGlobalPrefs.windowPos;
        EnsureWindowVisibility(ds->windowPos);
    }

    win->suppressPwdUI = suppressPwdUI;
    if (!LoadDocIntoWindow(fullpath, *win, ds, isNewWindow, true, showWin, true)) {
        /* failed to open */
        if (gFileHistory.MarkFileInexistent(fullpath))
            SavePrefs();
        return win;
    }

    if (!win->watcher)
        win->watcher = new FileWatcher(new FileChangeCallback(win));
    win->watcher->Init(fullpath);
#ifdef THREAD_BASED_FILEWATCH
    win->watcher->StartWatchThread();
#endif

    if (gGlobalPrefs.rememberOpenedFiles) {
        assert(str::Eq(fullpath, win->loadedFilePath));
        gFileHistory.MarkFileLoaded(fullpath);
        if (gGlobalPrefs.showStartPage)
            CreateThumbnailForFile(*win, *gFileHistory.Get(0));
        SavePrefs();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        SHAddToRecentDocs(SHARD_PATH, fullpath);

    return win;
}

// The current page edit box is updated with the current page number
void WindowInfo::PageNoChanged(int pageNo)
{
    assert(dm && dm->pageCount() > 0);
    if (!dm || dm->pageCount() == 0)
        return;

    if (INVALID_PAGE_NO != pageNo) {
        ScopedMem<TCHAR> buf(dm->engine->GetPageLabel(pageNo));
        win::SetText(hwndPageBox, buf);
        ToolbarUpdateStateForWindow(*this);
        if (dm->engine && dm->engine->HasPageLabels())
            UpdateToolbarPageText(*this, dm->pageCount());
    }
    if (pageNo != currPageNo) {
        UpdateTocSelection(pageNo);
        currPageNo = pageNo;

        MessageWnd *wnd = messages->GetFirst(NG_PAGE_INFO_HELPER);
        if (wnd) {
            ScopedMem<TCHAR> pageInfo(str::Format(_T("%s %d / %d"), _TR("Page:"), pageNo, dm->pageCount()));
            if (dm->engine && dm->engine->HasPageLabels()) {
                ScopedMem<TCHAR> label(dm->engine->GetPageLabel(pageNo));
                pageInfo.Set(str::Format(_T("%s %s (%d / %d)"), _TR("Page:"), label, pageNo, dm->pageCount()));
            }
            wnd->MessageUpdate(pageInfo);
        }
    }
}

/* Send the request to render a given page to a rendering thread */
void WindowInfo::RenderPage(int pageNo)
{
    assert(dm);
    if (!dm)
        return;
    // don't render any plain images on the rendering thread,
    // they'll be rendered directly in DrawDocument during
    // WM_PAINT on the UI thread
    if (dm->engine && dm->engine->IsImageCollection())
        return;

    gRenderCache.Render(dm, pageNo, NULL);
}

void WindowInfo::CleanUp(DisplayModel *dm)
{
    assert(dm);
    if (!dm)
        return;

    gRenderCache.CancelRendering(dm);
    gRenderCache.FreeForDisplayModel(dm);
}

void WindowInfo::UpdateScrollbars(SizeI canvas)
{
    assert(dm);
    if (!dm)
        return;

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    SizeI viewPort = dm->viewPort.Size();

    if (viewPort.dx >= canvas.dx) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = dm->viewPort.x;
        si.nMin = 0;
        si.nMax = canvas.dx - 1;
        si.nPage = viewPort.dx;
    }
    ShowScrollBar(hwndCanvas, SB_HORZ, viewPort.dx < canvas.dx);
    SetScrollInfo(hwndCanvas, SB_HORZ, &si, TRUE);

    if (viewPort.dy >= canvas.dy) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = dm->viewPort.y;
        si.nMin = 0;
        si.nMax = canvas.dy - 1;
        si.nPage = viewPort.dy;

        if (ZOOM_FIT_PAGE != dm->zoomVirtual()) {
            // keep the top/bottom 5% of the previous page visible after paging down/up
            si.nPage = (UINT)(si.nPage * 0.95);
            si.nMax -= viewPort.dy - si.nPage;
        }
    }
    ShowScrollBar(hwndCanvas, SB_VERT, viewPort.dy < canvas.dy);
    SetScrollInfo(hwndCanvas, SB_VERT, &si, TRUE);
}

void AssociateExeWithPdfExtension()
{
    if (!HasPermission(Perm_RegistryAccess)) return;

    DoAssociateExeWithPdfExtension(HKEY_CURRENT_USER);
    DoAssociateExeWithPdfExtension(HKEY_LOCAL_MACHINE);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, 0, 0);

    // Remind the user, when a different application takes over
    gGlobalPrefs.pdfAssociateShouldAssociate = TRUE;
    gGlobalPrefs.pdfAssociateDontAskAgain = FALSE;
}

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd)
{
    if (IsRunningInPortableMode() || !HasPermission(Perm_RegistryAccess) || gPluginMode)
        return false;

    if (IsExeAssociatedWithPdfExtension())
        return true;

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs.pdfAssociateDontAskAgain) {
        INT_PTR result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs.pdfAssociateDontAskAgain);
        if (IDNO == result) {
            gGlobalPrefs.pdfAssociateShouldAssociate = FALSE;
        } else {
            assert(IDYES == result);
            gGlobalPrefs.pdfAssociateShouldAssociate = TRUE;
        }
    }

    if (!gGlobalPrefs.pdfAssociateShouldAssociate)
        return false;

    AssociateExeWithPdfExtension();
    return true;
}

static void OnDropFiles(HDROP hDrop)
{
    TCHAR       filename[MAX_PATH];
    const int   count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (int i = 0; i < count; i++)
    {
        DragQueryFile(hDrop, i, filename, dimof(filename));
        if (str::EndsWithI(filename, _T(".lnk"))) {
            ScopedMem<TCHAR> resolved(ResolveLnk(filename));
            if (resolved)
                str::BufSet(filename, dimof(filename), resolved);
        }
        // The first dropped document may override the current window
        LoadDocument(filename);
    }
    DragFinish(hDrop);
}

static DWORD OnUrlDownloaded(HWND hParent, HttpReqCtx *ctx, bool silent)
{
    if (ctx->error)
        return ctx->error;
    if (!str::StartsWith(ctx->url, SUMATRA_UPDATE_INFO_URL))
        return ERROR_INTERNET_INVALID_URL;

    // See http://code.google.com/p/sumatrapdf/issues/detail?id=725
    // If a user configures os-wide proxy that is not regular ie proxy
    // (which we pick up) we might get complete garbage in response to
    // our query and it might accidentally contain a number bigger than
    // our version number which will make us ask to upgrade every time.
    // To fix that, we reject text that doesn't look like a valid version number.
    ScopedMem<char> txt(ctx->data->StealData());
    if (!IsValidProgramVersion(txt))
        return ERROR_INTERNET_INVALID_URL;

    ScopedMem<TCHAR> verTxt(str::conv::FromAnsi(txt));
    /* reduce the string to a single line (resp. drop the newline) */
    str::TransChars(verTxt, _T("\r\n"), _T("\0\0"));
    if (CompareVersion(verTxt, UPDATE_CHECK_VER) <= 0) {
        /* if automated => don't notify that there is no new version */
        if (!silent) {
            MessageBox(hParent, _TR("You have the latest version."),
                       _TR("SumatraPDF Update"), MB_ICONINFORMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        }
        return 0;
    }

    // if automated, respect gGlobalPrefs.versionToSkip
    if (silent && str::EqI(gGlobalPrefs.versionToSkip, verTxt))
        return 0;

    // ask whether to download the new version and allow the user to
    // either open the browser, do nothing or don't be reminded of
    // this update ever again
    bool skipThisVersion = false;
    INT_PTR res = Dialog_NewVersionAvailable(hParent, UPDATE_CHECK_VER, verTxt, &skipThisVersion);
    if (skipThisVersion)
        str::ReplacePtr(&gGlobalPrefs.versionToSkip, verTxt);
    if (IDYES == res)
        LaunchBrowser(SVN_UPDATE_LINK);
    SavePrefs();

    return 0;
}

class UpdateDownloadWorkItem : public UIThreadWorkItem, public HttpReqCallback
{
    bool autoCheck;
    HttpReqCtx *ctx;

public:
    UpdateDownloadWorkItem(WindowInfo *win, bool autoCheck) :
        UIThreadWorkItem(win), autoCheck(autoCheck), ctx(NULL) { }

    virtual void Callback(HttpReqCtx *ctx) {
        this->ctx = ctx;
        QueueWorkItem(this);
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && ctx) {
            DWORD error = OnUrlDownloaded(win->hwndFrame, ctx, autoCheck);
            if (error && !autoCheck) {
                // notify the user about the error during a manual update check
                ScopedMem<TCHAR> msg(str::Format(_TR("Can't connect to the Internet (error %#x)."), error));
                MessageBox(win->hwndFrame, msg, _TR("SumatraPDF Update"), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
            }
        }
        delete ctx;
    }
};

static void DownloadSumatraUpdateInfo(WindowInfo& win, bool autoCheck)
{
    if (!HasPermission(Perm_InternetAccess) || gPluginMode)
        return;

    /* For auto-check, only check if at least a day passed since last check */
    if (autoCheck && gGlobalPrefs.lastUpdateTime) {
        FILETIME lastUpdateTimeFt, currentTimeFt;
        _HexToMem(gGlobalPrefs.lastUpdateTime, &lastUpdateTimeFt);
        GetSystemTimeAsFileTime(&currentTimeFt);
        int secs = FileTimeDiffInSecs(currentTimeFt, lastUpdateTimeFt);
        assert(secs >= 0);
        // if secs < 0 => somethings wrong, so ignore that case
        if ((secs > 0) && (secs < SECS_IN_DAY))
            return;
    }

    const TCHAR *url = SUMATRA_UPDATE_INFO_URL _T("?v=") UPDATE_CHECK_VER;
    new HttpReqCtx(url, new UpdateDownloadWorkItem(&win, autoCheck));

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    free(gGlobalPrefs.lastUpdateTime);
    gGlobalPrefs.lastUpdateTime = _MemToHex(&ft);
}

static void PaintTransparentRectangle(HDC hdc, RectI screenRc, RectI *rect, COLORREF selectionColor, BYTE alpha = 0x5f, int margin = 1) {
    // don't draw selection parts not visible on screen
    screenRc.Inflate(margin, margin);
    RectI isect = rect->Intersect(screenRc);
    if (isect.IsEmpty())
        return;
    rect = &isect;

    HDC rectDC = CreateCompatibleDC(hdc);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc, rect->dx, rect->dy);
    SelectObject(rectDC, hbitmap);
    if (!hbitmap)
        DBG_OUT("    selection rectangle too big to be drawn\n");

    // draw selection border
    RectI rc = *rect;
    rc.Offset(-rect->x, -rect->y);
    if (margin) {
        FillRect(rectDC, &rc.ToRECT(), gBrushBlack);
        rc.Inflate(-margin, -margin);
    }
    // fill selection
    HBRUSH brush = CreateSolidBrush(selectionColor);
    FillRect(rectDC, &rc.ToRECT(), brush);
    DeleteObject(brush);
    // blend selection rectangle over content
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, 0 };
    AlphaBlend(hdc, rect->x, rect->y, rect->dx, rect->dy, rectDC, 0, 0, rect->dx, rect->dy, bf);

    DeleteObject(hbitmap);
    DeleteDC(rectDC);
}

static void UpdateTextSelection(WindowInfo& win, bool select=true)
{
    assert(win.IsDocLoaded());
    if (!win.IsDocLoaded()) return;

    if (select) {
        int pageNo = win.dm->GetPageNoByPoint(win.selectionRect.BR());
        if (win.dm->validPageNo(pageNo)) {
            PointD pt = win.dm->CvtFromScreen(win.selectionRect.BR(), pageNo);
            win.dm->textSelection->SelectUpTo(pageNo, pt.x, pt.y);
        }
    }

    DeleteOldSelectionInfo(win);
    win.selectionOnPage = SelectionOnPage::FromTextSelect(&win.dm->textSelection->result);
    win.showSelection = true;
}

static void PaintSelection(WindowInfo& win, HDC hdc) {
    if (win.mouseAction == MA_SELECTING) {
        // during selecting
        RectI selRect = win.selectionRect;
        if (selRect.dx < 0) {
            selRect.x += selRect.dx;
            selRect.dx *= -1;
        }
        if (selRect.dy < 0) {
            selRect.y += selRect.dy;
            selRect.dy *= -1;
        }

        PaintTransparentRectangle(hdc, win.canvasRc, &selRect, COL_SELECTION_RECT);
    } else {
        if (MA_SELECTING_TEXT == win.mouseAction)
            UpdateTextSelection(win);

        // after selection is done
        for (size_t i = 0; i < win.selectionOnPage->Count(); i++)
            PaintTransparentRectangle(hdc, win.canvasRc,
                &win.selectionOnPage->At(i).GetRect(win.dm), COL_SELECTION_RECT);
    }
}

static void PaintForwardSearchMark(WindowInfo& win, HDC hdc) {
    PageInfo *pageInfo = win.dm->getPageInfo(win.fwdSearchMark.page);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio)
        return;
    
    // Draw the rectangles highlighting the forward search results
    for (UINT i = 0; i < win.fwdSearchMark.rects.Count(); i++) {
        RectD recD = win.fwdSearchMark.rects[i].Convert<double>();
        RectI recI = win.dm->CvtToScreen(win.fwdSearchMark.page, recD);
        if (gGlobalPrefs.fwdSearchOffset > 0) {
            recI.x = max(pageInfo->pageOnScreen.x, 0) + (int)(gGlobalPrefs.fwdSearchOffset * win.dm->zoomReal());
            recI.dx = (int)((gGlobalPrefs.fwdSearchWidth > 0 ? gGlobalPrefs.fwdSearchWidth : 15.0) * win.dm->zoomReal());
            recI.y -= 4;
            recI.dy += 8;
        }
        BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win.fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
        PaintTransparentRectangle(hdc, win.canvasRc, &recI, gGlobalPrefs.fwdSearchColor, alpha, 0);
    }
}

#ifdef DRAW_PAGE_SHADOWS
#define BORDER_SIZE   1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    // Frame info
    RectI frame = bounds;
    frame.Inflate(BORDER_SIZE, BORDER_SIZE);

    // Shadow info
    RectI shadow = frame;
    shadow.Offset(SHADOW_OFFSET, SHADOW_OFFSET);
    if (frame.x < 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = min(-pageRect.x, SHADOW_OFFSET);
        shadow.x -= diff; shadow.dx += diff;
    }
    if (frame.y < 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = min(-pageRect.y, SHADOW_OFFSET);
        shadow.y -= diff; shadow.dy += diff;
    }

    // Draw shadow
    if (!presentation)
        FillRect(hdc, &shadow.ToRECT(), gBrushShadow);

    // Draw frame
    HPEN pe = CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME);
    SelectObject(hdc, pe);
    SelectObject(hdc, gRenderCache.invertColors ? gBrushBlack : gBrushWhite);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
    DeletePen(pe);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    RectI frame = bounds;

    HPEN pe = CreatePen(PS_NULL, 0, 0);
    SelectObject(hdc, pe);
    SelectObject(hdc, gRenderCache.invertColors ? gBrushBlack : gBrushWhite);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx + 1, frame.y + frame.dy + 1);
    DeletePen(pe);
}
#endif

/* debug code to visualize links (can block while rendering) */
static void DebugShowLinks(DisplayModel& dm, HDC hdc)
{
    if (!gDebugShowLinks)
        return;

    RectI viewPortRect(PointI(), dm.viewPort.Size());
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (int pageNo = dm.pageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm.getPageInfo(pageNo);
        if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio)
            continue;

        Vec<PageElement *> *els = dm.engine->GetElements(pageNo);
        if (els) {
            for (size_t i = 0; i < els->Count(); i++) {
                RectI rect = dm.CvtToScreen(pageNo, els->At(i)->GetRect());
                RectI isect = viewPortRect.Intersect(rect);
                if (!isect.IsEmpty())
                    PaintRect(hdc, isect);
            }
            DeleteVecMembers(*els);
            delete els;
        }
    }

    DeletePen(SelectObject(hdc, oldPen));

    if (dm.zoomVirtual() == ZOOM_FIT_CONTENT) {
        // also display the content box when fitting content
        pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0x00, 0xff));
        oldPen = SelectObject(hdc, pen);

        for (int pageNo = dm.pageCount(); pageNo >= 1; --pageNo) {
            PageInfo *pageInfo = dm.getPageInfo(pageNo);
            if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio)
                continue;

            RectI rect = dm.CvtToScreen(pageNo, dm.engine->PageContentBox(pageNo));
            PaintRect(hdc, rect);
        }

        DeletePen(SelectObject(hdc, oldPen));
    }
}

static void DrawDocument(WindowInfo& win, HDC hdc, RECT *rcArea)
{
    DisplayModel* dm = win.dm;
    assert(dm);
    if (!dm) return;

    bool paintOnBlackWithoutShadow = win.presentation ||
    // draw comic books and single images on a black background (without frame and shadow)
                                     dm->engine && dm->engine->IsImageCollection();
    if (paintOnBlackWithoutShadow)
        FillRect(hdc, rcArea, gBrushBlack);
    else
        FillRect(hdc, rcArea, gBrushNoDocBg);

    bool rendering = false;
    RectI screen(PointI(), dm->viewPort.Size());

    DBG_OUT("DrawDocument() ");
    for (int pageNo = 1; pageNo <= dm->pageCount(); ++pageNo) {
        PageInfo *pageInfo = dm->getPageInfo(pageNo);
        if (0.0 == pageInfo->visibleRatio)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI bounds = pageInfo->pageOnScreen.Intersect(screen);
        PaintPageFrameAndShadow(hdc, bounds, pageInfo->pageOnScreen, paintOnBlackWithoutShadow);

        bool renderOutOfDateCue = false;
        UINT renderDelay = 0;
        if (dm->engine && dm->engine->IsImageCollection())
            dm->engine->RenderPage(hdc, pageInfo->pageOnScreen, pageNo, dm->zoomReal(pageNo), dm->rotation());
        else
            renderDelay = gRenderCache.Paint(hdc, &bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);

        if (renderDelay) {
            win::font::ScopedFont fontRightTxt(hdc, _T("MS Shell Dlg"), 14);
            win::HdcScopedSelectFont scope(hdc, fontRightTxt);
            SetTextColor(hdc, gRenderCache.invertColors ? WIN_COL_WHITE : WIN_COL_BLACK);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS)
                    win.RepaintAsync(REPAINT_MESSAGE_DELAY_IN_MS / 4);
                else
                    DrawCenteredText(hdc, bounds, _TR("Please wait - rendering..."), IsUIRightToLeft());
                DBG_OUT("drawing empty %d ", pageNo);
                rendering = true;
            } else {
                DrawCenteredText(hdc, bounds, _TR("Couldn't render the page"), IsUIRightToLeft());
                DBG_OUT("   missing bitmap on visible page %d\n", pageNo);
            }
            continue;
        }

        if (!renderOutOfDateCue)
            continue;

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            SelectObject(bmpDC, gBitmapReloadingCue);
            int size = (int)(16 * win.uiDPIFactor);
            int cx = min(bounds.dx, 2 * size), cy = min(bounds.dy, 2 * size);
            StretchBlt(hdc, bounds.x + bounds.dx - min((cx + size) / 2, cx),
                bounds.y + max((cy - size) / 2, 0), min(cx, size), min(cy, size),
                bmpDC, 0, 0, 16, 16, SRCCOPY);

            DeleteDC(bmpDC);
        }
    }

    if (win.showSelection)
        PaintSelection(win, hdc);
    
    if (win.fwdSearchMark.show)
        PaintForwardSearchMark(win, hdc);

    DBG_OUT("\n");
    if (!rendering)
        DebugShowLinks(*dm, hdc);
}

static void CopySelectionToClipboard(WindowInfo& win)
{
    if (!win.selectionOnPage) return;
    if (!win.dm->engine) return;

    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();

    if (!win.dm->engine->IsCopyingTextAllowed())
        win.ShowNotification(_TR("Copying text was denied (copying as image only)"));
    else if (!win.dm->engine->IsImageCollection()) {
        ScopedMem<TCHAR> selText;
        bool isTextSelection = win.dm->textSelection->result.len > 0;
        if (isTextSelection) {
            selText.Set(win.dm->textSelection->ExtractText(_T("\r\n")));
        }
        else {
            StrVec selections;
            for (size_t i = 0; i < win.selectionOnPage->Count(); i++) {
                SelectionOnPage *selOnPage = &win.selectionOnPage->At(i);
                TCHAR *text = win.dm->getTextInRegion(selOnPage->pageNo, selOnPage->rect);
                if (text)
                    selections.Push(text);
            }
            selText.Set(selections.Join());
        }

        // don't copy empty text
        if (!str::IsEmpty(selText.Get()))
            CopyTextToClipboard(selText, true);

        if (isTextSelection) {
            // don't also copy the first line of a text selection as an image
            CloseClipboard();
            return;
        }
    }

    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage *selOnPage = &win.selectionOnPage->At(0);
    RenderedBitmap * bmp = win.dm->engine->RenderBitmap(selOnPage->pageNo,
        win.dm->zoomReal(), win.dm->rotation(), &selOnPage->rect, Target_Export);
    if (bmp) {
        if (!SetClipboardData(CF_BITMAP, bmp->GetBitmap()))
            SeeLastError();
        delete bmp;
    }

    CloseClipboard();
}

static void DeleteOldSelectionInfo(WindowInfo& win, bool alsoTextSel)
{
    delete win.selectionOnPage;
    win.selectionOnPage = NULL;
    win.showSelection = false;

    if (alsoTextSel && win.IsDocLoaded())
        win.dm->textSelection->Reset();
}

// for testing only
static void CrashMe()
{
#if 1
    char *p = NULL;
    *p = 0;
#else
    SubmitCrashInfo();
#endif
}

static void ToggleGdiDebugging()
{
    gUseGdiRenderer = !gUseGdiRenderer;
    DebugGdiPlusDevice(gUseGdiRenderer);

    for (size_t i = 0; i < gWindows.Count(); i++) {
        DisplayModel *dm = gWindows[i]->dm;
        if (dm) {
            gRenderCache.CancelRendering(dm);
            gRenderCache.KeepForDisplayModel(dm, dm);
            gWindows[i]->RedrawAll(true);
        }
    }
}

static void OnSelectAll(WindowInfo& win, bool textOnly=false)
{
    if (!HasPermission(Perm_CopySelection))
        return;
    if (!win.IsDocLoaded())
        return;

    if (win.hwndFindBox == GetFocus() || win.hwndPageBox == GetFocus()) {
        Edit_SelectAll(GetFocus());
        return;
    }

    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !win.dm->getPageInfo(pageNo)->shown; pageNo++);
        win.dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win.dm->pageCount(); !win.dm->getPageInfo(pageNo)->shown; pageNo--);
        win.dm->textSelection->SelectUpTo(pageNo, -1);
        win.selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    }
    else {
        DeleteOldSelectionInfo(win, true);
        win.selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        win.selectionOnPage = SelectionOnPage::FromRectangle(win.dm, win.selectionRect);
    }

    win.showSelection = true;
    win.RepaintAsync();
}

// returns true if the double-click was handled and false if it wasn't
static bool OnInverseSearch(WindowInfo& win, int x, int y)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return false;
    if (!win.IsDocLoaded() || win.dm->engineType != Engine_PDF) return false;

    // Clear the last forward-search result
    win.fwdSearchMark.rects.Reset();
    InvalidateRect(win.hwndCanvas, NULL, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!win.pdfsync) {
        int err = Synchronizer::Create(win.loadedFilePath, win.dm, &win.pdfsync);
        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            DBG_OUT("Pdfsync: Sync file not found!\n");
            // Fall back to selecting a word when double-clicking over text in
            // a document with no corresponding synchronization file
            if (win.dm->IsOverText(PointI(x, y)))
                return false;
            // In order to avoid confusion for non-LaTeX users, we do not show
            // any error message if the SyncTeX enhancements are hidden from UI
            if (gGlobalPrefs.enableTeXEnhancements)
                win.ShowNotification(_TR("No synchronization file found"));
            return true;
        }
        if (err != PDFSYNCERR_SUCCESS) {
            DBG_OUT("Pdfsync: Sync file cannot be loaded!\n");
            win.ShowNotification(_TR("Synchronization file cannot be opened"));
            return true;
        }
        gGlobalPrefs.enableTeXEnhancements = true;
    }

    int pageNo = win.dm->GetPageNoByPoint(PointI(x, y));
    if (!win.dm->validPageNo(pageNo))
        return false;

    PointI pt = win.dm->CvtFromScreen(PointI(x, y), pageNo).Convert<int>();
    ScopedMem<TCHAR> srcfilepath;
    UINT line, col;
    int err = win.pdfsync->pdf_to_source(pageNo, pt, srcfilepath, &line, &col);
    if (err != PDFSYNCERR_SUCCESS) {
        DBG_OUT("cannot sync from pdf to source!\n");
        win.ShowNotification(_TR("No synchronization info at this position"));
        return true;
    }

    TCHAR *inverseSearch = gGlobalPrefs.inverseSearchCmdLine;
    if (!inverseSearch)
        // Detect a text editor and use it as the default inverse search handler for now
        inverseSearch = AutoDetectInverseSearchCommands();

    ScopedMem<TCHAR> cmdline;
    if (inverseSearch)
        cmdline.Set(win.pdfsync->prepare_commandline(inverseSearch, srcfilepath, line, col));
    if (!str::IsEmpty(cmdline.Get())) {
        HANDLE process = LaunchProcess(cmdline);
        if (process)
            CloseHandle(process);
        else
            win.ShowNotification(_TR("Cannot start inverse search command. Please check the command line in the settings."));
    }
    else if (gGlobalPrefs.enableTeXEnhancements)
        win.ShowNotification(_TR("Cannot start inverse search command. Please check the command line in the settings."));

    if (inverseSearch != gGlobalPrefs.inverseSearchCmdLine)
        free(inverseSearch);

    return true;
}

static void OnAboutContextMenu(WindowInfo& win, int x, int y)
{
    if (!HasPermission(Perm_SavePreferences) || !gGlobalPrefs.rememberOpenedFiles || !gGlobalPrefs.showStartPage)
        return;

    const TCHAR *filePath = GetStaticLink(win.staticLinks, x, y);
    if (!filePath || *filePath == '<')
        return;

    DisplayState *state = gFileHistory.Find(filePath);
    assert(state);
    if (!state)
        return;

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, dimof(menuDefContextStart), CreatePopupMenu());
    win::menu::SetChecked(popup, IDM_PIN_SELECTED_DOCUMENT, state->isPinned);
    POINT pt = { x, y };
    MapWindowPoints(win.hwndCanvas, HWND_DESKTOP, &pt, 1);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win.hwndFrame, NULL);
    switch (cmd) {
    case IDM_OPEN_SELECTED_DOCUMENT:
        LoadDocument(filePath, &win);
        break;

    case IDM_PIN_SELECTED_DOCUMENT:
        state->isPinned = !state->isPinned;
        win.DeleteInfotip();
        win.RedrawAll(true);
        break;

    case IDM_FORGET_SELECTED_DOCUMENT:
        gFileHistory.Remove(state);
        delete state;
        CleanUpThumbnailCache(gFileHistory);
        win.DeleteInfotip();
        win.RedrawAll(true);
        break;
    }
    DestroyMenu(popup);
}

static void OnContextMenu(WindowInfo& win, int x, int y)
{
    assert(win.IsDocLoaded());
    if (!win.IsDocLoaded())
        return;

    PageElement *pageEl = win.dm->GetElementAtPos(PointI(x, y));
    ScopedMem<TCHAR> value(pageEl ? pageEl->GetValue() : NULL);

    HMENU popup = BuildMenuFromMenuDef(menuDefContext, dimof(menuDefContext), CreatePopupMenu());
    if (!value || NULL == pageEl->AsLink())
        win::menu::Remove(popup, IDM_COPY_LINK_TARGET);
    if (!value || NULL != pageEl->AsLink())
        win::menu::Remove(popup, IDM_COPY_COMMENT);

    if (!win.selectionOnPage)
        win::menu::SetEnabled(popup, IDM_COPY_SELECTION, false);
    MenuUpdatePrintItem(win, popup, true);

    POINT pt = { x, y };
    MapWindowPoints(win.hwndCanvas, HWND_DESKTOP, &pt, 1);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win.hwndFrame, NULL);
    switch (cmd) {
    case IDM_COPY_SELECTION:
    case IDM_SELECT_ALL:
    case IDM_SAVEAS:
    case IDM_PRINT:
    case IDM_PROPERTIES:
        SendMessage(win.hwndFrame, WM_COMMAND, cmd, 0);
        break;

    case IDM_COPY_LINK_TARGET:
    case IDM_COPY_COMMENT:
        CopyTextToClipboard(value);
        break;
    }

    DestroyMenu(popup);
    delete pageEl;
}

static void OnDraggingStart(WindowInfo& win, int x, int y, bool right=false)
{
    SetCapture(win.hwndCanvas);
    win.mouseAction = right ? MA_DRAGGING_RIGHT : MA_DRAGGING;
    win.dragPrevPos = PointI(x, y);
    if (GetCursor())
        SetCursor(gCursorDrag);
    DBG_OUT(" dragging start, x=%d, y=%d\n", x, y);
}

static void OnDraggingStop(WindowInfo& win, int x, int y, bool aborted)
{
    if (GetCapture() != win.hwndCanvas)
        return;

    if (GetCursor())
        SetCursor(gCursorArrow);
    ReleaseCapture();

    if (aborted)
        return;

    SizeI drag(x - win.dragPrevPos.x, y - win.dragPrevPos.y);
    DBG_OUT(" dragging ends, x=%d, y=%d, dx=%d, dy=%d\n", x, y, drag.dx, drag.dy);
    win.MoveDocBy(drag.dx, -2 * drag.dy);
}

#define SELECT_AUTOSCROLL_AREA_WIDTH 15
#define SELECT_AUTOSCROLL_STEP_LENGTH 10

static void OnSelectionEdgeAutoscroll(WindowInfo& win, int x, int y)
{
    int dx = 0, dy = 0;

    if (x < SELECT_AUTOSCROLL_AREA_WIDTH * win.uiDPIFactor)
        dx = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (x > (win.canvasRc.dx - SELECT_AUTOSCROLL_AREA_WIDTH) * win.uiDPIFactor)
        dx = SELECT_AUTOSCROLL_STEP_LENGTH;
    if (y < SELECT_AUTOSCROLL_AREA_WIDTH * win.uiDPIFactor)
        dy = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (y > (win.canvasRc.dy - SELECT_AUTOSCROLL_AREA_WIDTH) * win.uiDPIFactor)
        dy = SELECT_AUTOSCROLL_STEP_LENGTH;

    if (dx != 0 || dy != 0) {
        PointI oldOffset = win.dm->viewPort.TL();
        win.MoveDocBy(dx, dy);

        dx = win.dm->viewPort.x - oldOffset.x;
        dy = win.dm->viewPort.y - oldOffset.y;
        win.selectionRect.x -= dx;
        win.selectionRect.y -= dy;
        win.selectionRect.dx += dx;
        win.selectionRect.dy += dy;
    }
}

static void OnMouseMove(WindowInfo& win, int x, int y, WPARAM flags)
{
    if (!win.IsDocLoaded())
        return;
    assert(win.dm);

    if (win.presentation) {
        // shortly display the cursor if the mouse has moved and the cursor is hidden
        if (!(PointI(x, y) == win.dragPrevPos) && !GetCursor()) {
            if (win.mouseAction == MA_IDLE)
                SetCursor(gCursorArrow);
            else
                SendMessage(win.hwndCanvas, WM_SETCURSOR, 0, 0);
            SetTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
        }
    }

    if (win.dragStartPending) {
        // have we already started a proper drag?
        if (abs(x - win.dragStart.x) <= GetSystemMetrics(SM_CXDRAG) &&
            abs(y - win.dragStart.y) <= GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        win.dragStartPending = false;
        delete win.linkOnLastButtonDown;
        win.linkOnLastButtonDown = NULL;
    }

    SizeI drag;
    switch (win.mouseAction) {
    case MA_SCROLLING:
        win.yScrollSpeed = (y - win.dragStart.y) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win.xScrollSpeed = (x - win.dragStart.x) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        break;
    case MA_SELECTING_TEXT:
        if (GetCursor())
            SetCursor(gCursorIBeam);
        /* fall through */
    case MA_SELECTING:
        win.selectionRect.dx = x - win.selectionRect.x;
        win.selectionRect.dy = y - win.selectionRect.y;
        win.RepaintAsync();
        OnSelectionEdgeAutoscroll(win, x, y);
        break;
    case MA_DRAGGING:
    case MA_DRAGGING_RIGHT:
        drag = SizeI(win.dragPrevPos.x - x, win.dragPrevPos.y - y);
        DBG_OUT(" drag move, x=%d, y=%d, dx=%d, dy=%d\n", x, y, drag.dx, drag.dy);
        win.MoveDocBy(drag.dx, drag.dy);
        break;
    }

    win.dragPrevPos = PointI(x, y);
}

static void OnSelectionStart(WindowInfo& win, int x, int y, WPARAM key)
{
    DeleteOldSelectionInfo(win, true);

    win.selectionRect = RectI(x, y, 0, 0);
    win.showSelection = true;
    win.mouseAction = MA_SELECTING;

    // Ctrl+drag forces a rectangular selection
    if (!(key & MK_CONTROL) || (key & MK_SHIFT)) {
        int pageNo = win.dm->GetPageNoByPoint(PointI(x, y));
        if (win.dm->validPageNo(pageNo)) {
            PointD pt = win.dm->CvtFromScreen(PointI(x, y), pageNo);
            win.dm->textSelection->StartAt(pageNo, pt.x, pt.y);
            win.mouseAction = MA_SELECTING_TEXT;
        }
    }

    SetCapture(win.hwndCanvas);
    SetTimer(win.hwndCanvas, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);

    win.RepaintAsync();
}

static void OnSelectionStop(WindowInfo& win, int x, int y, bool aborted)
{
    if (GetCapture() == win.hwndCanvas)
        ReleaseCapture();
    KillTimer(win.hwndCanvas, SMOOTHSCROLL_TIMER_ID);

    // update the text selection before changing the selectionRect
    if (MA_SELECTING_TEXT == win.mouseAction)
        UpdateTextSelection(win);

    win.selectionRect = RectI::FromXY(win.selectionRect.x, win.selectionRect.y, x, y);
    if (aborted || (MA_SELECTING == win.mouseAction ? win.selectionRect.IsEmpty() : !win.selectionOnPage))
        DeleteOldSelectionInfo(win, true);
    else if (win.mouseAction == MA_SELECTING)
        win.selectionOnPage = SelectionOnPage::FromRectangle(win.dm, win.selectionRect);
    win.RepaintAsync();
}

static void OnMouseLeftButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    //DBG_OUT("Left button clicked on %d %d\n", x, y);
    if (win.IsAboutWindow())
        // remember a link under so that on mouse up we only activate
        // link if mouse up is on the same link as mouse down
        win.url = GetStaticLink(win.staticLinks, x, y);
    if (!win.IsDocLoaded())
        return;

    if (MA_DRAGGING_RIGHT == win.mouseAction)
        return;

    if (MA_SCROLLING == win.mouseAction) {
        win.mouseAction = MA_IDLE;
        return;
    }
    assert(win.mouseAction == MA_IDLE);
    assert(win.dm);

    SetFocus(win.hwndFrame);

    assert(!win.linkOnLastButtonDown);
    PageElement *pageEl = win.dm->GetElementAtPos(PointI(x, y));
    if (pageEl && pageEl->AsLink())
        win.linkOnLastButtonDown = pageEl;
    else
        delete pageEl;
    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    if (!HasPermission(Perm_CopySelection) || ((key & MK_SHIFT) || !win.dm->IsOverText(PointI(x, y))) && !(key & MK_CONTROL))
        OnDraggingStart(win, x, y);
    else
        OnSelectionStart(win, x, y, key);
}

static void OnMouseLeftButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    if (win.IsAboutWindow()) {
        const TCHAR *url = GetStaticLink(win.staticLinks, x, y);
        if (url && url == win.url) {
            if (str::Eq(url, SLINK_OPEN_FILE))
                SendMessage(win.hwndFrame, WM_COMMAND, IDM_OPEN, 0);
            else if (str::Eq(url, SLINK_LIST_HIDE)) {
                gGlobalPrefs.showStartPage = false;
                win.RedrawAll(true);
            } else if (str::Eq(url, SLINK_LIST_SHOW)) {
                gGlobalPrefs.showStartPage = true;
                win.RedrawAll(true);
            } else if (!str::StartsWithI(url, _T("http:")) &&
                       !str::StartsWithI(url, _T("https:")) &&
                       !str::StartsWithI(url, _T("mailto:")))
                LoadDocument(url, &win);
            else
                LaunchBrowser(url);
        }
        win.url = NULL;
    }
    if (!win.IsDocLoaded())
        return;

    assert(win.dm);
    if (MA_IDLE == win.mouseAction || MA_DRAGGING_RIGHT == win.mouseAction)
        return;
    assert(MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction || MA_DRAGGING == win.mouseAction);

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    if (MA_DRAGGING == win.mouseAction)
        OnDraggingStop(win, x, y, !didDragMouse);
    else
        OnSelectionStop(win, x, y, !didDragMouse);

    PointD ptPage = win.dm->CvtFromScreen(PointI(x, y));

    if (didDragMouse)
        /* pass */;
    else if (win.linkOnLastButtonDown && win.linkOnLastButtonDown->GetRect().Inside(ptPage)) {
        win.linkHandler->GotoLink(win.linkOnLastButtonDown->AsLink());
        SetCursor(gCursorArrow);
    }
    /* if we had a selection and this was just a click, hide the selection */
    else if (win.showSelection)
        ClearSearchResult(win);
    /* in presentation mode, change pages on left/right-clicks */
    else if (win.fullScreen || PM_ENABLED == win.presentation) {
        if ((key & MK_SHIFT))
            win.dm->goToPrevPage(0);
        else
            win.dm->goToNextPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);

    win.mouseAction = MA_IDLE;
    delete win.linkOnLastButtonDown;
    win.linkOnLastButtonDown = NULL;
}

static void OnMouseLeftButtonDblClk(WindowInfo& win, int x, int y, WPARAM key)
{
    //DBG_OUT("Left button clicked on %d %d\n", x, y);
    if ((win.fullScreen || win.presentation) && !(key & ~MK_LBUTTON) || win.IsAboutWindow()) {
        // in presentation and fullscreen modes, left clicks turn the page,
        // make two quick left clicks (AKA one double-click) turn two pages
        OnMouseLeftButtonDown(win, x, y, key);
        return;
    }

    bool dontSelect = false;
    if (gGlobalPrefs.enableTeXEnhancements && !(key & ~MK_LBUTTON))
        dontSelect = OnInverseSearch(win, x, y);

    if (dontSelect || !win.IsDocLoaded() || !win.dm->IsOverText(PointI(x, y)))
        return;

    int pageNo = win.dm->GetPageNoByPoint(PointI(x, y));
    if (win.dm->validPageNo(pageNo)) {
        PointD pt = win.dm->CvtFromScreen(PointI(x, y), pageNo);
        win.dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
    }

    UpdateTextSelection(win, false);
    win.RepaintAsync();
}

static void OnMouseMiddleButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    // Handle message by recording placement then moving document as mouse moves.

    switch (win.mouseAction) {
    case MA_IDLE:
        win.mouseAction = MA_SCROLLING;

        // record current mouse position, the farther the mouse is moved
        // from this position, the faster we scroll the document
        win.dragStart = PointI(x, y);
        SetCursor(gCursorScroll);
        break;

    case MA_SCROLLING:
        win.mouseAction = MA_IDLE;
        break;
    }
}

static void OnMouseRightButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    //DBG_OUT("Right button clicked on %d %d\n", x, y);
    if (!win.IsDocLoaded()) {
        win.dragStart = PointI(x, y);
        return;
    }

    if (MA_SCROLLING == win.mouseAction)
        win.mouseAction = MA_IDLE;
    else if (win.mouseAction != MA_IDLE)
        return;
    assert(win.dm);

    SetFocus(win.hwndFrame);

    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    OnDraggingStart(win, x, y, true);
}

static void OnMouseRightButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    if (!win.IsDocLoaded()) {
        bool didDragMouse =
            abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
            abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
        if (!didDragMouse)
            OnAboutContextMenu(win, x, y);
        return;
    }

    assert(win.dm);
    if (MA_DRAGGING_RIGHT != win.mouseAction)
        return;

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    OnDraggingStop(win, x, y, !didDragMouse);

    if (didDragMouse)
        /* pass */;
    else if (win.fullScreen || PM_ENABLED == win.presentation) {
        if ((key & MK_CONTROL))
            OnContextMenu(win, x, y);
        else if ((key & MK_SHIFT))
            win.dm->goToNextPage(0);
        else
            win.dm->goToPrevPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);
    else
        OnContextMenu(win, x, y);

    win.mouseAction = MA_IDLE;
}

static void OnMouseRightButtonDblClick(WindowInfo& win, int x, int y, WPARAM key)
{
    if ((win.fullScreen || win.presentation) && !(key & ~MK_RBUTTON)) {
        // in presentation and fullscreen modes, right clicks turn the page,
        // make two quick right clicks (AKA one double-click) turn two pages
        OnMouseRightButtonDown(win, x, y, key);
        return;
    }
}

static void OnPaint(WindowInfo& win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win.hwndCanvas, &ps);

    if (win.IsAboutWindow()) {
        if (HasPermission(Perm_SavePreferences) && gGlobalPrefs.rememberOpenedFiles && gGlobalPrefs.showStartPage)
            DrawStartPage(win, win.buffer->GetDC(), gFileHistory);
        else
            DrawAboutPage(win, win.buffer->GetDC());
        win.buffer->Flush(hdc);
    }
    else if (!win.IsDocLoaded()) {
        win::font::ScopedFont fontRightTxt(hdc, _T("MS Shell Dlg"), 14);
        win::HdcScopedSelectFont scope(hdc, fontRightTxt);
        SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &ps.rcPaint, gBrushNoDocBg);
        ScopedMem<TCHAR> msg(str::Format(_TR("Error loading %s"), win.loadedFilePath));
        DrawCenteredText(hdc, ClientRect(win.hwndCanvas), msg, IsUIRightToLeft());
    } else {
        switch (win.presentation) {
        case PM_BLACK_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushBlack);
            break;
        case PM_WHITE_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushWhite);
            break;
        default:
            DrawDocument(win, win.buffer->GetDC(), &ps.rcPaint);
            win.buffer->Flush(hdc);
        }
    }

    EndPaint(win.hwndCanvas, &ps);
}

static void OnMenuExit()
{
    if (gPluginMode)
        return;

    for (size_t i = 0; i < gWindows.Count(); i++) {
        gWindows[i]->AbortFinding();
        gWindows[i]->AbortPrinting();
    }

    SavePrefs();
    PostQuitMessage(0);
}

// Note: not sure if this is a good idea, if gWindows or its WindowInfo objects
// are corrupted, we might crash trying to access them. Maybe use IsBadReadPtr()
// to test pointers (even if it's not advised in general)
void GetFilesInfo(str::Str<char>& s)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *w = gWindows.At(i);
        if (!w)
            continue;
        if (!w->dm)
            continue;
        if (!w->loadedFilePath)
            continue;
        // only add paths to files encountered during an explicit stress test
        // (for privacy reasons, users should be able to decide themselves
        // whether they want to share what files they had opened during a crash)
        if (!w->stressTest)
            continue;

        s.Append("File: ");
        s.AppendAndFree(str::conv::ToUtf8(w->loadedFilePath));
        s.AppendAndFree(GetStressTestInfo((StressTest *)w->stressTest));
        s.Append("\r\n");
    }
}

/* Close the document associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
void CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose)
{
    assert(win);
    if (!win) return;
    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    if (gPluginMode && !forceClose)
        return;

    if (win->IsDocLoaded())
        win->dm->_dontRenderFlag = true;
    if (win->presentation)
        ExitFullscreen(*win);

    bool lastWindow = false;
    if (1 == gWindows.Count())
        lastWindow = true;

    if (lastWindow)
        SavePrefs();
    else
        UpdateCurrentFileDisplayStateForWin(*win);

    if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        delete win->watcher;
        win->watcher = NULL;
        SetSidebarVisibility(win, false, win->favVisible);
        ClearTocBox(win);
        win->AbortFinding(true);
        delete win->dm;
        win->dm = NULL;
        str::ReplacePtr(&win->loadedFilePath, NULL);
        delete win->pdfsync;
        win->pdfsync = NULL;
        win->messages->CleanUp(NG_RESPONSE_TO_ACTION);
        win->messages->CleanUp(NG_PAGE_INFO_HELPER);

        if (win->hwndProperties) {
            DestroyWindow(win->hwndProperties);
            assert(!win->hwndProperties);
        }
        UpdateToolbarPageText(*win, 0);
        UpdateToolbarFindText(*win);
        DeleteOldSelectionInfo(*win, true);
        win->RedrawAll();
        UpdateFindbox(win);
        SetFocus(win->hwndFrame);
    } else {
        HWND hwndToDestroy = win->hwndFrame;
        DeleteWindowInfo(win);
        DestroyWindow(hwndToDestroy);
    }

    if (lastWindow && quitIfLast) {
        assert(0 == gWindows.Count());
        PostQuitMessage(0);
    } else {
        UpdateToolbarAndScrollbarsForAllWindows();
    }
}

/* Zoom document in window 'hwnd' to zoom level 'zoom'.
   'zoom' is given as a floating-point number, 1.0 is 100%, 2.0 is 200% etc.
*/
static void OnMenuZoom(WindowInfo& win, UINT menuId)
{
    if (!win.IsDocLoaded())
        return;

    float zoom = ZoomMenuItemToZoom(menuId);
    win.ZoomToSelection(zoom, false);
}

static void OnMenuCustomZoom(WindowInfo& win)
{
    if (!win.IsDocLoaded())
        return;

    float zoom = win.dm->zoomVirtual();
    if (IDCANCEL == Dialog_CustomZoom(win.hwndFrame, &zoom))
        return;
    win.ZoomToSelection(zoom, false);
}

static bool CheckPrinterStretchDibSupport(HWND hwndForMsgBox, HDC hdc)
{
#ifdef USE_GDI_FOR_PRINTING
    // assume the printer supports enough of GDI(+) for reasonable results
    return true;
#else
    // most printers can support stretchdibits,
    // whereas a lot of printers do not support bitblt
    // quit if printer doesn't support StretchDIBits
    int rasterCaps = GetDeviceCaps(hdc, RASTERCAPS);
    int supportsStretchDib = rasterCaps & RC_STRETCHDIB;
    if (supportsStretchDib)
        return true;

    MessageBox(hwndForMsgBox, _T("This printer doesn't support the StretchDIBits function"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
    return false;
#endif
}

struct PrintData {
    HDC hdc; // owned by PrintData

    BaseEngine *engine;
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    int rotation;
    PrintRangeAdv rangeAdv;
    PrintScaleAdv scaleAdv;
    short orientation;

    PrintData(BaseEngine *engine, HDC hdc, DEVMODE *devMode,
              Vec<PRINTPAGERANGE>& ranges, int rotation=0,
              PrintRangeAdv rangeAdv=PrintRangeAll,
              PrintScaleAdv scaleAdv=PrintScaleShrink,
              Vec<SelectionOnPage> *sel=NULL) :
        engine(NULL), hdc(hdc), rotation(rotation), rangeAdv(rangeAdv), scaleAdv(scaleAdv)
    {
        if (engine)
            this->engine = engine->Clone();

        if (!sel)
            this->ranges = ranges;
        else
            this->sel = *sel;

        orientation = 0;
        if (devMode && (devMode->dmFields & DM_ORIENTATION))
            orientation = devMode->dmOrientation;
    }

    ~PrintData() {
        delete engine;
        DeleteDC(hdc);
    }
};

static void PrintToDevice(PrintData& pd, ProgressUpdateUI *progressUI=NULL)
{
    assert(pd.engine);
    if (!pd.engine) return;

    HDC hdc = pd.hdc;
    BaseEngine& engine = *pd.engine;

    DOCINFO di = { 0 };
    di.cbSize = sizeof (DOCINFO);
    di.lpszDocName = engine.FileName();

    int current = 0, total = 0;
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        if (pd.ranges[i].nToPage < pd.ranges[i].nFromPage)
            total += pd.ranges[i].nFromPage - pd.ranges[i].nToPage + 1;
        else
            total += pd.ranges[i].nToPage - pd.ranges[i].nFromPage + 1;
    }
    total += (int)pd.sel.Count();
    if (progressUI)
        progressUI->ProgressUpdate(current, total);

    if (StartDoc(hdc, &di) <= 0)
        return;

    SetMapMode(hdc, MM_TEXT);

    const int paperWidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    const int paperHeight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    const int printableWidth = GetDeviceCaps(hdc, HORZRES);
    const int printableHeight = GetDeviceCaps(hdc, VERTRES);
    const int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    const int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    const int rightMargin = paperWidth - printableWidth - leftMargin;
    const int bottomMargin = paperHeight - printableHeight - topMargin;
    const float dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / engine.GetFileDPI(),
                                GetDeviceCaps(hdc, LOGPIXELSY) / engine.GetFileDPI());
    bool bPrintPortrait = paperWidth < paperHeight;
    if (pd.orientation)
        bPrintPortrait = DMORIENT_PORTRAIT == pd.orientation;

    if (pd.sel.Count() > 0) {
        DBG_OUT(" printing:  drawing bitmap for selection\n");

        for (size_t i = 0; i < pd.sel.Count(); i++) {
            StartPage(hdc);
            RectD *clipRegion = &pd.sel[i].rect;

            Size<float> sSize = clipRegion->Size().Convert<float>();
            // Swap width and height for rotated documents
            int rotation = engine.PageRotation(pd.sel[i].pageNo) + pd.rotation;
            if (rotation % 180 != 0)
                swap(sSize.dx, sSize.dy);

            float zoom = min((float)printableWidth / sSize.dx,
                             (float)printableHeight / sSize.dy);
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleShrink == pd.scaleAdv)
                zoom = min(dpiFactor, zoom);
            else if (PrintScaleNone == pd.scaleAdv)
                zoom = dpiFactor;

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(printableWidth - sSize.dx * zoom) / 2,
                                     (int)(printableHeight - sSize.dy * zoom) / 2,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pd.sel[i].pageNo, zoom, pd.rotation, clipRegion, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pd.sel[i].pageNo, zoom, pd.rotation, clipRegion, Target_Print);
            if (bmp) {
                PointI TL((printableWidth - bmp->Size().dx) / 2,
                          (printableHeight - bmp->Size().dy) / 2);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI && !progressUI->ProgressUpdate(current, total)) {
                AbortDoc(hdc);
                return;
            }
        }

        EndDoc(hdc);
        return;
    }

    // print all the pages the user requested
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        int dir = pd.ranges[i].nFromPage > pd.ranges[i].nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges[i].nFromPage; pageNo != pd.ranges[i].nToPage + dir; pageNo += dir) {
            if ((PrintRangeEven == pd.rangeAdv && pageNo % 2 != 0) ||
                (PrintRangeOdd == pd.rangeAdv && pageNo % 2 == 0))
                continue;

            DBG_OUT(" printing:  drawing bitmap for page %d\n", pageNo);

            StartPage(hdc);
            // MM_TEXT: Each logical unit is mapped to one device pixel.
            // Positive x is to the right; positive y is down.

            Size<float> pSize = engine.PageMediabox(pageNo).Size().Convert<float>();
            int rotation = engine.PageRotation(pageNo);
            // Turn the document by 90 deg if it isn't in portrait mode
            if (pSize.dx > pSize.dy) {
                rotation += 90;
                swap(pSize.dx, pSize.dy);
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90 deg in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                swap(pSize.dx, pSize.dy);
            }

            // dpiFactor means no physical zoom
            float zoom = dpiFactor;
            // offset of the top-left corner of the page from the printable area
            // (negative values move the page into the left/top margins, etc.);
            // offset adjustments are needed because the GDI coordinate system
            // starts at the corner of the printable area and we rather want to
            // center the page on the physical paper (default behavior)
            int horizOffset = (paperWidth - printableWidth) / 2 - leftMargin;
            int vertOffset = (paperHeight - printableHeight) / 2 - topMargin;

            if (pd.scaleAdv != PrintScaleNone) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                RectD rect = engine.PageContentBox(pageNo, Target_Print);
                Rect<float> cbox = engine.Transform(rect, pageNo, 1.0, rotation).Convert<float>();
                zoom = min((float)printableWidth / cbox.dx,
                       min((float)printableHeight / cbox.dy,
                       min((float)paperWidth / pSize.dx,
                           (float)paperHeight / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == pd.scaleAdv && dpiFactor < zoom)
                    zoom = dpiFactor;
                // make sure that no content lies in the non-printable paper margins
                Rect<float> onPaper((paperWidth - pSize.dx * zoom) / 2 + cbox.x * zoom,
                                    (paperHeight - pSize.dy * zoom) / 2 + cbox.y * zoom,
                                    cbox.dx * zoom, cbox.dy * zoom);
                if (leftMargin > onPaper.x)
                    horizOffset = (int)(horizOffset + leftMargin - onPaper.x);
                else if (paperWidth - rightMargin < onPaper.BR().x)
                    horizOffset = (int)(horizOffset - onPaper.BR().x + (paperWidth - rightMargin));
                if (topMargin > onPaper.y)
                    vertOffset = (int)(vertOffset + topMargin - onPaper.y);
                else if (paperHeight - bottomMargin < onPaper.BR().y)
                    vertOffset = (int)(vertOffset - onPaper.BR().y + (paperHeight - bottomMargin));
            }

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(paperWidth - pSize.dx * zoom) / 2 + horizOffset - leftMargin,
                                     (int)(paperHeight - pSize.dy * zoom) / 2 + vertOffset - topMargin,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pageNo, zoom, rotation, NULL, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pageNo, zoom, rotation, NULL, Target_Print);
            if (bmp) {
                PointI TL((paperWidth - bmp->Size().dx) / 2 + horizOffset - leftMargin,
                          (paperHeight - bmp->Size().dy) / 2 + vertOffset - topMargin);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI && !progressUI->ProgressUpdate(current, total)) {
                AbortDoc(hdc);
                return;
            }
        }
    }

    EndDoc(hdc);
}

class PrintThreadUpdateWorkItem : public UIThreadWorkItem {
    MessageWnd *wnd;
    int current, total;

public:
    PrintThreadUpdateWorkItem(WindowInfo *win, MessageWnd *wnd, int current, int total)
        : UIThreadWorkItem(win), wnd(wnd), current(current), total(total) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->messages->Contains(wnd))
            wnd->ProgressUpdate(current, total);
    }
};

class PrintThreadWorkItem : public ProgressUpdateUI, public UIThreadWorkItem, public MessageWndCallback {
    MessageWnd *wnd;
    bool isCanceled;

public:
    // owned and deleted by PrintThreadWorkItem
    PrintData *data;

    PrintThreadWorkItem(WindowInfo *win, PrintData *data) :
        UIThreadWorkItem(win), data(data), isCanceled(false) { 
        wnd = new MessageWnd(win->hwndCanvas, _T(""), _TR("Printing page %d of %d..."), this);
        win->messages->Add(wnd);
    }
    ~PrintThreadWorkItem() {
        delete data;
        CleanUp(wnd);
    }

    virtual bool ProgressUpdate(int current, int total) {
        QueueWorkItem(new PrintThreadUpdateWorkItem(win, wnd, current, total));
        return WindowInfoStillValid(win) && !win->printCanceled && !isCanceled;
    }

    void CleanUp() {
        QueueWorkItem(this);
    }

    // called when printing has been canceled
    virtual void CleanUp(MessageWnd *wnd) {
        isCanceled = true;
        this->wnd = NULL;
        if (WindowInfoStillValid(win))
            win->messages->CleanUp(wnd);
    }

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;

        HANDLE thread = win->printThread;
        win->printThread = NULL;
        CloseHandle(thread);
    }
};

static DWORD WINAPI PrintThread(LPVOID data)
{
    PrintThreadWorkItem *progressUI = (PrintThreadWorkItem *)data;
    assert(progressUI && progressUI->data);
    if (progressUI->data)
        PrintToDevice(*progressUI->data, progressUI);
    progressUI->CleanUp();

    return 0;
}

static void PrintToDeviceOnThread(WindowInfo& win, PrintData *data)
{
    PrintThreadWorkItem *progressUI = new PrintThreadWorkItem(&win, data);
    win.printThread = CreateThread(NULL, 0, PrintThread, progressUI, 0, NULL);
}

#ifndef ID_APPLY_NOW
#define ID_APPLY_NOW 0x3021
#endif

static LRESULT CALLBACK DisableApplyBtnWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if (uiMsg == WM_ENABLE)
        EnableWindow(hWnd, FALSE);

    WNDPROC nextWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    return CallWindowProc(nextWndProc, hWnd, uiMsg, wParam, lParam);
}

/* minimal IPrintDialogCallback implementation for hiding the useless Apply button */
class ApplyButtonDiablingCallback : public IPrintDialogCallback
{
public:
    ApplyButtonDiablingCallback() : m_cRef(0) { };
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (riid == IID_IUnknown || riid == IID_IPrintDialogCallback) {
            *ppv = this;
            this->AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    };
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); };
    STDMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&m_cRef); };
    STDMETHODIMP HandleMessage(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult) {
        if (uiMsg == WM_INITDIALOG) {
            HWND hPropSheetContainer = GetParent(GetParent(hDlg));
            HWND hApplyButton = GetDlgItem(hPropSheetContainer, ID_APPLY_NOW);
            WNDPROC nextWndProc = (WNDPROC)SetWindowLongPtr(hApplyButton, GWLP_WNDPROC, (LONG_PTR)DisableApplyBtnWndProc);
            SetWindowLongPtr(hApplyButton, GWLP_USERDATA, (LONG_PTR)nextWndProc);
        }
        return S_FALSE;
    };
    STDMETHODIMP InitDone() { return S_FALSE; };
    STDMETHODIMP SelectionChange() { return S_FALSE; };
protected:
    LONG m_cRef;
    WNDPROC m_wndProc;
};

/* Show Print Dialog box to allow user to select the printer
and the pages to print.

Note: The following doesn't apply for USE_GDI_FOR_PRINTING

Creates a new dummy page for each page with a large zoom factor,
and then uses StretchDIBits to copy this to the printer's dc.

So far have tested printing from XP to
 - Acrobat Professional 6 (note that acrobat is usually set to
   downgrade the resolution of its bitmaps to 150dpi)
 - HP Laserjet 2300d
 - HP Deskjet D4160
 - Lexmark Z515 inkjet, which should cover most bases.
*/
#define MAXPAGERANGES 10
static void OnMenuPrint(WindowInfo& win)
{
    // In order to print with Adobe Reader instead:
    // ViewWithAcrobat(win, _T("/P"));

    if (!HasPermission(Perm_PrinterAccess)) return;

    DisplayModel *dm = win.dm;
    assert(dm);
    if (!dm) return;

    if (win.printThread) {
        int res = MessageBox(win.hwndFrame, _TR("Printing is still in progress. Abort and start over?"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_YESNO | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (res == IDNO)
            return;
    }
    win.AbortPrinting();

    PRINTDLGEX pd;
    ZeroMemory(&pd, sizeof(PRINTDLGEX));
    pd.lStructSize = sizeof(PRINTDLGEX);
    pd.hwndOwner   = win.hwndFrame;
    pd.hDevMode    = NULL;
    pd.hDevNames   = NULL;
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win.selectionOnPage)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges =1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE *ppr = SAZA(PRINTPAGERANGE, MAXPAGERANGES);
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->pageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->pageCount();
    pd.nStartPage = START_PAGE_GENERAL;
    pd.lpCallback = new ApplyButtonDiablingCallback();

    // TODO: remember these (and maybe all of PRINTDLGEX) at least for this document/WindowInfo?
    Print_Advanced_Data advanced = { PrintRangeAll, PrintScaleShrink };
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pd.lphPropertyPages = &hPsp;
    pd.nPropertyPages = 1;

    if (PrintDlgEx(&pd) == S_OK) {
        if (pd.dwResultAction == PD_RESULT_PRINT) {
            if (CheckPrinterStretchDibSupport(win.hwndFrame, pd.hDC)) {
                bool printSelection = false;
                Vec<PRINTPAGERANGE> ranges;
                if (pd.Flags & PD_CURRENTPAGE) {
                    PRINTPAGERANGE pr = { dm->currentPageNo(), dm->currentPageNo() };
                    ranges.Append(pr);
                } else if (win.selectionOnPage && (pd.Flags & PD_SELECTION)) {
                    printSelection = true;
                } else if (!(pd.Flags & PD_PAGENUMS)) {
                    PRINTPAGERANGE pr = { 1, dm->pageCount() };
                    ranges.Append(pr);
                } else {
                    assert(pd.nPageRanges > 0);
                    for (DWORD i = 0; i < pd.nPageRanges; i++)
                        ranges.Append(pd.lpPageRanges[i]);
                }

                LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
                PrintData *data = new PrintData(dm->engine, pd.hDC, devMode, ranges,
                                                dm->rotation(), advanced.range, advanced.scale,
                                                printSelection ? win.selectionOnPage : NULL);
                pd.hDC = NULL; // deleted by PrintData
                if (devMode)
                    GlobalUnlock(pd.hDevMode);

                PrintToDeviceOnThread(win, data);
            }
        }
    }
    else if (CommDlgExtendedError() != 0) { 
        /* if PrintDlg was cancelled then
           CommDlgExtendedError is zero, otherwise it returns the
           error code, which we could look at here if we wanted.
           for now just warn the user that printing has stopped
           becasue of an error */
        MessageBox(win.hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
    }

    free(ppr);
    free(pd.lpCallback);
    DeleteDC(pd.hDC);
    GlobalFree(pd.hDevNames);
    GlobalFree(pd.hDevMode);
}

// caller needs to free() the result
static TCHAR *ExtractFilenameFromURL(const TCHAR *url)
{
    ScopedMem<TCHAR> urlName(str::Dup(url));
    // try to extract the file name from the URL (last path component before query or hash)
    str::TransChars(urlName, _T("/?#"), _T("\\\0\0"));
    urlName.Set(str::Dup(path::GetBaseName(urlName)));
    // unescape hex-escapes (these are usually UTF-8)
    if (str::FindChar(urlName, '%')) {
        ScopedMem<char> utf8Name(str::conv::ToUtf8(urlName));
        char *src = utf8Name, *dst = utf8Name;
        while (*src) {
            if (*src == '%' && isxdigit(*(src + 1)) && isxdigit(*(src + 2))) {
                char hex[3] = { *(src + 1), *(src + 2), '\0' };
                *dst++ = (char)strtoul(hex, NULL, 16);
                src += 3;
            }
            else
                *dst++ = *src++;
        }
        *dst = '\0';
        urlName.Set(str::conv::FromUtf8(utf8Name));
    }
    // fall back to a generic "filename" instead of the more confusing temporary filename
    if (str::IsEmpty(urlName.Get()))
        return str::Dup(_T("filename"));
    return urlName.StealData();
}

static void OnMenuSaveAs(WindowInfo& win)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    assert(win.dm);
    if (!win.IsDocLoaded()) return;

    const TCHAR *srcFileName = win.dm->fileName();
    ScopedMem<TCHAR> urlName;
    if (gPluginMode) {
        urlName.Set(ExtractFilenameFromURL(gPluginURL));
        srcFileName = urlName;
    }

    assert(srcFileName);
    if (!srcFileName) return;

    // Can't save a document's content as plain text if text copying isn't allowed
    bool hasCopyPerm = !win.dm->engine->IsImageCollection() &&
                       win.dm->engine->IsCopyingTextAllowed();
    bool canConvertToPDF = Engine_PS == win.dm->engineType;

    const TCHAR *defExt = win.dm->engine->GetDefaultFileExt();

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str<TCHAR> fileFilter(256);
    switch (win.dm->engineType) {
    case Engine_XPS:    fileFilter.Append(_TR("XPS documents")); break;
    case Engine_DjVu:   fileFilter.Append(_TR("DjVu documents")); break;
    case Engine_ComicBook: fileFilter.Append(_TR("Comic books")); break;
    case Engine_Image:  fileFilter.AppendFmt(_TR("Image files (*.%s)"), defExt + 1); break;
    case Engine_PS:     fileFilter.AppendFmt(_TR("Postscript documents")); break;
    default:            fileFilter.Append(_TR("PDF documents")); break;
    }
    fileFilter.AppendFmt(_T("\1*%s\1"), defExt);
    if (hasCopyPerm) {
        fileFilter.Append(_TR("Text documents"));
        fileFilter.Append(_T("\1*.txt\1"));
    }
    if (canConvertToPDF) {
        fileFilter.Append(_TR("PDF documents"));
        fileFilter.Append(_T("\1*.pdf\1"));
    }
    fileFilter.Append(_TR("All files"));
    fileFilter.Append(_T("\1*.*\1"));
    str::TransChars(fileFilter.Get(), _T("\1"), _T("\0"));

    TCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseName(srcFileName));
    // TODO: fix saving embedded PDF documents
    str::TransChars(dstFileName, _T(":"), _T("_"));
    // Remove the extension so that it can be re-added depending on the chosen filter
    if (str::EndsWithI(dstFileName, defExt))
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrDefExt = defExt + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return;

    TCHAR * realDstFileName = dstFileName;
    // Make sure that the file has a valid ending
    if (!str::EndsWithI(dstFileName, defExt) &&
        !(hasCopyPerm && str::EndsWithI(dstFileName, _T(".txt"))) &&
        !(canConvertToPDF && str::EndsWithI(dstFileName, _T(".pdf")))) {
        if (hasCopyPerm && 2 == ofn.nFilterIndex)
            defExt = _T(".txt");
        else if (canConvertToPDF && (hasCopyPerm ? 3 : 2) == ofn.nFilterIndex)
            defExt = _T(".pdf");
        realDstFileName = str::Format(_T("%s%s"), dstFileName, defExt);
    }
    // Extract all text when saving as a plain text file
    if (hasCopyPerm && str::EndsWithI(realDstFileName, _T(".txt"))) {
        str::Str<TCHAR> text(1024);
        for (int pageNo = 1; pageNo <= win.dm->pageCount(); pageNo++)
            text.AppendAndFree(win.dm->engine->ExtractPageText(pageNo, _T("\r\n"), NULL, Target_Export));

        ScopedMem<char> textUTF8(str::conv::ToUtf8(text.LendData()));
        ScopedMem<char> textUTF8BOM(str::Join("\xEF\xBB\xBF", textUTF8));
        file::WriteAll(realDstFileName, textUTF8BOM, str::Len(textUTF8BOM));
    }
    // Convert the Postscript file into a PDF one
    else if (canConvertToPDF && str::EndsWithI(realDstFileName, _T(".pdf"))) {
        size_t dataLen;
        ScopedMem<unsigned char> data(static_cast<PsEngine *>(win.dm->engine)->GetPDFData(&dataLen));
        if (data)
            file::WriteAll(realDstFileName, data, dataLen);
        else
            MessageBox(win.hwndFrame, _TR("Failed to save a file"), _TR("Warning"), MB_OK | MB_ICONEXCLAMATION | (IsUIRightToLeft() ? MB_RTLREADING : 0));
    }
    // Recreate inexistant PDF files from memory...
    else if (!file::Exists(srcFileName)) {
        size_t dataLen;
        ScopedMem<unsigned char> data(win.dm->engine->GetFileData(&dataLen));
        if (data)
            file::WriteAll(realDstFileName, data, dataLen);
        else
            MessageBox(win.hwndFrame, _TR("Failed to save a file"), _TR("Warning"), MB_OK | MB_ICONEXCLAMATION | (IsUIRightToLeft() ? MB_RTLREADING : 0));
    }
    // ... else just copy the file
    else {
        bool ok = CopyFileEx(srcFileName, realDstFileName, NULL, NULL, NULL, 0);
        if (ok) {
            // Make sure that the copy isn't write-locked or hidden
            const DWORD attributesToDrop = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
            DWORD attributes = GetFileAttributes(realDstFileName);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & attributesToDrop))
                SetFileAttributes(realDstFileName, attributes & ~attributesToDrop);
        } else {
            TCHAR *msgBuf, *errorMsg;
            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPTSTR)&msgBuf, 0, NULL)) {
                errorMsg = str::Format(_T("%s\n\n%s"), _TR("Failed to save a file"), msgBuf);
                LocalFree(msgBuf);
            } else {
                errorMsg = str::Dup(_TR("Failed to save a file"));
            }
            MessageBox(win.hwndFrame, errorMsg, _TR("Warning"), MB_OK | MB_ICONEXCLAMATION | (IsUIRightToLeft() ? MB_RTLREADING : 0));
            free(errorMsg);
        }
    }
    if (realDstFileName != dstFileName)
        free(realDstFileName);
}

bool LinkSaver::SaveEmbedded(unsigned char *data, int len)
{
    if (!HasPermission(Perm_DiskAccess))
        return false;

    TCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), fileName ? fileName : _T(""));

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<TCHAR> fileFilter(str::Format(_T("%s\1*.*\1"), _TR("All files")));
    str::TransChars(fileFilter, _T("\1"), _T("\0"));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = this->hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return false;
    return file::WriteAll(dstFileName, data, len);
}

static void OnMenuSaveBookmark(WindowInfo& win)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return;
    assert(win.dm);
    if (!win.IsDocLoaded()) return;

    const TCHAR *defExt = win.dm->engine->GetDefaultFileExt();

    TCHAR dstFileName[MAX_PATH];
    // Remove the extension so that it can be re-added depending on the chosen filter
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseName(win.dm->fileName()));
    str::TransChars(dstFileName, _T(":"), _T("_"));
    if (str::EndsWithI(dstFileName, defExt))
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<TCHAR> fileFilter(str::Format(_T("%s\1*.lnk\1"), _TR("Bookmark Shortcuts")));
    str::TransChars(fileFilter, _T("\1"), _T("\0"));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = _T("lnk");
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return;

    ScopedMem<TCHAR> filename(str::Dup(dstFileName));
    if (!str::EndsWithI(dstFileName, _T(".lnk")))
        filename.Set(str::Join(dstFileName, _T(".lnk")));

    ScrollState ss = win.dm->GetScrollState();
    const TCHAR *viewMode = DisplayModeConv::NameFromEnum(win.dm->displayMode());
    ScopedMem<TCHAR> zoomVirtual(str::Format(_T("%.2f"), win.dm->zoomVirtual()));
    if (ZOOM_FIT_PAGE == win.dm->zoomVirtual())
        zoomVirtual.Set(str::Dup(_T("fitpage")));
    else if (ZOOM_FIT_WIDTH == win.dm->zoomVirtual())
        zoomVirtual.Set(str::Dup(_T("fitwidth")));
    else if (ZOOM_FIT_CONTENT == win.dm->zoomVirtual())
        zoomVirtual.Set(str::Dup(_T("fitcontent")));

    ScopedMem<TCHAR> exePath(GetExePath());
    ScopedMem<TCHAR> args(str::Format(_T("\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d -reuse-instance"),
                          win.dm->fileName(), ss.page, viewMode, zoomVirtual, (int)ss.x, (int)ss.y));
    ScopedMem<TCHAR> desc(str::Format(_TR("Bookmark shortcut to page %d of %s"),
                          ss.page, path::GetBaseName(win.dm->fileName())));

    CreateShortcut(filename, exePath, args, desc, 1);
}

#if 0
// code adapted from http://support.microsoft.com/kb/131462/en-us
static UINT_PTR CALLBACK FileOpenHook(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uiMsg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
        break;
    case WM_NOTIFY:
        if (((LPOFNOTIFY)lParam)->hdr.code == CDN_SELCHANGE) {
            LPOPENFILENAME lpofn = (LPOPENFILENAME)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            // make sure that the filename buffer is large enough to hold
            // all the selected filenames
            int cbLength = CommDlg_OpenSave_GetSpec(GetParent(hDlg), NULL, 0) + MAX_PATH;
            if (cbLength >= 0 && lpofn->nMaxFile < (DWORD)cbLength) {
                TCHAR *oldBuffer = lpofn->lpstrFile;
                lpofn->lpstrFile = (LPTSTR)realloc(lpofn->lpstrFile, cbLength * sizeof(TCHAR));
                if (lpofn->lpstrFile)
                    lpofn->nMaxFile = cbLength;
                else
                    lpofn->lpstrFile = oldBuffer;
            }
        }
        break;
    }

    return 0;
}
#endif

static void OnMenuOpen(WindowInfo& win)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    // don't allow opening different files in plugin mode
    if (gPluginMode)
        return;

    struct {
        const TCHAR *name;
        TCHAR *filter;
        bool available;
    } fileFormats[] = {
        { _TR("PDF documents"),         _T("*.pdf"),        true },
        { _TR("XPS documents"),         _T("*.xps"),        true },
        { _TR("DjVu documents"),        _T("*.djvu"),       true },
        { _TR("Postscript documents"),  _T("*.ps;*.eps"),   PsEngine::IsAvailable() },
        { _TR("Comic books"),           _T("*.cbz;*.cbr"),  true },
    };
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str<TCHAR> fileFilter;
    StrVec filters;
    for (int i = 0; i < dimof(fileFormats); i++)
        if (fileFormats[i].available)
            filters.Append(fileFormats[i].filter);
    fileFilter.Append(_TR("All supported documents"));
    fileFilter.Append('\1');
    fileFilter.AppendAndFree(filters.Join(_T(";")));
    fileFilter.Append('\1');
    filters.Reset();

    for (int i = 0; i < dimof(fileFormats); i++)
        if (fileFormats[i].available)
            fileFilter.AppendAndFree(str::Format(_T("%s\1%s\1"), fileFormats[i].name, fileFormats[i].filter));
    fileFilter.AppendAndFree(str::Format(_T("%s\1*.*\1"), _TR("All files")));
    str::TransChars(fileFilter.Get(), _T("\1"), _T("\0"));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;

    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 100;
#if 0
    if (!WindowsVerVistaOrGreater())
    {
        ofn.lpfnHook = FileOpenHook;
        ofn.Flags |= OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH / 2;
    }
    // note: ofn.lpstrFile can be reallocated by GetOpenFileName -> FileOpenHook
#endif
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    TCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        LoadDocument(ofn.lpstrFile, &win);
        return;
    }

    while (*fileName) {
        ScopedMem<TCHAR> filePath(path::Join(ofn.lpstrFile, fileName));
        if (filePath)
            LoadDocument(filePath, &win);
        fileName += str::Len(fileName) + 1;
    }
}

static void BrowseFolder(WindowInfo& win, bool forward)
{
    assert(win.loadedFilePath);
    if (win.IsAboutWindow()) return;
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return;

    // TODO: browse through all supported file types at the same time?
    const TCHAR *fileExt = path::GetExt(win.loadedFilePath);
    if (win.IsDocLoaded())
        fileExt = win.dm->engine->GetDefaultFileExt();
    ScopedMem<TCHAR> pattern(str::Format(_T("*%s"), fileExt));
    ScopedMem<TCHAR> dir(path::GetDir(win.loadedFilePath));
    pattern.Set(path::Join(dir, pattern));

    StrVec files;
    if (!CollectPathsFromDirectory(pattern, files))
        return;

    if (-1 == files.Find(win.loadedFilePath))
        files.Append(str::Dup(win.loadedFilePath));
    files.SortNatural();

    int index = files.Find(win.loadedFilePath);
    if (forward)
        index = (index + 1) % files.Count();
    else
        index = (int)(index + files.Count() - 1) % files.Count();

    UpdateCurrentFileDisplayStateForWin(win);
    LoadDocument(files[index], &win, true, true);
}

static void OnVScroll(WindowInfo& win, WPARAM wParam)
{
    if (!win.IsDocLoaded())
        return;
    assert(win.dm);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    int iVertPos = si.nPos;
    int lineHeight = 16;
    if (!displayModeContinuous(win.dm->displayMode()) && ZOOM_FIT_PAGE == win.dm->zoomVirtual())
        lineHeight = 1;

    switch (LOWORD(wParam)) {
    case SB_TOP:        si.nPos = si.nMin; break;
    case SB_BOTTOM:     si.nPos = si.nMax; break;
    case SB_LINEUP:     si.nPos -= lineHeight; break;
    case SB_LINEDOWN:   si.nPos += lineHeight; break;
    case SB_PAGEUP:     si.nPos -= si.nPage; break;
    case SB_PAGEDOWN:   si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    // If the position has changed, scroll the window and update it
    if (win.IsDocLoaded() && (si.nPos != iVertPos))
        win.dm->scrollYTo(si.nPos);
}

static void OnHScroll(WindowInfo& win, WPARAM wParam)
{
    if (!win.IsDocLoaded())
        return;
    assert(win.dm);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    int iVertPos = si.nPos;
    switch (LOWORD(wParam)) {
    case SB_LEFT:       si.nPos = si.nMin; break;
    case SB_RIGHT:      si.nPos = si.nMax; break;
    case SB_LINELEFT:   si.nPos -= 16; break;
    case SB_LINERIGHT:  si.nPos += 16; break;
    case SB_PAGELEFT:   si.nPos -= si.nPage; break;
    case SB_PAGERIGHT:  si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_HORZ, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    // If the position has changed, scroll the window and update it
    if (win.IsDocLoaded() && (si.nPos != iVertPos))
        win.dm->scrollXTo(si.nPos);
}

static void AdjustWindowEdge(WindowInfo& win)
{
    DWORD exStyle = GetWindowLong(win.hwndCanvas, GWL_EXSTYLE);
    DWORD newStyle = exStyle;

    // Remove the canvas' edge in the cases where the vertical scrollbar
    // would otherwise touch the screen's edge, making the scrollbar much
    // easier to hit with the mouse (cf. Fitts' law)
    if (IsZoomed(win.hwndFrame) || win.fullScreen || win.presentation)
        newStyle &= ~WS_EX_STATICEDGE;
    else
        newStyle |= WS_EX_STATICEDGE;

    if (newStyle != exStyle) {
        SetWindowLong(win.hwndCanvas, GWL_EXSTYLE, newStyle);
        SetWindowPos(win.hwndCanvas, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

static void OnFrameSize(WindowInfo* win, int dx, int dy)
{
    int rebBarDy = 0;
    if (gGlobalPrefs.showToolbar) {
        SetWindowPos(win->hwndReBar, NULL, 0, 0, dx, rebBarDy, SWP_NOZORDER);
        rebBarDy = WindowRect(win->hwndReBar).dy;
    }

    bool showToc = win->tocLoaded && win->tocVisible;
    bool showFav = win->favVisible;
    if (showToc || showFav)
        SetSidebarVisibility(win, showToc, showFav);
    else
        SetWindowPos(win->hwndCanvas, NULL, 0, rebBarDy, dx, dy - rebBarDy, SWP_NOZORDER);
}

static void OnMenuChangeLanguage(WindowInfo& win)
{
    int langId = Trans::GetLanguageIndex(gGlobalPrefs.currentLanguage);
    int newLangId = Dialog_ChangeLanguge(win.hwndFrame, langId);

    if (newLangId != -1 && langId != newLangId) {
        const char *langName = Trans::GetLanguageCode(newLangId);
        assert(langName);
        if (!langName)
            return;

        CurrLangNameSet(langName);
        UpdateRtlLayoutForAllWindows();
        RebuildMenuBar();
        UpdateToolbarToolText();
        if (gWindows.Count() > 0 && gWindows[0]->IsAboutWindow())
            gWindows[0]->RedrawAll(true);
        SavePrefs();
    }
}

static void OnMenuViewShowHideToolbar()
{
    gGlobalPrefs.showToolbar = !gGlobalPrefs.showToolbar;
    ShowOrHideToolbarGlobally();
}

static void OnMenuSettings(WindowInfo& win)
{
    if (!HasPermission(Perm_SavePreferences)) return;

    if (IDOK != Dialog_Settings(win.hwndFrame, &gGlobalPrefs))
        return;

    if (!gGlobalPrefs.rememberOpenedFiles) {
        gFileHistory.Clear();
        CleanUpThumbnailCache(gFileHistory);
    }
    if (gWindows.Count() > 0 && gWindows[0]->IsAboutWindow())
        gWindows[0]->RedrawAll(true);

    SavePrefs();
}

// toggles 'show pages continuously' state
static void OnMenuViewContinuous(WindowInfo& win)
{
    if (!win.IsDocLoaded())
        return;

    DisplayMode newMode = win.dm->displayMode();
    switch (newMode) {
        case DM_SINGLE_PAGE:
        case DM_CONTINUOUS:
            newMode = displayModeContinuous(newMode) ? DM_SINGLE_PAGE : DM_CONTINUOUS;
            break;
        case DM_FACING:
        case DM_CONTINUOUS_FACING:
            newMode = displayModeContinuous(newMode) ? DM_FACING : DM_CONTINUOUS_FACING;
            break;
        case DM_BOOK_VIEW:
        case DM_CONTINUOUS_BOOK_VIEW:
            newMode = displayModeContinuous(newMode) ? DM_BOOK_VIEW : DM_CONTINUOUS_BOOK_VIEW;
            break;
    }
    win.SwitchToDisplayMode(newMode);
}

static void ToggleToolbarViewButton(WindowInfo& win, float newZoom, bool pagesContinuously)
{
    if (!win.IsDocLoaded())
        return;

    float zoom = win.dm->zoomVirtual();
    DisplayMode mode = win.dm->displayMode();
    DisplayMode newMode = pagesContinuously ? DM_CONTINUOUS : DM_SINGLE_PAGE;

    if (mode != newMode || zoom != newZoom) {
        DisplayMode prevMode = win.prevDisplayMode;
        float prevZoom = win.prevZoomVirtual;

        if (mode != newMode)
            win.SwitchToDisplayMode(newMode);
        OnMenuZoom(win, MenuIdFromVirtualZoom(newZoom));

        // remember the previous values for when the toolbar button is unchecked
        if (INVALID_ZOOM == prevZoom) {
            win.prevZoomVirtual = zoom;
            win.prevDisplayMode = mode;
        }
        // keep the rememberd values when toggling between the two toolbar buttons
        else {
            win.prevZoomVirtual = prevZoom;
            win.prevDisplayMode = prevMode;
        }
    }
    else if (win.prevZoomVirtual != INVALID_ZOOM) {
        float prevZoom = win.prevZoomVirtual;
        win.SwitchToDisplayMode(win.prevDisplayMode);
        win.ZoomToSelection(prevZoom, false);
    }
}

static void FocusPageNoEdit(HWND hwndPageBox)
{
    if (GetFocus() == hwndPageBox)
        SendMessage(hwndPageBox, WM_SETFOCUS, 0, 0);
    else
        SetFocus(hwndPageBox);
}

static void OnMenuGoToPage(WindowInfo& win)
{
    if (!win.IsDocLoaded())
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.showToolbar && !win.fullScreen && PM_DISABLED == win.presentation) {
        FocusPageNoEdit(win.hwndPageBox);
        return;
    }

    ScopedMem<TCHAR> label(win.dm->engine->GetPageLabel(win.dm->currentPageNo()));
    ScopedMem<TCHAR> newPageLabel(Dialog_GoToPage(win.hwndFrame, label, win.dm->pageCount(),
                                                  !win.dm->engine->HasPageLabels()));
    if (!newPageLabel)
        return;

    int newPageNo = win.dm->engine->GetPageByLabel(newPageLabel);
    if (win.dm->validPageNo(newPageNo))
        win.dm->goToPage(newPageNo, 0, true);
}

static bool NeedsFindUI(WindowInfo& win)
{
    return !win.IsDocLoaded() || win.dm->engine && !win.dm->engine->IsImageCollection();
}

static void OnMenuFind(WindowInfo& win)
{
    if (!win.IsDocLoaded() || !NeedsFindUI(win))
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.showToolbar && !win.fullScreen && PM_DISABLED == win.presentation) {
        if (GetFocus() == win.hwndFindBox)
            SendMessage(win.hwndFindBox, WM_SETFOCUS, 0, 0);
        else
            SetFocus(win.hwndFindBox);
        return;
    }

    ScopedMem<TCHAR> previousFind(win::GetText(win.hwndFindBox));
    WORD state = (WORD)SendMessage(win.hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    ScopedMem<TCHAR> findString(Dialog_Find(win.hwndFrame, previousFind, &matchCase));
    if (!findString)
        return;

    win::SetText(win.hwndFindBox, findString);
    Edit_SetModify(win.hwndFindBox, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase)
            state |= TBSTATE_CHECKED;
        else
            state &= ~TBSTATE_CHECKED;
        SendMessage(win.hwndToolbar, TB_SETSTATE, IDM_FIND_MATCH, state);
        win.dm->textSearch->SetSensitive(matchCase);
    }

    FindTextOnThread(&win);
}

static void EnterFullscreen(WindowInfo& win, bool presentation)
{
    if ((presentation ? win.presentation : win.fullScreen) ||
        !IsWindowVisible(win.hwndFrame) || gPluginMode)
        return;

    assert(presentation ? !win.fullScreen : !win.presentation);
    if (presentation) {
        assert(win.dm);
        if (!win.IsDocLoaded())
            return;

        if (IsZoomed(win.hwndFrame))
            win.windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        else
            win.windowStateBeforePresentation = WIN_STATE_NORMAL;
        win.presentation = PM_ENABLED;
        win.tocBeforeFullScreen = win.tocVisible;
        win.favBeforeFullScreen = win.favVisible;

        SetTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
    }
    else {
        win.fullScreen = true;
        win.tocBeforeFullScreen = win.IsDocLoaded() ? win.tocVisible : false;
        win.favBeforeFullScreen = win.favVisible;
    }

    // Remove TOC from full screen, add back later on exit fullscreen
    if (win.tocVisible || win.favVisible)
        SetSidebarVisibility(&win, win.tocVisible, win.favVisible);

    RectI rect;
    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    HMONITOR m = MonitorFromWindow(win.hwndFrame, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(m, (LPMONITORINFOEX)&mi))
        rect = RectI(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    else
        rect = RectI::FromRECT(mi.rcMonitor);
    long ws = GetWindowLong(win.hwndFrame, GWL_STYLE);
    if (!presentation || !win.fullScreen)
        win.prevStyle = ws;
    ws &= ~(WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    win.frameRc = WindowRect(win.hwndFrame);

    SetMenu(win.hwndFrame, NULL);
    ShowWindow(win.hwndReBar, SW_HIDE);
    SetWindowLong(win.hwndFrame, GWL_STYLE, ws);
    SetWindowPos(win.hwndFrame, HWND_NOTOPMOST, rect.x, rect.y, rect.dx, rect.dy, SWP_FRAMECHANGED|SWP_NOZORDER);
    SetWindowPos(win.hwndCanvas, NULL, 0, 0, rect.dx, rect.dy, SWP_NOZORDER);

    if (presentation)
        win.dm->setPresentationMode(true);

    // Make sure that no toolbar/sidebar keeps the focus
    SetFocus(win.hwndFrame);
}

static void ExitFullscreen(WindowInfo& win)
{
    if (!win.fullScreen && !win.presentation) 
        return;

    bool wasPresentation = PM_DISABLED != win.presentation;
    if (wasPresentation && win.dm) {
        win.dm->setPresentationMode(false);
        win.presentation = PM_DISABLED;
    }
    else
        win.fullScreen = false;

    if (wasPresentation) {
        KillTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID);
        SetCursor(gCursorArrow);
    }

    bool showToc = win.IsDocLoaded() && win.tocBeforeFullScreen;
    bool showFav = win.favBeforeFullScreen;
    if (showToc || showFav)
        SetSidebarVisibility(&win, showToc, showFav);

    if (gGlobalPrefs.showToolbar)
        ShowWindow(win.hwndReBar, SW_SHOW);
    SetMenu(win.hwndFrame, win.menu);
    SetWindowLong(win.hwndFrame, GWL_STYLE, win.prevStyle);
    SetWindowPos(win.hwndFrame, HWND_NOTOPMOST,
                 win.frameRc.x, win.frameRc.y,
                 win.frameRc.dx, win.frameRc.dy,
                 SWP_FRAMECHANGED | SWP_NOZORDER);
}

static void OnMenuViewFullscreen(WindowInfo& win, bool presentation=false)
{
    bool enterFullscreen = presentation ? !win.presentation : !win.fullScreen;

    if (!win.presentation && !win.fullScreen)
        RememberWindowPosition(win);
    else
        ExitFullscreen(win);

    if (enterFullscreen && (!presentation || win.IsDocLoaded()))
        EnterFullscreen(win, presentation);
}

static void OnMenuViewPresentation(WindowInfo& win)
{
    OnMenuViewFullscreen(win, true);
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void WindowInfo::ShowForwardSearchResult(const TCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects)
{
    this->fwdSearchMark.rects.Reset();
    if (ret == PDFSYNCERR_SUCCESS && rects.Count() > 0) {
        // remember the position of the search result for drawing the rect later on
        const PageInfo *pi = this->dm->getPageInfo(page);
        if (pi) {
            fwdSearchMark.rects = rects;
            fwdSearchMark.page = page;
            fwdSearchMark.show = true;
            if (!gGlobalPrefs.fwdSearchPermanent)  {
                fwdSearchMark.hideStep = 0;
                SetTimer(this->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, NULL);
            }

            // Scroll to show the overall highlighted zone
            int pageNo = page;
            RectI overallrc = rects[0];
            for (size_t i = 1; i < rects.Count(); i++)
                overallrc = overallrc.Union(rects[i]);
            TextSel res = { 1, &pageNo, &overallrc };
            if (!this->dm->pageVisible(page))
                this->dm->goToPage(page, 0, true);
            if (!this->dm->ShowResultRectToScreen(&res))
                this->RepaintAsync();
            if (IsIconic(this->hwndFrame))
                ShowWindowAsync(this->hwndFrame, SW_RESTORE);
            return;
        }
    }

    ScopedMem<TCHAR> buf;
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND )
        ShowNotification(_TR("No synchronization file found"));
    else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
        ShowNotification(_TR("Synchronization file cannot be opened"));
    else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER)
        buf.Set(str::Format(_TR("Page number %u inexistant"), page));
    else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION)
        ShowNotification(_TR("No synchronization info at this position"));
    else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE)
        buf.Set(str::Format(_TR("Unknown source file (%s)"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE)
        buf.Set(str::Format(_TR("Source file %s has no synchronization point"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    if (buf)
        ShowNotification(buf);
}

static void OnMenuFindNext(WindowInfo& win)
{
    if (!NeedsFindUI(win))
        return;
    if (SendMessage(win.hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_NEXT, 0))
        FindTextOnThread(&win, FIND_FORWARD);
}

static void OnMenuFindPrev(WindowInfo& win)
{
    if (!NeedsFindUI(win))
        return;
    if (SendMessage(win.hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_PREV, 0))
        FindTextOnThread(&win, FIND_BACKWARD);
}

static void OnMenuFindMatchCase(WindowInfo& win)
{
    if (!NeedsFindUI(win))
        return;
    WORD state = (WORD)SendMessage(win.hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win.dm->textSearch->SetSensitive((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win.hwndFindBox, TRUE);
}

static void AdvanceFocus(WindowInfo* win)
{
    // Tab order: Frame -> Page -> Find -> ToC -> Favorites -> Frame -> ...

    bool hasToolbar = !win->fullScreen && !win->presentation &&
                      gGlobalPrefs.showToolbar && win->IsDocLoaded();
    int direction = IsShiftPressed() ? -1 : 1;

    struct {
        HWND hwnd;
        bool isAvailable;
    } tabOrder[] = {
        { win->hwndFrame,   true                                },
        { win->hwndPageBox, hasToolbar                          },
        { win->hwndFindBox, hasToolbar && NeedsFindUI(*win)     },
        { win->hwndTocTree, win->tocLoaded && win->tocVisible   },
        { win->hwndFavTree, win->favVisible                     },
    };

    /* // make sure that at least one element is available
    bool hasAvailable = false;
    for (int i = 0; i < dimof(tabOrder) && !hasAvailable; i++)
        hasAvailable = tabOrder[i].isAvailable;
    if (!hasAvailable)
        return;
    // */

    // find the currently focused element
    HWND current = GetFocus();
    int ix;
    for (ix = 0; ix < dimof(tabOrder); ix++)
        if (tabOrder[ix].hwnd == current)
            break;
    // if it's not in the tab order, start at the beginning
    if (ix == dimof(tabOrder))
        ix = -direction;

    // focus the next available element
    do {
        ix = (ix + direction + dimof(tabOrder)) % dimof(tabOrder);
    } while (!tabOrder[ix].isAvailable);
    SetFocus(tabOrder[ix].hwnd);
}

// allow to distinguish a '/' caused by VK_DIVIDE (rotates a document)
// from one typed on the main keyboard (focuses the find textbox)
static bool gIsDivideKeyDown = false;

static bool OnKeydown(WindowInfo& win, WPARAM key, LPARAM lparam, bool inTextfield=false)
{
    if ((VK_LEFT == key || VK_RIGHT == key) &&
        IsShiftPressed() && IsCtrlPressed() &&
        win.loadedFilePath && !inTextfield) {
        // folder browsing should also work when an error page is displayed,
        // so special-case it before the win.IsDocLoaded() check
        BrowseFolder(win, VK_RIGHT == key);
        return true;
    }

    if (!win.IsDocLoaded())
        return false;
    
    //DBG_OUT("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        return false;

    if (VK_PRIOR == key) {
        int currentPos = GetScrollPos(win.hwndCanvas, SB_VERT);
        if (win.dm->zoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        if (GetScrollPos(win.hwndCanvas, SB_VERT) == currentPos)
            win.dm->goToPrevPage(-1);
    } else if (VK_NEXT == key) {
        int currentPos = GetScrollPos(win.hwndCanvas, SB_VERT);
        if (win.dm->zoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        if (GetScrollPos(win.hwndCanvas, SB_VERT) == currentPos)
            win.dm->goToNextPage(0);
    } else if (VK_UP == key) {
        if (win.dm->needVScroll())
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        else
            win.dm->goToPrevPage(-1);
    } else if (VK_DOWN == key) {
        if (win.dm->needVScroll())
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        else
            win.dm->goToNextPage(0);
    } else if (inTextfield) {
        // The remaining keys have a different meaning
        return false;
    } else if (VK_LEFT == key) {
        if (win.dm->needHScroll())
            SendMessage(win.hwndCanvas, WM_HSCROLL, IsShiftPressed() ? SB_PAGELEFT : SB_LINELEFT, 0);
        else
            win.dm->goToPrevPage(0);
    } else if (VK_RIGHT == key) {
        if (win.dm->needHScroll())
            SendMessage(win.hwndCanvas, WM_HSCROLL, IsShiftPressed() ? SB_PAGERIGHT : SB_LINERIGHT, 0);
        else
            win.dm->goToNextPage(0);
    } else if (VK_HOME == key) {
        win.dm->goToFirstPage();
    } else if (VK_END == key) {
        win.dm->goToLastPage();
    } else if (VK_MULTIPLY == key) {
        win.dm->rotateBy(90);
    } else if (VK_DIVIDE == key) {
        win.dm->rotateBy(-90);
        gIsDivideKeyDown = true;
    } else {
        return false;
    }

    return true;
}

static void OnChar(WindowInfo& win, WPARAM key)
{
//    DBG_OUT("char=%d,%c\n", key, (char)key);

    if (IsCharUpper((TCHAR)key))
        key = (TCHAR)CharLower((LPTSTR)(TCHAR)key);

    if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation) {
        win.ChangePresentationMode(PM_ENABLED);
        return;
    }

    switch (key) {
    case VK_ESCAPE:
        if (win.findThread)
            win.AbortFinding();
        else if (win.messages->GetFirst(NG_PAGE_INFO_HELPER))
            win.messages->CleanUp(NG_PAGE_INFO_HELPER);
        else if (win.presentation)
            OnMenuViewPresentation(win);
        else if (gGlobalPrefs.escToExit)
            DestroyWindow(win.hwndFrame);
        else if (win.fullScreen)
            OnMenuViewFullscreen(win);
        else if (win.showSelection)
            ClearSearchResult(win);
        return;
    case 'q':
        if (!gPluginMode)
            DestroyWindow(win.hwndFrame);
        return;
    case 'r':
        win.Reload();
        return;
    case VK_TAB:
        AdvanceFocus(&win);
        break;
    }

    if (!win.IsDocLoaded())
        return;

    switch (key) {
    case VK_SPACE:
    case VK_RETURN:
        OnKeydown(win, IsShiftPressed() ? VK_PRIOR : VK_NEXT, 0);
        break;
    case VK_BACK:
        {
            bool forward = IsShiftPressed();
            win.dm->navigate(forward ? 1 : -1);
        }
        break;
    case 'g':
        OnMenuGoToPage(win);
        break;
    case 'j':
        SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        break;
    case 'k':
        SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        break;
    case 'n':
        win.dm->goToNextPage(0);
        break;
    case 'p':
        win.dm->goToPrevPage(0);
        break;
    case 'z':
        win.ToggleZoom();
        break;
    case '+':
        win.ZoomToSelection(ZOOM_IN_FACTOR, true);
        break;
    case '-':
        win.ZoomToSelection(ZOOM_OUT_FACTOR, true);
        break;
    case '/':
        if (!gIsDivideKeyDown)
            OnMenuFind(win);
        gIsDivideKeyDown = false;
        break;
    case 'c':
        OnMenuViewContinuous(win);
        break;
    case 'b':
        if (!displayModeSingle(win.dm->displayMode())) {
            // "e-book view": flip a single page
            bool forward = !IsShiftPressed();
            int currPage = win.dm->currentPageNo();
            if (forward ? win.dm->lastBookPageVisible() : win.dm->firstBookPageVisible())
                break;

            DisplayMode newMode = DM_BOOK_VIEW;
            if (displayModeShowCover(win.dm->displayMode()))
                newMode = DM_FACING;
            win.SwitchToDisplayMode(newMode, true);

            if (forward && currPage >= win.dm->currentPageNo() && (currPage > 1 || newMode == DM_BOOK_VIEW))
                win.dm->goToNextPage(0);
            else if (!forward && currPage <= win.dm->currentPageNo())
                win.dm->goToPrevPage(0);
        }
        break;
    case '.':
        // for Logitech's wireless presenters which target PowerPoint's shortcuts
        if (win.presentation)
            win.ChangePresentationMode(PM_BLACK_SCREEN);
        break;
    case 'w':
        if (win.presentation)
            win.ChangePresentationMode(PM_WHITE_SCREEN);
        break;
    case 'i':
        // experimental "page info" tip: make figuring out current page and
        // total pages count a one-key action (unless they're already visible)
        if (!gGlobalPrefs.showToolbar || win.fullScreen || PM_ENABLED == win.presentation) {
            int current = win.dm->currentPageNo(), total = win.dm->pageCount();
            ScopedMem<TCHAR> pageInfo(str::Format(_T("%s %d / %d"), _TR("Page:"), current, total));
            if (win.dm->engine && win.dm->engine->HasPageLabels()) {
                ScopedMem<TCHAR> label(win.dm->engine->GetPageLabel(current));
                pageInfo.Set(str::Format(_T("%s %s (%d / %d)"), _TR("Page:"), label, current, total));
            }
            bool autoDismiss = !IsShiftPressed();
            win.ShowNotification(pageInfo, autoDismiss, false, NG_PAGE_INFO_HELPER);
        }
        break;
#ifdef DEBUG
    case '$':
        ToggleGdiDebugging();
        break;
#endif
    }
}

class GoToTocLinkWorkItem : public UIThreadWorkItem
{
    DocToCItem *tocItem;

public:
    GoToTocLinkWorkItem(WindowInfo *win, DocToCItem *ti) :
        UIThreadWorkItem(win), tocItem(ti) {}

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->IsDocLoaded())
            win->linkHandler->GotoLink(tocItem->GetLink());
    }
};

static void GoToTocLinkForTVItem(WindowInfo* win, HWND hTV, HTREEITEM hItem=NULL, bool allowExternal=true)
{
    if (!hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);
    DocToCItem *tocItem = (DocToCItem *)item.lParam;
    if (win->IsDocLoaded() && tocItem &&
        (allowExternal || tocItem->GetLink() && str::Eq(tocItem->GetLink()->GetType(), "ScrollTo"))) {
        QueueWorkItem(new GoToTocLinkWorkItem(win, tocItem));
    }
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    TBBUTTON tbButton = {0};
    tbButton.idCommand = gToolbarButtons[i].cmdId;
    if (TbIsSeparator(gToolbarButtons[i])) {
        tbButton.fsStyle = TBSTYLE_SEP;
    } else {
        tbButton.iBitmap = gToolbarButtons[i].bmpIndex;
        tbButton.fsState = TBSTATE_ENABLED;
        tbButton.fsStyle = TBSTYLE_BUTTON;
        tbButton.iString = (INT_PTR)Trans::GetTranslation(gToolbarButtons[i].toolTip);
    }
    return tbButton;
}

#define WS_TOOLBAR (WS_CHILD | WS_CLIPSIBLINGS | \
                    TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | \
                    TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN)

static void BuildTBBUTTONINFO(TBBUTTONINFO& info, TCHAR *txt) {
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
    info.pszText = txt;
}

// Set toolbar button tooltips taking current language into account.
static void UpdateToolbarButtonsToolTipsForWindow(WindowInfo& win)
{
    TBBUTTONINFO buttonInfo;
    HWND hwnd = win.hwndToolbar;
    LRESULT res;
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        WPARAM buttonId = (WPARAM)i;
        const char *txt = gToolbarButtons[i].toolTip;
        if (NULL == txt)
            continue;
        const TCHAR *translation = Trans::GetTranslation(txt);
        BuildTBBUTTONINFO(buttonInfo, (TCHAR *)translation);
        res = SendMessage(hwnd, TB_SETBUTTONINFO, buttonId, (LPARAM)&buttonInfo);
        assert(0 != res);
    }
}

static void UpdateSidebarTitles(WindowInfo& win)
{
    HWND tocTitle = GetDlgItem(win.hwndTocBox, IDC_TOC_TITLE);
    win::SetText(tocTitle, _TR("Bookmarks"));
    if (win.tocVisible) {
        InvalidateRect(win.hwndTocBox, NULL, TRUE);
        UpdateWindow(win.hwndTocBox);
    }

    HWND favTitle = GetDlgItem(win.hwndFavBox, IDC_FAV_TITLE);
    win::SetText(favTitle, _T("Favorites")); // TODO: translate
    if (win.favVisible) {
        InvalidateRect(win.hwndFavBox, NULL, TRUE);
        UpdateWindow(win.hwndFavBox);
    }
}

// TODO: rename function to UpdateUITextForLanguage ?
static void UpdateToolbarToolText()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        UpdateToolbarPageText(*win, -1);
        UpdateToolbarFindText(*win);
        UpdateToolbarButtonsToolTipsForWindow(*win);
        // also update the sidebar title at this point
        UpdateSidebarTitles(*win);
    }        
}

static void RebuildMenuBar()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        HMENU oldMenu = win->menu;
        win->menu = BuildMenu(win);
        DestroyMenu(oldMenu);
    }
}

#define UWM_DELAYED_SET_FOCUS (WM_APP + 1)

// selects all text in an edit box if it's selected either
// through a keyboard shortcut or a non-selecting mouse click
static bool FocusUnselectedWndProc(HWND hwnd, UINT message)
{
    static bool delayFocus = false;

    switch (message) {
    case WM_LBUTTONDOWN:
        delayFocus = GetFocus() != hwnd;
        return true;

    case WM_LBUTTONUP:
        if (delayFocus) {
            DWORD sel = Edit_GetSel(hwnd);
            if (LOWORD(sel) == HIWORD(sel))
                PostMessage(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
            delayFocus = false;
        }
        return true;

    case WM_SETFOCUS:
        if (!delayFocus)
            PostMessage(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
        return true;

    case UWM_DELAYED_SET_FOCUS:
        Edit_SelectAll(hwnd);
        return true;

    default:
        return false;
    }
}

static WNDPROC DefWndProcFindBox = NULL;
static LRESULT CALLBACK WndProcFindBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded())
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (FocusUnselectedWndProc(hwnd, message)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == message) {
        switch (wParam) {
        case VK_ESCAPE:
            if (win->findThread)
                win->AbortFinding();
            else
                SetFocus(win->hwndFrame);
            return 1;

        case VK_RETURN:
            FindTextOnThread(win);
            return 1;

        case VK_TAB:
            AdvanceFocus(win);
            return 1;
        }
    }
    else if (WM_ERASEBKGND == message) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    }
    else if (WM_KEYDOWN == message) {
        if (OnKeydown(*win, wParam, lParam, true))
            return 0;
    }

    LRESULT ret = CallWindowProc(DefWndProcFindBox, hwnd, message, wParam, lParam);

    if (WM_CHAR  == message ||
        WM_PASTE == message ||
        WM_CUT   == message ||
        WM_CLEAR == message ||
        WM_UNDO  == message) {
        ToolbarUpdateStateForWindow(*win);
    }

    return ret;
}

static void ShowSearchResult(WindowInfo& win, TextSel *result, bool addNavPt)
{
    assert(result->len > 0);
    if (addNavPt || !win.dm->pageShown(result->pages[0]) ||
        (win.dm->zoomVirtual() == ZOOM_FIT_PAGE || win.dm->zoomVirtual() == ZOOM_FIT_CONTENT))
        win.dm->goToPage(result->pages[0], 0, addNavPt);

    win.dm->textSelection->CopySelection(win.dm->textSearch);

    UpdateTextSelection(win, false);
    win.dm->ShowResultRectToScreen(result);
    win.RepaintAsync();
}

static void ClearSearchResult(WindowInfo& win)
{
    DeleteOldSelectionInfo(win, true);
    win.RepaintAsync();
}

class UpdateFindStatusWorkItem : public UIThreadWorkItem {
    MessageWnd *wnd;
    int current, total;

public:
    UpdateFindStatusWorkItem(WindowInfo *win, MessageWnd *wnd, int current, int total)
        : UIThreadWorkItem(win), wnd(wnd), current(current), total(total) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && !win->findCanceled && win->messages->Contains(wnd))
            wnd->ProgressUpdate(current, total);
    }
};

struct FindThreadData : public ProgressUpdateUI {
    WindowInfo *win;
    TextSearchDirection direction;
    bool wasModified;
    TCHAR *text;

    FindThreadData(WindowInfo& win, TextSearchDirection direction, HWND findBox) :
        win(&win), direction(direction) {
        text = win::GetText(findBox);
        wasModified = Edit_GetModify(findBox);
    }
    ~FindThreadData() { free(text); }

    void ShowUI() const {
        const LPARAM disable = (LPARAM)MAKELONG(0, 0);

        MessageWnd *wnd = new MessageWnd(win->hwndCanvas, _T(""), _TR("Searching %d of %d..."), win->messages);
        // let win->messages own the MessageWnd (FindThreadData might get deleted before)
        win->messages->Add(wnd, NG_FIND_PROGRESS);

        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, disable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, disable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, disable);
    }

    void HideUI(MessageWnd *wnd, bool success, bool loopedAround) const {
        LPARAM enable = (LPARAM)MAKELONG(1, 0);

        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, enable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, enable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, enable);

        if (!success && !loopedAround || !wnd) // i.e. canceled
            win->messages->CleanUp(wnd);
        else if (!success && loopedAround)
            wnd->MessageUpdate(_TR("No matches were found"), 3000);
        else if (!loopedAround) {
            ScopedMem<TCHAR> buf(str::Format(_TR("Found text at page %d"), win->dm->currentPageNo()));
            wnd->MessageUpdate(buf, 3000);
        } else {
            ScopedMem<TCHAR> buf(str::Format(_TR("Found text at page %d (again)"), win->dm->currentPageNo()));
            wnd->MessageUpdate(buf, 3000, true);
        }    
    }

    virtual bool ProgressUpdate(int current, int total) {
        if (!WindowInfoStillValid(win) || !win->messages->GetFirst(NG_FIND_PROGRESS) || win->findCanceled)
            return false;
        QueueWorkItem(new UpdateFindStatusWorkItem(win, win->messages->GetFirst(NG_FIND_PROGRESS), current, total));
        return true;
    }
};

class FindEndWorkItem : public UIThreadWorkItem {
    FindThreadData *ftd;
    TextSel*textSel;
    bool    wasModifiedCanceled;
    bool    loopedAround;

public:
    FindEndWorkItem(WindowInfo *win, FindThreadData *ftd, TextSel *textSel,
                    bool wasModifiedCanceled, bool loopedAround=false) :
        UIThreadWorkItem(win), ftd(ftd), textSel(textSel),
        loopedAround(loopedAround), wasModifiedCanceled(wasModifiedCanceled) { }
    ~FindEndWorkItem() { delete ftd; }

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (!win->IsDocLoaded()) {
            // the UI has already been disabled and hidden
        } else if (textSel) {
            ShowSearchResult(*win, textSel, wasModifiedCanceled);
            ftd->HideUI(win->messages->GetFirst(NG_FIND_PROGRESS), true, loopedAround);
        } else {
            // nothing found or search canceled
            ClearSearchResult(*win);
            ftd->HideUI(win->messages->GetFirst(NG_FIND_PROGRESS), false, !wasModifiedCanceled);
        }

        HANDLE hThread = win->findThread;
        win->findThread = NULL;
        CloseHandle(hThread);
    }
};

static DWORD WINAPI FindThread(LPVOID data)
{
    FindThreadData *ftd = (FindThreadData *)data;
    assert(ftd && ftd->win && ftd->win->dm);
    WindowInfo *win = ftd->win;

    TextSel *rect;
    win->dm->textSearch->SetDirection(ftd->direction);
    if (ftd->wasModified || !win->dm->validPageNo(win->dm->textSearch->GetCurrentPageNo()) ||
        !win->dm->getPageInfo(win->dm->textSearch->GetCurrentPageNo())->visibleRatio)
        rect = win->dm->textSearch->FindFirst(win->dm->currentPageNo(), ftd->text, ftd);
    else
        rect = win->dm->textSearch->FindNext(ftd);

    bool loopedAround = false;
    if (!win->findCanceled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (FIND_FORWARD == ftd->direction) ? 1 : win->dm->pageCount();
        if (!ftd->wasModified || win->dm->currentPageNo() != startPage) {
            loopedAround = true;
            MessageBeep(MB_ICONINFORMATION);
            rect = win->dm->textSearch->FindFirst(startPage, ftd->text, ftd);
        }
    }

    if (!win->findCanceled && rect)
        QueueWorkItem(new FindEndWorkItem(win, ftd, rect, ftd->wasModified, loopedAround));
    else
        QueueWorkItem(new FindEndWorkItem(win, ftd, NULL, win->findCanceled));

    return 0;
}

void FindTextOnThread(WindowInfo* win, TextSearchDirection direction)
{
    win->AbortFinding(true);

    FindThreadData *ftd = new FindThreadData(*win, direction, win->hwndFindBox);
    Edit_SetModify(win->hwndFindBox, FALSE);

    if (str::IsEmpty(ftd->text)) {
        delete ftd;
        return;
    }

    ftd->ShowUI();
    win->findThread = CreateThread(NULL, 0, FindThread, ftd, 0, 0);
}

static WNDPROC DefWndProcToolbar = NULL;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (WM_CTLCOLORSTATIC == message) {
        SetBkMode((HDC)wParam, TRANSPARENT);
        SelectBrush((HDC)wParam, GetStockBrush(NULL_BRUSH));
        return 0;
    }
    return CallWindowProc(DefWndProcToolbar, hwnd, message, wParam, lParam);
}

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SIZE TextSizeInHwnd(HWND hwnd, const TCHAR *txt)
{
    SIZE sz;
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ prev = SelectObject(dc, f);
    GetTextExtentPoint32(dc, txt, (int)txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return sz;
}

#define TOOLBAR_MIN_ICON_SIZE 16
#define FIND_BOX_WIDTH 160

// Note: a bit of a hack, but doing just ShowWindow(..., SW_HIDE | SW_SHOW)
// didn't work for me
static void MoveOffScreen(HWND hwnd)
{
    WindowRect r(hwnd);
    MoveWindow(hwnd, -200, -100, r.dx, r.dy, FALSE);
    ShowWindow(hwnd, SW_HIDE);
}

static void HideToolbarFindUI(WindowInfo& win)
{
    MoveOffScreen(win.hwndFindText);
    MoveOffScreen(win.hwndFindBg);
    MoveOffScreen(win.hwndFindBox);
}

static void UpdateToolbarFindText(WindowInfo& win)
{
    if (!NeedsFindUI(win)) {
        HideToolbarFindUI(win);
        return;
    }

    ShowWindow(win.hwndFindText, SW_SHOW);
    ShowWindow(win.hwndFindBg, SW_SHOW);
    ShowWindow(win.hwndFindBox, SW_SHOW);

    const TCHAR *text = _TR("Find:");
    win::SetText(win.hwndFindText, text);

    WindowRect findWndRect(win.hwndFindBg);

    RECT r;
    SendMessage(win.hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - findWndRect.dy) / 2;

    SIZE size = TextSizeInHwnd(win.hwndFindText, text);
    size.cx += 6;

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win.hwndFindText, pos_x, (findWndRect.dy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, TRUE);
    MoveWindow(win.hwndFindBg, pos_x + size.cx, pos_y, findWndRect.dx, findWndRect.dy, FALSE);
    MoveWindow(win.hwndFindBox, pos_x + size.cx + padding, (findWndRect.dy - size.cy + 1) / 2 + pos_y,
        findWndRect.dx - 2 * padding, size.cy, FALSE);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.cx + findWndRect.dx + 12);
    SendMessage(win.hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

static void CreateFindBox(WindowInfo& win)
{
    HWND findBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(FIND_BOX_WIDTH * win.uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 4),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND find = CreateWindowEx(0, WC_EDIT, _T(""), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                            0, 1, (int)(FIND_BOX_WIDTH * win.uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 2),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    SetWindowFont(label, gDefaultGuiFont, FALSE);
    SetWindowFont(find, gDefaultGuiFont, FALSE);

    if (!DefWndProcToolbar)
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win.hwndToolbar, GWLP_WNDPROC);
    SetWindowLongPtr(win.hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcFindBox);

    win.hwndFindText = label;
    win.hwndFindBox = find;
    win.hwndFindBg = findBg;

    UpdateToolbarFindText(win);
}

static WNDPROC DefWndProcPageBox = NULL;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded())
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (FocusUnselectedWndProc(hwnd, message)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == message) {
        switch (wParam) {
        case VK_RETURN: {
            ScopedMem<TCHAR> buf(win::GetText(win->hwndPageBox));
            int newPageNo = win->dm->engine->GetPageByLabel(buf);
            if (win->dm->validPageNo(newPageNo)) {
                win->dm->goToPage(newPageNo, 0, true);
                SetFocus(win->hwndFrame);
            }
            return 1;
        }
        case VK_ESCAPE:
            SetFocus(win->hwndFrame);
            return 1;

        case VK_TAB:
            AdvanceFocus(win);
            return 1;
        }
    } else if (WM_ERASEBKGND == message) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    } else if (WM_KEYDOWN == message) {
        if (OnKeydown(*win, wParam, lParam, true))
            return 0;
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, message, wParam, lParam);
}

#define PAGE_BOX_WIDTH 40
void UpdateToolbarPageText(WindowInfo& win, int pageCount)
{
    const TCHAR *text = _TR("Page:");
    win::SetText(win.hwndPageText, text);
    SIZE size = TextSizeInHwnd(win.hwndPageText, text);
    size.cx += 6;

    WindowRect pageWndRect(win.hwndPageBg);

    RECT r;
    SendMessage(win.hwndToolbar, TB_GETRECT, IDM_PRINT, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - pageWndRect.dy) / 2;

    TCHAR *buf;
    SIZE size2 = { 0 };
    if (-1 == pageCount)
        buf = win::GetText(win.hwndPageTotal);
    else if (!pageCount)
        buf = str::Dup(_T(""));
    else if (!win.dm || !win.dm->engine || !win.dm->engine->HasPageLabels())
        buf = str::Format(_T(" / %d"), pageCount);
    else {
        buf = str::Format(_T(" (%d / %d)"), win.dm->currentPageNo(), pageCount);
        ScopedMem<TCHAR> buf2(str::Format(_T(" (%d / %d)"), pageCount, pageCount));
        size2 = TextSizeInHwnd(win.hwndPageTotal, buf2);
    }

    win::SetText(win.hwndPageTotal, buf);
    if (0 == size2.cx)
        size2 = TextSizeInHwnd(win.hwndPageTotal, buf);
    size2.cx += 6;
    free(buf);

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win.hwndPageText, pos_x, (pageWndRect.dy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    if (IsUIRightToLeft())
        pos_x += size2.cx - 6;
    MoveWindow(win.hwndPageBg, pos_x + size.cx, pos_y, pageWndRect.dx, pageWndRect.dy, false);
    MoveWindow(win.hwndPageBox, pos_x + size.cx + padding, (pageWndRect.dy - size.cy + 1) / 2 + pos_y,
        pageWndRect.dx - 2 * padding, size.cy, false);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRightToLeft()) {
        pos_x -= size2.cx;
        MoveWindow(win.hwndPageTotal, pos_x + size.cx, (pageWndRect.dy - size.cy + 1) / 2 + pos_y, size2.cx, size.cy, false);
    }
    else
        MoveWindow(win.hwndPageTotal, pos_x + size.cx + pageWndRect.dx, (pageWndRect.dy - size.cy + 1) / 2 + pos_y, size2.cx, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.cx + pageWndRect.dx + size2.cx + 12);
    SendMessage(win.hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
}

static void CreatePageBox(WindowInfo& win)
{
    HWND pageBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(PAGE_BOX_WIDTH * win.uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 4),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND page = CreateWindowEx(0, WC_EDIT, _T("0"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                            0, 1, (int)(PAGE_BOX_WIDTH * win.uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 2),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND total = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    SetWindowFont(label, gDefaultGuiFont, FALSE);
    SetWindowFont(page, gDefaultGuiFont, FALSE);
    SetWindowFont(total, gDefaultGuiFont, FALSE);

    if (!DefWndProcPageBox)
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win.hwndPageText = label;
    win.hwndPageBox = page;
    win.hwndPageBg = pageBg;
    win.hwndPageTotal = total;

    UpdateToolbarPageText(win, -1);
}

static HBITMAP LoadExternalBitmap(HINSTANCE hInst, TCHAR * filename, INT resourceId)
{
    ScopedMem<TCHAR> path(AppGenDataFilename(filename));
    
    HBITMAP hBmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hBmp)
        hBmp = LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));
    return hBmp;
}

static void CreateToolbar(WindowInfo& win) {
    HWND hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_TOOLBAR,
                                 0,0,0,0, win.hwndFrame,(HMENU)IDC_TOOLBAR, ghinst, NULL);
    win.hwndToolbar = hwndToolbar;
    LRESULT lres = SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    ShowWindow(hwndToolbar, SW_SHOW);
    TBBUTTON tbButtons[TOOLBAR_BUTTONS_COUNT];

    // the name of the bitmap contains the number of icons so that after adding/removing
    // icons a complete default toolbar is used rather than an incomplete customized one
    HBITMAP hbmp = LoadExternalBitmap(ghinst, _T("toolbar_11.bmp"), IDB_TOOLBAR);
    BITMAP bmp;
    GetObject(hbmp, sizeof(BITMAP), &bmp);
    // stretch the toolbar bitmaps for higher DPI settings
    // TODO: get nicely interpolated versions of the toolbar icons for higher resolutions
    if (bmp.bmHeight < TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor) {
        bmp.bmWidth *= (LONG)(win.uiDPIFactor + 0.5f);
        bmp.bmHeight *= (LONG)(win.uiDPIFactor + 0.5f);
        hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, bmp.bmWidth, bmp.bmHeight, LR_COPYDELETEORG);
    }
    // Assume square icons
    HIMAGELIST himl = ImageList_Create(bmp.bmHeight, bmp.bmHeight, ILC_COLORDDB | ILC_MASK, 0, 0);
    ImageList_AddMasked(himl, hbmp, RGB(255, 0, 255));
    DeleteObject(hbmp);

    // in Plugin mode, replace the Open with a Save As button
    if (gPluginMode && bmp.bmWidth / bmp.bmHeight == 13) {
        gToolbarButtons[0].bmpIndex = 12;
        gToolbarButtons[0].cmdId = IDM_SAVEAS;
        gToolbarButtons[0].toolTip = _TRN("Save As");
        gToolbarButtons[0].flags = MF_REQ_DISK_ACCESS;
    }
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        tbButtons[i] = TbButtonFromButtonInfo(i);
        if (gToolbarButtons[i].cmdId == IDM_FIND_MATCH)
            tbButtons[i].fsStyle = BTNS_CHECK;
    }
    lres = SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    LRESULT exstyle = SendMessage(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    lres = SendMessage(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    lres = SendMessage(hwndToolbar, TB_ADDBUTTONS, TOOLBAR_BUTTONS_COUNT, (LPARAM)tbButtons);

    RECT rc;
    lres = SendMessage(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);

    DWORD  reBarStyle = WS_REBAR | WS_VISIBLE;
    win.hwndReBar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL, reBarStyle,
                             0,0,0,0, win.hwndFrame, (HMENU)IDC_REBAR, ghinst, NULL);
    if (!win.hwndReBar)
        SeeLastError();

    REBARINFO rbi;
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    lres = SendMessage(win.hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    REBARBANDINFO rbBand;
    rbBand.cbSize  = sizeof(REBARBANDINFO);
    rbBand.fMask   = /*RBBIM_COLORS | RBBIM_TEXT | RBBIM_BACKGROUND | */
                   RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE /*| RBBIM_SIZE*/;
    rbBand.fStyle  = /*RBBS_CHILDEDGE |*//* RBBS_BREAK |*/ RBBS_FIXEDSIZE /*| RBBS_GRIPPERALWAYS*/;
    if (IsAppThemed())
        rbBand.fStyle |= RBBS_CHILDEDGE;
    rbBand.hbmBack = NULL;
    rbBand.lpText     = _T("Toolbar");
    rbBand.hwndChild  = hwndToolbar;
    rbBand.cxMinChild = (rc.right - rc.left) * TOOLBAR_BUTTONS_COUNT;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx         = 0;
    lres = SendMessage(win.hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win.hwndReBar, NULL, 0, 0, 0, 0, SWP_NOZORDER);
    
    CreatePageBox(win);
    CreateFindBox(win);
}

// TODO: the layout logic here is similar to what we do in SetSidebarVisibility()
// would be nice to consolidate.
static void ResizeSidebar(WindowInfo *win)
{
    POINT pcur;
    GetCursorPos(&pcur);
    ScreenToClient(win->hwndFrame, &pcur);
    int sidebarDx = pcur.x; // without splitter

    ClientRect rToc(win->hwndTocBox);
    ClientRect rFav(win->hwndFavBox);
    assert(rToc.dx == rFav.dx);
    ClientRect rFrame(win->hwndFrame);

    // make sure to keep this in sync with the calculations in SetSidebarVisibility
    // note: without the min/max(..., rToc.dx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    if (sidebarDx < min(SIDEBAR_MIN_WIDTH, rToc.dx) ||
        sidebarDx > max(rFrame.dx / 2, rToc.dx)) {
        SetCursor(gCursorNo);
        return;
    }

    SetCursor(gCursorSizeWE);

    int canvasDx = rFrame.dx - sidebarDx - SPLITTER_DX;
    int y = 0;
    int totalDy = rFrame.dy;
    if (gGlobalPrefs.showToolbar && !win->fullScreen && !win->presentation) {
        y = WindowRect(win->hwndReBar).dy;
        totalDy -= y;
    }
    // rToc.y is always 0, as rToc is a ClientRect, so we first have
    // to convert it into coordinates relative to hwndFrame:
    assert(MapRectToWindow(rToc, win->hwndTocBox, win->hwndFrame).y == y);
    //assert(totalDy == (rToc.dy + rFav.dy));

    MoveWindow(win->hwndTocBox, 0, y,           sidebarDx, rToc.dy, true);
    MoveWindow(win->hwndFavBox, 0, y + rToc.dy, sidebarDx, rFav.dy, true);
    MoveWindow(win->hwndSidebarSpliter, sidebarDx, y, SPLITTER_DX, totalDy, true);
    MoveWindow(win->hwndCanvas, sidebarDx + SPLITTER_DX, y, canvasDx, totalDy, true);
}

static LRESULT CALLBACK WndProcSpliter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case WM_MOUSEMOVE:
            if (win->sidebarBeingResized) {
                ResizeSidebar(win);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            win->sidebarBeingResized = true;
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            win->sidebarBeingResized = false;
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void TreeView_ExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree=false)
{
    while (hItem) {
        TreeView_Expand(hTree, hItem, flag);
        HTREEITEM child = TreeView_GetChild(hTree, hItem);
        if (child)
            TreeView_ExpandRecursively(hTree, child, flag);
        if (subtree)
            break;
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC     (WM_APP + 1)
#endif

static WNDPROC DefWndProcTocTree = NULL;
static LRESULT CALLBACK WndProcTocTree(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcTocTree, hwnd, message, wParam, lParam);

    switch (message) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.escToExit)
                DestroyWindow(win->hwndFrame);
            break;
        case WM_KEYDOWN:
            // consistently expand/collapse whole (sub)trees
            if (VK_MULTIPLY == wParam && IsShiftPressed())
                TreeView_ExpandRecursively(hwnd, TreeView_GetRoot(hwnd), TVE_EXPAND);
            else if (VK_MULTIPLY == wParam)
                TreeView_ExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_EXPAND, true);
            else if (VK_DIVIDE == wParam && IsShiftPressed()) {
                HTREEITEM root = TreeView_GetRoot(hwnd);
                if (!TreeView_GetNextSibling(hwnd, root))
                    root = TreeView_GetChild(hwnd, root);
                TreeView_ExpandRecursively(hwnd, root, TVE_COLLAPSE);
            }
            else if (VK_DIVIDE == wParam)
                TreeView_ExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_COLLAPSE, true);
            else
                break;
            TreeView_EnsureVisible(hwnd, TreeView_GetSelection(hwnd));
            return 0;
        case WM_MOUSEWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->hwndTocTree))
                return SendMessage(win->hwndCanvas, message, wParam, lParam);
            break;
#ifdef DISPLAY_TOC_PAGE_NUMBERS
        case WM_SIZE:
        case WM_HSCROLL:
            // Repaint the ToC so that RelayoutTocItem is called for all items
            PostMessage(hwnd, WM_APP_REPAINT_TOC, 0, 0);
            break;
        case WM_APP_REPAINT_TOC:
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
            break;
#endif
    }
    return CallWindowProc(DefWndProcTocTree, hwnd, message, wParam, lParam);
}

static void CustomizeToCInfoTip(LPNMTVGETINFOTIP nmit);
#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd);
#endif

// A tree container, used for toc and favorites, is a window with following children:
// - title (id + 1)
// - close button (id + 2)
// - tree window (id + 3)
// This function lays out the child windows inside the container based
// on the container size.
static void LayoutTreeContainer(HWND hwndContainer, int id)
{
    HWND hwndTitle = GetDlgItem(hwndContainer, id + 1);
    HWND hwndClose = GetDlgItem(hwndContainer, id + 2);
    HWND hwndTree  = GetDlgItem(hwndContainer, id + 3);

    ScopedMem<TCHAR> title(win::GetText(hwndTitle));
    SIZE size = TextSizeInHwnd(hwndTitle, title);

    WindowInfo *win = FindWindowInfoByHwnd(hwndContainer);
    assert(win);
    int offset = win ? (int)(2 * win->uiDPIFactor) : 2;
    if (size.cy < 16)
        size.cy = 16;
    size.cy += 2 * offset;

    WindowRect rc(hwndContainer);   
    MoveWindow(hwndTitle, offset, offset, rc.dx - 2 * offset - 16, size.cy - 2 * offset, true);
    MoveWindow(hwndClose, rc.dx - 16, (size.cy - 16) / 2, 16, 16, true);
    MoveWindow(hwndTree, 0, size.cy, rc.dx, rc.dy - size.cy, true);
}

static WNDPROC DefWndProcTocBox = NULL;

static LRESULT OnTocTreeNotify(WindowInfo *win, LPNMTREEVIEW pnmtv)
{
    switch (pnmtv->hdr.code) 
    {
        case TVN_SELCHANGED: 
            // When the focus is set to the toc window the first item in the treeview is automatically
            // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action == 0x00001000.
            // We have to ignore this message to prevent the current page to be changed.
            if (TVC_BYKEYBOARD == pnmtv->action || TVC_BYMOUSE == pnmtv->action)
                GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom, pnmtv->itemNew.hItem, TVC_BYMOUSE == pnmtv->action);
            // The case pnmtv->action==TVC_UNKNOWN is ignored because 
            // it corresponds to a notification sent by
            // the function TreeView_DeleteAllItems after deletion of the item.
            break;

        case TVN_KEYDOWN: {
            TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)pnmtv;
            if (VK_TAB == ptvkd->wVKey) {
                AdvanceFocus(win);
                return 1;
            }
            break;
        }
        case NM_CLICK: {
            // Determine which item has been clicked (if any)
            TVHITTESTINFO ht = {0};
            DWORD pos = GetMessagePos();
            ht.pt.x = GET_X_LPARAM(pos);
            ht.pt.y = GET_Y_LPARAM(pos);
            MapWindowPoints(HWND_DESKTOP, pnmtv->hdr.hwndFrom, &ht.pt, 1);
            TreeView_HitTest(pnmtv->hdr.hwndFrom, &ht);

            // let TVN_SELCHANGED handle the click, if it isn't on the already selected item
            if ((ht.flags & TVHT_ONITEM) && TreeView_GetSelection(pnmtv->hdr.hwndFrom) == ht.hItem)
                GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom, ht.hItem);
            break;
        }
        case NM_RETURN:
            GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom);
            break;

        case NM_CUSTOMDRAW:
#ifdef DISPLAY_TOC_PAGE_NUMBERS
            switch (((LPNMCUSTOMDRAW)pnmtv)->dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    return CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
                case CDDS_ITEMPOSTPAINT:
                    RelayoutTocItem((LPNMTVCUSTOMDRAW)pnmtv);
                    // fall through
                default:
                    return CDRF_DODEFAULT;
            }
            break;
#else
            return CDRF_DODEFAULT;
#endif

        case TVN_GETINFOTIP:
            CustomizeToCInfoTip((LPNMTVGETINFOTIP)pnmtv);
            break;
    }
    return -1;
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcTocBox, hwnd, message, wParam, lParam);

    switch (message) {
        case WM_SIZE:
            LayoutTreeContainer(hwnd, IDC_TOC_BOX);
            break;

        case WM_DRAWITEM:
            if (IDC_TOC_CLOSE == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawFrameControl(dis->hDC, &dis->rcItem, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (HIWORD(wParam) == STN_CLICKED)
                ToggleTocBox(win);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_TOC_TREE) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
                LRESULT res = OnTocTreeNotify(win, pnmtv);
                if (res != -1)
                    return res;
            }
            break;
    }
    return CallWindowProc(DefWndProcTocBox, hwnd, message, wParam, lParam);
}

static WNDPROC DefWndProcFavTree = NULL;
static LRESULT CALLBACK WndProcFavTree(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcFavTree, hwnd, message, wParam, lParam);

    switch (message) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.escToExit)
                DestroyWindow(win->hwndFrame);
            break;
        case WM_MOUSEWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->hwndFavTree))
                return SendMessage(win->hwndCanvas, message, wParam, lParam);
            break;
    }

    return CallWindowProc(DefWndProcFavTree, hwnd, message, wParam, lParam);
}

class GoToFavoriteWorkItem : public UIThreadWorkItem
{
    int pageNo;

public:
    GoToFavoriteWorkItem(WindowInfo *win, int pageNo) :
        UIThreadWorkItem(win), pageNo(pageNo) {}

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (win->IsDocLoaded())
            win->dm->goToPage(pageNo, 0);
        SetForegroundWindow(win->hwndFrame);
    }
};

// Going to a bookmark within current file scrolls to a given page.
// Going to a bookmark in another file, loads the file and scrolls to a page
// (similar to how invoking one of the recently opened files works)
static void GoToFavorite(WindowInfo *win, FileFavs *f, FavName *fn)
{
    WindowInfo *existingWin = FindWindowInfoByFile(f->filePath);
    if (existingWin)
        win = existingWin;
    else
        win = LoadDocument(f->filePath, win);
    if (win)
        QueueWorkItem(new GoToFavoriteWorkItem(win, fn->pageNo));
}

static void GoToFavoriteByMenuId(WindowInfo *win, int wmId)
{
    size_t idx;
    FileFavs *f = gFavorites->GetByMenuId(wmId, idx);
    FavName *fn = f->favNames.At(idx);
    GoToFavorite(win, f, fn);
}

static void GoToFavForTVItem(WindowInfo* win, HWND hTV, HTREEITEM hItem=NULL)
{
    if (NULL == hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);

    FavName *fn = (FavName*)item.lParam;
    if (!fn)
        return;
    FileFavs *f = gFavorites->GetByFavName(fn);
    GoToFavorite(win, f, fn);
}

static void RememberFavTreeExpansionState(WindowInfo *win)
{
    win->expandedFavorites.Reset();
    // TODO: write me

/*
    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(hwndTocTree, &item);

        // add the ids of toggled items to tocState
        DocToCItem *tocItem = item.lParam ? (DocToCItem *)item.lParam : NULL;
        bool wasExpanded = tocItem && !(item.state & TVIS_EXPANDED) == tocItem->open;
        if (wasExpanded)
            tocState.Append(tocItem->id);

        if (tocItem && tocItem->child)
            UpdateToCExpansionState(TreeView_GetChild(hwndTocTree, hItem));
        hItem = TreeView_GetNextSibling(hwndTocTree, hItem);
    }
    */
}

static LRESULT OnFavTreeNotify(WindowInfo *win, LPNMTREEVIEW pnmtv)
{
    switch (pnmtv->hdr.code) 
    {
        // TVN_SELCHANGED intentionally not implemented (mouse clicks are handled
        // in NM_CLICK, and keyboard navigation in NM_RETURN and TVN_KEYDOWN)

        case TVN_KEYDOWN: {
            TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)pnmtv;
            if (VK_TAB == ptvkd->wVKey) {
                AdvanceFocus(win);
                return 1;
            }
            break;
        }

        case NM_CLICK: {
            // Determine which item has been clicked (if any)
            TVHITTESTINFO ht = {0};
            DWORD pos = GetMessagePos();
            ht.pt.x = GET_X_LPARAM(pos);
            ht.pt.y = GET_Y_LPARAM(pos);
            MapWindowPoints(HWND_DESKTOP, pnmtv->hdr.hwndFrom, &ht.pt, 1);
            TreeView_HitTest(pnmtv->hdr.hwndFrom, &ht);

            GoToFavForTVItem(win, pnmtv->hdr.hwndFrom, ht.hItem);
            break;
        }

        case NM_RETURN:
            GoToFavForTVItem(win, pnmtv->hdr.hwndFrom);
            break;

        case NM_CUSTOMDRAW:
            return CDRF_DODEFAULT;
    }
    return -1;
}

static WNDPROC DefWndProcFavBox = NULL;
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
    switch (message) {

        case WM_SIZE:
            LayoutTreeContainer(hwnd, IDC_FAV_BOX);
            break;

        case WM_DRAWITEM:
            if (IDC_FAV_CLOSE == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawFrameControl(dis->hDC, &dis->rcItem, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (HIWORD(wParam) == STN_CLICKED)
                ToggleFavorites(win);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_FAV_TREE) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
                LRESULT res = OnFavTreeNotify(win, pnmtv);
                if (res != -1)
                    return res;
            }
            break;
    }
    return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
}

static void CreateSidebar(WindowInfo* win)
{
    HWND tmp, title, hwndClose;

    // toc windows
    tmp = CreateWindow(SPLITER_CLASS_NAME, _T(""), WS_CHILDWINDOW, 0, 0, 0, 0,
                       win->hwndFrame, (HMENU)0, ghinst, NULL);
    win->hwndSidebarSpliter = tmp;
    win->hwndTocBox = CreateWindow(WC_STATIC, _T(""), WS_CHILD,
                        0,0,gGlobalPrefs.sidebarDx,0, win->hwndFrame, (HMENU)0, ghinst, NULL);
    title = CreateWindow(WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                         0,0,0,0, win->hwndTocBox, (HMENU)IDC_TOC_TITLE, ghinst, NULL);
    SetWindowFont(title, gDefaultGuiFont, FALSE);
    win::SetText(title, _TR("Bookmarks"));
    hwndClose = CreateWindow(WC_STATIC, _T(""),
                        SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                        0, 0, 16, 16, win->hwndTocBox, (HMENU)IDC_TOC_CLOSE, ghinst, NULL);
    SetClassLongPtr(hwndClose, GCLP_HCURSOR, (LONG_PTR)gCursorHand);

    win->hwndTocTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("TOC"),
                        TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                        TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                        WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                        0,0,0,0, win->hwndTocBox, (HMENU)IDC_TOC_TREE, ghinst, NULL);

    // Note: those must be consecutive numbers and in title/close/tree order
    CASSERT(IDC_TOC_BOX + 1 == IDC_TOC_TITLE &&
            IDC_TOC_BOX + 2 == IDC_TOC_CLOSE &&
            IDC_TOC_BOX + 3 == IDC_TOC_TREE, consecutive_toc_ids);

#ifdef UNICODE
    TreeView_SetUnicodeFormat(win->hwndTocTree, true);
#endif

    if (NULL == DefWndProcTocTree)
        DefWndProcTocTree = (WNDPROC)GetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC, (LONG_PTR)WndProcTocTree);

    if (NULL == DefWndProcTocBox)
        DefWndProcTocBox = (WNDPROC)GetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC, (LONG_PTR)WndProcTocBox);

    // favorites windows
    win->hwndFavBox = CreateWindow(WC_STATIC, _T(""), WS_CHILD,
                        0,0,gGlobalPrefs.sidebarDx,0, win->hwndFrame, (HMENU)0, ghinst, NULL);
    title = CreateWindow(WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                         0,0,0,0, win->hwndFavBox, (HMENU)IDC_FAV_TITLE, ghinst, NULL);
    SetWindowFont(title, gDefaultGuiFont, FALSE);
    win::SetText(title, _T("Favorites")); // TODO: translate
    hwndClose = CreateWindow(WC_STATIC, _T(""),
                        SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                        0, 0, 16, 16, win->hwndFavBox, (HMENU)IDC_FAV_CLOSE, ghinst, NULL);
    SetClassLongPtr(hwndClose, GCLP_HCURSOR, (LONG_PTR)gCursorHand);

    win->hwndFavTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("Fav"),
                        TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                        TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                        WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                        0,0,0,0, win->hwndFavBox, (HMENU)IDC_FAV_TREE, ghinst, NULL);
    
    // Note: those must be consecutive numbers and in title/close/tree order
    CASSERT(IDC_FAV_BOX + 1 == IDC_FAV_TITLE &&
            IDC_FAV_BOX + 2 == IDC_FAV_CLOSE &&
            IDC_FAV_BOX + 3 == IDC_FAV_TREE, consecutive_fav_ids);

#ifdef UNICODE
    TreeView_SetUnicodeFormat(win->hwndFavTree, true);
#endif

    if (NULL == DefWndProcFavTree)
        DefWndProcFavTree = (WNDPROC)GetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC, (LONG_PTR)WndProcFavTree);

    if (NULL == DefWndProcFavBox)
        DefWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    if (win->tocVisible) {
        InvalidateRect(win->hwndTocBox, NULL, TRUE);
        UpdateWindow(win->hwndTocBox);
    }

    if (win->favVisible) {
        InvalidateRect(win->hwndFavBox, NULL, TRUE);
        UpdateWindow(win->hwndFavBox);
    }
}

static HTREEITEM AddTocItemToView(HWND hwnd, DocToCItem *entry, HTREEITEM parent, bool expandItem)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvinsert.itemex.state = entry->open != expandItem ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)entry;
    // Replace unprintable whitespace with regular spaces
    str::NormalizeWS(entry->title);
    tvinsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (entry->pageNo && win && win->IsDocLoaded() && win->dm->engine) {
        ScopedMem<TCHAR> label(win->dm->engine->GetPageLabel(entry->pageNo));
        ScopedMem<TCHAR> text(str::Format(_T("%s  %s"), entry->title, label));
        tvinsert.itemex.pszText = text;
        return TreeView_InsertItem(hwnd, &tvinsert);
    }
#endif

    return TreeView_InsertItem(hwnd, &tvinsert);
}

static bool WasTocItemExpanded(DocToCItem *entry, Vec<int>& tocState)
{
    for (size_t i = 0; i < tocState.Count(); i++)
        if (tocState[i] == entry->id)
            return true;
    return false;
}

static void PopulateTocTreeView(HWND hwnd, DocToCItem *entry, Vec<int>& tocState, HTREEITEM parent = NULL)
{
    for (; entry; entry = entry->next) {
        bool expand = WasTocItemExpanded(entry, tocState);
        HTREEITEM node = AddTocItemToView(hwnd, entry, parent, expand);
        PopulateTocTreeView(hwnd, entry->child, tocState, node);
    }
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd)
{
    // code inspired by http://www.codeguru.com/cpp/controls/treeview/multiview/article.php/c3985/
    LPNMCUSTOMDRAW ncd = &ntvcd->nmcd;
    HWND hTV = ncd->hdr.hwndFrom;
    HTREEITEM hItem = (HTREEITEM)ncd->dwItemSpec;
    RECT rcItem;
    if (0 == ncd->rc.right - ncd->rc.left || 0 == ncd->rc.bottom - ncd->rc.top)
        return;
    if (!TreeView_GetItemRect(hTV, hItem, &rcItem, TRUE))
        return;
    if (rcItem.right > ncd->rc.right)
        rcItem.right = ncd->rc.right;

    // Clear the label
    RECT rcFullWidth = rcItem;
    rcFullWidth.right = ncd->rc.right;
    HBRUSH brushBg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(ncd->hdc, &rcFullWidth, brushBg);
    DeleteObject(brushBg);

    // Get the label's text
    TCHAR szText[MAX_PATH];
    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.pszText = szText;
    item.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTV, &item);

    // Draw the page number right-aligned (if there is one)
    WindowInfo *win = FindWindowInfoByHwnd(hTV);
    DocToCItem *tocItem = (DocToCItem *)item.lParam;
    ScopedMem<TCHAR> label;
    if (tocItem->pageNo && win && win->IsDocLoaded() && win->dm->engine) {
        label.Set(win->dm->engine->GetPageLabel(tocItem->pageNo));
        label.Set(str::Join(_T("  "), label));
    }
    if (label && str::EndsWith(item.pszText, label)) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, label, str::Len(label), &txtSize);
        rcPageNo.left = rcPageNo.right - txtSize.cx;

        SetTextColor(ncd->hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(ncd->hdc, GetSysColor(COLOR_WINDOW));
        DrawText(ncd->hdc, label, -1, &rcPageNo, DT_SINGLELINE | DT_VCENTER);

        // Reduce the size of the label and cut off the page number
        rcItem.right = max(rcItem.right - txtSize.cx, 0);
        szText[str::Len(szText) - str::Len(label)] = '\0';
    }

    SetTextColor(ncd->hdc, ntvcd->clrText);
    SetBkColor(ncd->hdc, ntvcd->clrTextBk);

    // Draw the focus rectangle (including proper background color)
    brushBg = CreateSolidBrush(ntvcd->clrTextBk);
    FillRect(ncd->hdc, &rcItem, brushBg);
    DeleteObject(brushBg);
    if ((ncd->uItemState & CDIS_FOCUS))
        DrawFocusRect(ncd->hdc, &rcItem);

    InflateRect(&rcItem, -2, -1);
    DrawText(ncd->hdc, szText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

void WindowInfo::LoadTocTree()
{
    if (tocLoaded)
        return;
    tocLoaded = true;

    tocRoot = NULL;
    if (dm->engine)
        tocRoot = dm->engine->GetToCTree();
    if (!tocRoot)
        return;

    SendMessage(hwndTocTree, WM_SETREDRAW, FALSE, 0);
    bool enable = dm->engine->PreferredLayout() & Layout_R2L;
    ToggleWindowStyle(hwndTocTree, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, enable, GWL_EXSTYLE);
    PopulateTocTreeView(hwndTocTree, tocRoot, tocState);
    SendMessage(hwndTocTree, WM_SETREDRAW, TRUE, 0);
    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwndTocTree, NULL, NULL, fl);
}

static HTREEITEM InsertFavSecondLevelNode(HWND hwnd, HTREEITEM parent, FavName *f)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    tvinsert.itemex.state = 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)f;
    ScopedMem<TCHAR> s(FavReadableName(f));
    tvinsert.itemex.pszText = s;
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void InsertFavSecondLevelNodes(HWND hwnd, HTREEITEM parent, FileFavs *f)
{
    for (size_t i = 0; i < f->favNames.Count(); i++)
    {
        InsertFavSecondLevelNode(hwnd, parent, f->favNames.At(i));
    }
}

static HTREEITEM InsertFavTopLevelNode(HWND hwnd, FileFavs *fav, bool isExpanded)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = NULL;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE;
    tvinsert.itemex.state = isExpanded ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.pszText = (TCHAR*)path::GetBaseName(fav->filePath);
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void PopulateFavTreeIfNeeded(WindowInfo *win)
{
    HWND hwndTree = win->hwndFavTree;
    if (TreeView_GetCount(hwndTree) > 0)
        return;

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    for (size_t i=0; i<gFavorites->Count(); i++)
    {
        FileFavs *f = gFavorites->favs.At(i);
        bool isExpanded = (win->expandedFavorites.Find(f->filePath) != -1);
        HTREEITEM node = InsertFavTopLevelNode(hwndTree, f, isExpanded);
        InsertFavSecondLevelNodes(hwndTree, node, f);
    }

    SendMessage(hwndTree, WM_SETREDRAW, TRUE, 0);
    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwndTree, NULL, NULL, fl);
}

// TODO: what to do if we the last favorite is removed? Hide favorites?
static void UpdateFavortesTreeIfNecessary(WindowInfo *win)
{
    HWND hwndTree = win->hwndFavTree;
    if (0 == TreeView_GetCount(hwndTree))
        return;

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwndTree);
    PopulateFavTreeIfNeeded(win);
}

static void UpdateFavoritesTreeForAllWindows()
{
    for (size_t i=0; i<gWindows.Count(); i++)
    {
        UpdateFavortesTreeIfNecessary(gWindows.At(i));
    }
}

/*
static int SidebarDxFromDisplayState(const DisplayState *ds=NULL, int defaultDx=0)
{
    if (ds && !gGlobalPrefs.globalPrefsOnly)
        return ds->sidebarDx;
    if (!defaultDx)
        return gGlobalPrefs.sidebarDx;
    return defaultDx;
}*/

static void SetWinPos(HWND hwnd, RectI r, bool isVisible)
{
    UINT flags = SWP_NOZORDER;
    if (isVisible)
        flags |= SWP_SHOWWINDOW;
    else
        flags |= SWP_HIDEWINDOW;
    SetWindowPos(hwnd, NULL, r.x, r.y, r.dx, r.dy, flags);
}

void SetSidebarVisibility(WindowInfo *win, bool tocVisible, bool favVisible)
{
    if (!win->IsDocLoaded() || !win->dm->HasTocTree())
        tocVisible = false;

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
    {
        tocVisible = false;
        favVisible = false;
    }

    bool sidebarVisible = tocVisible || favVisible;

    if (tocVisible)
        win->LoadTocTree();
    if (favVisible)
        PopulateFavTreeIfNeeded(win);

    win->tocVisible = tocVisible;
    win->favVisible = favVisible;

    ClientRect rFrame(win->hwndFrame);
    int toolbarDy = 0;
    if (gGlobalPrefs.showToolbar && !win->fullScreen && !win->presentation)
        toolbarDy = WindowRect(win->hwndReBar).dy;
    int dy = rFrame.dy - toolbarDy;

    if (!sidebarVisible) {
        if (GetFocus() == win->hwndTocTree || GetFocus() == win->hwndFavTree)
            SetFocus(win->hwndFrame);
        
        SetWindowPos(win->hwndCanvas, NULL, 0, toolbarDy, rFrame.dx, dy, SWP_NOZORDER);
        ShowWindow(win->hwndSidebarSpliter, SW_HIDE);
        ShowWindow(win->hwndTocBox, SW_HIDE);
        ShowWindow(win->hwndFavBox, SW_HIDE);
        return;
    }

    if (rFrame.IsEmpty()) {
        // don't adjust the ToC sidebar size while the window is minimized
        if (win->tocVisible)
            win->UpdateTocSelection(win->dm->currentPageNo());
        return;
    }

    int y = toolbarDy;
    ClientRect sidebarRc(win->hwndTocBox);
    int tocDx = sidebarRc.dx;
    if (tocDx == 0) {
        // TODO: use saved panelDx from saved preferences
        tocDx = rFrame.dx / 4;
    }
    //int dx = SidebarDxFromDisplayState(NULL, rc.dx / 4);

    // make sure that the sidebar is never too wide or too narrow
    // (when changing these values, also adjust ResizeSidebar)
    tocDx = limitValue(tocDx, SIDEBAR_MIN_WIDTH, rFrame.dx / 2);

    int tocDy = !tocVisible ? 0 : !favVisible ? dy : dy / 2;
    int canvasX = tocDx + SPLITTER_DX;

    RectI rToc(0, y, tocDx, tocDy);
    RectI rFav(0, y + tocDy, tocDx, dy - tocDy);
    RectI rSplitter(tocDx, y, SPLITTER_DX, dy);
    RectI rCanvas(canvasX, y, rFrame.dx - canvasX, dy);
    
    SetWinPos(win->hwndTocBox, rToc, tocVisible);
    SetWinPos(win->hwndFavBox, rFav, favVisible);
    SetWinPos(win->hwndSidebarSpliter, rSplitter, true);
    SetWinPos(win->hwndCanvas, rCanvas, true);

    if (tocVisible)
        win->UpdateTocSelection(win->dm->currentPageNo());
}

static void ToggleTocBox(WindowInfo *win)
{
    if (!win->IsDocLoaded())
        return;
    if (win->tocVisible) {
        SetSidebarVisibility(win, false, win->favVisible);
    } else {
        SetSidebarVisibility(win, true,  win->favVisible);
        SetFocus(win->hwndTocTree);
    }
}

static void ClearTocBox(WindowInfo *win)
{
    if (!win->tocLoaded) return;

    SendMessage(win->hwndTocTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(win->hwndTocTree);
    SendMessage(win->hwndTocTree, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(win->hwndTocTree, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    delete win->tocRoot;
    win->tocRoot = NULL;

    win->tocLoaded = false;
    win->currPageNo = 0;
}

static void ToggleFavorites(WindowInfo *win)
{
    if (win->favVisible) {
        SetSidebarVisibility(win, win->tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->tocVisible, true);
        SetFocus(win->hwndFavTree);
    }
}

static void CustomizeToCInfoTip(LPNMTVGETINFOTIP nmit)
{
    DocToCItem *tocItem = (DocToCItem *)nmit->lParam;
    ScopedMem<TCHAR> path(tocItem->GetLink() ? tocItem->GetLink()->GetDestValue() : NULL);
    if (!path)
        return;

    str::Str<TCHAR> infotip(INFOTIPSIZE);

    RECT rcLine, rcLabel;
    HWND hTV = nmit->hdr.hwndFrom;
    // Display the item's full label, if it's overlong
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLine, FALSE);
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLabel, TRUE);
    if (rcLine.right + 2 < rcLabel.right) {
        TVITEM item;
        item.hItem = nmit->hItem;
        item.mask = TVIF_TEXT;
        item.pszText = infotip.Get();
        item.cchTextMax = INFOTIPSIZE;
        TreeView_GetItem(hTV, &item);
        infotip.LenIncrease(str::Len(item.pszText));
        infotip.Append(_T("\r\n"));
    }

    if (tocItem->GetLink() && str::Eq(tocItem->GetLink()->GetType(), "LaunchEmbedded"))
        path.Set(str::Format(_TR("Attachment: %s"), path));

    infotip.Append(path);
    str::BufSet(nmit->pszText, nmit->cchTextMax, infotip.Get());
}

static LRESULT OnSetCursor(WindowInfo& win, HWND hwnd)
{
    POINT pt;

    if (win.IsAboutWindow()) {
        if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
            StaticLinkInfo linkInfo;
            if (GetStaticLink(win.staticLinks, pt.x, pt.y, &linkInfo)) {
                win.CreateInfotip(linkInfo.infotip, linkInfo.rect);
                SetCursor(gCursorHand);
                return TRUE;
            }
        }
    }
    if (!win.IsDocLoaded()) {        
        win.DeleteInfotip();
        return FALSE;
    }

    if (win.mouseAction != MA_IDLE)
        win.DeleteInfotip();

    switch (win.mouseAction) {
        case MA_DRAGGING:
        case MA_DRAGGING_RIGHT:
            SetCursor(gCursorDrag);
            return TRUE;
        case MA_SCROLLING:
            SetCursor(gCursorScroll);
            return TRUE;
        case MA_SELECTING_TEXT:
            SetCursor(gCursorIBeam);
            return TRUE;
        case MA_SELECTING:
            break;
        case MA_IDLE:
            if (GetCursor() && GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                PointI pti(pt.x, pt.y);
                PageElement *pageEl = win.dm->GetElementAtPos(pti);
                if (pageEl) {
                    ScopedMem<TCHAR> text(pageEl->GetValue());
                    RectI rc = win.dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());
                    win.CreateInfotip(text, rc);

                    bool isLink = pageEl->AsLink() != NULL;
                    delete pageEl;

                    if (isLink) {
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
                else
                    win.DeleteInfotip();
                if (win.dm->IsOverText(pti))
                    SetCursor(gCursorIBeam);
                else
                    SetCursor(gCursorArrow);
                return TRUE;
            }
            win.DeleteInfotip();
    }
    if (win.presentation)
        return TRUE;
    return FALSE;
}

static void OnTimer(WindowInfo& win, HWND hwnd, WPARAM timerId)
{
    POINT pt;

    switch (timerId) {
    case REPAINT_TIMER_ID:
        win.delayedRepaintTimer = 0;
        KillTimer(hwnd, REPAINT_TIMER_ID);
        win.RedrawAll();
        break;

    case SMOOTHSCROLL_TIMER_ID:
        if (MA_SCROLLING == win.mouseAction)
            win.MoveDocBy(win.xScrollSpeed, win.yScrollSpeed);
        else if (MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction) {
            GetCursorPos(&pt);
            ScreenToClient(win.hwndCanvas, &pt);
            OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
        }
        else {
            KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
            win.yScrollSpeed = 0;
            win.xScrollSpeed = 0;
        }
        break;

    case HIDE_CURSOR_TIMER_ID:
        KillTimer(hwnd, HIDE_CURSOR_TIMER_ID);
        if (win.presentation)
            SetCursor(NULL);
        break;

    case HIDE_FWDSRCHMARK_TIMER_ID:
        win.fwdSearchMark.hideStep++;
        if (1 == win.fwdSearchMark.hideStep) {
            SetTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS, NULL);
        }
        else if (win.fwdSearchMark.hideStep >= HIDE_FWDSRCHMARK_STEPS) {
            KillTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID);
            win.fwdSearchMark.show = false;
            win.fwdSearchMark.hideStep = 0;
            win.RepaintAsync();
        }
        else
            win.RepaintAsync();
        break;

    case AUTO_RELOAD_TIMER_ID:
        KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
        win.Reload(true);
        break;

    case DIR_STRESS_TIMER_ID:
        win.stressTest->Callback();
        break;
    }
}

// these can be global, as the mouse wheel can't affect more than one window at once
static int  gDeltaPerLine = 0;         // for mouse wheel logic
static bool gWheelMsgRedirect = false; // set when WM_MOUSEWHEEL has been passed on (to prevent recursion)

static LRESULT OnMouseWheel(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!win.IsDocLoaded())
        return 0;

    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.hwndTocTree) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessage(win.hwndTocTree, message, wParam, lParam);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);

    // Note: not all mouse drivers correctly report the Ctrl key's state
    if ((LOWORD(wParam) & MK_CONTROL) || IsCtrlPressed() || (LOWORD(wParam) & MK_RBUTTON)) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(win.hwndCanvas, &pt);

        float factor = delta < 0 ? ZOOM_OUT_FACTOR : ZOOM_IN_FACTOR;
        win.dm->zoomBy(factor, &PointI(pt.x, pt.y));
        win.UpdateToolbarState();

        // don't show the context menu when zooming with the right mouse-button down
        if ((LOWORD(wParam) & MK_RBUTTON))
            win.dragStartPending = false;

        return 0;
    }
    
    // make sure to scroll whole pages in non-continuous Fit Content mode
    if (!displayModeContinuous(win.dm->displayMode()) &&
        ZOOM_FIT_CONTENT == win.dm->zoomVirtual()) {
        if (delta > 0)
            win.dm->goToPrevPage(0);
        else
            win.dm->goToNextPage(0);
        return 0;
    }

    if (gDeltaPerLine == 0)
       return 0;

    if (gDeltaPerLine < 0) {
        // scroll by (fraction of a) page
        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask  = SIF_PAGE;
        GetScrollInfo(win.hwndCanvas, SB_VERT, &si);
        win.dm->scrollYBy(-MulDiv(si.nPage, delta, WHEEL_DELTA), true);
        return 0;
    }

    win.wheelAccumDelta += delta;
    int currentScrollPos = GetScrollPos(win.hwndCanvas, SB_VERT);

    while (win.wheelAccumDelta >= gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        win.wheelAccumDelta -= gDeltaPerLine;
    }
    while (win.wheelAccumDelta <= -gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        win.wheelAccumDelta += gDeltaPerLine;
    }

    if (!displayModeContinuous(win.dm->displayMode()) &&
        GetScrollPos(win.hwndCanvas, SB_VERT) == currentScrollPos) {
        if (delta > 0)
            win.dm->goToPrevPage(-1);
        else
            win.dm->goToNextPage(0);
    }

    return 0;
}

static LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require win
    switch (message) {
        case WM_DROPFILES:
            OnDropFiles((HDROP)wParam);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;
    }

    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    // messages that require win
    switch (message) {
        case WM_VSCROLL:
            OnVScroll(*win, wParam);
            return WM_VSCROLL_HANDLED;

        case WM_HSCROLL:
            OnHScroll(*win, wParam);
            return WM_HSCROLL_HANDLED;

        case WM_MOUSEMOVE:
            OnMouseMove(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDBLCLK:
            OnMouseLeftButtonDblClk(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUp(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_MBUTTONDOWN:
            if (win->IsDocLoaded()) {
                SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);
                // TODO: Create window that shows location of initial click for reference
                OnMouseMiddleButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
            return 0;

        case WM_RBUTTONDBLCLK:
            OnMouseRightButtonDblClick(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_RBUTTONDOWN:
            OnMouseRightButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_RBUTTONUP:
            OnMouseRightButtonUp(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_CONTEXTMENU:
            if (win->IsDocLoaded())
                OnContextMenu(*win, 0, 0);
            else
                OnAboutContextMenu(*win, 0, 0);
            break;

        case WM_SETCURSOR:
            if (OnSetCursor(*win, hwnd))
                return TRUE;
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_TIMER:
            OnTimer(*win, hwnd, wParam);
            break;

        case WM_PAINT:
            OnPaint(*win);
            break;

        case WM_SIZE:
            if (!IsIconic(win->hwndFrame))
                win->UpdateCanvasSize();
            break;

        case WM_MOUSEWHEEL:
            return OnMouseWheel(*win, message, wParam, lParam);

        default:
            // process thread queue events happening during an inner message loop
            // (else the scrolling position isn't updated until the scroll bar is released)
            gUIThreadMarshaller.Execute();
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

class RepaintCanvasWorkItem : public UIThreadWorkItem
{
    UINT delay;

public:
    RepaintCanvasWorkItem(WindowInfo *win, UINT delay) 
        : UIThreadWorkItem(win), delay(delay)
    {}

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (!delay)
            WndProcCanvas(win->hwndCanvas, WM_TIMER, REPAINT_TIMER_ID, 0);
        else if (!win->delayedRepaintTimer)
            win->delayedRepaintTimer = SetTimer(win->hwndCanvas, REPAINT_TIMER_ID, delay, NULL);
    }
};

void WindowInfo::RepaintAsync(UINT delay)
{
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    // and let QueueWorkItem call PostMessage for us
    QueueWorkItem(new RepaintCanvasWorkItem(this, delay));
}

static void AddFavorite(WindowInfo *win)
{
    int pageNo = win->currPageNo;
    TCHAR *filePath = win->loadedFilePath;
    TCHAR *name = NULL;
    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageNo, &name);
    if (!shouldAdd)
        return;
    // TODO: trim whitespace from name ?
    gFavorites->AddOrReplace(filePath, pageNo, name);
    free(name);
    // TODO: show notification that a favorite was deleted?
    UpdateFavoritesTreeForAllWindows();
}

static void DelFavorite(WindowInfo *win)
{
    int pageNo = win->currPageNo;
    TCHAR *filePath = win->loadedFilePath;
    gFavorites->Remove(filePath, pageNo);
    // TODO: show notification that a favorite was deleted?
    UpdateFavoritesTreeForAllWindows();
}

static void UpdateMenu(WindowInfo *win, HMENU m)
{
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id)
        RebuildFileMenu(m);
    else if (id == menuDefFavorites[0].id)
        RebuildFavMenu(win, m);
    if (win)
        MenuUpdateStateForWindow(*win);
}

static LRESULT OnCommand(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    // check if the menuId belongs to an entry in the list of
    // recently opened files and load the referenced file if it does
    if ((wmId >= IDM_FILE_HISTORY_FIRST) && (wmId <= IDM_FILE_HISTORY_LAST))
    {
        DisplayState *state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
        if (state) {
            LoadDocument(state->filePath, win);
            return 0;
        }
    }

    // 10 submenus max with 10 items each max (=100) plus generous buffer => 200
    CASSERT(IDM_FAV_LAST - IDM_FAV_FIRST == 200, enough_fav_menu_ids);
    if ((wmId >= IDM_FAV_FIRST) && (wmId <= IDM_FAV_LAST))
    {
        GoToFavoriteByMenuId(win, wmId);
    }
    
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);
    
    // most of them require a win, the few exceptions are no-ops without
    switch (wmId)
    {
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpen(*win);
            break;
        case IDM_SAVEAS:
            OnMenuSaveAs(*win);
            break;
    
        case IDT_FILE_PRINT:
        case IDM_PRINT:
            OnMenuPrint(*win);
            break;
    
        case IDT_FILE_EXIT:
        case IDM_CLOSE:
            CloseWindow(win, false);
            break;
    
        case IDM_EXIT:
            OnMenuExit();
            break;
    
        case IDM_REFRESH:
            win->Reload();
            break;
    
        case IDM_SAVEAS_BOOKMARK:
            OnMenuSaveBookmark(*win);
            break;
    
        case IDT_VIEW_FIT_WIDTH:
            ToggleToolbarViewButton(*win, ZOOM_FIT_WIDTH, true);
            break;
    
        case IDT_VIEW_FIT_PAGE:
            ToggleToolbarViewButton(*win, ZOOM_FIT_PAGE, false);
            break;
    
        case IDT_VIEW_ZOOMIN:
            win->ZoomToSelection(ZOOM_IN_FACTOR, true);
            break;
    
        case IDT_VIEW_ZOOMOUT:
            win->ZoomToSelection(ZOOM_OUT_FACTOR, true);
            break;
    
        case IDM_ZOOM_6400:
        case IDM_ZOOM_3200:
        case IDM_ZOOM_1600:
        case IDM_ZOOM_800:
        case IDM_ZOOM_400:
        case IDM_ZOOM_200:
        case IDM_ZOOM_150:
        case IDM_ZOOM_125:
        case IDM_ZOOM_100:
        case IDM_ZOOM_50:
        case IDM_ZOOM_25:
        case IDM_ZOOM_12_5:
        case IDM_ZOOM_8_33:
        case IDM_ZOOM_FIT_PAGE:
        case IDM_ZOOM_FIT_WIDTH:
        case IDM_ZOOM_FIT_CONTENT:
        case IDM_ZOOM_ACTUAL_SIZE:
            OnMenuZoom(*win, wmId);
            break;
    
        case IDM_ZOOM_CUSTOM:
            OnMenuCustomZoom(*win);
            break;
    
        case IDM_VIEW_SINGLE_PAGE:
            win->SwitchToDisplayMode(DM_SINGLE_PAGE, true);
            break;
    
        case IDM_VIEW_FACING:
            win->SwitchToDisplayMode(DM_FACING, true);
            break;
    
        case IDM_VIEW_BOOK:
            win->SwitchToDisplayMode(DM_BOOK_VIEW, true);
            break;
    
        case IDM_VIEW_CONTINUOUS:
            OnMenuViewContinuous(*win);
            break;
    
        case IDM_VIEW_SHOW_HIDE_TOOLBAR:
            OnMenuViewShowHideToolbar();
            break;
    
        case IDM_CHANGE_LANGUAGE:
            OnMenuChangeLanguage(*win);
            break;
    
        case IDM_VIEW_BOOKMARKS:
            ToggleTocBox(win);
            break;
    
        case IDM_GOTO_NEXT_PAGE:
            if (win->IsDocLoaded())
                win->dm->goToNextPage(0);
            break;
    
        case IDM_GOTO_PREV_PAGE:
            if (win->IsDocLoaded())
                win->dm->goToPrevPage(0);
            break;
    
        case IDM_GOTO_FIRST_PAGE:
            if (win->IsDocLoaded())
                win->dm->goToFirstPage();
            break;
    
        case IDM_GOTO_LAST_PAGE:
            if (win->IsDocLoaded())
                win->dm->goToLastPage();
            break;
    
        case IDM_GOTO_PAGE:
            OnMenuGoToPage(*win);
            break;
    
        case IDM_VIEW_PRESENTATION_MODE:
            OnMenuViewPresentation(*win);
            break;
    
        case IDM_VIEW_FULLSCREEN:
            OnMenuViewFullscreen(*win);
            break;
    
        case IDM_VIEW_ROTATE_LEFT:
            if (win->IsDocLoaded())
                win->dm->rotateBy(-90);
            break;
    
        case IDM_VIEW_ROTATE_RIGHT:
            if (win->IsDocLoaded())
                win->dm->rotateBy(90);
            break;
    
        case IDM_FIND_FIRST:
            OnMenuFind(*win);
            break;
    
        case IDM_FIND_NEXT:
            OnMenuFindNext(*win);
            break;
    
        case IDM_FIND_PREV:
            OnMenuFindPrev(*win);
            break;
    
        case IDM_FIND_MATCH:
            OnMenuFindMatchCase(*win);
            break;
    
        case IDM_VISIT_WEBSITE:
            LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/"));
            break;
    
        case IDM_MANUAL:
            LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/manual.html"));
            break;
            
        case IDM_CONTRIBUTE_TRANSLATION:
            LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html"));
            break;
    
        case IDM_ABOUT:
            OnMenuAbout();
            break;
    
        case IDM_CHECK_UPDATE:
            DownloadSumatraUpdateInfo(*win, false);
            break;
    
        case IDM_SETTINGS:
            OnMenuSettings(*win);
            break;
    
        case IDM_VIEW_WITH_ACROBAT:
            ViewWithAcrobat(win);
            break;
    
        case IDM_VIEW_WITH_FOXIT:
            ViewWithFoxit(win);
            break;
    
        case IDM_VIEW_WITH_PDF_XCHANGE:
            ViewWithPDFXChange(win);
            break;
    
        case IDM_SEND_BY_EMAIL:
            SendAsEmailAttachment(win);
            break;
    
        case IDM_PROPERTIES:
            OnMenuProperties(*win);
            break;
    
        case IDM_MOVE_FRAME_FOCUS:
            if (win->hwndFrame != GetFocus())
                SetFocus(win->hwndFrame);
            else if (win->tocVisible)
                SetFocus(win->hwndTocTree);
            break;
    
        case IDM_GOTO_NAV_BACK:
            if (win->IsDocLoaded())
                win->dm->navigate(-1);
            break;
            
        case IDM_GOTO_NAV_FORWARD:
            if (win->IsDocLoaded())
                win->dm->navigate(1);
            break;
    
        case IDM_COPY_SELECTION:
            // Don't break the shortcut for text boxes
            if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
                SendMessage(GetFocus(), WM_COPY, 0, 0);
            else if (win->hwndProperties == GetForegroundWindow())
                CopyPropertiesToClipboard(win->hwndProperties);
            else if (!HasPermission(Perm_CopySelection))
                break;
            else if (win->selectionOnPage)
                CopySelectionToClipboard(*win);
            else
                win->ShowNotification(_TR("Select content with Ctrl+left mouse button"));
            break;
    
        case IDM_SELECT_ALL:
            OnSelectAll(*win);
            break;
    
        case IDM_DEBUG_SHOW_LINKS:
            gDebugShowLinks = !gDebugShowLinks;
            for (size_t i = 0; i < gWindows.Count(); i++)
                gWindows[i]->RedrawAll(true);
            break;
    
        case IDM_DEBUG_GDI_RENDERER:
            ToggleGdiDebugging();
            break;
    
        case IDM_DEBUG_CRASH_ME:
            CrashMe();
            break;
    
        case IDM_FAV_ADD:
            AddFavorite(win);
            break;

        case IDM_FAV_DEL:
            DelFavorite(win);
            break;

        case IDM_FAV_TOGGLE:
            ToggleFavorites(win);
            break;
    
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *    win;
    ULONG           ulScrollLines;                   // for mouse wheel logic

    win = FindWindowInfoByHwnd(hwnd);

    switch (message)
    {
        case WM_CREATE:
            // do nothing
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED != wParam) {
                RememberWindowPosition(*win);
                AdjustWindowEdge(*win);

                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                OnFrameSize(win, dx, dy);
            }
            break;

        case WM_MOVE:
            if (win) {
                RememberWindowPosition(*win);
                AdjustWindowEdge(*win);
            }
            break;

        case WM_INITMENUPOPUP:
            UpdateMenu(win, (HMENU)wParam);
            break;

        case WM_COMMAND:
            return OnCommand(win, hwnd, message, wParam, lParam);

        case WM_APPCOMMAND:
            // both keyboard and mouse drivers should produce WM_APPCOMMAND
            // messages for their special keys, so handle these here and return
            // TRUE so as to not make them bubble up further
            switch (GET_APPCOMMAND_LPARAM(lParam)) {
            case APPCOMMAND_BROWSER_BACKWARD:
                SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_BACK, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_FORWARD:
                SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_FORWARD, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_REFRESH:
                SendMessage(hwnd, WM_COMMAND, IDM_REFRESH, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_SEARCH:
                SendMessage(hwnd, WM_COMMAND, IDM_FIND_FIRST, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_FAVORITES:
                SendMessage(hwnd, WM_COMMAND, IDM_VIEW_BOOKMARKS, 0);
                return TRUE;
            }
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_CHAR:
            if (win)
                OnChar(*win, wParam);
            break;

        case WM_KEYDOWN:
            if (win)
                OnKeydown(*win, wParam, lParam);
            break;

        case WM_CONTEXTMENU:
            // opening the context menu with a keyboard doesn't call the canvas'
            // WM_CONTEXTMENU, as it never has the focus (mouse right-clicks are
            // handled as expected)
            if (win && GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1 && GetFocus() != win->hwndTocTree)
                return SendMessage(win->hwndCanvas, WM_CONTEXTMENU, wParam, lParam);
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_SETTINGCHANGE:
InitMouseWheelInfo:
            SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
            // ulScrollLines usually equals 3 or 0 (for no scrolling) or -1 (for page scrolling)
            // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40
            if (ulScrollLines == (ULONG)-1)
                gDeltaPerLine = -1;
            else if (ulScrollLines != 0)
                gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
            else
                gDeltaPerLine = 0;
            return 0;

        case WM_MOUSEWHEEL:
            if (!win)
                break;
            // Pass the message to the canvas' window procedure
            // (required since the canvas itself never has the focus and thus
            // never receives WM_MOUSEWHEEL messages)
            return SendMessage(win->hwndCanvas, message, wParam, lParam);

        case WM_DESTROY:
            /* WM_DESTROY might be sent as a result of File\Close, in which case CloseWindow() has already been called */
            if (win)
                CloseWindow(win, TRUE, true);
            break;

        case WM_DDE_INITIATE:
            if (gPluginMode)
                break;
            return OnDDEInitiate(hwnd, wParam, lParam);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wParam, lParam);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wParam, lParam);

        case UWM_PREFS_FILE_UPDATED:
            ReloadPrefs();
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static bool RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcFrame;
    wcex.lpszClassName  = FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hInstance);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = WndProcCanvas;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = CANVAS_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcAbout;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = ABOUT_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcProperties;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = PROPERTIES_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcSpliter;
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = SPLITER_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = MessageWnd::WndProc;
    wcex.hCursor        = LoadCursor(NULL, IDC_APPSTARTING);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = MESSAGE_WND_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    return true;
}

#define IDC_HAND            MAKEINTRESOURCE(32649)
static bool InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;

    gCursorArrow = LoadCursor(NULL, IDC_ARROW);
    gCursorIBeam = LoadCursor(NULL, IDC_IBEAM);
    gCursorHand  = LoadCursor(NULL, IDC_HAND); // apparently only available if WINVER >= 0x0500
    if (!gCursorHand)
        gCursorHand = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));

    gCursorScroll   = LoadCursor(NULL, IDC_SIZEALL);
    gCursorDrag     = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gCursorSizeWE   = LoadCursor(NULL, IDC_SIZEWE);
    gCursorNo       = LoadCursor(NULL, IDC_NO);
    gBrushNoDocBg   = CreateSolidBrush(COL_WINDOW_BG);
    if (ABOUT_BG_COLOR_DEFAULT != gGlobalPrefs.bgColor)
        gBrushAboutBg = CreateSolidBrush(gGlobalPrefs.bgColor);
    else
        gBrushAboutBg = CreateSolidBrush(ABOUT_BG_COLOR);
    gBrushWhite     = CreateSolidBrush(WIN_COL_WHITE);
    gBrushBlack     = CreateSolidBrush(WIN_COL_BLACK);
    gBrushShadow    = CreateSolidBrush(COL_WINDOW_SHADOW);

    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirect(&ncm.lfMessageFont);
    gBitmapReloadingCue = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_RELOADING_CUE));
    
    return true;
}

static bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors=true)
{
    if (!HasPermission(Perm_PrinterAccess))
        return false;

    ScopedMem<TCHAR> fileName2(path::Normalize(fileName));
    BaseEngine *engine = EngineManager::CreateEngine(fileName2);
    if (!engine || !engine->IsPrintingAllowed()) {
        if (displayErrors)
            MessageBox(NULL, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    // Retrieve the printer, printer driver, and output-port names from WIN.INI. 
    TCHAR devstring[256];
    GetProfileString(_T("Devices"), printerName, _T(""), devstring, dimof(devstring));

    // Parse the string of names
    // If the string contains the required names, use them to
    // create a device context.
    ScopedMem<TCHAR> driver, port;
    if (!str::Parse(devstring, _T("%S,%S,"), &driver, &port) &&
        !str::Parse(devstring, _T("%S,%S"), &driver, &port) || !driver || !port) {
        if (displayErrors)
            MessageBox(NULL, _T("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    HANDLE printer;
    bool ok = OpenPrinter((LPTSTR)printerName, &printer, NULL);
    if (!ok) {
        if (displayErrors)
            MessageBox(NULL, _TR("Could not open Printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    DWORD structSize = DocumentProperties(NULL,
        printer,                /* Handle to our printer. */ 
        (LPTSTR)printerName,    /* Name of the printer. */ 
        NULL,                   /* Asking for size, so */ 
        NULL,                   /* these are not used. */ 
        0);                     /* Zero returns buffer size. */
    LPDEVMODE devMode = (LPDEVMODE)malloc(structSize);
    HDC hdcPrint = NULL;
    if (!devMode) goto Exit;

    // Get the default DevMode for the printer and modify it for your needs.
    DWORD returnCode = DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* The address of the buffer to fill. */ 
        NULL,           /* Not using the input buffer. */ 
        DM_OUT_BUFFER); /* Have the output buffer filled. */ 
    if (IDOK != returnCode) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _T("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }

    /*
     * Merge the new settings with the old.
     * This gives the driver an opportunity to update any private
     * portions of the DevMode structure.
     */ 
    DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* Reuse our buffer for output. */ 
        devMode,        /* Pass the driver our changes. */ 
        DM_IN_BUFFER |  /* Commands to Merge our changes and */ 
        DM_OUT_BUFFER); /* write the result. */ 

    ClosePrinter(printer);

    hdcPrint = CreateDC(driver, printerName, port, devMode); 
    if (!hdcPrint) {
        if (displayErrors)
            MessageBox(NULL, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }
    if (CheckPrinterStretchDibSupport(NULL, hdcPrint)) {
        PRINTPAGERANGE pr = { 1, engine->PageCount() };
        Vec<PRINTPAGERANGE> ranges;
        ranges.Append(pr);
        PrintData pd(engine, hdcPrint, devMode, ranges);
        hdcPrint = NULL; // deleted by PrintData

        PrintToDevice(pd);
        ok = true;
    }
Exit:
    free(devMode);
    DeleteDC(hdcPrint);
    return ok;
}

void UIThreadWorkItemQueue::Queue(UIThreadWorkItem *item)
{
    if (!item)
        return;

    ScopedCritSec scope(&cs);
    items.Append(item);

    if (item->win) {
        // hwndCanvas is less likely to enter internal message pump (during which
        // the messages are not visible to our processing in top-level message pump)
        PostMessage(item->win->hwndCanvas, WM_NULL, 0, 0);
    }
}

static void MakePluginWindow(WindowInfo& win, HWND hwndParent)
{
    assert(IsWindow(hwndParent));
    assert(gPluginMode);

    long ws = GetWindowLong(win.hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP|WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(win.hwndFrame, GWL_STYLE, ws);

    SetParent(win.hwndFrame, hwndParent);
    ClientRect rc(hwndParent);
    MoveWindow(win.hwndFrame, 0, 0, rc.dx, rc.dy, FALSE);
    ShowWindow(win.hwndFrame, SW_SHOW);

    // from here on, we depend on the plugin's host to resize us
    SetFocus(win.hwndFrame);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg = { 0 };

#ifdef DEBUG
    // Memory leak detection
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    //_CrtSetBreakAlloc(421);
#endif

    EnableNx();

    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");

#ifdef DEBUG
    extern void BaseUtils_UnitTests();
    BaseUtils_UnitTests();
    extern void SumatraPDF_UnitTests();
    SumatraPDF_UnitTests();
#endif

    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    srand((unsigned int)time(NULL));

    ScopedMem<TCHAR> crashDumpPath(AppGenDataFilename(CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);

    {
        ScopedMem<TCHAR> prefsFilename(GetPrefsFileName());
        if (!file::Exists(prefsFilename)) {
            // guess the ui language on first start
            CurrLangNameSet(Trans::GuessLanguage());
            gFavorites = new Favorites();
        } else {
            assert(gFavorites == NULL);
            Prefs::Load(prefsFilename, gGlobalPrefs, gFileHistory, &gFavorites);
            CurrLangNameSet(gGlobalPrefs.currentLanguage);
        }
    }

    CommandLineInfo i;
    i.bgColor = gGlobalPrefs.bgColor;
    i.fwdsearchOffset = gGlobalPrefs.fwdSearchOffset;
    i.fwdsearchWidth = gGlobalPrefs.fwdSearchWidth;
    i.fwdsearchColor = gGlobalPrefs.fwdSearchColor;
    i.fwdsearchPermanent = gGlobalPrefs.fwdSearchPermanent;
    i.escToExit = gGlobalPrefs.escToExit;

    i.ParseCommandLine(GetCommandLine());

    if (i.showConsole)
        RedirectIOToConsole();
    else if (i.consoleFile)
        RedirectIOToFile(i.consoleFile, true);
    if (i.makeDefault)
        AssociateExeWithPdfExtension();
    if (i.filesToBenchmark.Count() > 0) {
        Bench(i.filesToBenchmark);
        if (i.showConsole)
            system("pause");
    }
    if (i.exitImmediately)
        goto Exit;

    gGlobalPrefs.bgColor = i.bgColor;
    gGlobalPrefs.fwdSearchOffset = i.fwdsearchOffset;
    gGlobalPrefs.fwdSearchWidth = i.fwdsearchWidth;
    gGlobalPrefs.fwdSearchColor = i.fwdsearchColor;
    gGlobalPrefs.fwdSearchPermanent = i.fwdsearchPermanent;
    gGlobalPrefs.escToExit = i.escToExit;
    gPolicyRestrictions = GetPolicies(i.restrictedUse);
    gRenderCache.invertColors = i.invertColors;
    DebugGdiPlusDevice(gUseGdiRenderer);

    if (i.inverseSearchCmdLine) {
        str::ReplacePtr(&gGlobalPrefs.inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs.enableTeXEnhancements = true;
    }
    CurrLangNameSet(i.lang);

    msg.wParam = 1; // set an error code, in case we prematurely have to goto Exit
    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    if (i.hwndPluginParent) {
        if (!IsWindow(i.hwndPluginParent) || i.fileNames.Count() == 0)
            goto Exit;

        gPluginMode = true;
        gPluginURL = i.pluginURL;
        if (!gPluginURL)
            gPluginURL = i.fileNames[0];

        assert(i.fileNames.Count() == 1);
        while (i.fileNames.Count() > 1)
            free(i.fileNames.Pop());
        i.reuseInstance = i.exitOnPrint = false;
        // always display the toolbar when embedded (as there's no menubar in that case)
        gGlobalPrefs.showToolbar = true;
        // never allow esc as a shortcut to quit
        gGlobalPrefs.escToExit = false;
    }

    WindowInfo *win = NULL;
    bool firstIsDocLoaded = false;
    msg.wParam = 0;

    if (i.printerName) {
        // note: this prints all PDF files. Another option would be to
        // print only the first one
        for (size_t n = 0; n < i.fileNames.Count(); n++) {
            bool ok = PrintFile(i.fileNames[n], i.printerName, !i.silent);
            if (!ok)
                msg.wParam++;
        }
        goto Exit;
    }

    if (i.fileNames.Count() == 0 && gGlobalPrefs.rememberOpenedFiles && gGlobalPrefs.showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFO sfi;
        SHGetFileInfo(_T(".pdf"), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    }

    for (size_t n = 0; n < i.fileNames.Count(); n++) {
        if (i.reuseInstance && !i.printDialog) {
            // delegate file opening to a previously running instance by sending a DDE message 
            TCHAR fullpath[MAX_PATH];
            GetFullPathName(i.fileNames[n], dimof(fullpath), fullpath, NULL);
            ScopedMem<TCHAR> command(str::Format(_T("[") DDECOMMAND_OPEN _T("(\"%s\", 0, 1, 0)]"), fullpath));
            DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            if (i.destName && !firstIsDocLoaded) {
                ScopedMem<TCHAR> command(str::Format(_T("[") DDECOMMAND_GOTO _T("(\"%s\", \"%s\")]"), fullpath, i.destName));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
            else if (i.pageNumber > 0 && !firstIsDocLoaded) {
                ScopedMem<TCHAR> command(str::Format(_T("[") DDECOMMAND_PAGE _T("(\"%s\", %d)]"), fullpath, i.pageNumber));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
            if ((i.startView != DM_AUTOMATIC || i.startZoom != INVALID_ZOOM ||
                 i.startScroll.x != -1 && i.startScroll.y != -1) && !firstIsDocLoaded) {
                const TCHAR *viewMode = DisplayModeConv::NameFromEnum(i.startView);
                ScopedMem<TCHAR> command(str::Format(_T("[") DDECOMMAND_SETVIEW _T("(\"%s\", \"%s\", %.2f, %d, %d)]"),
                                         fullpath, viewMode, i.startZoom, i.startScroll.x, i.startScroll.y));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
        }
        else {
            bool showWin = !(i.printDialog && i.exitOnPrint) && !gPluginMode;
            win = LoadDocument(i.fileNames[n], NULL, showWin);
            if (!win || !win->IsDocLoaded())
                msg.wParam++; // set an error code for the next goto Exit
            if (!win)
                goto Exit;
            if (win->IsDocLoaded() && i.destName && !firstIsDocLoaded) {
                win->linkHandler->GotoNamedDest(i.destName);
            }
            else if (win->IsDocLoaded() && i.pageNumber > 0 && !firstIsDocLoaded) {
                if (win->dm->validPageNo(i.pageNumber))
                    win->dm->goToPage(i.pageNumber, 0);
            }
            if (i.hwndPluginParent)
                MakePluginWindow(*win, i.hwndPluginParent);
            if (win->IsDocLoaded() && !firstIsDocLoaded) {
                if (i.enterPresentation || i.enterFullscreen)
                    EnterFullscreen(*win, i.enterPresentation);
                if (i.startView != DM_AUTOMATIC)
                    win->SwitchToDisplayMode(i.startView);
                if (i.startZoom != INVALID_ZOOM)
                    win->ZoomToSelection(i.startZoom, false);
                if (i.startScroll.x != -1 || i.startScroll.y != -1) {
                    ScrollState ss = win->dm->GetScrollState();
                    ss.x = i.startScroll.x;
                    ss.y = i.startScroll.y;
                    win->dm->SetScrollState(ss);
                }
            }
        }

        if (i.printDialog)
            OnMenuPrint(*win);
        firstIsDocLoaded = true;
    }

    if (i.reuseInstance || i.printDialog && i.exitOnPrint)
        goto Exit;
 
    if (!firstIsDocLoaded) {
        bool enterFullscreen = (WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState);
        win = CreateWindowInfo();
        if (!win) {
            msg.wParam = 1;
            goto Exit;
        }

        if (WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState ||
            WIN_STATE_MAXIMIZED == gGlobalPrefs.windowState)
            ShowWindow(win->hwndFrame, SW_MAXIMIZE);
        else
            ShowWindow(win->hwndFrame, SW_SHOW);
        UpdateWindow(win->hwndFrame);

        if (enterFullscreen)
            EnterFullscreen(*win);
    }

    UpdateToolbarToolText(); // needed for RTL languages
    if (!firstIsDocLoaded)
        UpdateToolbarAndScrollbarsForAllWindows();

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs.pdfAssociateShouldAssociate && win)
        RegisterForPdfExtentions(win->hwndFrame);

    if (gGlobalPrefs.enableAutoUpdate && gWindows.Count() > 0)
        DownloadSumatraUpdateInfo(*gWindows[0], true);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SUMATRAPDF));
#ifndef THREAD_BASED_FILEWATCH
    const UINT_PTR timerID = SetTimer(NULL, -1, FILEWATCH_DELAY_IN_MS, NULL);
#endif

    if (i.stressTestPath) {
        StartStressTest(win, i.stressTestPath, i.stressTestRanges, i.stressTestCycles, &gRenderCache,
                        i.disableDjvu, i.disablePdf, i.disableCbx);
    }

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
#ifndef THREAD_BASED_FILEWATCH
        if (NULL == msg.hwnd && WM_TIMER == msg.message && timerID == msg.wParam) {
            RefreshUpdatedFiles();
            continue;
        }
#endif
        // Dispatch the accelerator to the correct window
        win = FindWindowInfoByHwnd(msg.hwnd);
        HWND accHwnd = win ? win->hwndFrame : msg.hwnd;
        if (TranslateAccelerator(accHwnd, hAccelTable, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // process these messages here so that we don't have to add this
        // handling to every WndProc that might receive those messages
        // note: this isn't called during an inner message loop, so
        //       Execute() also has to be called from a WndProc
        gUIThreadMarshaller.Execute();
    }

#ifndef THREAD_BASED_FILEWATCH
    KillTimer(NULL, timerID);
#endif

    CleanUpThumbnailCache(gFileHistory);

Exit:
    while (gWindows.Count() > 0)
        DeleteWindowInfo(gWindows[0]);
    DeleteObject(gBrushNoDocBg);
    DeleteObject(gBrushAboutBg);
    DeleteObject(gBrushWhite);
    DeleteObject(gBrushBlack);
    DeleteObject(gBrushShadow);
    DeleteObject(gDefaultGuiFont);
    DeleteBitmap(gBitmapReloadingCue);

    delete gFavorites;

    return (int)msg.wParam;
}
