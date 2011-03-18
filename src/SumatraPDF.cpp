/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include <shlobj.h>
#include <wininet.h>

// TODO: move all cbz/cbr related code into ComicEngine.cpp
// minizip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

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

#include "WinUtil.h"
#include "Http.h"
#include "CrashHandler.h"
#include "ParseCommandLine.h"
#include "Benchmark.h"

#include "LangMenuDef.h"
#include "translations.h"
#include "Version.h"

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST           IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST            IDM_VIEW_CONTINUOUS
#define IDM_ZOOM_FIRST                  IDM_ZOOM_FIT_PAGE
#define IDM_ZOOM_LAST                   IDM_ZOOM_CUSTOM

// Undefine any of these two, if you prefer MuPDF/Fitz to render the whole page
// (using FreeType for fonts) at the expense of higher memory/spooler requirements.
#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
// #define USE_GDI_FOR_RENDERING
#endif
#define USE_GDI_FOR_PRINTING

/* undefine if you don't want to use memory consuming double-buffering for rendering the PDF */
#define DOUBLE_BUFFER

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

/* Define THREAD_BASED_FILEWATCH to use the thread-based implementation of file change detection. */
#define THREAD_BASED_FILEWATCH

/* Define if you want to display additional debug helpers in the Help menu */
// #define SHOW_DEBUG_MENU_ITEMS

#define ZOOM_IN_FACTOR      1.2f
#define ZOOM_OUT_FACTOR     1.0f / ZOOM_IN_FACTOR

/* if TRUE, we're in debug mode where we show links as blue rectangle on
   the screen. Makes debugging code related to links easier.
   TODO: make a menu item in DEBUG build to turn it on/off. */
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

/* default UI settings */

#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_ROTATION        0
#define DEFAULT_LANGUAGE        "en"

#if defined(SVN_PRE_RELEASE_VER) && !defined(BLACK_ON_YELLOW)
#define ABOUT_BG_COLOR          RGB(255,0,0)
#else
#define ABOUT_BG_COLOR          RGB(255,242,0)
#endif

#define COL_WINDOW_BG           RGB(0xcc, 0xcc, 0xcc)
#define COL_WINDOW_SHADOW       RGB(0x40, 0x40, 0x40)
#define COL_PAGE_FRAME          RGB(0x88, 0x88, 0x88)
#define COL_FWDSEARCH_BG        RGB(0x65, 0x81 ,0xff)
#define COL_SELECTION_RECT      RGB(0xF5, 0xFC, 0x0C)

#define SUMATRA_WINDOW_TITLE    _T("SumatraPDF")
#define CANVAS_CLASS_NAME       _T("SUMATRA_PDF_CANVAS")
#define SPLITER_CLASS_NAME      _T("Spliter")
#define FINDSTATUS_CLASS_NAME   _T("FindStatus")
#define PREFS_FILE_NAME         _T("sumatrapdfprefs.dat")

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO (612.0/792.0)

#define SPLITTER_DX  5
#define SPLITTER_MIN_WIDTH 150

#define REPAINT_TIMER_ID            1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define SMOOTHSCROLL_TIMER_ID       2
#define SMOOTHSCROLL_DELAY_IN_MS    20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

#define HIDE_CURSOR_TIMER_ID        3
#define HIDE_CURSOR_DELAY_IN_MS     3000

#define FIND_STATUS_WIDTH       200 // Default width for the find status window
#define FIND_STATUS_MARGIN      8
#define FIND_STATUS_PROGRESS_HEIGHT 5

#define HIDE_FWDSRCHMARK_TIMER_ID                4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS             400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS     100
#define HIDE_FWDSRCHMARK_STEPS                   5

#define AUTO_RELOAD_TIMER_ID        5
#define AUTO_RELOAD_DELAY_IN_MS     100

#ifndef THREAD_BASED_FILEWATCH
#define FILEWATCH_DELAY_IN_MS       1000
#endif

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
static HBRUSH                       gBrushBg;
static HBRUSH                       gBrushWhite;
static HBRUSH                       gBrushBlack;
static HBRUSH                       gBrushShadow;
static HFONT                        gDefaultGuiFont;
static HBITMAP                      gBitmapReloadingCue;

static RenderCache                  gRenderCache;
static Vec<WindowInfo*>             gWindows;
static FileHistory                  gFileHistory;
static UIThreadWorkItemQueue        gUIThreadMarshaller;

static int                          gReBarDy;
static int                          gReBarDyFrame;

// in restricted mode, all commands that could affect the OS are
// disabled (such as opening files, printing, following URLs), so
// that SumatraPDF can be used as a PDF reader on locked down systems
       bool                         gRestrictedUse = false;

SerializableGlobalPrefs             gGlobalPrefs = {
    false, // bool m_globalPrefsOnly
    DEFAULT_LANGUAGE, // const char *m_currentLanguage
    true, // bool m_showToolbar
    false, // bool m_pdfAssociateDontAskAgain
    false, // bool m_pdfAssociateShouldAssociate
    true, // bool m_enableAutoUpdate
    true, // bool m_rememberOpenedFiles
    ABOUT_BG_COLOR, // int  m_bgColor
    false, // bool m_escToExit
    NULL, // TCHAR *m_inverseSearchCmdLine
    false, // bool m_enableTeXEnhancements
    NULL, // TCHAR *m_versionToSkip
    NULL, // char *m_lastUpdateTime
    DEFAULT_DISPLAY_MODE, // DisplayMode m_defaultDisplayMode
    DEFAULT_ZOOM, // float m_defaultZoom
    WIN_STATE_NORMAL, // int  m_windowState
    RectI(), // RectI m_windowPos
    true, // bool m_showToc
    0, // int  m_tocDx
    0, // int  m_fwdsearchOffset
    COL_FWDSEARCH_BG, // int  m_fwdsearchColor
    15, // int  m_fwdsearchWidth
    0, // bool m_fwdsearchPermanent
    false, // bool m_invertColors
};

typedef struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    int           bmpIndex;
    int           cmdId;
    const char *  toolTip;
    int           flags;
} ToolbarButtonInfo;

enum ToolbarButtonFlag {
    TBF_RESTRICTED = 0x1 
};

static ToolbarButtonInfo gToolbarButtons[] = {
    { 0,   IDM_OPEN,              _TRN("Open"),           TBF_RESTRICTED },
    { -1,  IDM_GOTO_PAGE,         NULL,                   0,             },
    { 1,   IDM_GOTO_PREV_PAGE,    _TRN("Previous Page"),  0,             },
    { 2,   IDM_GOTO_NEXT_PAGE,    _TRN("Next Page"),      0,             },
    { -1,  NULL,                  NULL,                   0,             },
    { 3,   IDT_VIEW_FIT_WIDTH,    _TRN("Fit Width and Show Pages Continuously"), 0, },
    { 4,   IDT_VIEW_FIT_PAGE,     _TRN("Fit a Single Page"),    0,       },
    { 5,   IDT_VIEW_ZOOMOUT,      _TRN("Zoom Out"),       0,             },
    { 6,   IDT_VIEW_ZOOMIN,       _TRN("Zoom In"),        0,             },
    { -1,  IDM_FIND_FIRST,        NULL,                   0,             },
    { 7,   IDM_FIND_PREV,         _TRN("Find Previous"),  0,             },
    { 8,   IDM_FIND_NEXT,         _TRN("Find Next"),      0,             },
    // TODO: is this button really used often enough?
    { 9,   IDM_FIND_MATCH,        _TRN("Match Case"),     0,             },
};

#define TOOLBAR_BUTTONS_COUNT dimof(gToolbarButtons)

static void CreateToolbar(WindowInfo *win, HINSTANCE hInst);
static void CreateTocBox(WindowInfo *win, HINSTANCE hInst);
static void UpdateToolbarFindText(WindowInfo *win);
static void UpdateToolbarPageText(WindowInfo *win, int pageCount);
static void UpdateToolbarToolText(void);
static void RebuildMenuBar(void);
static void OnMenuFindMatchCase(WindowInfo *win);
static bool LoadPdfIntoWindow(const TCHAR *fileName, WindowInfo *win, 
    const DisplayState *state, bool isNewWindow, bool tryRepair, 
    bool showWin, bool placeWindow);
static WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win, bool showWin=true);
static void WindowInfo_ShowMessage_Async(WindowInfo *win, const TCHAR *message, bool resize);

static void Find(WindowInfo *win, PdfSearchDirection direction = FIND_FORWARD);
static void DeleteOldSelectionInfo(WindowInfo *win);
static void ClearSearch(WindowInfo *win);
static void WindowInfo_EnterFullscreen(WindowInfo *win, bool presentation=false);
static void WindowInfo_ExitFullscreen(WindowInfo *win);

extern "C" {
// needed because we compile bzip2 with #define BZ_NO_STDIO
void bz_internal_error(int errcode)
{
    // do nothing
}
}

static int LangGetIndex(const char *name)
{
    for (int i = 0; i < LANGS_COUNT; i++) {
        const char *langName = g_langs[i]._langName;
        if (Str::Eq(name, langName))
            return i;
    }
    return -1;
}

bool CurrLangNameSet(const char* langName)
{
    if (!langName)
        return false;

    int langIndex = LangGetIndex(langName);
    if (-1 == langIndex)
        return false;
    gGlobalPrefs.m_currentLanguage = (char *)g_langs[langIndex]._langName;

    bool ok = Translations_SetCurrentLanguage(langName);
    assert(ok);
    return ok;
}

/* Get current UTS cystem time as string.
   Caller needs to free() the result. */
char *GetSystemTimeAsStr()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return _MemToHex(&ft);
}

static void FileTimeToLargeInteger(FILETIME *ft, ULARGE_INTEGER *lt)
{
    lt->LowPart = ft->dwLowDateTime;
    lt->HighPart = ft->dwHighDateTime;
}

/* Return <ft1> - <ft2> in seconds */
DWORD FileTimeDiffInSecs(FILETIME *ft1, FILETIME *ft2)
{
    ULARGE_INTEGER t1;
    ULARGE_INTEGER t2;
    FileTimeToLargeInteger(ft1, &t1);
    FileTimeToLargeInteger(ft2, &t2);
    // diff is in 100 nanoseconds
    LONGLONG diff = t1.QuadPart - t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (DWORD)diff;
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

static void SerializableGlobalPrefs_Init() {
}

static void SerializableGlobalPrefs_Deinit()
{
    free(gGlobalPrefs.m_versionToSkip);
    free(gGlobalPrefs.m_inverseSearchCmdLine);
    free(gGlobalPrefs.m_lastUpdateTime);
}

void LaunchBrowser(const TCHAR *url)
{
    if (gRestrictedUse) return;
    launch_url(url);
}

static bool HasValidFileOrNoFile(WindowInfo *win)
{
    if (!win) return false;
    if (!win->loadedFilePath) return true;
    return File::Exists(win->loadedFilePath);
}

static bool CanViewWithFoxit(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Foxit
    if (gRestrictedUse || !HasValidFileOrNoFile(win))
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
    ScopedMem<TCHAR> params(Str::Format(_T("%s \"%s\" -n %d"), args, win->loadedFilePath, win->dm->currentPageNo()));
    exec_with_params(exePath, params, FALSE);
    return true;
}

static bool CanViewWithPDFXChange(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Foxit
    if (gRestrictedUse || !HasValidFileOrNoFile(win))
        return false;
    ScopedMem<TCHAR> path(GetPDFXChangePath());
    return path != NULL;
}

static bool ViewWithPDFXChange(WindowInfo *win, TCHAR *args=NULL)
{
    if (!CanViewWithPDFXChange(win))
        return false;

    ScopedMem<TCHAR> exePath(GetPDFXChangePath());
    if (exePath)
        return false;
    if (!args)
        args = _T("");

    // PDFXChange cmd-line format:
    // [/A "param=value [&param2=value ..."] [PDF filename] 
    // /A params: page=<page number>
    ScopedMem<TCHAR> params(Str::Format(_T("%s /A \"page=%d\" \"%s\""), args, win->dm->currentPageNo(), win->loadedFilePath));
    exec_with_params(exePath, params, FALSE);
    return true;
}

static bool CanViewWithAcrobat(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (gRestrictedUse || !HasValidFileOrNoFile(win))
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
        params.Set(Str::Format(_T("/A \"page=%d\" %s \"%s\""), win->dm->currentPageNo(), args, win->dm->fileName()));
    else
        params.Set(Str::Format(_T("%s \"%s\""), args, win->loadedFilePath));
    exec_with_params(exePath, params, FALSE);

    return true;
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE); 

static bool CanSendAsEmailAttachment(WindowInfo *win)
{
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (gRestrictedUse || !HasValidFileOrNoFile(win))
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

static void MenuUpdateDisplayMode(WindowInfo *win)
{
    bool enabled = false;
    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    if (win->dm) {
        enabled = true;
        displayMode = win->dm->displayMode();
    }

    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++) {
        Win::Menu::Enable(win->menu, id, enabled);
    }

    UINT id = 0;
    switch (displayMode) {
        case DM_SINGLE_PAGE: id = IDM_VIEW_SINGLE_PAGE; break;
        case DM_FACING: id = IDM_VIEW_FACING; break;
        case DM_BOOK_VIEW: id = IDM_VIEW_BOOK; break;
        case DM_CONTINUOUS: id = IDM_VIEW_SINGLE_PAGE; break;
        case DM_CONTINUOUS_FACING: id = IDM_VIEW_FACING; break;
        case DM_CONTINUOUS_BOOK_VIEW: id = IDM_VIEW_BOOK; break;
        default: assert(!win->dm && DM_AUTOMATIC == displayMode); break;
    }

    CheckMenuRadioItem(win->menu, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    if (displayModeContinuous(displayMode))
        Win::Menu::Check(win->menu, IDM_VIEW_CONTINUOUS, true);
}

void WindowInfo::SwitchToDisplayMode(DisplayMode displayMode, bool keepContinuous)
{
    if (!this->dm)
        return;

    if (keepContinuous && displayModeContinuous(this->dm->displayMode())) {
        switch (displayMode) {
            case DM_SINGLE_PAGE: displayMode = DM_CONTINUOUS; break;
            case DM_FACING: displayMode = DM_CONTINUOUS_FACING; break;
            case DM_BOOK_VIEW: displayMode = DM_CONTINUOUS_BOOK_VIEW; break;
        }
    }

    this->prevCanvasBR.x = this->prevCanvasBR.y = -1;
    this->dm->changeDisplayMode(displayMode);
    UpdateToolbarState();
}

#define SEP_ITEM "-----"

enum menuFlags {
    MF_NOT_IN_RESTRICTED = 0x1,
    MF_NO_TRANSLATE      = 0x2,
    MF_REMOVED           = 0x4
};

typedef struct MenuDef {
    const char *title;
    int         id;
    int         flags;
} MenuDef;

MenuDef menuDefFile[] = {
    { _TRN("&Open\tCtrl-O"),                IDM_OPEN ,                  MF_NOT_IN_RESTRICTED },
    { _TRN("&Close\tCtrl-W"),               IDM_CLOSE,                  MF_NOT_IN_RESTRICTED },
    { _TRN("&Save As...\tCtrl-S"),          IDM_SAVEAS,                 MF_NOT_IN_RESTRICTED },
    { _TRN("&Print...\tCtrl-P"),            IDM_PRINT,                  MF_NOT_IN_RESTRICTED },
    { SEP_ITEM,                             0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("Save &Bookmark...\tCtrl-Shift-S"), IDM_SAVEAS_BOOKMARK,     MF_NOT_IN_RESTRICTED },
    { _TRN("Open in &Adobe Reader"),        IDM_VIEW_WITH_ACROBAT,      MF_NOT_IN_RESTRICTED },
    { _TRN("Open in &Foxit Reader"),        IDM_VIEW_WITH_FOXIT,        MF_NOT_IN_RESTRICTED },
    { _TRN("Open in PDF-XChange"),          IDM_VIEW_WITH_PDF_XCHANGE,  MF_NOT_IN_RESTRICTED },
    { _TRN("Send by &E-mail..."),           IDM_SEND_BY_EMAIL,          MF_NOT_IN_RESTRICTED },
    { SEP_ITEM,                             0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("P&roperties\tCtrl-D"),          IDM_PROPERTIES,             0 },
    { SEP_ITEM,                             0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("E&xit\tCtrl-Q"),                IDM_EXIT,                   0 }
};

MenuDef menuDefView[] = {
    { _TRN("Single Page"),                  IDM_VIEW_SINGLE_PAGE,       0  },
    { _TRN("Facing"),                       IDM_VIEW_FACING,            0  },
    { _TRN("Book View"),                    IDM_VIEW_BOOK,              0  },
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
    { SEP_ITEM, 0, 0  },
    { _TRN("Select &All\tCtrl-A"),          IDM_SELECT_ALL,             0  },
    { _TRN("&Copy Selection\tCtrl-C"),      IDM_COPY_SELECTION,         0  },
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

MenuDef menuDefLang[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
#if 0
    { _TRN("Contribute Translation"),       IDM_CONTRIBUTE_TRANSLATION, MF_NOT_IN_RESTRICTED },
    { SEP_ITEM,                             0,                          MF_NOT_IN_RESTRICTED },
#endif
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_NOT_IN_RESTRICTED }
};

MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_NOT_IN_RESTRICTED },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_NOT_IN_RESTRICTED },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_NOT_IN_RESTRICTED },
    { SEP_ITEM,                             0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("&About"),                       IDM_ABOUT,                  0  }
#ifdef SHOW_DEBUG_MENU_ITEMS
    ,{ SEP_ITEM,                            0,                          MF_NOT_IN_RESTRICTED },
    { "Crash me",                           IDM_CRASH_ME,               MF_NO_TRANSLATE  },
    { "Stress test running",                IDM_THREAD_STRESS,          MF_NO_TRANSLATE  }
#endif
};

static void AddFileMenuItem(HMENU menuFile, DisplayState *state, UINT index)
{
    assert(state && menuFile);
    if (!state || ! menuFile) return;

    ScopedMem<TCHAR> menuString(Str::Format(_T("&%d) %s"), (index + 1) % 10, Path::GetBaseName(state->filePath)));
    UINT menuId = IDM_FILE_HISTORY_FIRST + index;
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
}

static void EmptyMenu(HMENU m)
{
    for (;;) {
        bool ok = RemoveMenu(m, 0, MF_BYPOSITION);
        if (!ok)
            return;
    }
}

static HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int n, HMENU m=NULL)
{
    if (m)
        EmptyMenu(m);
    else
        m = CreateMenu();

    for (int i=0; i < n; i++) {
        MenuDef md = menuDefs[i];
        const char *title = md.title;
        if (md.flags & MF_REMOVED)
            continue;

        if (gRestrictedUse && (md.flags & MF_NOT_IN_RESTRICTED))
            continue;

        if (Str::Eq(title, SEP_ITEM)) {
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            ScopedMem<TCHAR> tmp(Str::Conv::FromUtf8(title));
            AppendMenu(m, MF_STRING, (UINT_PTR)md.id, tmp);
        } else {
            const TCHAR *tmp =  Translations_GetTranslation(title);
            AppendMenu(m, MF_STRING, (UINT_PTR)md.id, tmp);
        }
    }
    return m;
}

static void AppendRecentFilesToMenu(HMENU m)
{
    if (gRestrictedUse) return;
    if (gFileHistory.IsEmpty()) return;

    for (int index = 0; index < MAX_RECENT_FILES_IN_MENU; index++) {
        DisplayState *state = gFileHistory.Get(index);
        if (!state)
            break;
        assert(state->filePath);
        if (state->filePath)
            AddFileMenuItem(m, state, index);
        if (MAX_RECENT_FILES_IN_MENU == index)
            DBG_OUT("  not adding, reached max %d items\n", MAX_RECENT_FILES_IN_MENU);
    }

    InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
}

static void AddOrRemoveFileMenuAtPos(int n, bool add)
{
    if (add)
        menuDefFile[n].flags &= ~MF_REMOVED;
    else
        menuDefFile[n].flags |= MF_REMOVED;
}

// Suppress menu items that depend on specific software being installed:
// e-mail client, Adobe Reader, Foxit, PDF-XChange
static void SetupProgramDependentMenus(WindowInfo *win)
{
    bool acrobat = CanViewWithAcrobat(win);
    bool foxit = CanViewWithFoxit(win);
    bool pdfexch = CanViewWithPDFXChange(win);
    bool email = CanSendAsEmailAttachment(win);
    for (int i = 0; i < dimof(menuDefFile); i++) {
        if (IDM_VIEW_WITH_ACROBAT == menuDefFile[i].id)
            AddOrRemoveFileMenuAtPos(i, acrobat);
        else if (IDM_VIEW_WITH_FOXIT == menuDefFile[i].id)
            AddOrRemoveFileMenuAtPos(i, foxit);
        else if (IDM_VIEW_WITH_PDF_XCHANGE ==  menuDefFile[i].id)
            AddOrRemoveFileMenuAtPos(i, pdfexch);
        else if (IDM_SEND_BY_EMAIL == menuDefFile[i].id)
            AddOrRemoveFileMenuAtPos(i, email);
    }
}

static HMENU RebuildFileMenu(WindowInfo *win, HMENU menu)
{
    SetupProgramDependentMenus(win);
    menu = BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), menu);
    AppendRecentFilesToMenu(menu);
    return menu;
}

static void BuildMenu(WindowInfo *win)
{
    assert(NULL == win->menu);

    HMENU mainMenu = CreateMenu();
    HMENU m = RebuildFileMenu(win, NULL);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, dimof(menuDefView));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, dimof(menuDefGoTo));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    m = BuildMenuFromMenuDef(menuDefZoom, dimof(menuDefZoom));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));
    m = BuildMenuFromMenuDef(menuDefLang, dimof(menuDefLang));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));

    win->menu = mainMenu;
    SetMenu(win->hwndFrame, win->menu);
}

WindowInfo* FindWindowInfoByHwnd(HWND hwnd)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (hwnd == win->hwndFrame      ||
            hwnd == win->hwndCanvas     ||
            hwnd == win->hwndReBar      ||
            hwnd == win->hwndFindBox    ||
            hwnd == win->hwndFindStatus ||
            hwnd == win->hwndPageBox    ||
            hwnd == win->hwndTocBox     ||
            hwnd == win->hwndTocTree    ||
            hwnd == win->hwndSpliter    ||
            hwnd == win->hwndPdfProperties)
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

// Find the first windows showing a given PDF file 
WindowInfo* FindWindowInfoByFile(TCHAR *file)
{
    ScopedMem<TCHAR> normFile(Path::Normalize(file));
    if (!normFile)
        return NULL;

    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (win->loadedFilePath && Path::IsSame(win->loadedFilePath, normFile))
            return win;
    }

    return NULL;
}

/* Get password for a given 'fileName', can be NULL if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
TCHAR *WindowInfo::GetPassword(const TCHAR *fileName, unsigned char *fileDigest, unsigned char decryptionKeyOut[32], bool *saveKey)
{
    DisplayState *fileFromHistory = gFileHistory.Find(fileName);
    if (fileFromHistory && fileFromHistory->decryptionKey) {
        ScopedMem<char> fingerprint(Str::MemToHex(fileDigest, 16));
        *saveKey = Str::StartsWith(fileFromHistory->decryptionKey, fingerprint.Get());
        if (*saveKey && Str::HexToMem(fileFromHistory->decryptionKey + 32, decryptionKeyOut, 32))
            return NULL;
    }

    *saveKey = false;
    fileName = Path::GetBaseName(fileName);
    return Dialog_GetPassword(this->hwndFrame, fileName, gGlobalPrefs.m_rememberOpenedFiles ? saveKey : NULL);
}

/* Caller needs to free() the result. */
static TCHAR *Prefs_GetFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

/* Caller needs to free() the result */
static TCHAR *GetUniqueCrashDumpPath()
{
    TCHAR *path;
    TCHAR *fileName;
    for (int n = 0; n <= 20; n++) {
        if (n == 0) {
            fileName = Str::Dup(_T("SumatraPDF.dmp"));
        } else {
            fileName = Str::Format(_T("SumatraPDF-%d.dmp"), n);
        }
        path = AppGenDataFilename(fileName);
        free(fileName);
        if (!File::Exists(path) || (n==20))
            return path;
        free(path);
    }
    return NULL;
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
        Win::Menu::Enable(m, gZoomMenuIds[i].itemId, canZoom);

    if (IDM_ZOOM_100 == menuItemId)
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;
    CheckMenuRadioItem(m, IDM_ZOOM_FIRST, IDM_ZOOM_LAST, menuItemId, MF_BYCOMMAND);
    if (IDM_ZOOM_ACTUAL_SIZE == menuItemId)
        CheckMenuRadioItem(m, IDM_ZOOM_100, IDM_ZOOM_100, IDM_ZOOM_100, MF_BYCOMMAND);
}

static void MenuUpdateZoom(WindowInfo *win)
{
    float zoomVirtual = gGlobalPrefs.m_defaultZoom;
    if (win->dm)
        zoomVirtual = win->dm->zoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, NULL != win->dm);
}

static void RememberWindowPosition(WindowInfo *win)
{
    // update global windowState for next default launch when either
    // no pdf is opened or a document without window dimension information
    if (win->presentation)
        gGlobalPrefs.m_windowState = win->_windowStateBeforePresentation;
    else if (win->fullScreen)
        gGlobalPrefs.m_windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_NORMAL;

    gGlobalPrefs.m_tocDx = WindowRect(win->hwndTocBox).dx;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL == gGlobalPrefs.m_windowState &&
        !IsIconic(win->hwndFrame) && !win->presentation) {
        // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
        //       the non-maximized dimensions for proper restoration)
        gGlobalPrefs.m_windowPos = WindowRect(win->hwndFrame);
    }
}

static void UpdateDisplayStateWindowRect(WindowInfo *win, DisplayState *ds, bool updateGlobal=true)
{
    if (updateGlobal)
        RememberWindowPosition(win);

    ds->windowState = gGlobalPrefs.m_windowState;
    ds->windowPos = gGlobalPrefs.m_windowPos;
    ds->tocDx = gGlobalPrefs.m_tocDx;
}

static void UpdateCurrentFileDisplayStateForWin(WindowInfo *win)
{
    if (!win)
        return;

    RememberWindowPosition(win);
    if (WS_SHOWING_PDF != win->state)
        return;
    if (!win->dm)
        return;

    const TCHAR *fileName = win->dm->fileName();
    assert(fileName);
    if (!fileName)
        return;

    DisplayState *state = gFileHistory.Find(fileName);
    assert(state || !gGlobalPrefs.m_rememberOpenedFiles);
    if (!state)
        return;

    if (!win->dm->displayStateFromModel(state))
        return;
    state->useGlobalValues = gGlobalPrefs.m_globalPrefsOnly;
    UpdateDisplayStateWindowRect(win, state, false);
    win->DisplayStateFromToC(state);
}

static bool Prefs_Save(void)
{
    // don't save preferences for plugin windows
    if (gPluginMode)
        return FALSE;

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++)
        UpdateCurrentFileDisplayStateForWin(gWindows[i]);

    ScopedMem<TCHAR> path(Prefs_GetFileName());
    return Prefs::Save(path, &gGlobalPrefs, &gFileHistory);
}

void WindowInfo::Reload(bool autorefresh)
{
    if (this->pdfsync)
        this->pdfsync->discard_index();
    DisplayState ds;
    ds.useGlobalValues = gGlobalPrefs.m_globalPrefsOnly;
    if (!this->dm || !this->dm->displayStateFromModel(&ds)) {
        if (!autorefresh && !this->dm && this->loadedFilePath)
            LoadPdf(this->loadedFilePath, this);
        return;
    }
    UpdateDisplayStateWindowRect(this, &ds);
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
    ScopedMem<TCHAR> path(Str::Dup(this->loadedFilePath));
    LoadPdfIntoWindow(path, this, &ds, false, tryRepair, true, false);

    if (this->dm) {
        // save a newly remembered password into file history so that
        // we don't ask again at the next refresh
        DisplayState *state = gFileHistory.Find(ds.filePath);
        char *decryptionKey = this->dm->pdfEngine->getDecryptionKey();
        if (state && !Str::Eq(state->decryptionKey, decryptionKey)) {
            free(state->decryptionKey);
            state->decryptionKey = decryptionKey;
        }
        else
            free(decryptionKey);
    }
}

static void WindowInfo_Delete(WindowInfo *win)
{
    // must DestroyWindow(win->hwndPdfProperties) before removing win from
    // the list of properties beacuse WM_DESTROY handler needs to find
    // WindowInfo for its HWND
    if (win->hwndPdfProperties) {
        DestroyWindow(win->hwndPdfProperties);
        assert(NULL == win->hwndPdfProperties);
    }
    win->AbortFinding();
    gWindows.Remove(win);

    DragAcceptFiles(win->hwndCanvas, FALSE);
    DeleteOldSelectionInfo(win);

    delete win;
}

static void UpdateToolbarBg(HWND hwnd, bool enabled)
{
    DWORD newStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (enabled)
        newStyle |= SS_WHITERECT;
    else
        newStyle &= ~SS_WHITERECT;
    SetWindowLong(hwnd, GWL_STYLE, newStyle);
}

static void WindowInfo_UpdateFindbox(WindowInfo *win) {
    UpdateToolbarBg(win->hwndFindBg, win->dm != NULL);
    UpdateToolbarBg(win->hwndPageBg, win->dm != NULL);

    InvalidateRect(win->hwndToolbar, NULL, true);
    if (!win->dm) {  // Avoid focus on Find box
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorArrow);
        HideCaret(NULL);
    } else {
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorIBeam);
        ShowCaret(NULL);
    }
}

static bool FileCloseMenuEnabled(void) {
    for (size_t i = 0; i < gWindows.Count(); i++)
        if (gWindows[i]->state != WS_ABOUT)
            return true;
    return false;
}

bool TbIsSeparator(ToolbarButtonInfo *tbi) {
    return tbi->bmpIndex < 0;
}
 
static void ToolbarUpdateStateForWindow(WindowInfo *win) {
    const LPARAM enable = (LPARAM)MAKELONG(1,0);
    const LPARAM disable = (LPARAM)MAKELONG(0,0);

    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        if (TbIsSeparator(&gToolbarButtons[i]))
            continue;

        int cmdId = gToolbarButtons[i].cmdId;
        // Assume the button is enabled.
        LPARAM buttonState = enable;

        if (gRestrictedUse && gToolbarButtons[i].flags & TBF_RESTRICTED) // If restricted, disable
            buttonState = disable;
        else if (WS_SHOWING_PDF != win->state) { // If no file open, only enable open button.
            if (IDM_OPEN != cmdId)
                buttonState = disable;
        }
        else // Figure out what to show.
        {
            switch (cmdId)
            {
                case IDM_OPEN:
                    // don'opening different files isn't allowed in plugin mode
                    if (win->pluginParent)
                        buttonState = disable;
                    break;

                case IDM_FIND_NEXT:
                case IDM_FIND_PREV: 
                    // TODO: Update on whether there's more to find, not just on whether there is text.
                    if (Win::GetTextLen(win->hwndFindBox) == 0)
                        buttonState = disable;
                    break;

                case IDM_GOTO_NEXT_PAGE:
                    if (win->dm->currentPageNo() == win->dm->pageCount())
                         buttonState = disable;
                    break;
                case IDM_GOTO_PREV_PAGE:
                    if (win->dm->currentPageNo() == 1)
                        buttonState = disable;
                    break;
            }
        }
  
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, cmdId, buttonState);
    }
}

static void MenuUpdatePrintItem(WindowInfo *win) {
    bool filePrintEnabled = false;
    if (win->dm && win->dm->pdfEngine)
        filePrintEnabled = true;
    bool filePrintAllowed = !filePrintEnabled || win->dm->pdfEngine->hasPermission(PDF_PERM_PRINT);

    int ix;
    for (ix = 0; ix < dimof(menuDefFile) && menuDefFile[ix].id != IDM_PRINT; ix++);
    assert(ix < dimof(menuDefFile));
    if (ix < dimof(menuDefFile)) {
        const TCHAR *printItem = Translations_GetTranslation(menuDefFile[ix].title);
        if (!filePrintAllowed)
            printItem = _TR("&Print... (denied)");
        ModifyMenu(win->menu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
    }

    Win::Menu::Enable(win->menu, IDM_PRINT, filePrintEnabled && filePrintAllowed);
}

static void MenuUpdateStateForWindow(WindowInfo *win) {
    static UINT menusToDisableIfNoPdf[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SAVEAS_BOOKMARK, IDM_SEND_BY_EMAIL,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE, 
        IDM_SELECT_ALL, IDM_COPY_SELECTION, IDM_PROPERTIES, 
        IDM_VIEW_PRESENTATION_MODE, IDM_THREAD_STRESS };

    assert(FileCloseMenuEnabled() == (win->loadedFilePath != NULL)); // TODO: ???
    Win::Menu::Enable(win->menu, IDM_CLOSE, FileCloseMenuEnabled());
        
    MenuUpdatePrintItem(win);

    bool enabled = WS_SHOWING_PDF == win->state && win->dm && win->dm->hasTocTree();
    Win::Menu::Enable(win->menu, IDM_VIEW_BOOKMARKS, enabled);

    bool documentSpecific = win->PdfLoaded();
    bool checked = documentSpecific ? win->tocShow : gGlobalPrefs.m_showToc;
    Win::Menu::Check(win->menu, IDM_VIEW_BOOKMARKS, checked);

    Win::Menu::Check(win->menu, IDM_VIEW_SHOW_HIDE_TOOLBAR, gGlobalPrefs.m_showToolbar);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);
    Win::Menu::Check(win->menu, IDM_THREAD_STRESS, win->threadStressRunning);

    if (WS_SHOWING_PDF == win->state) {
        Win::Menu::Enable(win->menu, IDM_GOTO_NAV_BACK, win->dm->canNavigate(-1));
        Win::Menu::Enable(win->menu, IDM_GOTO_NAV_FORWARD, win->dm->canNavigate(1));
    }

    for (int i = 0; i < dimof(menusToDisableIfNoPdf); i++) {
        UINT id = menusToDisableIfNoPdf[i];
        Win::Menu::Enable(win->menu, id, WS_SHOWING_PDF == win->state);
    }
}

static void UpdateToolbarAndScrollbarsForAllWindows(void) 
{
    for (size_t i = 0; i < gWindows.Count(); i++)
    {
        WindowInfo *win = gWindows[i];
        ToolbarUpdateStateForWindow(win);

        if (WS_SHOWING_PDF != win->state) {
            ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
            if (WS_ABOUT == win->state)
                Win::SetText(win->hwndFrame, SUMATRA_WINDOW_TITLE);
        }
    }
}

#define MIN_WIN_DX 50
#define MIN_WIN_DY 50

static void EnsureWindowVisibility(RectI *rect)
{
    RECT rc = rect->ToRECT();

    // adjust to the work-area of the current monitor (not necessarily the primary one)
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST), &mi))
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    // make sure that the window is neither too small nor bigger than the monitor
    if (rect->dx < MIN_WIN_DX || rect->dx > RectDx(&mi.rcWork))
        rect->dx = (int)min(RectDy(&mi.rcWork) * DEF_PAGE_RATIO, RectDx(&mi.rcWork));
    if (rect->dy < MIN_WIN_DY || rect->dy > RectDy(&mi.rcWork))
        rect->dy = RectDy(&mi.rcWork);

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    rc.bottom = rc.top + captionDy;
    rc.top += captionDy / 2;
    if (!IntersectRect(&mi.rcMonitor, &mi.rcWork, &rc)) {
        rect->x = mi.rcWork.left;
        rect->y = mi.rcWork.top;
    }
}

static WindowInfo* WindowInfo_CreateEmpty(void)
{
    RectI windowPos;
    if (gGlobalPrefs.m_windowPos.IsEmpty()) {
        // center the window on the primary monitor
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        windowPos.y = workArea.top;
        windowPos.dy = RectDy(&workArea);
        windowPos.dx = (int)min(windowPos.dy * DEF_PAGE_RATIO, RectDx(&workArea));
        windowPos.x = (RectDx(&workArea) - windowPos.dx) / 2;
    }
    else {
        windowPos = gGlobalPrefs.m_windowPos;
        EnsureWindowVisibility(&windowPos);
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
    BuildMenu(win);

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    CreateToolbar(win, ghinst);
    CreateTocBox(win, ghinst);
    WindowInfo_UpdateFindbox(win);
    DragAcceptFiles(win->hwndCanvas, TRUE);

    win->stopFindStatusThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    gWindows.Append(win);
    return win;
}

static void UpdateTocWidth(WindowInfo *win, const DisplayState *ds=NULL, int defaultDx=0)
{
    WindowRect rc(win->hwndTocBox);
    if (rc.IsEmpty())
        return;

    if (ds && !gGlobalPrefs.m_globalPrefsOnly)
        rc.dx = ds->tocDx;
    else if (!defaultDx)
        rc.dx = gGlobalPrefs.m_tocDx;
    // else assume the correct width has been set previously
    if (!rc.dx) // first time
        rc.dx = defaultDx;

    SetWindowPos(win->hwndTocBox, NULL, rc.x, rc.y, rc.dx, rc.dy, SWP_NOZORDER);
}

static void RecalcSelectionPosition (WindowInfo *win) {
    SelectionOnPage *   selOnPage = win->selectionOnPage;
    PdfPageInfo*        pageInfo;

    while (selOnPage != NULL) {
        pageInfo = win->dm->getPageInfo(selOnPage->pageNo);
        /* if page is not visible, we hide seletion by simply moving it off
         * the canvas */
        if (!pageInfo->visible) {
            selOnPage->selectionCanvas.x = -100;
            selOnPage->selectionCanvas.y = -100;
            selOnPage->selectionCanvas.dx = 0;
            selOnPage->selectionCanvas.dy = 0;
        } else {
            RectD selD = selOnPage->selectionPage;
            win->dm->rectCvtUserToScreen(selOnPage->pageNo, &selD);
            selOnPage->selectionCanvas = selD.Convert<int>();
        }
        selOnPage = selOnPage->next;
    }
}

static bool LoadPdfIntoWindow(
    const TCHAR *fileName, // path to the PDF
    WindowInfo *win,       // destination window
    const DisplayState *state,   // state
    bool isNewWindow,    // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool tryRepair,        // if true then try to repair the PDF if it is broken
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (isNewWindow=false)
{
    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.m_globalPrefsOnly || state && state->useGlobalValues))
        state = NULL;

    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    int startPage = 1;
    ScrollState ss = { 1, -1, -1 };
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState;
    int showType = gGlobalPrefs.m_windowState == WIN_STATE_MAXIMIZED || showAsFullScreen ? SW_MAXIMIZE : SW_NORMAL;

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

    /* TODO: need to get rid of that, but not sure if that won't break something
       i.e. GetCanvasSize() caches size of canvas and some code might depend
       on this being a cached value, not the real value at the time of calling */
    win->GetCanvasSize();

    DisplayModel *previousmodel = win->dm;
    win->AbortFinding();

    free(win->loadedFilePath);
    win->loadedFilePath = Str::Dup(fileName);
    win->dm = DisplayModel::CreateFromFileName(win, fileName, displayMode, startPage);

    if (!win->dm) {
        DBG_OUT("failed to load file %s\n", fileName);
        win->needrefresh = true;
        // if there is an error while reading the pdf and pdfrepair is not requested
        // then fallback to the previous state
        if (!tryRepair) {
            win->dm = previousmodel;
        } else {
            delete previousmodel;
            win->state = WS_ERROR_LOADING_PDF;
            ScopedMem<TCHAR> title(Str::Format(_T("%s - %s"), Path::GetBaseName(fileName), SUMATRA_WINDOW_TITLE));
            Win::SetText(win->hwndFrame, title);
            goto Error;
        }
    } else {
        if (previousmodel && Str::Eq(win->dm->fileName(), previousmodel->fileName()))
            gRenderCache.KeepForDisplayModel(previousmodel, win->dm);
        delete previousmodel;
        win->needrefresh = false;
        win->prevCanvasBR.x = win->prevCanvasBR.y = -1;
    }

    float zoomVirtual = gGlobalPrefs.m_defaultZoom;
    int rotation = DEFAULT_ROTATION;

    win->state = WS_SHOWING_PDF;
    if (state) {
        if (win->dm->validPageNo(startPage)) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else let win->dm->relayout scroll to fit the page (again)
        }
        else if (startPage > win->dm->pageCount())
            ss.page = win->dm->pageCount();
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;

        win->tocShow = state->showToc;
        free(win->tocState);
        if (state->tocState)
            win->tocState = (int *)memdup(state->tocState, (state->tocState[0] + 1) * sizeof(int));
        else
            win->tocState = NULL;
    }
    else {
        win->tocShow = gGlobalPrefs.m_showToc;
    }
    UpdateTocWidth(win, state);

    // Review needed: Is the following block really necessary?
    /*
    // The WM_SIZE message must be sent *after* updating win->showToc
    // otherwise the bookmark window reappear even if state->showToc=false.
    ClientRect rect(win->hwndFrame);
    SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    */

    win->dm->relayout(zoomVirtual, rotation);
    // Only restore the scroll state when everything is visible
    // (otherwise we might have to relayout twice, which can take
    //  a while for longer documents)
    // win->dm->setScrollState(&ss);

    if (!isNewWindow) {
        win->RedrawAll();
        OnMenuFindMatchCase(win);
    }
    WindowInfo_UpdateFindbox(win);

    int pageCount = win->dm->pageCount();
    if (pageCount > 0) {
        UpdateToolbarPageText(win, pageCount);
        UpdateToolbarFindText(win);
    }

    const TCHAR *baseName = Path::GetBaseName(win->dm->fileName());
    TCHAR *title = Str::Format(_T("%s - %s"), baseName, SUMATRA_WINDOW_TITLE);
    if (win->needrefresh) {
        TCHAR *msg = Str::Format(_TR("[Changes detected; refreshing] %s"), title);
        free(title);
        title = msg;
    }
    Win::SetText(win->hwndFrame, title);
    free(title);

Error:
    if (isNewWindow || placeWindow && state) {
        assert(win);
        if (isNewWindow && state && !state->windowPos.IsEmpty()) {
            RectI rect = state->windowPos;
            // Make sure it doesn't have a position like outside of the screen etc.
            rect_shift_to_work_area(&rect.ToRECT(), FALSE);
            // This shouldn't happen until win->state != WS_ABOUT, so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect.x, rect.y, rect.dx, rect.dy, TRUE);
        }
#if 0 // not ready yet
        else {
            IntelligentWindowResize(win);
        }
#endif
        if (showWin) {
            ShowWindow(win->hwndFrame, showType);
        }
        UpdateWindow(win->hwndFrame);
    }
    if (win->tocLoaded)
        win->ClearTocBox();
    if (win->dm)
        win->dm->setScrollState(&ss);
    if (win->PdfLoaded() && win->tocShow) {
        if (win->dm->hasTocTree()) {
            win->ShowTocBox();
        } else {
            // Hide the now useless ToC sidebar and force an update afterwards
            win->HideTocBox();
            win->RedrawAll(true);
        }
    }
    UpdateToolbarAndScrollbarsForAllWindows();
    if (win->state == WS_ERROR_LOADING_PDF) {
        win->RedrawAll();
        return false;
    }
    // This should only happen after everything else is ready
    if ((isNewWindow || placeWindow) && showWin && showAsFullScreen)
        WindowInfo_EnterFullscreen(win);
    if (!isNewWindow && win->presentation && win->dm)
        win->dm->setPresentationMode(true);

    return true;
}

class FileChangeCallback : public CallbackFunc, public UIThreadWorkItem
{
public:
    FileChangeCallback(WindowInfo *win) : UIThreadWorkItem(win) { }

    virtual void Callback(void *arg) {
        // We cannot call win->Reload directly as it could cause race conditions
        // between the watching thread and the main thread (and only pass a copy of this
        // callback to the UIThreadMarshaller, as the object will be deleted after use)
        gUIThreadMarshaller.Queue(new FileChangeCallback(win));
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win)) {
            // delay the reload slightly, in case we get another request immediately ofter this one
            SetTimer(win->hwndCanvas, AUTO_RELOAD_TIMER_ID, AUTO_RELOAD_DELAY_IN_MS, NULL);
        }
    }
};

#ifndef THREAD_BASED_FILEWATCH
static void RefreshUpdatedFiles(void) {
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        if (win->watcher)
            win->watcher->CheckForChanges();
    }
}
#endif

static void CheckPositionAndSize(DisplayState* ds)
{
    if (!ds)
        return;

    if (ds->windowPos.IsEmpty())
        ds->windowPos = gGlobalPrefs.m_windowPos;
    EnsureWindowVisibility(&ds->windowPos);
}

bool IsComicBook(const TCHAR *fileName)
{
    if (Str::EndsWithI(fileName, _T(".cbz")))
        return true;
#if 0 // not yet
    if (Str::EndsWithI(fileName, _T(".cbr")))
        return true;
#endif
    return false;
}

// Load *.cbz / *.cbr file
// TODO: far from being done
static WindowInfo* LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin)
{    
    zlib_filefunc64_def ffunc;
    unzFile uf;
    fill_win32_filefunc64(&ffunc);

    uf = unzOpen2_64(fileName, &ffunc);
    if (!uf) {
        goto Error;
    }
    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        goto Error;
    }

    // extract all contained files one by one
    int pngCount = 0, jpegCount = 0;
    for (int n = 0; n < ginfo.number_entry; n++) {
        char filename[MAX_PATH];
        unz_file_info64 finfo;
        err = unzGetCurrentFileInfo64(uf, &finfo, filename, dimof(filename), NULL, 0, NULL, 0);
        if (err != UNZ_OK) {
            goto Error;
        }

        err = unzOpenCurrentFilePassword(uf, NULL);
        if (err != UNZ_OK) {
            goto Error;
        }

        if (Str::EndsWithI(filename, ".png"))
            pngCount++;
        else if (Str::EndsWithI(filename, ".jpg") || Str::EndsWithI(filename, ".jpeg"))
            jpegCount++;

        err = unzCloseCurrentFile(uf);
        if (err != UNZ_OK) {
            goto Error;
        }

        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

    unzClose(uf);

    // TODO: if (pngCount + jpegCount > 0) => treat it as a valid comic book file
    return NULL;

Error:
    if (uf)
        unzClose(uf);
    return NULL;
}

static WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win, bool showWin)
{
    assert(fileName);
    if (!fileName) return NULL;

    ScopedMem<TCHAR> fullpath(Path::Normalize(fileName));
    if (!fullpath)
        return win;

    bool isNewWindow = false;
    if (!win && 1 == gWindows.Count() && WS_ABOUT == gWindows[0]->state) {
        win = gWindows[0];
    }
    else if (!win || WS_SHOWING_PDF == win->state) {
        isNewWindow = true;
        win = WindowInfo_CreateEmpty();
        if (!win)
            return NULL;
    }

    DisplayState *ds = gFileHistory.Find(fullpath);
    if (ds) {
        AdjustRemovableDriveLetter(fullpath);
        CheckPositionAndSize(ds);
    }

    if (!LoadPdfIntoWindow(fullpath, win, ds, isNewWindow, true, showWin, true)) {
        /* failed to open */
        gFileHistory.MarkFileInexistent(fullpath);
        return win;
    }

    if (!win->watcher)
        win->watcher = new FileWatcher(new FileChangeCallback(win));
    win->watcher->Init(fullpath);
#ifdef THREAD_BASED_FILEWATCH
    win->watcher->StartWatchThread();
#endif

    if (!gRestrictedUse) {
        UINT res = CreateSynchronizer(fullpath, &win->pdfsync);
        // expose SyncTeX in the UI
        if (PDFSYNCERR_SUCCESS == res)
            gGlobalPrefs.m_enableTeXEnhancements;
    }

    if (gGlobalPrefs.m_rememberOpenedFiles)
        gFileHistory.MarkFileLoaded(fullpath);

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    SHAddToRecentDocs(SHARD_PATH, fullpath);

    return win;
}

WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win, bool showWin)
{
#if 0 // not yet
    if (IsComicBook(fileName))
        return LoadComicBook(fileName, win, showWin);
#endif
    return LoadPdf(fileName, win, showWin);
}

// The current page edit box is updated with the current page number
void DisplayModel::pageChanged()
{
    WindowInfo *win = _appData;
    assert(win);
    if (!win) return;

    int currPageNo = currentPageNo();
    int pageCount = win->dm->pageCount();
    if (pageCount > 0) {
        if (INVALID_PAGE_NO != currPageNo) {
            ScopedMem<TCHAR> buf(Str::Format(_T("%d"), currPageNo));
            Win::SetText(win->hwndPageBox, buf);
            ToolbarUpdateStateForWindow(win);
        }
        if (currPageNo != win->currPageNo) {
            win->UpdateTocSelection(currPageNo);
            if (ZOOM_FIT_CONTENT == _zoomVirtual)
                // re-allow hiding the scroll bars
                win->prevCanvasBR.x = win->prevCanvasBR.y = -1;
            win->currPageNo = currPageNo;
        }
    }
}

/* Send the request to render a given page to a rendering thread */
void DisplayModel::StartRenderingPage(int pageNo)
{
    gRenderCache.Render(this, pageNo);
}

void DisplayModel::clearAllRenderings(void)
{
    gRenderCache.CancelRendering(this);
    gRenderCache.FreeForDisplayModel(this);
}

void DisplayModel::setScrollbarsState(void)
{
    WindowInfo *win = this->_appData;
    assert(win);
    if (!win) return;

    SCROLLINFO      si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    int canvasDx = _canvasSize.dx;
    int canvasDy = _canvasSize.dy;
    int drawAreaDx = drawAreaSize.dx;
    int drawAreaDy = drawAreaSize.dy;

    // When hiding the scroll bars and fitting content, it could be that we'd have to
    // display the scroll bars right again for the new zoom. Make sure we haven't just done
    // that - or if so, force the scroll bars to remain visible.
    if (ZOOM_FIT_CONTENT == _zoomVirtual) {
        if (win->prevCanvasBR.y == drawAreaDy && win->prevCanvasBR.x == drawAreaDx + GetSystemMetrics(SM_CXVSCROLL)) {
            if (drawAreaDy == canvasDy)
                canvasDy++;
        }
        else if (win->prevCanvasBR.x == drawAreaDx && win->prevCanvasBR.y == drawAreaDy + GetSystemMetrics(SM_CYHSCROLL)) {
            if (drawAreaDx == canvasDx)
                canvasDx++;
        }
        else {
            win->prevCanvasBR.x = drawAreaDx;
            win->prevCanvasBR.y = drawAreaDy;
        }
    }

    if (drawAreaDx >= canvasDx) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = areaOffset.x;
        si.nMin = 0;
        si.nMax = canvasDx-1;
        si.nPage = drawAreaDx;
    }
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);

    // When hiding the scroll bars and fitting width, it could be that we'd have to
    // display the scroll bars right again for the new width. Make sure we haven't just done
    // that - or if so, force the vertical scroll bar to remain visible.
    if (ZOOM_FIT_WIDTH == _zoomVirtual || ZOOM_FIT_PAGE == _zoomVirtual) {
        if (win->prevCanvasBR.y != drawAreaDy || win->prevCanvasBR.x != drawAreaDx + GetSystemMetrics(SM_CXVSCROLL)) {
            win->prevCanvasBR.x = drawAreaDx;
            win->prevCanvasBR.y = drawAreaDy;
        }
        else if (drawAreaDy == canvasDy) {
            canvasDy++;
        }
    }

    if (drawAreaDy >= canvasDy) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = areaOffset.y;
        si.nMin = 0;
        si.nMax = canvasDy-1;
        si.nPage = drawAreaDy;

        if (ZOOM_FIT_PAGE != _zoomVirtual) {
            // keep the top/bottom 5% of the previous page visible after paging down/up
            si.nPage = (UINT)(si.nPage * 0.95);
            si.nMax -= drawAreaDy - si.nPage;
        }
    }
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
}

void AssociateExeWithPdfExtension(void)
{
    DoAssociateExeWithPdfExtension(HKEY_CURRENT_USER);
    DoAssociateExeWithPdfExtension(HKEY_LOCAL_MACHINE);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, 0, 0);

    // Remind the user, when a different application takes over
    gGlobalPrefs.m_pdfAssociateShouldAssociate = TRUE;
    gGlobalPrefs.m_pdfAssociateDontAskAgain = FALSE;
}

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd)
{
    if (IsRunningInPortableMode() || gRestrictedUse || gPluginMode)
        return false;

    if (IsExeAssociatedWithPdfExtension())
        return true;

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs.m_pdfAssociateDontAskAgain) {
        INT_PTR result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs.m_pdfAssociateDontAskAgain);
        if (DIALOG_NO_PRESSED == result) {
            gGlobalPrefs.m_pdfAssociateShouldAssociate = FALSE;
        } else {
            assert(DIALOG_OK_PRESSED == result);
            gGlobalPrefs.m_pdfAssociateShouldAssociate = TRUE;
        }
    }

    if (!gGlobalPrefs.m_pdfAssociateShouldAssociate)
        return false;

    AssociateExeWithPdfExtension();
    return true;
}

static void OnDropFiles(WindowInfo *win, HDROP hDrop)
{
    int         i;
    TCHAR       filename[MAX_PATH];
    const int   files_count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (i = 0; i < files_count; i++)
    {
        DragQueryFile(hDrop, i, filename, MAX_PATH);
        if (Str::EndsWithI(filename, _T(".lnk"))) {
            ScopedMem<TCHAR> resolved(ResolveLnk(filename));
            if (resolved)
                Str::BufSet(filename, MAX_PATH, resolved);
        }
        // The first dropped document may override the current window
        LoadDocument(filename, i == 0 ? win : NULL);
    }
    DragFinish(hDrop);

    if (files_count > 0)
        win->RedrawAll();
}

bool WindowInfo::DoubleBuffer_New()
{
    this->DoubleBuffer_Delete();

    this->hdc = GetDC(this->hwndCanvas);
    this->hdcToDraw = this->hdc;
    this->GetCanvasSize();

#ifdef DOUBLE_BUFFER
    if (0 == this->winDx() || 0 == this->winDy())
        return true;

    this->hdcDoubleBuffer = CreateCompatibleDC(this->hdc);
    if (!this->hdcDoubleBuffer)
        return false;

    this->bmpDoubleBuffer = CreateCompatibleBitmap(this->hdc, this->winDx(), this->winDy());
    if (!this->bmpDoubleBuffer) {
        this->DoubleBuffer_Delete();
        return false;
    }
    SelectObject(this->hdcDoubleBuffer, this->bmpDoubleBuffer);
    /* fill out everything with background color */
    RectI r(PointI(0, 0), this->winSize());
    FillRect(this->hdcDoubleBuffer, &r.ToRECT(), this->presentation ? gBrushBlack : gBrushBg);
    this->hdcToDraw = this->hdcDoubleBuffer;
#endif

    return true;
}

static bool ShowNewVersionDialog(WindowInfo *win, const TCHAR *newVersion)
{
    Dialog_NewVersion_Data data = {0};
    data.currVersion = UPDATE_CHECK_VER;
    data.newVersion = newVersion;
    data.skipThisVersion = false;
    INT_PTR res = Dialog_NewVersionAvailable(win->hwndFrame, &data);
    if (data.skipThisVersion) {
        free(gGlobalPrefs.m_versionToSkip);
        gGlobalPrefs.m_versionToSkip = Str::Dup(newVersion);
    }
    return DIALOG_OK_PRESSED == res;
}

static DWORD OnUrlDownloaded(WindowInfo *win, HttpReqCtx *ctx, bool silent)
{
    if (ctx->error)
        return ctx->error;
    if (!Str::StartsWith(ctx->url, SUMATRA_UPDATE_INFO_URL))
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

    ScopedMem<TCHAR> verTxt(Str::Conv::FromAnsi(txt));
    /* reduce the string to a single line (resp. drop the newline) */
    Str::TransChars(verTxt, _T("\r\n"), _T("\0\0"));
    if (CompareVersion(verTxt, UPDATE_CHECK_VER) <= 0) {
        /* if automated => don't notify that there is no new version */
        if (!silent) {
            MessageBox(win->hwndFrame, _TR("You have the latest version."),
                       _TR("SumatraPDF Update"), MB_ICONINFORMATION | MB_OK);
        }
        return 0;
    }

    // if automated, respect gGlobalPrefs.m_versionToSkip
    if (silent && Str::EqI(gGlobalPrefs.m_versionToSkip, verTxt))
        return 0;

    bool download = ShowNewVersionDialog(win, verTxt);
    if (download)
        LaunchBrowser(SVN_UPDATE_LINK);

    return 0;
}

class UpdateDownloadWorkItem : public UIThreadWorkItem, public CallbackFunc
{
    bool autoCheck;
    HttpReqCtx *ctx;

public:
    UpdateDownloadWorkItem(WindowInfo *win, bool autoCheck) :
        UIThreadWorkItem(win), autoCheck(autoCheck), ctx(NULL) { }

    virtual void Callback(void *arg) {
        ctx = (HttpReqCtx *)arg;
        gUIThreadMarshaller.Queue(this);
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && ctx) {
            DWORD error = OnUrlDownloaded(win, ctx, autoCheck);
            if (error && !autoCheck) {
                // notify the user about the error during a manual update check
                ScopedMem<TCHAR> msg(Str::Format(_TR("Can't connect to the Internet (error %#x)."), error));
                MessageBox(win->hwndFrame, msg, _TR("SumatraPDF Update"), MB_ICONEXCLAMATION | MB_OK);
            }
        }
        delete ctx;
    }
};

void DownloadSumatraUpdateInfo(WindowInfo *win, bool autoCheck)
{
    if (gRestrictedUse || gPluginMode)
        return;
    assert(win);
    HWND hwndToNotify = win->hwndFrame;

    /* For auto-check, only check if at least a day passed since last check */
    if (autoCheck && gGlobalPrefs.m_lastUpdateTime) {
        FILETIME lastUpdateTimeFt;
        _HexToMem(gGlobalPrefs.m_lastUpdateTime, &lastUpdateTimeFt);
        FILETIME currentTimeFt;
        GetSystemTimeAsFileTime(&currentTimeFt);
        int secs = FileTimeDiffInSecs(&currentTimeFt, &lastUpdateTimeFt);
        assert(secs >= 0);
        // if secs < 0 => somethings wrong, so ignore that case
        if ((secs > 0) && (secs < SECS_IN_DAY))
            return;
    }

    const TCHAR *url = SUMATRA_UPDATE_INFO_URL _T("?v=") UPDATE_CHECK_VER;
    new HttpReqCtx(url, new UpdateDownloadWorkItem(win, autoCheck));

    free(gGlobalPrefs.m_lastUpdateTime);
    gGlobalPrefs.m_lastUpdateTime = GetSystemTimeAsStr();
}

static void PaintTransparentRectangle(WindowInfo *win, HDC hdc, RectI *rect, COLORREF selectionColor, BYTE alpha = 0x5f, int margin = 1) {
    // don't draw selection parts not visible on screen
    RectI screen(-margin, -margin, win->winDx() + 2 * margin, win->winDy() + 2 * margin);
    RectI isect = rect->Intersect(screen);
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

static void UpdateTextSelection(WindowInfo *win, bool select=true)
{
    assert(win && win->dm);
    if (!win || !win->dm) return;

    if (select) {
        int pageNo;
        double dX = win->selectionRect.x + win->selectionRect.dx;
        double dY = win->selectionRect.y + win->selectionRect.dy;
        if (win->dm->cvtScreenToUser(&pageNo, &dX, &dY))
            win->dm->textSelection->SelectUpTo(pageNo, dX, dY);
    }

    DeleteOldSelectionInfo(win);

    PdfSel *result = &win->dm->textSelection->result;
    for (int i = result->len - 1; i >= 0; i--) {
        SelectionOnPage *selOnPage = new SelectionOnPage;
        selOnPage->pageNo = result->pages[i];
        selOnPage->selectionPage = result->rects[i].Convert<double>();
        selOnPage->next = win->selectionOnPage;
        win->selectionOnPage = selOnPage;
    }
    win->showSelection = true;
}

static void PaintSelection (WindowInfo *win, HDC hdc) {
    if (win->mouseAction == MA_SELECTING) {
        // during selecting
        RectI selRect = win->selectionRect;
        if (selRect.dx < 0) {
            selRect.x += selRect.dx;
            selRect.dx *= -1;
        }
        if (selRect.dy < 0) {
            selRect.y += selRect.dy;
            selRect.dy *= -1;
        }

        PaintTransparentRectangle(win, hdc, &selRect, COL_SELECTION_RECT);
    } else {
        if (MA_SELECTING_TEXT == win->mouseAction)
            UpdateTextSelection(win);

        // after selection is done
        // TODO: Move recalcing to better place
        RecalcSelectionPosition(win);
        for (SelectionOnPage *sel = win->selectionOnPage; sel; sel = sel->next)
            PaintTransparentRectangle(win, hdc, &sel->selectionCanvas, COL_SELECTION_RECT);
    }
}

static void PaintForwardSearchMark(WindowInfo *win, HDC hdc) {
    PdfPageInfo *pageInfo = win->dm->getPageInfo(win->fwdsearchmarkPage);
    if (!pageInfo->visible)
        return;
    
    // Draw the rectangles highlighting the forward search results
    for (UINT i = 0; i < win->fwdsearchmarkRects.Count(); i++)
    {
        RectD recD = win->fwdsearchmarkRects[i].Convert<double>();
        win->dm->rectCvtUserToScreen(win->fwdsearchmarkPage, &recD);
        if (gGlobalPrefs.m_fwdsearchOffset > 0) {
            recD.x = pageInfo->screenX + (double)gGlobalPrefs.m_fwdsearchOffset * win->dm->zoomReal();
            recD.dx = (gGlobalPrefs.m_fwdsearchWidth > 0 ? (double)gGlobalPrefs.m_fwdsearchWidth : 15.0) * win->dm->zoomReal();
            recD.y -= 4;
            recD.dy += 8;
        }
        RectI recI = recD.Convert<int>();
        BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win->fwdsearchmarkHideStep) / HIDE_FWDSRCHMARK_STEPS);
        PaintTransparentRectangle(win, hdc, &recI, gGlobalPrefs.m_fwdsearchColor, alpha, 0);
    }
}

#ifdef DRAW_PAGE_SHADOWS
#define BORDER_SIZE   1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, PdfPageInfo * pageInfo, bool presentation, RectI *bounds)
{
    int xDest = pageInfo->screenX;
    int yDest = pageInfo->screenY;
    int bmpDx = pageInfo->bitmap.dx;
    int bmpDy = pageInfo->bitmap.dy;

    *bounds = RectI(xDest, yDest, bmpDx, bmpDy);

    // Frame info
    int fx = xDest - BORDER_SIZE, fy = yDest - BORDER_SIZE;
    int fw = bmpDx + 2 * BORDER_SIZE, fh = bmpDy + 2 * BORDER_SIZE;

    // Shadow info
    int sx = fx + SHADOW_OFFSET, sy = fy + SHADOW_OFFSET, sw = fw, sh = fh;
    if (xDest <= 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = min(pageInfo->bitmap.x, SHADOW_OFFSET);
        sx -= diff; sw += diff;
    }
    if (yDest <= 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = min(pageInfo->bitmap.y, SHADOW_OFFSET);
        sy -= diff; sh += diff;
    }

    // Draw shadow
    if (!presentation) {
        RectI rc(sx, sy, sw, sh);
        FillRect(hdc, &rc.ToRECT(), gBrushShadow);
    }

    // Draw frame
    HPEN pe = CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME);
    SelectObject(hdc, pe);
    SelectObject(hdc, gGlobalPrefs.m_invertColors ? gBrushBlack : gBrushWhite);
    Rectangle(hdc, fx, fy, fx + fw, fy + fh);
    DeletePen(pe);
}
#else
#define BORDER_SIZE   0
#define SHADOW_OFFSET 0
static void PaintPageFrameAndShadow(HDC hdc, PdfPageInfo *pageInfo, bool presentation, RectI *bounds)
{
    int xDest = pageInfo->screenX;
    int yDest = pageInfo->screenY;
    int bmpDx = pageInfo->bitmap.dx;
    int bmpDy = pageInfo->bitmap.dy;

    *bounds = RectI(xDest, yDest, bmpDx, bmpDy);

    // Frame info
    int fx = xDest - BORDER_SIZE, fy = yDest - BORDER_SIZE;
    int fw = bmpDx + 2 * BORDER_SIZE, fh = bmpDy + 2 * BORDER_SIZE;

    HPEN pe = CreatePen(PS_NULL, 0, 0);
    SelectObject(hdc, pe);
    SelectObject(hdc, gGlobalPrefs.m_invertColors ? gBrushBlack : gBrushWhite);
    Rectangle(hdc, fx, fy, fx + fw + 1, fy + fh + 1);
    DeletePen(pe);
}
#endif

static void WindowInfo_Paint(WindowInfo *win, HDC hdc, PAINTSTRUCT *ps)
{
    RectI bounds;
    bool rendering = false;

    assert(win);
    if (!win) return;
    DisplayModel* dm = win->dm;
    assert(dm);
    if (!dm) return;

    assert(win->hdcToDraw);
    hdc = win->hdcToDraw;

    FillRect(hdc, &ps->rcPaint, win->presentation ? gBrushBlack : gBrushBg);

    DBG_OUT("WindowInfo_Paint() ");
    for (int pageNo = 1; pageNo <= dm->pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = dm->getPageInfo(pageNo);
        if (!pageInfo->visible)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        PaintPageFrameAndShadow(hdc, pageInfo, PM_ENABLED == win->presentation, &bounds);

        bool renderOutOfDateCue = false;
        UINT renderDelay = gRenderCache.Paint(hdc, &bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);

        if (renderDelay) {
            HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
            HGDIOBJ origFont = SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
            SetTextColor(hdc, gGlobalPrefs.m_invertColors ? WIN_COL_WHITE : WIN_COL_BLACK);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS)
                    win->RepaintAsync(REPAINT_MESSAGE_DELAY_IN_MS / 4);
                else
                    draw_centered_text(hdc, bounds, _TR("Please wait - rendering..."));
                DBG_OUT("drawing empty %d ", pageNo);
                rendering = true;
            } else {
                draw_centered_text(hdc, bounds, _TR("Couldn't render the page"));
                DBG_OUT("   missing bitmap on visible page %d\n", pageNo);
            }
            SelectObject(hdc, origFont);
            Win32_Font_Delete(fontRightTxt);
            continue;
        }

        if (!renderOutOfDateCue)
            continue;

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            SelectObject(bmpDC, gBitmapReloadingCue);
            int size = (int)(16 * win->uiDPIFactor);
            int cx = min(bounds.dx, 2 * size), cy = min(bounds.dy, 2 * size);
            StretchBlt(hdc, bounds.x + bounds.dx - min((cx + size) / 2, cx),
                bounds.y + max((cy - size) / 2, 0), min(cx, size), min(cy, size),
                bmpDC, 0, 0, 16, 16, SRCCOPY);

            DeleteDC(bmpDC);
        }
    }

    if (win->showSelection)
        PaintSelection(win, hdc);
    
    if (win->showForwardSearchMark)
        PaintForwardSearchMark(win, hdc);

    DBG_OUT("\n");

    if (gDebugShowLinks && !rendering) {
        /* debug code to visualize links (can block while rendering) */
        fz_bbox drawAreaRect = { 0, 0, dm->drawAreaSize.dx, dm->drawAreaSize.dy };
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        for (int pageNo = win->dm->pageCount(); pageNo >= 1; --pageNo) {
            PdfPageInfo *pageInfo = win->dm->getPageInfo(pageNo);
            if (!pageInfo->shown || !pageInfo->visible)
                continue;

            pdf_link *links = NULL;
            int linkCount = dm->getPdfLinks(pageNo, &links);
            for (int i = 0; i < linkCount; i++) {
                fz_bbox isect = fz_intersectbbox(fz_roundrect(links[i].rect), drawAreaRect);
                if (fz_isemptybbox(isect))
                    continue;

                RectI rectScreen = RectI::FromXY(isect.x0, isect.y0, isect.x1, isect.y1);
                paint_rect(hdc, &rectScreen.ToRECT());
            }
            free(links);
        }

        DeletePen(SelectObject(hdc, oldPen));

        if (dm->zoomVirtual() == ZOOM_FIT_CONTENT) {
            // also display the content box when fitting content
            pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0x00, 0xff));
            oldPen = SelectObject(hdc, pen);

            for (int pageNo = win->dm->pageCount(); pageNo >= 1; --pageNo) {
                PdfPageInfo *pageInfo = win->dm->getPageInfo(pageNo);
                if (!pageInfo->shown || !pageInfo->visible)
                    continue;

                fz_bbox cbox = dm->pdfEngine->pageContentBox(pageNo);
                RectD rect = RectI::FromXY(cbox.x0, cbox.y0, cbox.x1, cbox.y1).Convert<double>();
                if (dm->rectCvtUserToScreen(pageNo, &rect))
                    paint_rect(hdc, &rect.ToRECT());
            }

            DeletePen(SelectObject(hdc, oldPen));
        }
    }
}

static void CopySelectionToClipboard(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!win->selectionOnPage) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();

    if (win->dm->pdfEngine->hasPermission(PDF_PERM_COPY)) {
        TCHAR *selText;
        if (win->dm->textSelection->result.len > 0) {
            selText = win->dm->textSelection->ExtractText();
        }
        else {
            VStrList selections;
            for (SelectionOnPage *selOnPage = win->selectionOnPage; selOnPage; selOnPage = selOnPage->next) {
                selText = win->dm->getTextInRegion(selOnPage->pageNo, &selOnPage->selectionPage);
                if (selText)
                    selections.Push(selText);
            }
            selText = selections.Join();
        }

        // don't copy empty text
        if (!Str::IsEmpty(selText)) {
            HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (Str::Len(selText) + 1) * sizeof(TCHAR));
            if (handle) {
                TCHAR *globalText = (TCHAR *)GlobalLock(handle);
                lstrcpy(globalText, selText);
                GlobalUnlock(handle);

                if (!SetClipboardData(CF_T_TEXT, handle))
                    SeeLastError();
            }
        }
        free(selText);

        if (win->dm->textSelection->result.len > 0) {
            // don't also copy the first line of a text selection as an image
            CloseClipboard();
            return;
        }
    }
    else
        WindowInfo_ShowMessage_Async(win, _TR("Copying text was denied (copying as image only)"), true);

    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage *selOnPage = win->selectionOnPage;
    RectD *r = &selOnPage->selectionPage;
    fz_rect clipRegion;
    clipRegion.x0 = (float)r->x; clipRegion.x1 = (float)(r->x + r->dx);
    clipRegion.y0 = (float)r->y; clipRegion.y1 = (float)(r->y + r->dy);

    RenderedBitmap * bmp = win->dm->renderBitmap(selOnPage->pageNo, win->dm->zoomReal(),
        win->dm->rotation(), &clipRegion, Target_Export, gUseGdiRenderer);
    if (bmp) {
        if (!SetClipboardData(CF_BITMAP, bmp->getBitmap()))
            SeeLastError();
        delete bmp;
    }

    CloseClipboard();
}

static void DeleteOldSelectionInfo(WindowInfo *win) {
    SelectionOnPage *selOnPage = win->selectionOnPage;
    while (selOnPage != NULL) {
        SelectionOnPage *tmp = selOnPage->next;
        delete selOnPage;
        selOnPage = tmp;
    }
    win->selectionOnPage = NULL;
    win->showSelection = false;
}

static void ConvertSelectionRectToSelectionOnPage(WindowInfo *win) {
    win->dm->textSelection->Reset();
    for (int pageNo = win->dm->pageCount(); pageNo >= 1; --pageNo) {
        PdfPageInfo *pageInfo = win->dm->getPageInfo(pageNo);
        assert(!pageInfo->visible || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI intersect = win->selectionRect.Intersect(pageInfo->pageOnScreen);
        if (intersect.IsEmpty())
            continue;

        /* selection intersects with a page <pageNo> on the screen */
        int selPageNo;
        RectD isectD = intersect.Convert<double>();
        if (!win->dm->rectCvtScreenToUser(&selPageNo, &isectD))
            continue;

        assert(pageNo == selPageNo);

        SelectionOnPage *selOnPage = new SelectionOnPage;
        selOnPage->pageNo = selPageNo;
        selOnPage->selectionPage = isectD;
        selOnPage->next = win->selectionOnPage;
        win->selectionOnPage = selOnPage;
    }
}

// for testing only
static void CrashMe()
{
    char *p = NULL;
    *p = 0;
}

static void StartStressRenderingPage(WindowInfo *win, int pageNo);

class StressTestPageRenderedWorkItem : public UIThreadWorkItem, public CallbackFunc
{
    int pageNo;
    static int iterations;

public:
    StressTestPageRenderedWorkItem(WindowInfo *win, int pageNo) :
        UIThreadWorkItem(win), pageNo(pageNo) { }
    
    virtual void Callback(void *arg) {
        if ((bool)arg)
            iterations++;
        gUIThreadMarshaller.Queue(this);
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win))
            StartStressRenderingPage(win, pageNo + 1);
    }
};
int StressTestPageRenderedWorkItem::iterations = 0;

static void StartStressRenderingPage(WindowInfo *win, int pageNo)
{
    assert(win);
    if (!win) return;

    if (win->state != WS_SHOWING_PDF || win->dm == NULL || win->dm->_dontRenderFlag) {
        win->threadStressRunning = false;
    }
    if (!win->threadStressRunning)
        return;

    if (pageNo > win->dm->pageCount()) {
        gRenderCache.FreeForDisplayModel(win->dm);
        pageNo = 1;
    }
    CallbackFunc *callback = new StressTestPageRenderedWorkItem(win, pageNo);
    gRenderCache.Render(win->dm, pageNo, callback);
}

// TODO: start text search thread as well
static void ToggleThreadStress(WindowInfo *win)
{
    if (win->threadStressRunning) {
        win->threadStressRunning = false;
        return;
    }

    win->threadStressRunning = true;
    StartStressRenderingPage(win, 1);
}

static void OnSelectAll(WindowInfo *win, bool textOnly=false)
{
    assert(win && win->dm);
    if (!win || !win->dm) return;

    if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus()) {
        Edit_SelectAll(GetFocus());
        return;
    }

    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !win->dm->getPageInfo(pageNo)->shown; pageNo++);
        win->dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win->dm->pageCount(); !win->dm->getPageInfo(pageNo)->shown; pageNo--);
        win->dm->textSelection->SelectUpTo(pageNo, -1);
        win->selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    }
    else {
        DeleteOldSelectionInfo(win);
        win->selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        ConvertSelectionRectToSelectionOnPage(win);
    }

    win->showSelection = true;
    win->RepaintAsync();
}

static void OnInverseSearch(WindowInfo *win, UINT x, UINT y)
{
    assert(win);
    if (!win || !win->dm) return;
    if (gRestrictedUse || gPluginMode) return;

    // Clear the last forward-search result
    win->fwdsearchmarkRects.Reset();
    InvalidateRect(win->hwndCanvas, NULL, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!win->pdfsync) {
        UINT err = CreateSynchronizer(win->loadedFilePath, &win->pdfsync);

        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            // In order to avoid confusion for non-LaTeX users, we do not show
            // any error message if the SyncTeX enhancements are hidden from UI
            DBG_OUT("Pdfsync: Sync file not found!\n");
            if (gGlobalPrefs.m_enableTeXEnhancements)
                WindowInfo_ShowMessage_Async(win, _TR("No synchronization file found"), true);
            return;
        }
        else if (err != PDFSYNCERR_SUCCESS || !win->pdfsync) {
            DBG_OUT("Pdfsync: Sync file cannot be loaded!\n");
            WindowInfo_ShowMessage_Async(win, _TR("Synchronization file cannot be opened"), true);
            return;
        }
    }

    int pageNo = POINT_OUT_OF_PAGE;
    double dblx = x, dbly = y;
    win->dm->cvtScreenToUser(&pageNo, &dblx, &dbly);
    if (pageNo == POINT_OUT_OF_PAGE) 
        return;
    x = (UINT)dblx; y = (UINT)dbly;

    const PdfPageInfo *pageInfo = win->dm->getPageInfo(pageNo);
    TCHAR srcfilepath[MAX_PATH];
    win->pdfsync->convert_coord_to_internal(&x, &y, pageInfo->page.Convert<int>().dy, BottomLeft);
    UINT line, col;
    UINT err = win->pdfsync->pdf_to_source(pageNo, x, y, srcfilepath, dimof(srcfilepath),&line,&col); // record 101
    if (err != PDFSYNCERR_SUCCESS) {
        DBG_OUT("cannot sync from pdf to source!\n");
        WindowInfo_ShowMessage_Async(win, _TR("No synchronization info at this position"), true);
        return;
    }

    TCHAR *inverseSearch = gGlobalPrefs.m_inverseSearchCmdLine;
    if (!inverseSearch)
        // Detect a text editor and use it as the default inverse search handler for now
        inverseSearch = AutoDetectInverseSearchCommands();

    TCHAR *cmdline = NULL;
    if (inverseSearch)
        cmdline = win->pdfsync->prepare_commandline(inverseSearch, srcfilepath, line, col);
    if (!Str::IsEmpty(cmdline)) {
        //ShellExecute(NULL, NULL, cmdline, cmdline, NULL, SW_SHOWNORMAL);
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            DBG_OUT("CreateProcess failed (%d): '%s'.\n", GetLastError(), cmdline);
            WindowInfo_ShowMessage_Async(win, _TR("Cannot start inverse search command. Please check the command line in the settings."), true);
        }
    }
    else if (gGlobalPrefs.m_enableTeXEnhancements)
        WindowInfo_ShowMessage_Async(win, _TR("Cannot start inverse search command. Please check the command line in the settings."), true);
    free(cmdline);

    if (inverseSearch != gGlobalPrefs.m_inverseSearchCmdLine)
        free(inverseSearch);
}

static void ChangePresentationMode(WindowInfo *win, PresentationMode mode)
{
    win->presentation = mode;
    win->RedrawAll();
}

static void OnDraggingStart(WindowInfo *win, int x, int y, bool right=false)
{
    SetCapture(win->hwndCanvas);
    win->mouseAction = right ? MA_DRAGGING_RIGHT : MA_DRAGGING;
    win->dragPrevPosX = x;
    win->dragPrevPosY = y;
    if (GetCursor())
        SetCursor(gCursorDrag);
    DBG_OUT(" dragging start, x=%d, y=%d\n", x, y);
}

static void OnDraggingStop(WindowInfo *win, int x, int y, bool aborted)
{
    if (GetCapture() != win->hwndCanvas)
        return;

    if (GetCursor())
        SetCursor(gCursorArrow);
    ReleaseCapture();

    if (aborted)
        return;

    int dragDx = x - win->dragPrevPosX;
    int dragDy = y - win->dragPrevPosY;
    DBG_OUT(" dragging ends, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
    win->MoveDocBy(dragDx, -dragDy*2);
}

#define SELECT_AUTOSCROLL_AREA_WIDTH 15
#define SELECT_AUTOSCROLL_STEP_LENGTH 10

static void OnSelectionEdgeAutoscroll(WindowInfo *win, int x, int y)
{
    int dx = 0, dy = 0;

    if (x < SELECT_AUTOSCROLL_AREA_WIDTH * win->uiDPIFactor)
        dx = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (x > (win->winDx() - SELECT_AUTOSCROLL_AREA_WIDTH) * win->uiDPIFactor)
        dx = SELECT_AUTOSCROLL_STEP_LENGTH;
    if (y < SELECT_AUTOSCROLL_AREA_WIDTH * win->uiDPIFactor)
        dy = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (y > (win->winDy() - SELECT_AUTOSCROLL_AREA_WIDTH) * win->uiDPIFactor)
        dy = SELECT_AUTOSCROLL_STEP_LENGTH;

    if (dx != 0 || dy != 0) {
        int oldX = win->dm->areaOffset.x, oldY = win->dm->areaOffset.y;
        win->MoveDocBy(dx, dy);

        dx = win->dm->areaOffset.x - oldX;
        dy = win->dm->areaOffset.y - oldY;
        win->selectionRect.x -= dx;
        win->selectionRect.y -= dy;
        win->selectionRect.dx += dx;
        win->selectionRect.dy += dy;
    }
}

static void OnMouseMove(WindowInfo *win, int x, int y, WPARAM flags)
{
    int dragDx, dragDy;

    assert(win);
    if (!win || WS_SHOWING_PDF != win->state)
        return;
    assert(win->dm);
    if (!win->dm) return;

    if (win->presentation) {
        // shortly display the cursor if the mouse has moved and the cursor is hidden
        if ((x != win->dragPrevPosX || y != win->dragPrevPosY) && !GetCursor()) {
            if (win->mouseAction == MA_IDLE)
                SetCursor(gCursorArrow);
            else
                SendMessage(win->hwndCanvas, WM_SETCURSOR, 0, 0);
            SetTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
        }
    }

    if (win->dragStartPending) {
        // have we already started a proper drag?
        if (abs(x - win->dragStartX) <= GetSystemMetrics(SM_CXDRAG) &&
            abs(y - win->dragStartY) <= GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        win->dragStartPending = false;
        win->linkOnLastButtonDown = NULL;
    }

    switch (win->mouseAction) {
    case MA_SCROLLING:
        win->yScrollSpeed = (y - win->dragStartY) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win->xScrollSpeed = (x - win->dragStartX) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        break;
    case MA_SELECTING_TEXT:
        if (GetCursor())
            SetCursor(gCursorIBeam);
        /* fall through */
    case MA_SELECTING:
        win->selectionRect.dx = x - win->selectionRect.x;
        win->selectionRect.dy = y - win->selectionRect.y;
        win->RepaintAsync();
        OnSelectionEdgeAutoscroll(win, x, y);
        break;
    case MA_DRAGGING:
    case MA_DRAGGING_RIGHT:
        dragDx = win->dragPrevPosX - x;
        dragDy = win->dragPrevPosY - y;
        DBG_OUT(" drag move, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
        win->MoveDocBy(dragDx, dragDy);
        break;
    }

    win->dragPrevPosX = x;
    win->dragPrevPosY = y;
}

static void OnSelectionStart(WindowInfo *win, int x, int y, WPARAM key)
{
    DeleteOldSelectionInfo (win);

    win->selectionRect.x = x;
    win->selectionRect.y = y;
    win->selectionRect.dx = 0;
    win->selectionRect.dy = 0;
    win->showSelection = true;
    win->mouseAction = MA_SELECTING;

    // Ctrl+drag forces a rectangular selection
    if (!(key & MK_CONTROL) || (key & MK_SHIFT)) {
        int pageNo;
        double dX = x, dY = y;
        if (win->dm->cvtScreenToUser(&pageNo, &dX, &dY)) {
            win->dm->textSelection->StartAt(pageNo, dX, dY);
            win->mouseAction = MA_SELECTING_TEXT;
        }
    }

    SetCapture(win->hwndCanvas);
    SetTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);

    win->RepaintAsync();
}

static void OnSelectionStop(WindowInfo *win, int x, int y, bool aborted)
{
    if (GetCapture() == win->hwndCanvas)
        ReleaseCapture();
    KillTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID);

    if (aborted)
        return;

    // update the text selection before changing the selectionRect
    if (MA_SELECTING_TEXT == win->mouseAction)
        UpdateTextSelection(win);

    win->selectionRect.dx = abs(x - win->selectionRect.x);
    win->selectionRect.dy = abs(y - win->selectionRect.y);
    win->selectionRect.x = min(win->selectionRect.x, x);
    win->selectionRect.y = min(win->selectionRect.y, y);

    if (win->selectionRect.dx == 0 || win->selectionRect.dy == 0) {
        DeleteOldSelectionInfo(win);
    } else if (win->mouseAction == MA_SELECTING) {
        ConvertSelectionRectToSelectionOnPage (win);
    }
    win->RepaintAsync();
}

static void OnMouseLeftButtonDblClk(WindowInfo *win, int x, int y, WPARAM key)
{
    //DBG_OUT("Left button clicked on %d %d\n", x, y);
    assert (win);
    if (!win) return;
    OnInverseSearch(win, x, y);
}

static void OnMouseLeftButtonDown(WindowInfo *win, int x, int y, WPARAM key)
{
    //DBG_OUT("Left button clicked on %d %d\n", x, y);
    if (!win || (WS_SHOWING_PDF != win->state))
        return;

    if (MA_DRAGGING_RIGHT == win->mouseAction)
        return;

    if (MA_SCROLLING == win->mouseAction) {
        win->mouseAction = MA_IDLE;
        return;
    }
    assert(win->mouseAction == MA_IDLE);
    assert(win->dm);

    SetFocus(win->hwndFrame);

    win->linkOnLastButtonDown = win->dm->getLinkAtPosition(x, y);
    win->dragStartPending = true;
    win->dragStartX = x;
    win->dragStartY = y;

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - in restricted mode, selections aren't allowed
    if (gRestrictedUse || ((key & MK_SHIFT) || !win->dm->isOverText(x,y)) && !(key & MK_CONTROL))
        OnDraggingStart(win, x, y);
    else
        OnSelectionStart(win, x, y, key);
}

static void OnMouseLeftButtonUp(WindowInfo *win, int x, int y, WPARAM key)
{
    if (!win || (WS_SHOWING_PDF != win->state))
        return;

    assert(win->dm);
    if (MA_IDLE == win->mouseAction || MA_DRAGGING_RIGHT == win->mouseAction)
        return;
    assert(MA_SELECTING == win->mouseAction || MA_SELECTING_TEXT == win->mouseAction || MA_DRAGGING == win->mouseAction);

    bool didDragMouse = !win->dragStartPending ||
        abs(x - win->dragStartX) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win->dragStartY) > GetSystemMetrics(SM_CYDRAG);
    if (MA_DRAGGING == win->mouseAction)
        OnDraggingStop(win, x, y, !didDragMouse);
    else
        OnSelectionStop(win, x, y, !didDragMouse);

    if (didDragMouse)
        /* pass */;
    else if (win->linkOnLastButtonDown && win->dm->getLinkAtPosition(x, y) == win->linkOnLastButtonDown) {
        win->dm->goToTocLink(win->linkOnLastButtonDown);
        SetCursor(gCursorArrow);
    }
    /* if we had a selection and this was just a click, hide the selection */
    else if (win->showSelection)
        ClearSearch(win);
    /* in presentation mode, change pages on left/right-clicks */
    else if (win->fullScreen || PM_ENABLED == win->presentation) {
        if ((key & MK_SHIFT))
            win->dm->goToPrevPage(0);
        else
            win->dm->goToNextPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
        ChangePresentationMode(win, PM_ENABLED);

    win->mouseAction = MA_IDLE;
    win->linkOnLastButtonDown = NULL;
}

static void OnMouseMiddleButtonDown(WindowInfo *win, int x, int y, int key)
{
    assert(win);
    if (!win) return;

    // Handle message by recording placement then moving document as mouse moves.

    switch (win->mouseAction) {
    case MA_IDLE:
        win->mouseAction = MA_SCROLLING;

        // record current mouse position, distance mouse moves
        // from this poition is speed to shift document
        win->dragStartX = x;
        win->dragStartY = y; 
        SetCursor(gCursorScroll);
        break;

    case MA_SCROLLING:
        win->mouseAction = MA_IDLE;
        break;
    }
}

static void OnMouseRightButtonDown(WindowInfo *win, int x, int y, int key)
{
    //DBG_OUT("Right button clicked on %d %d\n", x, y);
    assert (win);
    if (!win) return;

    if (WS_SHOWING_PDF != win->state)
        return;

    if (MA_SCROLLING == win->mouseAction)
        win->mouseAction = MA_IDLE;
    else if (win->mouseAction != MA_IDLE)
        return;
    assert(win->dm);

    SetFocus(win->hwndFrame);

    win->dragStartPending = true;
    win->dragStartX = x;
    win->dragStartY = y;

    OnDraggingStart(win, x, y, true);
}

static void OnMouseRightButtonUp(WindowInfo *win, int x, int y, WPARAM key)
{
    assert (win);
    if (!win) return;

    if (WS_SHOWING_PDF != win->state)
        return;

    assert(win->dm);
    if (MA_DRAGGING_RIGHT != win->mouseAction)
        return;

    bool didDragMouse = !win->dragStartPending ||
        abs(x - win->dragStartX) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win->dragStartY) > GetSystemMetrics(SM_CYDRAG);
    OnDraggingStop(win, x, y, !didDragMouse);

    if (didDragMouse)
        /* pass */;
    else if (win->fullScreen || PM_ENABLED == win->presentation) {
        if ((key & MK_SHIFT))
            win->dm->goToNextPage(0);
        else
            win->dm->goToPrevPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
        ChangePresentationMode(win, PM_ENABLED);

    win->mouseAction = MA_IDLE;
}

static void OnPaint(WindowInfo *win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    if (WS_ERROR_LOADING_PDF == win->state) {
        HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
        HGDIOBJ origFont = SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
        SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &ps.rcPaint, gBrushBg);
        draw_centered_text(hdc, ClientRect(win->hwndCanvas), _TR("Error loading PDF file."));
        SelectObject(hdc, origFont);
        Win32_Font_Delete(fontRightTxt);
    } else if (WS_SHOWING_PDF == win->state) {
        switch (win->presentation) {
        case PM_BLACK_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushBlack);
            break;
        case PM_WHITE_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushWhite);
            break;
        default:
            win->ResizeIfNeeded();
            WindowInfo_Paint(win, hdc, &ps);
            win->DoubleBuffer_Show(hdc);
        }
    } else
        assert(0);

    EndPaint(win->hwndCanvas, &ps);
}

static void OnMenuExit(void)
{
    if (gPluginMode)
        return;

    for (size_t i = 0; i < gWindows.Count(); i++)
        gWindows[i]->AbortFinding();

    Prefs_Save();
    PostQuitMessage(0);
}

/* Close the document associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
static void CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose=false)
{
    assert(win);
    if (!win)  return;
    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    if (win->pluginParent && !forceClose)
        return;

    if (win->dm)
        win->dm->_dontRenderFlag = true;
    if (win->presentation)
        WindowInfo_ExitFullscreen(win);

    bool lastWindow = false;
    if (1 == gWindows.Count())
        lastWindow = true;

    if (lastWindow)
        Prefs_Save();
    else
        UpdateCurrentFileDisplayStateForWin(win);

    win->state = WS_ABOUT;

    if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        delete win->watcher;
        win->watcher = NULL;
        if (win->tocShow)
            win->HideTocBox();
        win->ClearTocBox();
        win->AbortFinding();
        delete win->dm;
        win->dm = NULL;
        free(win->loadedFilePath);
        win->loadedFilePath = NULL;

        if (win->hwndPdfProperties) {
            DestroyWindow(win->hwndPdfProperties);
            assert(NULL == win->hwndPdfProperties);
        }
        UpdateToolbarPageText(win, 0);
        UpdateToolbarFindText(win);
        win->RedrawAll();
        WindowInfo_UpdateFindbox(win);
        DeleteOldSelectionInfo(win);
    } else {
        HWND hwndToDestroy = win->hwndFrame;
        WindowInfo_Delete(win);
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
static void OnMenuZoom(WindowInfo *win, UINT menuId)
{
    if (!win->dm)
        return;

    float zoom = ZoomMenuItemToZoom(menuId);
    win->ZoomToSelection(zoom, false);
}

static void OnMenuCustomZoom(WindowInfo *win)
{
    if (!win->dm)
        return;

    float zoom = win->dm->zoomVirtual();
    if (DIALOG_CANCEL_PRESSED == Dialog_CustomZoom(win->hwndFrame, &zoom))
        return;
    win->ZoomToSelection(zoom, false);
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

// TODO: make it run in a background thread
static void PrintToDevice(PdfEngine *pdfEngine, HDC hdc, LPDEVMODE devMode,
                          int nPageRanges, LPPRINTPAGERANGE pr,
                          int dm_rotation=0,
                          enum PrintRangeAdv rangeAdv=PrintRangeAll,
                          enum PrintScaleAdv scaleAdv=PrintScaleShrink,
                          SelectionOnPage *sel=NULL) {

    assert(pdfEngine);
    if (!pdfEngine) return;

    DOCINFO di = {0};
    di.cbSize = sizeof (DOCINFO);
    di.lpszDocName = pdfEngine->fileName();

    if (StartDoc(hdc, &di) <= 0)
        return;

    SetMapMode(hdc, MM_TEXT);

    int paperWidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    int paperHeight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    int printableWidth = GetDeviceCaps(hdc, HORZRES);
    int printableHeight = GetDeviceCaps(hdc, VERTRES);
    int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    int rightMargin = paperWidth - printableWidth - leftMargin;
    int bottomMargin = paperHeight - printableHeight - topMargin;
    float dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / PDF_FILE_DPI,
                          GetDeviceCaps(hdc, LOGPIXELSY) / PDF_FILE_DPI);
    bool bPrintPortrait = paperWidth < paperHeight;
    if (devMode && devMode->dmFields & DM_ORIENTATION)
        bPrintPortrait = DMORIENT_PORTRAIT == devMode->dmOrientation;

    // print all the pages the user requested
    for (int i = 0; i < nPageRanges; i++) {
        if (-1 == pr->nFromPage && -1 == pr->nToPage) {
            assert(1 == nPageRanges && sel);
            DBG_OUT(" printing:  drawing bitmap for selection\n");

            for (; sel; sel = sel->next) {
                StartPage(hdc);

                RectD *r = &sel->selectionPage;
                fz_rect clipRegion;
                clipRegion.x0 = (float)r->x; clipRegion.x1 = (float)(r->x + r->dx);
                clipRegion.y0 = (float)r->y; clipRegion.y1 = (float)(r->y + r->dy);

                int rotation = pdfEngine->pageRotation(sel->pageNo) + dm_rotation;
                // Swap width and height for rotated documents
                SizeD sSize = (rotation % 180) == 0 ? SizeD(r->dx, r->dy) : SizeD(r->dy, r->dx);

                float zoom = (float)min((double)printableWidth / sSize.dx, (double)printableHeight / sSize.dy);
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == scaleAdv)
                    zoom = min(dpiFactor, zoom);
                else if (PrintScaleNone == scaleAdv)
                    zoom = dpiFactor;

#ifdef USE_GDI_FOR_PRINTING
                RectI rc((LONG)(printableWidth - sSize.dx * zoom) / 2,
                         (LONG)(printableHeight - sSize.dy * zoom) / 2,
                         printableWidth, printableHeight);
                pdfEngine->renderPage(hdc, sel->pageNo, &rc, NULL, zoom, dm_rotation, &clipRegion, Target_Print);
#else
                RenderedBitmap *bmp = pdfEngine->renderBitmap(sel->pageNo, zoom, dm_rotation, &clipRegion, Target_Print, gUseGdiRenderer);
                if (bmp) {
                    bmp->stretchDIBits(hdc, (printableWidth - bmp->dx()) / 2,
                        (printableHeight - bmp->dy()) / 2, bmp->dx(), bmp->dy());
                    delete bmp;
                }
#endif
                if (EndPage(hdc) <= 0) {
                    AbortDoc(hdc);
                    return;
                }
            }
            break;
        }

        assert(pr->nFromPage <= pr->nToPage);
        for (DWORD pageNo = pr->nFromPage; pageNo <= pr->nToPage; pageNo++) {
            if ((PrintRangeEven == rangeAdv && pageNo % 2 != 0) ||
                (PrintRangeOdd == rangeAdv && pageNo % 2 == 0))
                continue;

            DBG_OUT(" printing:  drawing bitmap for page %d\n", pageNo);

            StartPage(hdc);
            // MM_TEXT: Each logical unit is mapped to one device pixel.
            // Positive x is to the right; positive y is down.

            SizeD pSize = pdfEngine->pageSize(pageNo);
            int rotation = pdfEngine->pageRotation(pageNo);
            // Turn the document by 90 deg if it isn't in portrait mode
            if (pSize.dx > pSize.dy) {
                rotation += 90;
                pSize = SizeD(pSize.dy, pSize.dx);
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90 deg in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                pSize = SizeD(pSize.dy, pSize.dx);
            }

            // dpiFactor means no physical zoom
            float zoom = dpiFactor;
            // offset of the top-left corner of the page from the printable area
            // (positive values move the page into the left/top margins, etc.);
            // offset adjustments are needed because the GDI coordinate system
            // starts at the corner of the printable area and because the page
            // is consequently scaled from the center of the printable area;
            // default to centering the document page on the paper page
            int horizOffset = leftMargin + (printableWidth - paperWidth) / 2;
            int vertOffset = topMargin + (printableHeight - paperHeight) / 2;

            if (scaleAdv != PrintScaleNone) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                fz_rect rect = fz_bboxtorect(pdfEngine->pageContentBox(pageNo, Target_Print));
                fz_rect cbox = fz_transformrect(pdfEngine->viewctm(pageNo, 1.0, rotation), rect);
                zoom = (float)min((double)printableWidth / (cbox.x1 - cbox.x0),
                              min((double)printableHeight / (cbox.y1 - cbox.y0),
                              min((double)paperWidth / pSize.dx,
                                  (double)paperHeight / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == scaleAdv && dpiFactor < zoom)
                    zoom = dpiFactor;
                // make sure that no content lies in the non-printable paper margins
                if (leftMargin > cbox.x0 * zoom)
                    horizOffset = (int)(horizOffset - leftMargin + cbox.x0 * zoom);
                else if (rightMargin > (pSize.dx - cbox.x1) * zoom)
                    horizOffset = (int)(horizOffset + rightMargin - (pSize.dx - cbox.x1) * zoom);
                if (topMargin > cbox.y0 * zoom)
                    vertOffset = (int)(vertOffset - topMargin + cbox.y0 * zoom);
                else if (bottomMargin > (pSize.dy - cbox.y1) * zoom)
                    vertOffset = (int)(vertOffset + bottomMargin - (pSize.dy - cbox.y1) * zoom);
            }

#ifdef USE_GDI_FOR_PRINTING
            RectI rc((LONG)(printableWidth - pSize.dx * zoom) / 2 - horizOffset,
                     (LONG)(printableHeight - pSize.dy * zoom) / 2 - vertOffset,
                     printableWidth, printableHeight);
            pdfEngine->renderPage(hdc, pageNo, &rc, NULL, zoom, rotation, NULL, Target_Print);
#else
            RenderedBitmap *bmp = pdfEngine->renderBitmap(pageNo, zoom, rotation, NULL, Target_Print, gUseGdiRenderer);
            if (bmp) {
                bmp->stretchDIBits(hdc, (printableWidth - bmp->dx()) / 2 - horizOffset,
                    (printableHeight - bmp->dy()) / 2 - vertOffset, bmp->dx(), bmp->dy());
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }
        }
        pr++;
    }

    EndDoc(hdc);
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
            *ppv = static_cast<IUnknown*>(this);
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
static void OnMenuPrint(WindowInfo *win)
{
    // In order to print with Adobe Reader instead:
    // ViewWithAcrobat(win, _T("/P"));

    PRINTDLGEX       pd;
    LPPRINTPAGERANGE ppr = NULL;

    if (gRestrictedUse) return;

    assert(win);
    if (!win) return;

    DisplayModel *dm = win->dm;
    assert(dm);
    if (!dm) return;

    /* printing uses the WindowInfo' dm that is created for the
       screen, it may be possible to create a new PdfEngine
       for printing so we don't mess with the screen one,
       but the user is not inconvenienced too much, and this
       way we only need to concern ourselves with one dm. */
    ZeroMemory(&pd, sizeof(PRINTDLGEX));
    pd.lStructSize = sizeof(PRINTDLGEX);
    pd.hwndOwner   = win->hwndFrame;
    pd.hDevMode    = NULL;   
    pd.hDevNames   = NULL;   
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->selectionOnPage)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges =1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    ppr = SAZA(PRINTPAGERANGE, MAXPAGERANGES);
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->pageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->pageCount();
    pd.nStartPage = START_PAGE_GENERAL;
    pd.lpCallback = new ApplyButtonDiablingCallback();

    // TODO: remember these (and maybe all of PRINTDLGEX) at least for this document/WindowInfo?
    Print_Advanced_Data advanced = { PrintRangeAll, PrintScaleShrink };
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(ghinst, &advanced);
    pd.lphPropertyPages = &hPsp;
    pd.nPropertyPages = 1;

    if (PrintDlgEx(&pd) == S_OK) {
        if (pd.dwResultAction == PD_RESULT_PRINT) {
            if (CheckPrinterStretchDibSupport(win->hwndFrame, pd.hDC)) {
                if (pd.Flags & PD_CURRENTPAGE) {
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=dm->currentPageNo();
                    pd.lpPageRanges->nToPage  =dm->currentPageNo();
                } else if (win->selectionOnPage && (pd.Flags & PD_SELECTION)) {
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=-1; // hint for PrintToDevice
                    pd.lpPageRanges->nToPage  =-1;
                } else if (!(pd.Flags & PD_PAGENUMS)) {
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=1;
                    pd.lpPageRanges->nToPage  =dm->pageCount();
                }
                LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
                PrintToDevice(dm->pdfEngine, pd.hDC, devMode, pd.nPageRanges, pd.lpPageRanges,
                              dm->rotation(), advanced.range, advanced.scale, win->selectionOnPage);
                if (devMode)
                    GlobalUnlock(pd.hDevMode);
            }
        }
    }
    else {
        if (CommDlgExtendedError()) { 
            /* if PrintDlg was cancelled then
               CommDlgExtendedError is zero, otherwise it returns the
               error code, which we could look at here if we wanted.
               for now just warn the user that printing has stopped
               becasue of an error */
            MessageBox(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        }
    }

    free(ppr);
    free(pd.lpCallback);
    if (pd.hDC != NULL) DeleteDC(pd.hDC);
    if (pd.hDevNames != NULL) GlobalFree(pd.hDevNames);
    if (pd.hDevMode != NULL) GlobalFree(pd.hDevMode);
}

static void OnMenuSaveAs(WindowInfo *win)
{
    OPENFILENAME   ofn = {0};
    TCHAR          dstFileName[MAX_PATH] = {0};
    const TCHAR *  srcFileName = NULL;
    bool           hasCopyPerm = true;

    if (gRestrictedUse) return;
    assert(win);
    if (!win) return;
    assert(win->dm);
    if (!win->dm) return;

    srcFileName = win->dm->fileName();
    assert(srcFileName);
    if (!srcFileName) return;

    // Can't save a PDF's content as a plain text if text copying isn't allowed
    if (!win->dm->pdfEngine->hasPermission(PDF_PERM_COPY))
        hasCopyPerm = false;

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    Str::Str<TCHAR> fileFilter(256);
    fileFilter.Append(_TR("PDF documents"));
    fileFilter.Append(_T("\1*.pdf\1"));
    if (hasCopyPerm) {
        fileFilter.Append(_TR("Text documents"));
        fileFilter.Append(_T("\1*.txt\1"));
    }
    fileFilter.Append(_TR("All files"));
    fileFilter.Append(_T("\1*.*\1"));
    Str::TransChars(fileFilter.Get(), _T("\1"), _T("\0"));

    // Remove the extension so that it can be re-added depending on the chosen filter
    Str::BufSet(dstFileName, dimof(dstFileName), Path::GetBaseName(srcFileName));
    // TODO: fix saving embedded PDF documents
    Str::TransChars(dstFileName, _T(":"), _T("_"));
    if (Str::EndsWithI(dstFileName, _T(".pdf")))
        dstFileName[Str::Len(dstFileName) - 4] = 0;

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrDefExt = _T("pdf");
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return;

    TCHAR * realDstFileName = dstFileName;
    // Make sure that the file has a valid ending
    if (!Str::EndsWithI(dstFileName, _T(".pdf")) && !(hasCopyPerm && Str::EndsWithI(dstFileName, _T(".txt")))) {
        TCHAR *defaultExt = hasCopyPerm && 2 == ofn.nFilterIndex ? _T(".txt") : _T(".pdf");
        realDstFileName = Str::Format(_T("%s%s"), dstFileName, defaultExt);
    }
    // Extract all text when saving as a plain text file
    if (hasCopyPerm && Str::EndsWithI(realDstFileName, _T(".txt"))) {
        ScopedMem<TCHAR> text(win->dm->extractAllText(Target_Export));
        ScopedMem<char> textUTF8(Str::Conv::ToUtf8(text));
        ScopedMem<char> textUTF8BOM(Str::Join("\xEF\xBB\xBF", textUTF8));
        File::WriteAll(realDstFileName, textUTF8BOM, Str::Len(textUTF8BOM));
    }
    // Recreate inexistant PDF files from memory...
    else if (!File::Exists(srcFileName)) {
        fz_buffer *data = win->dm->pdfEngine->getStreamData();
        if (data) {
            File::WriteAll(realDstFileName, data->data, data->len);
            fz_dropbuffer(data);
        } else {
            MessageBox(win->hwndFrame, _TR("Failed to save a file"), _TR("Warning"), MB_OK | MB_ICONEXCLAMATION);
        }
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
                errorMsg = Str::Format(_T("%s\n\n%s"), _TR("Failed to save a file"), msgBuf);
                LocalFree(msgBuf);
            } else {
                errorMsg = Str::Dup(_TR("Failed to save a file"));
            }
            MessageBox(win->hwndFrame, errorMsg, _TR("Warning"), MB_OK | MB_ICONEXCLAMATION);
            free(errorMsg);
        }
    }
    if (realDstFileName != dstFileName)
        free(realDstFileName);
}

bool DisplayModel::saveStreamAs(unsigned char *data, int dataLen, const TCHAR *fileName)
{
    if (gRestrictedUse)
        return false;

    TCHAR dstFileName[MAX_PATH] = { 0 };
    if (fileName)
        Str::BufSet(dstFileName, dimof(dstFileName), fileName);

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<TCHAR> fileFilter(Str::Format(_T("%s\1*.*\1"), _TR("All files")));
    Str::TransChars(fileFilter, _T("\1"), _T("\0"));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = _appData->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return false;
    return File::WriteAll(dstFileName, data, dataLen);
}

static void OnMenuSaveBookmark(WindowInfo *win)
{
    if (gRestrictedUse) return;
    assert(win);
    if (!win) return;
    assert(win->dm);
    if (!win->dm) return;

    TCHAR dstFileName[MAX_PATH] = { 0 };
    // Remove the extension so that it can be re-added depending on the chosen filter
    Str::BufSet(dstFileName, dimof(dstFileName), Path::GetBaseName(win->dm->fileName()));
    Str::TransChars(dstFileName, _T(":"), _T("_"));
    if (Str::EndsWithI(dstFileName, _T(".pdf")))
        dstFileName[Str::Len(dstFileName) - 4] = 0;

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<TCHAR> fileFilter(Str::Format(_T("%s\1*.lnk\1"), _TR("Bookmark Links")));
    Str::TransChars(fileFilter, _T("\1"), _T("\0"));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = _T("lnk");
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return;

    ScopedMem<TCHAR> filename(Str::Dup(dstFileName));
    if (!Str::EndsWithI(dstFileName, _T(".lnk")))
        filename.Set(Str::Join(dstFileName, _T(".lnk")));

    ScrollState ss;
    if (!win->dm->getScrollState(&ss)) {
        ss.page = win->dm->currentPageNo();
        ss.x = ss.y = -1;
    }
    const char *modeName = DisplayModeNameFromEnum(win->dm->displayMode());
    ScopedMem<TCHAR> viewMode(Str::Conv::FromAnsi(modeName));
    ScopedMem<TCHAR> zoomVirtual(Str::Format(_T("%.2f"), win->dm->zoomVirtual()));
    if (ZOOM_FIT_PAGE == win->dm->zoomVirtual())
        zoomVirtual.Set(Str::Dup(_T("fitpage")));
    else if (ZOOM_FIT_WIDTH == win->dm->zoomVirtual())
        zoomVirtual.Set(Str::Dup(_T("fitwidth")));
    else if (ZOOM_FIT_CONTENT == win->dm->zoomVirtual())
        zoomVirtual.Set(Str::Dup(_T("fitcontent")));

    ScopedMem<TCHAR> args(Str::Format(_T("\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d -reuse-instance"),
                          win->dm->fileName(), ss.page, viewMode, zoomVirtual, (int)ss.x, (int)ss.y));
    ScopedMem<TCHAR> exePath(ExePathGet());
    ScopedMem<TCHAR> desc(Str::Format(_TR("Bookmark link to page %d of %s"),
                          ss.page, Path::GetBaseName(win->dm->fileName())));

    CreateShortcut(filename, exePath, args, desc, 1);
}

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

static void OnMenuOpen(WindowInfo *win)
{
    if (gRestrictedUse) return;
    // don't allow opening different files in plugin mode
    if (win->pluginParent)
        return;

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<TCHAR> fileFilter(Str::Format(_T("%s\1*.pdf\1%s\1*.*\1"),
        _TR("PDF documents"), _TR("All files")));
    Str::TransChars(fileFilter, _T("\1"), _T("\0"));

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;

    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpfnHook = FileOpenHook;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_ENABLEHOOK;

    ofn.nMaxFile = MAX_PATH / 2;
    if (WindowsVerVistaOrGreater()) {
        // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
        // and later, so don't use it and just allocate enough memory to contain
        // several dozen file paths and hope that this is enough
        // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
        ofn.Flags &= ~OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH * 100;
    }
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    TCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        LoadDocument(ofn.lpstrFile, win);
        return;
    }

    while (*fileName) {
        ScopedMem<TCHAR> filePath(Path::Join(ofn.lpstrFile, fileName));
        if (filePath)
            LoadDocument(filePath, win);
        fileName += Str::Len(fileName) + 1;
    }
}

static void RotateLeft(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->prevCanvasBR.x = win->prevCanvasBR.y = -1;
    win->dm->rotateBy(-90);
}

static void RotateRight(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->prevCanvasBR.x = win->prevCanvasBR.y = -1;
    win->dm->rotateBy(90);
}

static void OnVScroll(WindowInfo *win, WPARAM wParam)
{
    SCROLLINFO   si = {0};
    int          iVertPos;
    int          lineHeight = 16;

    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);

    iVertPos = si.nPos;
    if (!displayModeContinuous(win->dm->displayMode()) && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
        lineHeight = 1;

    switch (LOWORD(wParam))
    {
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
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);

    // If the position has changed, scroll the window and update it
    if (win->dm && (si.nPos != iVertPos)) {
        win->dm->scrollYTo(si.nPos);
    }
}

static void OnHScroll(WindowInfo *win, WPARAM wParam)
{
    SCROLLINFO   si = {0};
    int          iVertPos;

    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);

    iVertPos = si.nPos;

    switch (LOWORD(wParam))
    {
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
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);
    GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);

    // If the position has changed, scroll the window and update it
    if (win->dm && (si.nPos != iVertPos))
        win->dm->scrollXTo(si.nPos);
}

static void OnMenuViewSinglePage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    win->SwitchToDisplayMode(DM_SINGLE_PAGE, true);
}

static void OnMenuViewFacing(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    win->SwitchToDisplayMode(DM_FACING, true);
}

static void OnMenuViewBook(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    win->SwitchToDisplayMode(DM_BOOK_VIEW, true);
}

static void AdjustWindowEdge(WindowInfo *win)
{
    DWORD exStyle = GetWindowLong(win->hwndCanvas, GWL_EXSTYLE);
    DWORD newStyle = exStyle;

    // Remove the canvas' edge in the cases where the vertical scrollbar
    // would otherwise touch the screen's edge, making the scrollbar much
    // easier to hit with the mouse (cf. Fitts' law)
    if (IsZoomed(win->hwndFrame) || win->fullScreen || win->presentation)
        newStyle &= ~WS_EX_STATICEDGE;
    else
        newStyle |= WS_EX_STATICEDGE;

    if (newStyle != exStyle) {
        SetWindowLong(win->hwndCanvas, GWL_EXSTYLE, newStyle);
        SetWindowPos(win->hwndCanvas, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

static void OnSize(WindowInfo *win, int dx, int dy)
{
    int rebBarDy = 0;
    if (gGlobalPrefs.m_showToolbar) {
        SetWindowPos(win->hwndReBar, NULL, 0, 0, dx, rebBarDy, SWP_NOZORDER);
        rebBarDy = gReBarDy + gReBarDyFrame;
    }
    
    if (win->tocLoaded && win->tocShow)
        win->ShowTocBox();
    else
        SetWindowPos(win->hwndCanvas, NULL, 0, rebBarDy, dx, dy-rebBarDy, SWP_NOZORDER);
    // Need this here, so that -page and -nameddest work correctly in continuous mode
    if (WS_SHOWING_PDF == win->state)
        win->ResizeIfNeeded();
}

void OnMenuCheckUpdate(WindowInfo *win)
{
    DownloadSumatraUpdateInfo(win, false);
}

static void OnMenuChangeLanguage(WindowInfo *win)
{
    int langId = LangGetIndex(gGlobalPrefs.m_currentLanguage);
    int newLangId = Dialog_ChangeLanguge(win->hwndFrame, langId);

    if (newLangId != -1 && langId != newLangId) {
        assert(0 <= newLangId && newLangId < LANGS_COUNT);
        if (newLangId < 0 || LANGS_COUNT <= newLangId)
            return;
        const char *langName = g_langs[newLangId]._langName;

        CurrLangNameSet(langName);
        RebuildMenuBar();
        UpdateToolbarToolText();
    }
}

static void OnMenuViewShowHideToolbar(WindowInfo *win)
{
    if (gGlobalPrefs.m_showToolbar)
        gGlobalPrefs.m_showToolbar = FALSE;
    else
        gGlobalPrefs.m_showToolbar = TRUE;

    // Move the focus out of the toolbar
    // TODO: do this for all windows?
    if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
        SetFocus(win->hwndFrame);

    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        if (gGlobalPrefs.m_showToolbar)
            ShowWindow(win->hwndReBar, SW_SHOW);
        else
            ShowWindow(win->hwndReBar, SW_HIDE);
        ClientRect rect(win->hwndFrame);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    }
}

static void OnMenuSettings(WindowInfo *win)
{
    if (gRestrictedUse) return;

    if (DIALOG_OK_PRESSED != Dialog_Settings(win->hwndFrame, &gGlobalPrefs))
        return;

    if (!gGlobalPrefs.m_rememberOpenedFiles)
        gFileHistory.Clear();

    Prefs_Save();
}

// toggles 'show pages continuously' state
static void OnMenuViewContinuous(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!win->dm) return;

    DisplayMode newMode = win->dm->displayMode();
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
    win->SwitchToDisplayMode(newMode);
}

static void ToogleToolbarViewButton(WindowInfo *win, float newZoom, bool pagesContinuously)
{
    assert(win && win->dm);
    if (!win || !win->dm) return;

    float zoom = win->dm->zoomVirtual();
    DisplayMode mode = win->dm->displayMode();

    if (displayModeContinuous(mode) != pagesContinuously || zoom != newZoom) {
        DisplayMode prevMode = win->prevDisplayMode;
        float prevZoom = win->prevZoomVirtual;

        if (displayModeContinuous(mode) != pagesContinuously)
            OnMenuViewContinuous(win);
        OnMenuZoom(win, MenuIdFromVirtualZoom(newZoom));

        // remember the previous values for when the toolbar button is unchecked
        if (INVALID_ZOOM == prevZoom) {
            win->prevZoomVirtual = zoom;
            win->prevDisplayMode = mode;
        }
        // keep the rememberd values when toggling between the two toolbar buttons
        else {
            win->prevZoomVirtual = prevZoom;
            win->prevDisplayMode = prevMode;
        }
    }
    else if (win->prevZoomVirtual != INVALID_ZOOM) {
        float prevZoom = win->prevZoomVirtual;
        win->SwitchToDisplayMode(win->prevDisplayMode);
        win->ZoomToSelection(prevZoom, false);
    }
}

static void OnMenuFitWidthContinuous(WindowInfo *win)
{
    ToogleToolbarViewButton(win, ZOOM_FIT_WIDTH, true);
}

static void OnMenuFitSinglePage(WindowInfo *win)
{
    ToogleToolbarViewButton(win, ZOOM_FIT_PAGE, false);
}

static void OnMenuGoToNextPage(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->dm->goToNextPage(0);
}

static void OnMenuGoToPrevPage(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->dm->goToPrevPage(0);
}

static void OnMenuGoToLastPage(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->dm->goToLastPage();
}

static void OnMenuGoToFirstPage(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;
    win->dm->goToFirstPage();
}

void WindowInfo::FocusPageNoEdit()
{
    if (GetFocus() == hwndPageBox)
        SendMessage(hwndPageBox, WM_SETFOCUS, 0, 0);
    else
        SetFocus(hwndPageBox);
}

static void OnMenuGoToPage(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.m_showToolbar && !win->fullScreen && PM_DISABLED == win->presentation) {
        win->FocusPageNoEdit();
        return;
    }

    int newPageNo = Dialog_GoToPage(win);
    if (win->dm->validPageNo(newPageNo))
        win->dm->goToPage(newPageNo, 0, true);
}

static void OnMenuFind(WindowInfo *win)
{
    if (!win || !win->PdfLoaded())
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.m_showToolbar && !win->fullScreen && PM_DISABLED == win->presentation) {
        win->FindStart();
        return;
    }

    ScopedMem<TCHAR> previousFind(Win::GetText(win->hwndFindBox));
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    ScopedMem<TCHAR> findString(Dialog_Find(win->hwndFrame, previousFind, &matchCase));
    if (!findString)
        return;

    Win::SetText(win->hwndFindBox, findString);
    Edit_SetModify(win->hwndFindBox, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase)
            state |= TBSTATE_CHECKED;
        else
            state &= ~TBSTATE_CHECKED;
        SendMessage(win->hwndToolbar, TB_SETSTATE, IDM_FIND_MATCH, state);
        win->dm->SetFindMatchCase(matchCase);
    }

    Find(win, FIND_FORWARD);
}

static void OnMenuViewRotateLeft(WindowInfo *win)
{
    RotateLeft(win);
}

static void OnMenuViewRotateRight(WindowInfo *win)
{
    RotateRight(win);
}

static void WindowInfo_EnterFullscreen(WindowInfo *win, bool presentation)
{
    if ((presentation ? win->presentation : win->fullScreen) ||
        !IsWindowVisible(win->hwndFrame) || gPluginMode)
        return;

    assert(presentation ? !win->fullScreen : !win->presentation);
    if (presentation) {
        assert(win->dm);
        if (!win->dm)
            return;

        if (IsZoomed(win->hwndFrame))
            win->_windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        else
            win->_windowStateBeforePresentation = WIN_STATE_NORMAL;
        win->presentation = PM_ENABLED;
        win->_tocBeforePresentation = win->tocShow;

        SetTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
    }
    else {
        win->fullScreen = true;
        win->_tocBeforeFullScreen = win->PdfLoaded() ? win->tocShow : false;
    }

    // Remove TOC from full screen, add back later on exit fullscreen
    if (win->tocShow)
        win->HideTocBox();

    int x, y, w, h;
    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    HMONITOR m = MonitorFromWindow(win->hwndFrame, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(m, (LPMONITORINFOEX)&mi)) {
        x = 0;
        y = 0;
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
    }
    else {
        x = mi.rcMonitor.left;
        y = mi.rcMonitor.top;
        w = RectDx(&mi.rcMonitor);
        h = RectDy(&mi.rcMonitor);
    }
    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    if (!presentation || !win->fullScreen)
        win->prevStyle = ws;
    ws &= ~(WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    win->frameRc = WindowRect(win->hwndFrame);

    SetMenu(win->hwndFrame, NULL);
    ShowWindow(win->hwndReBar, SW_HIDE);
    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);
    SetWindowPos(win->hwndFrame, HWND_NOTOPMOST, x, y, w, h, SWP_FRAMECHANGED|SWP_NOZORDER);
    SetWindowPos(win->hwndCanvas, NULL, 0, 0, w, h, SWP_NOZORDER);

    if (presentation)
        win->dm->setPresentationMode(true);

    // Make sure that no toolbar/sidebar keeps the focus
    SetFocus(win->hwndFrame);
}

static void WindowInfo_ExitFullscreen(WindowInfo *win)
{
    if (!win->fullScreen && !win->presentation) 
        return;

    bool wasPresentation = PM_DISABLED != win->presentation;
    if (wasPresentation && win->dm) {
        win->dm->setPresentationMode(false);
        win->presentation = PM_DISABLED;
    }
    else
        win->fullScreen = false;

    if (wasPresentation) {
        KillTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID);
        SetCursor(gCursorArrow);
    }

    if (win->dm && (wasPresentation ? win->_tocBeforePresentation : win->_tocBeforeFullScreen))
        win->ShowTocBox();

    if (gGlobalPrefs.m_showToolbar)
        ShowWindow(win->hwndReBar, SW_SHOW);
    SetMenu(win->hwndFrame, win->menu);
    SetWindowLong(win->hwndFrame, GWL_STYLE, win->prevStyle);
    SetWindowPos(win->hwndFrame, HWND_NOTOPMOST,
                 win->frameRc.x, win->frameRc.y,
                 win->frameRc.dx, win->frameRc.dy,
                 SWP_FRAMECHANGED|SWP_NOZORDER);
}

static void OnMenuViewFullscreen(WindowInfo *win, bool presentation=false)
{
    assert(win);
    if (!win)
        return;

    bool enterFullscreen = presentation ? !win->presentation : !win->fullScreen;

    if (!win->presentation && !win->fullScreen)
        RememberWindowPosition(win);
    else
        WindowInfo_ExitFullscreen(win);

    if (enterFullscreen)
        WindowInfo_EnterFullscreen(win, presentation);
}

static void OnMenuViewPresentation(WindowInfo *win)
{
    OnMenuViewFullscreen(win, true);
}

static void WindowInfo_ShowSearchResult(WindowInfo *win, PdfSel *result, bool wasModified)
{
    assert(result->len > 0);
    win->dm->goToPage(result->pages[0], 0, wasModified);

    PdfSelection *sel = win->dm->textSelection;
    sel->Reset();
    sel->result.pages = (int *)memdup(result->pages, result->len * sizeof(int));
    sel->result.rects = (RectI *)memdup(result->rects, result->len * sizeof(RectI));
    sel->result.len = result->len;

    UpdateTextSelection(win, false);
    win->dm->ShowResultRectToScreen(result);
    win->RepaintAsync();
}

// Show a message for 3000 millisecond at most
static DWORD WINAPI ShowMessageThread(LPVOID data)
{
    WindowInfo *win = (WindowInfo *)data;
    ShowWindowAsync(win->hwndFindStatus, SW_SHOWNA);
    WaitForSingleObject(win->stopFindStatusThreadEvent, 3000);
    if (!win->findStatusVisible)
        ShowWindowAsync(win->hwndFindStatus, SW_HIDE);
    return 0;
}

// Display the message 'message' asynchronously
// If resize = true then the window width is adjusted to the length of the text
static void WindowInfo_ShowMessage_Async(WindowInfo *win, const TCHAR *message, bool resize)
{
    if (message)
        Win::SetText(win->hwndFindStatus, message);
    if (resize) {
        // compute the length of the message
        RECT rc = RectI(0, 0, FIND_STATUS_WIDTH, 0).ToRECT();
        HDC hdc = GetDC(win->hwndFindStatus);
        HGDIOBJ oldFont = SelectObject(hdc, gDefaultGuiFont);
        DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        ReleaseDC(win->hwndFindStatus, hdc);
        rc.right += MulDiv(15, win->dpi, USER_DEFAULT_SCREEN_DPI);
        rc.bottom = MulDiv(23, win->dpi, USER_DEFAULT_SCREEN_DPI);
        AdjustWindowRectEx(&rc, GetWindowLong(win->hwndFindStatus, GWL_STYLE), FALSE, GetWindowLong(win->hwndFindStatus, GWL_EXSTYLE));
        MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN + rc.left, FIND_STATUS_MARGIN + rc.top, rc.right - rc.left + FIND_STATUS_MARGIN, rc.bottom - rc.top, TRUE);
    }

    win->findStatusVisible = false;
    // if a thread has previously been started then make sure it has ended
    if (win->findStatusThread) {
        SetEvent(win->stopFindStatusThreadEvent);
        WaitForSingleObject(win->findStatusThread, INFINITE);
        CloseHandle(win->findStatusThread);
    }
    ResetEvent(win->stopFindStatusThreadEvent);
    win->findStatusThread = CreateThread(NULL, 0, ShowMessageThread, win, 0, 0);
}

// hide the message
static void WindowInfo_HideMessage(WindowInfo *win)
{
    if (!win->findStatusThread) 
        return;

    SetEvent(win->findStatusThread);
    CloseHandle(win->findStatusThread);
    win->findStatusThread = NULL;
    ShowWindowAsync(win->hwndFindStatus, SW_HIDE);
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects)
{
    win->fwdsearchmarkRects.Reset();
    if (ret == PDFSYNCERR_SUCCESS && rects.Count() > 0 ) {
        // remember the position of the search result for drawing the rect later on
        const PdfPageInfo *pi = win->dm->getPageInfo(page);
        if (pi) {
            WindowInfo_HideMessage(win);

            RectI overallrc;
            RectI rc = rects[0];
            win->pdfsync->convert_coord_from_internal(&rc, pi->page.Convert<int>().dy, BottomLeft);

            overallrc = rc;
            for (size_t i = 0; i < rects.Count(); i++)
            {
                rc = rects[i];
                win->pdfsync->convert_coord_from_internal(&rc, pi->page.Convert<int>().dy, BottomLeft);
                overallrc = overallrc.Union(rc);
                win->fwdsearchmarkRects.Push(rc);
            }
            win->fwdsearchmarkPage = page;
            win->showForwardSearchMark = true;
            if (!gGlobalPrefs.m_fwdsearchPermanent) 
            {
                win->fwdsearchmarkHideStep = 0;
                SetTimer(win->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, NULL);
            }
            
            // Scroll to show the overall highlighted zone
            int pageNo = page;
            PdfSel res = { 1, &pageNo, &overallrc };
            if (!win->dm->pageVisible(page))
                win->dm->goToPage(page, 0, true);
            if (!win->dm->ShowResultRectToScreen(&res))
                win->RepaintAsync();
            if (IsIconic(win->hwndFrame))
                ShowWindowAsync(win->hwndFrame, SW_RESTORE);
            return;
        }
    }

    TCHAR *buf = NULL;    
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND )
        buf = Str::Dup(_TR("No synchronization file found"));
    else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
        buf = Str::Dup(_TR("Synchronization file cannot be opened"));
    else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER)
        buf = Str::Format(_TR("Page number %u inexistant"), page);
    else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION)
        buf = Str::Dup(_TR("No synchronization info at this position"));
    else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE)
        buf = Str::Format(_TR("Unknown source file (%s)"), srcfilename);
    else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE)
        buf = Str::Format(_TR("Source file %s has no synchronization point"), srcfilename);
    else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE)
        buf = Str::Format(_TR("No result found around line %u in file %s"), line, srcfilename);
    else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD)
        buf = Str::Format(_TR("No result found around line %u in file %s"), line, srcfilename);

    WindowInfo_ShowMessage_Async(win, buf, true);
    free(buf);
}

static void WindowInfo_ShowFindStatus(WindowInfo *win)
{
    LPARAM disable = (LPARAM)MAKELONG(0,0);

    MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN, FIND_STATUS_MARGIN, MulDiv(FIND_STATUS_WIDTH, win->dpi, USER_DEFAULT_SCREEN_DPI), MulDiv(23, win->dpi, USER_DEFAULT_SCREEN_DPI) + FIND_STATUS_PROGRESS_HEIGHT + 8, false);
    ShowWindow(win->hwndFindStatus, SW_SHOWNA);
    win->findStatusVisible = true;

    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, disable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, disable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, disable);
}

static void WindowInfo_HideFindStatus(WindowInfo *win, bool canceled=false)
{
    LPARAM enable = (LPARAM)MAKELONG(1,0);

    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, enable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, enable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, enable);

    // resize the window, in case another message has been displayed in the meantime
    MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN, FIND_STATUS_MARGIN, MulDiv(FIND_STATUS_WIDTH, win->dpi, USER_DEFAULT_SCREEN_DPI), MulDiv(23, win->dpi, USER_DEFAULT_SCREEN_DPI) + FIND_STATUS_PROGRESS_HEIGHT + 8, false);
    if (canceled)
        WindowInfo_ShowMessage_Async(win, NULL, false);
    else if (!win->dm->bFoundText)
        WindowInfo_ShowMessage_Async(win, _TR("No matches were found"), false);
    else {
        ScopedMem<TCHAR> buf(Str::Format(_TR("Found text at page %d"), win->dm->currentPageNo()));
        WindowInfo_ShowMessage_Async(win, buf, false);
    }    
}

static void OnMenuFindNext(WindowInfo *win)
{
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_NEXT, 0))
        Find(win, FIND_FORWARD);
}

static void OnMenuFindPrev(WindowInfo *win)
{
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_PREV, 0))
        Find(win, FIND_BACKWARD);
}

static void OnMenuFindMatchCase(WindowInfo *win)
{
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win->dm->SetFindMatchCase((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindBox, TRUE);
}

static void AdvanceFocus(WindowInfo *win)
{
    // Tab order: Frame -> Page -> Find -> ToC -> Frame -> ...

    bool reversed = IsShiftPressed();
    bool hasToolbar = !win->fullScreen && !win->presentation && gGlobalPrefs.m_showToolbar;
    bool hasToC = win->tocLoaded && win->tocShow;

    HWND current = GetFocus();
    HWND next = win->hwndFrame;

    if (current == win->hwndFrame) {
        if ((!hasToolbar || reversed) && hasToC)
            next = win->hwndTocTree;
        else if (!reversed && hasToolbar)
            next = win->hwndPageBox;
        else if (reversed && hasToolbar)
            next = win->hwndFindBox;
    } else if (current == win->hwndTocTree) {
        if (reversed && hasToolbar)
            next = win->hwndFindBox;
    } else if (current == win->hwndPageBox) {
        if (!reversed)
            next = win->hwndFindBox;
    } else if (current == win->hwndFindBox) {
        if (reversed)
            next = win->hwndPageBox;
        else if (hasToC)
            next = win->hwndTocTree;
    }

    SetFocus(next);
}

static bool OnKeydown(WindowInfo *win, WPARAM key, LPARAM lparam, bool inTextfield=false)
{
    if (!win || !win->dm)
        return false;
    
    //DBG_OUT("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
        return false;

    if (VK_PRIOR == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->dm->zoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToPrevPage(-1);
    } else if (VK_NEXT == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->dm->zoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToNextPage(0);
    } else if (VK_UP == key) {
        if (win->dm->needVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        else
            win->dm->goToPrevPage(-1);
    } else if (VK_DOWN == key) {
        if (win->dm->needVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        else
            win->dm->goToNextPage(0);
    } else if (inTextfield) {
        // The remaining keys have a different meaning
        return false;
    } else if (VK_LEFT == key) {
        if (win->dm->needHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, IsShiftPressed() ? SB_PAGELEFT : SB_LINELEFT, 0);
        else
            win->dm->goToPrevPage(0);
    } else if (VK_RIGHT == key) {
        if (win->dm->needHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, IsShiftPressed() ? SB_PAGERIGHT : SB_LINERIGHT, 0);
        else
            win->dm->goToNextPage(0);
    } else if (VK_HOME == key) {
        win->dm->goToFirstPage();
    } else if (VK_END == key) {
        win->dm->goToLastPage();    
    } else {
        return false;
    }

    return true;
}

static void ClearSearch(WindowInfo *win)
{
    DeleteOldSelectionInfo(win);
    if (win->dm)
        win->dm->textSelection->Reset();
    win->RepaintAsync();
}

static void OnChar(WindowInfo *win, WPARAM key)
{
//    DBG_OUT("char=%d,%c\n", key, (char)key);

    if (!win) return;
    if (IsCharUpper((TCHAR)key))
        key = (TCHAR)CharLower((LPTSTR)(TCHAR)key);

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        ChangePresentationMode(win, PM_ENABLED);
        return;
    }

    switch (key) {
    case VK_ESCAPE:
        if (win->findThread)
            win->AbortFinding();
        else if (win->presentation)
            OnMenuViewPresentation(win);
        else if (gGlobalPrefs.m_escToExit)
            CloseWindow(win, TRUE);
        else if (win->fullScreen)
            OnMenuViewFullscreen(win);
        else if (win->showSelection)
            ClearSearch(win);
        return;
    case 'q':
        CloseWindow(win, TRUE);
        return;
    case 'r':
        win->Reload();
        return;
    }

    if (!win->dm)
        return;

    switch (key) {
    case VK_TAB:
        AdvanceFocus(win);
        break;
    case VK_SPACE:
    case VK_RETURN:
        OnKeydown(win, IsShiftPressed() ? VK_PRIOR : VK_NEXT, 0);
        break;
    case VK_BACK:
        {
            bool forward = IsShiftPressed();
            win->dm->navigate(forward ? 1 : -1);
        }
        break;
    case 'g':
        OnMenuGoToPage(win);
        break;
    case 'j':
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        break;
    case 'k':
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        break;
    case 'n':
        win->dm->goToNextPage(0);
        break;
    case 'p':
        win->dm->goToPrevPage(0);
        break;
    case 'z':
        win->ToggleZoom();
        break;
    case '+':
        win->ZoomToSelection(ZOOM_IN_FACTOR, true);
        break;
    case '-':
        win->ZoomToSelection(ZOOM_OUT_FACTOR, true);
        break;
    case '/':
        OnMenuFind(win);
        break;
    case 'c':
        OnMenuViewContinuous(win);
        break;
    case 'b':
        {
            // experimental "e-book view": flip a single page
            bool forward = !IsShiftPressed();
            bool alreadyFacing = displayModeFacing(win->dm->displayMode());
            int currPage = win->dm->currentPageNo();

            if (alreadyFacing && (forward ? win->dm->lastBookPageVisible()
                                          : win->dm->firstBookPageVisible()))
                break;

            DisplayMode newMode = DM_BOOK_VIEW;
            if (displayModeShowCover(win->dm->displayMode()))
                newMode = DM_FACING;
            if (displayModeContinuous(win->dm->displayMode()))
                newMode = DM_BOOK_VIEW == newMode ? DM_CONTINUOUS_BOOK_VIEW : DM_CONTINUOUS_FACING;
            win->SwitchToDisplayMode(newMode);

            if (!alreadyFacing)
                ; // don't do anything further
            else if (forward && currPage >= win->dm->currentPageNo() &&
                     (currPage > 1 || newMode == DM_BOOK_VIEW || newMode == DM_CONTINUOUS_BOOK_VIEW))
                win->dm->goToNextPage(0);
            else if (!forward && currPage <= win->dm->currentPageNo())
                win->dm->goToPrevPage(0);
        }
        break;
    case '.':
        // for Logitech's wireless presenters which target PowerPoint's shortcuts
        if (win->presentation)
            ChangePresentationMode(win, PM_BLACK_SCREEN);
        break;
    case 'w':
        if (win->presentation)
            ChangePresentationMode(win, PM_WHITE_SCREEN);
        break;
    case 'i':
        // experimental "page info" tip: make figuring out current page and
        // total pages count a one-key action (unless they're already visible)
        if (!gGlobalPrefs.m_showToolbar || win->fullScreen || PM_ENABLED == win->presentation) {
            int current = win->dm->currentPageNo(), total = win->dm->pageCount();
            ScopedMem<TCHAR> pageInfo(Str::Format(_T("%s %d / %d"), _TR("Page:"), current, total));
            WindowInfo_ShowMessage_Async(win, pageInfo, true);
        }
        break;
#ifdef DEBUG
    case '$':
        gUseGdiRenderer = !gUseGdiRenderer;
        win->Reload();
        break;
#endif
    }
}

class GoToTocLinkWorkItem : public UIThreadWorkItem
{
    PdfTocItem *tocItem;

public:
    GoToTocLinkWorkItem(WindowInfo *win, PdfTocItem *ti) :
        UIThreadWorkItem(win), tocItem(ti) {}

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->dm)
            win->dm->goToTocLink(tocItem->link);
    }
};

static void GoToTocLinkForTVItem(WindowInfo *win, HWND hTV, HTREEITEM hItem=NULL, bool allowExternal=true)
{
    if (!hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);
    PdfTocItem *tocItem = (PdfTocItem *)item.lParam;
    if (win->dm && tocItem && (allowExternal || tocItem->link && PDF_LGOTO == tocItem->link->kind)) {
        gUIThreadMarshaller.Queue(new GoToTocLinkWorkItem(win, tocItem));
    }
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    TBBUTTON tbButton = {0};
    tbButton.idCommand = gToolbarButtons[i].cmdId;
    if (TbIsSeparator(&gToolbarButtons[i])) {
        tbButton.fsStyle = TBSTYLE_SEP;
    } else {
        tbButton.iBitmap = gToolbarButtons[i].bmpIndex;
        tbButton.fsState = TBSTATE_ENABLED;
        tbButton.fsStyle = TBSTYLE_BUTTON;
        tbButton.iString = (INT_PTR)Translations_GetTranslation(gToolbarButtons[i].toolTip);
    }
    return tbButton;
}

#define WS_TOOLBAR (WS_CHILD | WS_CLIPSIBLINGS | \
                    TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | \
                    TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN )

static void BuildTBBUTTONINFO(TBBUTTONINFO& info, TCHAR *txt) {
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
    info.pszText = txt;
}

// Set toolbar button tooltips taking current language into account.
static void UpdateToolbarButtonsToolTipsForWindow(WindowInfo* win)
{
    TBBUTTONINFO buttonInfo;
    HWND hwnd = win->hwndToolbar;
    LRESULT res;
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        WPARAM buttonId = (WPARAM)i;
        const char *txt = gToolbarButtons[i].toolTip;
        if (NULL == txt)
            continue;
        const TCHAR *translation = Translations_GetTranslation(txt);
        BuildTBBUTTONINFO(buttonInfo, (TCHAR *)translation);
        res = SendMessage(hwnd, TB_SETBUTTONINFOW, buttonId, (LPARAM)&buttonInfo);
        assert(0 != res);
    }
}

static void UpdateToolbarToolText(void)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        UpdateToolbarPageText(win, -1);
        UpdateToolbarFindText(win);
        UpdateToolbarButtonsToolTipsForWindow(win);
    }        
}

static void RebuildMenuBar(void)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows[i];
        HMENU oldMenu = win->menu;
        win->menu = NULL;
        BuildMenu(win);
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
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (FocusUnselectedWndProc(hwnd, message)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == message) {
        if (VK_ESCAPE == wParam)
        {
            if (win->findThread)
                win->AbortFinding();
            else
                SetFocus(win->hwndFrame);
            return 1;
        } 

        if (VK_RETURN == wParam)
        {
            Find(win);
            return 1;
        }

        if (VK_TAB == wParam)
        {
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
        if (OnKeydown(win, wParam, lParam, true))
            return 0;
    }

    LRESULT ret = CallWindowProc(DefWndProcFindBox, hwnd, message, wParam, lParam);

    if (WM_CHAR  == message ||
        WM_PASTE == message ||
        WM_CUT   == message ||
        WM_CLEAR == message ||
        WM_UNDO  == message) {
        ToolbarUpdateStateForWindow(win);
    }

    return ret;
}

class FindEndWorkItem : public UIThreadWorkItem
{
    PdfSel *pdfSel;
    bool    wasModifiedCanceled;

public:
    FindEndWorkItem(WindowInfo *win, PdfSel *pdfSel, bool wasModifiedCanceled) :
        UIThreadWorkItem(win), pdfSel(pdfSel), wasModifiedCanceled(wasModifiedCanceled) {}

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (!win->dm) {
            // the document was closed while finding
            WindowInfo_ShowMessage_Async(win, NULL, false);
        } else if (pdfSel) {
            WindowInfo_ShowSearchResult(win, pdfSel, wasModifiedCanceled);
            WindowInfo_HideFindStatus(win);
        } else {
            // nothing found or search canceled
            ClearSearch(win);
            WindowInfo_HideFindStatus(win, wasModifiedCanceled);
        }
    }
};

typedef struct FindThreadData {
    WindowInfo *win;
    PdfSearchDirection direction;
    bool wasModified;
    TCHAR text[256];
} FindThreadData;

static DWORD WINAPI FindThread(LPVOID data)
{
    FindThreadData *ftd = (FindThreadData *)data;
    WindowInfo *win = ftd->win;

    PdfSel *rect;
    if (ftd->wasModified || !win->dm->validPageNo(win->dm->lastFoundPage()) ||
        !win->dm->getPageInfo(win->dm->lastFoundPage())->visible)
        rect = win->dm->Find(ftd->direction, ftd->text);
    else
        rect = win->dm->Find(ftd->direction);

    if (!win->findCanceled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (FIND_FORWARD == ftd->direction) ? 1 : win->dm->pageCount();
        if (!ftd->wasModified || win->dm->currentPageNo() != startPage)
            rect = win->dm->Find(ftd->direction, ftd->text, startPage);
    }

    if (rect && !win->findCanceled)
        gUIThreadMarshaller.Queue(new FindEndWorkItem(win, rect, ftd->wasModified));
    else
        gUIThreadMarshaller.Queue(new FindEndWorkItem(win, NULL, win->findCanceled));

    free(ftd);

    HANDLE hThread = win->findThread;
    win->findThread = NULL;
    CloseHandle(hThread);

    return 0;
}

static void Find(WindowInfo *win, PdfSearchDirection direction)
{
    win->AbortFinding();

    FindThreadData ftd;
    ftd.win = win;
    ftd.direction = direction;
    GetWindowText(win->hwndFindBox, ftd.text, dimof(ftd.text));
    ftd.wasModified = Edit_GetModify(win->hwndFindBox);
    Edit_SetModify(win->hwndFindBox, FALSE);

    bool hasText = Str::Len(ftd.text) > 0;
    if (hasText)
        win->findThread = CreateThread(NULL, 0, FindThread, _memdup(&ftd), 0, 0);
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

static LRESULT CALLBACK WndProcFindStatus(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_ERASEBKGND == message) {
        ClientRect rect(hwnd);
        DrawFrameControl((HDC)wParam, &rect.ToRECT(), DFC_BUTTON, DFCS_BUTTONPUSH);
        return true;
    } else if (WM_PAINT == message) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HFONT oldfnt = SelectFont(hdc, gDefaultGuiFont);
        TCHAR text[256];

        ClientRect rect(hwnd);
        GetWindowText(hwnd, text, 256);

        SetBkMode(hdc, TRANSPARENT);
        rect.x += 10; rect.dx -= 10;
        rect.y += 4; rect.dy -= 4;
        DrawText(hdc, text, Str::Len(text), &rect.ToRECT(), DT_LEFT);
        
        int width = MulDiv(FIND_STATUS_WIDTH, win->dpi, USER_DEFAULT_SCREEN_DPI) - 20;
        rect.dx = width;
        rect.y += MulDiv(20, win->dpi, USER_DEFAULT_SCREEN_DPI);
        rect.dy = FIND_STATUS_PROGRESS_HEIGHT;
        paint_rect(hdc, &rect.ToRECT());
        
        int percent = win->findPercent;
        if (percent > 100)
            percent = 100;
        rect.x += 2;
        rect.dx = width * percent / 100 - 3;
        rect.y += 2;
        rect.dy -= 3;
        FillRect(hdc, &rect.ToRECT(), gBrushShadow);

        SelectFont(hdc, oldfnt);
        EndPaint(hwnd, &ps);
        return WM_PAINT_HANDLED;
    } else if (WM_SETTEXT == message) {
        InvalidateRect(hwnd, NULL, true);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SIZE TextSizeInHwnd(HWND hwnd, const TCHAR *txt)
{
    SIZE sz;
    size_t txtLen = Str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ prev = SelectObject(dc, f);
    GetTextExtentPoint32(dc, txt, txtLen, &sz);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return sz;
}

#define TOOLBAR_MIN_ICON_SIZE 16
#define FIND_BOX_WIDTH 160

static void UpdateToolbarFindText(WindowInfo *win)
{
    const TCHAR *text = _TR("Find:");
    Win::SetText(win->hwndFindText, text);

    WindowRect findWndRect(win->hwndFindBg);
    int findWndDx = findWndRect.dx;
    int findWndDy = findWndRect.dy;

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - findWndDy) / 2;

    SIZE size = TextSizeInHwnd(win->hwndFindText, text);
    size.cx += 6;

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win->hwndFindText, pos_x, (findWndDy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    MoveWindow(win->hwndFindBg, pos_x + size.cx, pos_y, findWndDx, findWndDy, false);
    MoveWindow(win->hwndFindBox, pos_x + size.cx + padding, (findWndDy - size.cy + 1) / 2 + pos_y,
        findWndDx - 2 * padding, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.cx + findWndDx + 12);
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

static void CreateFindBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND findBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(FIND_BOX_WIDTH * win->uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 4),
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND find = CreateWindowEx(0, WC_EDIT, _T(""), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                            0, 1, (int)(FIND_BOX_WIDTH * win->uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 2),
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND status = CreateWindowEx(WS_EX_TOPMOST, FINDSTATUS_CLASS_NAME, _T(""), WS_CHILD|SS_CENTER,
                            0, 0, 0, 0, win->hwndCanvas, (HMENU)0, hInst, NULL);

    SetWindowFont(label, gDefaultGuiFont, true);
    SetWindowFont(find, gDefaultGuiFont, true);
    SetWindowFont(status, gDefaultGuiFont, true);

    if (!DefWndProcToolbar)
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcFindBox);

    win->hwndFindText = label;
    win->hwndFindBox = find;
    win->hwndFindBg = findBg;
    win->hwndFindStatus = status;

    UpdateToolbarFindText(win);
}

static WNDPROC DefWndProcPageBox = NULL;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (FocusUnselectedWndProc(hwnd, message)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == message) {
        if (VK_RETURN == wParam) {
            ScopedMem<TCHAR> buf(Win::GetText(win->hwndPageBox));
            int newPageNo;
            newPageNo = _ttoi(buf);
            if (win->dm->validPageNo(newPageNo)) {
                win->dm->goToPage(newPageNo, 0, true);
                SetFocus(win->hwndFrame);
            }
            return 1;
        }
        else if (VK_ESCAPE == wParam) {
            SetFocus(win->hwndFrame);
            return 1;
        }
        else if (VK_TAB == wParam) {
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
        if (OnKeydown(win, wParam, lParam, true))
            return 0;
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, message, wParam, lParam);
}

#define PAGE_BOX_WIDTH 40
static void UpdateToolbarPageText(WindowInfo *win, int pageCount)
{
    const TCHAR *text = _TR("Page:");
    Win::SetText(win->hwndPageText, text);
    SIZE size = TextSizeInHwnd(win->hwndPageText, text);
    size.cx += 6;

    WindowRect pageWndRect(win->hwndPageBg);
    int pageWndDx = pageWndRect.dx;
    int pageWndDy = pageWndRect.dy;

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDM_OPEN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - pageWndDy) / 2;

    TCHAR *buf;
    if (-1 == pageCount)
        buf = Win::GetText(win->hwndPageTotal);
    else if (0 != pageCount)
        buf = Str::Format(_T(" / %d"), pageCount);
    else
        buf = Str::Dup(_T(""));

    Win::SetText(win->hwndPageTotal, buf);
    SIZE size2 = TextSizeInHwnd(win->hwndPageTotal, buf);
    size2.cx += 6;
    free(buf);

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win->hwndPageText, pos_x, (pageWndDy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    MoveWindow(win->hwndPageBg, pos_x + size.cx, pos_y, pageWndDx, pageWndDy, false);
    MoveWindow(win->hwndPageBox, pos_x + size.cx + padding, (pageWndDy - size.cy + 1) / 2 + pos_y,
        pageWndDx - 2 * padding, size.cy, false);
    MoveWindow(win->hwndPageTotal, pos_x + size.cx + pageWndDx, (pageWndDy - size.cy + 1) / 2 + pos_y, size2.cx, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.cx + pageWndDx + size2.cx + 12);
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
}

static void CreatePageBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND pageBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(PAGE_BOX_WIDTH * win->uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 4),
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND page = CreateWindowEx(0, WC_EDIT, _T("0"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                            0, 1, (int)(PAGE_BOX_WIDTH * win->uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 2),
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND total = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    SetWindowFont(label, gDefaultGuiFont, true);
    SetWindowFont(page, gDefaultGuiFont, true);
    SetWindowFont(total, gDefaultGuiFont, true);

    if (!DefWndProcPageBox)
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win->hwndPageText = label;
    win->hwndPageBox = page;
    win->hwndPageBg = pageBg;
    win->hwndPageTotal = total;

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

static void CreateToolbar(WindowInfo *win, HINSTANCE hInst) {
    HWND hwndOwner = win->hwndFrame;
    HWND hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_TOOLBAR,
                                 0,0,0,0, hwndOwner,(HMENU)IDC_TOOLBAR, hInst,NULL);
    win->hwndToolbar = hwndToolbar;
    LRESULT lres = SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    ShowWindow(hwndToolbar, SW_SHOW);
    HIMAGELIST himl = 0;
    TBBUTTON tbButtons[TOOLBAR_BUTTONS_COUNT];

    HBITMAP hbmp = LoadExternalBitmap(hInst, _T("toolbar_10.bmp"), IDB_TOOLBAR);
    BITMAP bmp;
    GetObject(hbmp, sizeof(BITMAP), &bmp);
    // stretch the toolbar bitmaps for higher DPI settings
    // TODO: get nicely interpolated versions of the toolbar icons for higher resolutions
    if (win->uiDPIFactor > 1 && bmp.bmHeight < TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor) {
        bmp.bmWidth = (LONG)(bmp.bmWidth * win->uiDPIFactor);
        bmp.bmHeight = (LONG)(bmp.bmHeight * win->uiDPIFactor);
        hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, bmp.bmWidth, bmp.bmHeight, LR_COPYDELETEORG);
    }
    // Assume square icons
    himl = ImageList_Create(bmp.bmHeight, bmp.bmHeight, ILC_COLORDDB | ILC_MASK, 0, 0);
    ImageList_AddMasked(himl, hbmp, RGB(255, 0, 255));
    DeleteObject(hbmp);

    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        tbButtons[i] = TbButtonFromButtonInfo(i);
        if (gToolbarButtons[i].cmdId == IDM_FIND_MATCH) {
            tbButtons[i].fsStyle = BTNS_CHECK;
        }
    }
    lres = SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    LRESULT exstyle = SendMessage(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    lres = SendMessage(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    lres = SendMessage(hwndToolbar, TB_ADDBUTTONSW, TOOLBAR_BUTTONS_COUNT, (LPARAM)tbButtons);

    RECT rc;
    lres = SendMessage(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);

    DWORD  reBarStyle = WS_REBAR | WS_VISIBLE;
    win->hwndReBar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL, reBarStyle,
                             0,0,0,0, hwndOwner, (HMENU)IDC_REBAR, hInst, NULL);
    if (!win->hwndReBar)
        SeeLastError();

    REBARINFO rbi;
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    lres = SendMessage(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);

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
    lres = SendMessage(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, NULL, 0, 0, 0, 0, SWP_NOZORDER);
    gReBarDy = WindowRect(win->hwndReBar).dy;
    //TODO: this was inherited but doesn't seem to be right (makes toolbar
    // partially unpainted if using classic scheme on xp or vista
    //gReBarDyFrame = bIsAppThemed ? 0 : 2;
    gReBarDyFrame = 0;
    
    CreatePageBox(win, hInst);
    CreateFindBox(win, hInst);
}

static LRESULT CALLBACK WndProcSpliter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);

    switch (message)
    {
        case WM_MOUSEMOVE:
            if (win->resizingTocBox) {
                POINT pcur;
                GetCursorPos(&pcur);
                ScreenToClient(win->hwndFrame, &pcur);
                int tocWidth = pcur.x;

                ClientRect r(win->hwndTocBox);
                int prevTocWidth = r.dx;
                r = ClientRect(win->hwndFrame);
                int width = r.dx - tocWidth - SPLITTER_DX;
                int prevCanvasWidth = r.dx - prevTocWidth - SPLITTER_DX;
                int height = r.dy;

                // TODO: ensure that the window is always wide enough for both
                if (tocWidth < min(SPLITTER_MIN_WIDTH, prevTocWidth) ||
                    width < min(SPLITTER_MIN_WIDTH, prevCanvasWidth)) {
                    SetCursor(gCursorNo);
                    break;
                }
                SetCursor(gCursorSizeWE);

                int tocY = 0;
                if (gGlobalPrefs.m_showToolbar && !win->fullScreen && !win->presentation) {
                    tocY = gReBarDy + gReBarDyFrame;
                    height -= tocY;
                }

                MoveWindow(win->hwndTocBox, 0, tocY, tocWidth, height, true);
                MoveWindow(win->hwndCanvas, tocWidth + SPLITTER_DX, tocY, width, height, true);
                MoveWindow(hwnd, tocWidth, tocY, SPLITTER_DX, height, true);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            win->resizingTocBox = true;
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            win->resizingTocBox = false;
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void WindowInfo::FindStart()
{
    if (GetFocus() == hwndFindBox)
        SendMessage(hwndFindBox, WM_SETFOCUS, 0, 0);
    else
        SetFocus(hwndFindBox);
}

class UpdateFindStatusWorkItem : public UIThreadWorkItem
{
    int current;
    int total;
public:
    UpdateFindStatusWorkItem(WindowInfo *win, int current, int total)
        : UIThreadWorkItem(win), current(current), total(total)
    {}

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        win->findPercent = current * 100 / total;
        if (!win->findStatusVisible)
            WindowInfo_ShowFindStatus(win);

        ScopedMem<TCHAR> buf(Str::Format(_TR("Searching %d of %d..."), current, total));
        Win::SetText(win->hwndFindStatus, buf);
    }
};

bool WindowInfo::FindUpdateStatus(int current, int total)
{
    if (!findCanceled)
        gUIThreadMarshaller.Queue(new UpdateFindStatusWorkItem(this, current, total));
    return !findCanceled;
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
    switch (message) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.m_escToExit)
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
            if (win && !IsCursorOverWindow(win->hwndTocTree))
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

static void CustomizeToCInfoTip(WindowInfo *win, LPNMTVGETINFOTIP nmit);
#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd);
#endif

static WNDPROC DefWndProcTocBox = NULL;
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    switch (message) {
    case WM_SIZE: {
        WindowRect rc(hwnd);

        HWND titleLabel = GetDlgItem(hwnd, 0);
        ScopedMem<TCHAR> text(Win::GetText(titleLabel));
        SIZE size = TextSizeInHwnd(titleLabel, text);

        int offset = (int)(2 * win->uiDPIFactor);
        if (size.cy < 16) size.cy = 16;
        size.cy += 2 * offset;

        HWND closeIcon = GetDlgItem(hwnd, 1);
        MoveWindow(titleLabel, offset, offset, rc.dx - 2 * offset, size.cy - 2 * offset, true);
        MoveWindow(closeIcon, rc.dx - 16, (size.cy - 16) / 2, 16, 16, true);
        MoveWindow(win->hwndTocTree, 0, size.cy, rc.dx, rc.dy - size.cy, true);
        break;
    }
    case WM_DRAWITEM:
        if (1 == wParam) // close button
        {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            DrawFrameControl(dis->hDC, &dis->rcItem, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
            return TRUE;
        }
        break;
    case WM_COMMAND:
        if (HIWORD(wParam) == STN_CLICKED)
            win->ToggleTocBox();
        break;
    case WM_NOTIFY:
        if (LOWORD(wParam) == IDC_PDF_TOC_TREE) {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
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
                TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)lParam;
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
                switch (((LPNMCUSTOMDRAW)lParam)->dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                        return CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
                    case CDDS_ITEMPOSTPAINT:
                        RelayoutTocItem((LPNMTVCUSTOMDRAW)lParam);
                        // fall through
                    default:
                        return CDRF_DODEFAULT;
                }
                break;
#else
                return CDRF_DODEFAULT;
#endif
            case TVN_GETINFOTIP:
                CustomizeToCInfoTip(win, (LPNMTVGETINFOTIP)lParam);
                break;
            }
        }
        break;
    }
    return CallWindowProc(DefWndProcTocBox, hwnd, message, wParam, lParam);
}

static void CreateTocBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND spliter = CreateWindow(SPLITER_CLASS_NAME, _T(""), WS_CHILDWINDOW, 0, 0, 0, 0,
                                win->hwndFrame, (HMENU)0, hInst, NULL);
    win->hwndSpliter = spliter;
    
    win->hwndTocBox = CreateWindow(WC_STATIC, _T(""), WS_CHILD,
                        0,0,gGlobalPrefs.m_tocDx,0, win->hwndFrame, (HMENU)IDC_PDF_TOC_TREE_TITLE, hInst, NULL);
    HWND titleLabel = CreateWindow(WC_STATIC, _TR("Bookmarks"), WS_VISIBLE | WS_CHILD,
                        0,0,0,0, win->hwndTocBox, (HMENU)0, hInst, NULL);
    SetWindowFont(titleLabel, gDefaultGuiFont, true);

    HWND closeToc = CreateWindow(WC_STATIC, _T(""),
                        SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                        0, 0, 16, 16, win->hwndTocBox, (HMENU)1, hInst, NULL);
    SetClassLongPtr(closeToc, GCLP_HCURSOR, (LONG_PTR)gCursorHand);

    win->hwndTocTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("TOC"),
                        TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                        TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                        WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                        0,0,0,0, win->hwndTocBox, (HMENU)IDC_PDF_TOC_TREE, hInst, NULL);

    assert(win->hwndTocTree);
    if (!win->hwndTocTree)
        SeeLastError();
#ifdef UNICODE
    else
        TreeView_SetUnicodeFormat(win->hwndTocTree, true);
#endif

    if (NULL == DefWndProcTocTree)
        DefWndProcTocTree = (WNDPROC)GetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC, (LONG_PTR)WndProcTocTree);

    if (NULL == DefWndProcTocBox)
        DefWndProcTocBox = (WNDPROC)GetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC, (LONG_PTR)WndProcTocBox);
}

static HTREEITEM AddTocItemToView(HWND hwnd, PdfTocItem *entry, HTREEITEM parent, bool toggleItem)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvinsert.itemex.state = entry->open != toggleItem ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)entry;
    // Replace unprintable whitespace with regular spaces
    Str::TransChars(entry->title, _T("\t\n\v\f\r"), _T("     "));
    tvinsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    if (entry->pageNo) {
        tvinsert.itemex.pszText = Str::Format(_T("%s  %d"), entry->title, entry->pageNo);
        HTREEITEM hItem = TreeView_InsertItem(hwnd, &tvinsert);
        free(tvinsert.itemex.pszText);
        return hItem;
    }
#endif
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static bool WasItemToggled(PdfTocItem *entry, int *tocState)
{
    if (!tocState || tocState[0] <= 0)
        return false;

    for (int i = 1; i <= tocState[0]; i++)
        if (tocState[i] == entry->id)
            return true;

    return false;
}

static void PopulateTocTreeView(HWND hwnd, PdfTocItem *entry, int *tocState, HTREEITEM parent = NULL)
{
    for (; entry; entry = entry->next) {
        bool toggleItem = WasItemToggled(entry, tocState);
        HTREEITEM node = AddTocItemToView(hwnd, entry, parent, toggleItem);
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
    item.mask = TVIF_TEXT;
    item.pszText = szText;
    item.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTV, &item);

    // Draw the page number right-aligned (if there is one)
    TCHAR *lpPageNo = item.pszText + Str::Len(item.pszText);
    for (lpPageNo--; lpPageNo > item.pszText && ChrIsDigit(*lpPageNo); lpPageNo--);
    if (lpPageNo > item.pszText && ' ' == *lpPageNo && *(lpPageNo + 1) && ' ' == *--lpPageNo) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, lpPageNo, Str::Len(lpPageNo), &txtSize);
        rcPageNo.left = rcPageNo.right - txtSize.cx;

        SetTextColor(ncd->hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(ncd->hdc, GetSysColor(COLOR_WINDOW));
        DrawText(ncd->hdc, lpPageNo, -1, &rcPageNo, DT_SINGLELINE | DT_VCENTER);

        // Reduce the size of the label and cut off the page number
        rcItem.right = max(rcItem.right - txtSize.cx, 0);
        *lpPageNo = 0;
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
    DrawText(ncd->hdc, item.pszText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

void WindowInfo::LoadTocTree()
{
    if (tocLoaded)
        return;

    tocRoot = dm->getTocTree();
    if (tocRoot)
        PopulateTocTreeView(hwndTocTree, tocRoot, tocState);

    tocLoaded = true;
}

void WindowInfo::ToggleTocBox()
{
    if (!PdfLoaded())
        return;
    if (!tocShow) {
        ShowTocBox();
        SetFocus(hwndTocTree);
    } else {
        HideTocBox();
    }
}

void WindowInfo::ShowTocBox()
{
    if (!dm->hasTocTree()) {
        tocShow = true;
        return;
    }

    if (PM_BLACK_SCREEN == presentation || PM_WHITE_SCREEN == presentation)
        return;

    LoadTocTree();

    int cw, ch, cx, cy;

    ClientRect rframe(hwndFrame);
    UpdateTocWidth(this, NULL, rframe.dx / 4);

    if (gGlobalPrefs.m_showToolbar && !fullScreen && !presentation)
        cy = gReBarDy + gReBarDyFrame;
    else
        cy = 0;
    ch = rframe.dy - cy;

    // make sure that the sidebar is never too wide or too narrow
    cx = WindowRect(hwndTocBox).dx;
    if (rframe.dx <= 2 * SPLITTER_MIN_WIDTH)
        cx = rframe.dx / 2;
    else if (cx >= rframe.dx - SPLITTER_MIN_WIDTH)
        cx = rframe.dx - SPLITTER_MIN_WIDTH;
    else if (cx < SPLITTER_MIN_WIDTH)
        cx = SPLITTER_MIN_WIDTH;
    cw = rframe.dx - cx - SPLITTER_DX;

    SetWindowPos(hwndTocBox, NULL, 0, cy, cx, ch, SWP_NOZORDER|SWP_SHOWWINDOW);
    SetWindowPos(hwndSpliter, NULL, cx, cy, SPLITTER_DX, ch, SWP_NOZORDER|SWP_SHOWWINDOW);
    SetWindowPos(hwndCanvas, NULL, cx + SPLITTER_DX, cy, cw, ch, SWP_NOZORDER|SWP_SHOWWINDOW);

    tocShow = true;
    this->UpdateTocSelection(dm->currentPageNo());
}

void WindowInfo::HideTocBox()
{
    int cy = 0;
    if (gGlobalPrefs.m_showToolbar && !fullScreen && !presentation)
        cy = gReBarDy + gReBarDyFrame;

    if (GetFocus() == hwndTocTree)
        SetFocus(hwndFrame);

    ClientRect r(hwndFrame);
    SetWindowPos(hwndCanvas, NULL, 0, cy, r.dx, r.dy - cy, SWP_NOZORDER);
    ShowWindow(hwndTocBox, SW_HIDE);
    ShowWindow(hwndSpliter, SW_HIDE);

    tocShow = false;
}

void WindowInfo::ClearTocBox()
{
    if (!tocLoaded) return;

    TreeView_DeleteAllItems(hwndTocTree);
    delete tocRoot;
    tocRoot = NULL;

    tocLoaded = false;
    currPageNo = 0;
}

static void CustomizeToCInfoTip(WindowInfo *win, LPNMTVGETINFOTIP nmit)
{
    PdfTocItem *tocItem = (PdfTocItem *)nmit->lParam;
    TCHAR *path = win->dm->getLinkPath(tocItem->link);
    if (!path)
        return;

    Str::Str<TCHAR> infotip(INFOTIPSIZE);

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
        infotip.Append(_T("\r\n"));
    }

    if (PDF_LLAUNCH == tocItem->link->kind && fz_dictgets(tocItem->link->dest, "EF")) {
        TCHAR *comment = Str::Format(_TR("Attachment: %s"), path);
        free(path);
        path = comment;
    }

    infotip.Append(path);
    Str::BufSet(nmit->pszText, nmit->cchTextMax, infotip.Get());
    free(path);
}

TOOLINFO WindowInfo::CreateToolInfo(const TCHAR *text)
{
    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = this->hwndCanvas;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (TCHAR *)text;
    return ti;
}

void WindowInfo::DeleteInfotip()
{
    if (!this->infotipVisible)
        return;

    SendMessage(this->hwndInfotip, TTM_DELTOOL, 0, (LPARAM)&this->CreateToolInfo());
    this->infotipVisible = false;
}

static void CreateInfotipForPdfLink(WindowInfo *win, int pageNo, pdf_link *link)
{
    ScopedMem<TCHAR> linkPath(win->dm->getLinkPath(link));
    if (linkPath) {
        TOOLINFO ti = win->CreateToolInfo(linkPath);
        fz_rect rect = win->dm->rectCvtUserToScreen(pageNo, link->rect);
        ti.rect = Rect<float>(rect.x0, rect.y0, rect.x1, rect.y1).ToRECT();

        SendMessage(win->hwndInfotip, win->infotipVisible ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0, (LPARAM)&ti);
        win->infotipVisible = true;
    } else
        win->DeleteInfotip();
}

static int  gDeltaPerLine = 0;         // for mouse wheel logic
static bool gWheelMsgRedirect = false; // set when WM_MOUSEWHEEL has been passed on (to prevent recursion)

static LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int          currentScrollPos;
    LRESULT      res;
    POINT        pt;

    WindowInfo * win = FindWindowInfoByHwnd(hwnd);
    if (win && win->IsAboutWindow()) {
        bool handled;
        res = HandleWindowAboutMsg(win, hwnd, message, wParam, lParam, handled);
        if (handled)
            return res;
    }

    switch (message)
    {
        case WM_VSCROLL:
            OnVScroll(win, wParam);
            return WM_VSCROLL_HANDLED;

        case WM_HSCROLL:
            OnHScroll(win, wParam);
            return WM_HSCROLL_HANDLED;

        case WM_MOUSEMOVE:
            if (win)
                OnMouseMove(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDBLCLK:
            if (win) {
                if ((win->fullScreen || win->presentation) && !gGlobalPrefs.m_enableTeXEnhancements)
                    OnMouseLeftButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
                else
                    OnMouseLeftButtonDblClk(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
            break;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUp(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_MBUTTONDOWN:
            if (win && WS_SHOWING_PDF == win->state)
            {
                SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);
                // TODO: Create window that shows location of initial click for reference
                OnMouseMiddleButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (win)
                OnMouseRightButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_RBUTTONUP:
            if (win)
                OnMouseRightButtonUp(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_SETCURSOR:
            if (!win)
                return DefWindowProc(hwnd, message, wParam, lParam);

            switch (win->state) {
            case WS_SHOWING_PDF:
                if (win->mouseAction != MA_IDLE)
                    win->DeleteInfotip();

                switch (win->mouseAction) {
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
                        pdf_link *link = win->dm->getLinkAtPosition(pt.x, pt.y);
                        if (link) {
                            int pageNo = win->dm->getPageNoByPoint(pt.x, pt.y);
                            CreateInfotipForPdfLink(win, pageNo, link);
                            SetCursor(gCursorHand);
                            return TRUE;
                        }
                        win->DeleteInfotip();
                        if (win->dm->isOverText(pt.x, pt.y))
                            SetCursor(gCursorIBeam);
                        else
                            SetCursor(gCursorArrow);
                        return TRUE;
                    }
                    win->DeleteInfotip();
                }
                if (win->presentation)
                    return TRUE;
                break;

            default:
                win->DeleteInfotip();
                break;
            }
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_TIMER:
            assert(win);
            if (win) {
                switch (wParam) {
                case REPAINT_TIMER_ID:
                    win->delayedRepaintTimer = 0;
                    KillTimer(hwnd, REPAINT_TIMER_ID);
                    win->RedrawAll();
                    break;
                case SMOOTHSCROLL_TIMER_ID:
                    if (MA_SCROLLING == win->mouseAction)
                        win->MoveDocBy(win->xScrollSpeed, win->yScrollSpeed);
                    else if (MA_SELECTING == win->mouseAction || MA_SELECTING_TEXT == win->mouseAction) {
                        GetCursorPos(&pt);
                        ScreenToClient(win->hwndCanvas, &pt);
                        OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
                    }
                    else {
                        KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
                        win->yScrollSpeed = 0;
                        win->xScrollSpeed = 0;
                    }
                    break;
                case HIDE_CURSOR_TIMER_ID:
                    KillTimer(hwnd, HIDE_CURSOR_TIMER_ID);
                    if (win->presentation)
                        SetCursor(NULL);
                    break;
                case HIDE_FWDSRCHMARK_TIMER_ID:
                    {
                        win->fwdsearchmarkHideStep++;
                        if (win->fwdsearchmarkHideStep == 1 )
                        {
                            SetTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS, NULL);
                        }
                        else if (win->fwdsearchmarkHideStep >= HIDE_FWDSRCHMARK_STEPS )
                        {
                            KillTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID);
                            win->showForwardSearchMark = false;
                            win->fwdsearchmarkHideStep = 0;
                            win->RepaintAsync();
                        }
                        else
                        {
                            win->RepaintAsync();
                        }
                    }
                    break;
                case AUTO_RELOAD_TIMER_ID:
                    KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
                    win->Reload(true);
                    break;
                }
            }
            break;

        case WM_DROPFILES:
            if (win)
                OnDropFiles(win, (HDROP)wParam);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            /* it might happen that we get WM_PAINT after destroying a window */
            if (win) {
                OnPaint(win);
            }
            break;

        case WM_MOUSEWHEEL:
            if (!win || !win->dm)
                break;

            // Scroll the ToC sidebar, if it's visible and the cursor is in it
            if (win->tocShow && IsCursorOverWindow(win->hwndTocTree) && !gWheelMsgRedirect) {
                // Note: hwndTocTree's window procedure doesn't always handle
                //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
                //       here recursively - prevent that
                gWheelMsgRedirect = true;
                LRESULT res = SendMessage(win->hwndTocTree, message, wParam, lParam);
                gWheelMsgRedirect = false;
                return res;
            }

            // Note: not all mouse drivers correctly report the Ctrl key's state
            if ((LOWORD(wParam) & MK_CONTROL) || IsCtrlPressed() || (LOWORD(wParam) & MK_RBUTTON)) {
                GetCursorPos(&pt);
                ScreenToClient(win->hwndCanvas, &pt);

                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                float factor = delta < 0 ? ZOOM_OUT_FACTOR : ZOOM_IN_FACTOR;
                win->dm->zoomBy(factor, &pt);
                win->UpdateToolbarState();
                return 0;
            }

            if (gDeltaPerLine == 0)
               break;

            win->wheelAccumDelta += GET_WHEEL_DELTA_WPARAM(wParam);     // 120 or -120
            currentScrollPos = GetScrollPos(win->hwndCanvas, SB_VERT);

            while (win->wheelAccumDelta >= gDeltaPerLine) {
                SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
                win->wheelAccumDelta -= gDeltaPerLine;
            }
            while (win->wheelAccumDelta <= -gDeltaPerLine) {
                SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
                win->wheelAccumDelta += gDeltaPerLine;
            }

            if (!displayModeContinuous(win->dm->displayMode()) &&
                GetScrollPos(win->hwndCanvas, SB_VERT) == currentScrollPos) {
                if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
                    win->dm->goToPrevPage(-1);
                else
                    win->dm->goToNextPage(0);
            }
            return 0;

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
    // and let gUIThreadMarshaller.Queue call PostMessage for us
    gUIThreadMarshaller.Queue(new RepaintCanvasWorkItem(this, delay));
}

static void UpdateMenu(WindowInfo *win, HMENU m)
{
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id) {
        RebuildFileMenu(win, GetSubMenu(win->menu, 0));
    }
    MenuUpdateStateForWindow(win);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int             wmId;
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
                RememberWindowPosition(win);
                AdjustWindowEdge(win);

                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                OnSize(win, dx, dy);
            }
            break;

        case WM_MOVE:
            if (win) {
                RememberWindowPosition(win);
                AdjustWindowEdge(win);
            }
            break;

        case WM_INITMENUPOPUP:
            UpdateMenu(win, (HMENU)wParam);
            break;

        case WM_COMMAND:
            wmId = LOWORD(wParam);

            // check if the menuId belongs to an entry in the list of
            // recently opened files and load the referenced file if it does
            if (IDM_FILE_HISTORY_FIRST <= wmId && wmId <= IDM_FILE_HISTORY_LAST)
            {
                DisplayState *state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
                if (state) {
                    LoadDocument(state->filePath, win);
                    break;
                }
            }

            switch (wmId)
            {
                case IDM_OPEN:
                case IDT_FILE_OPEN:
                    OnMenuOpen(win);
                    break;
                case IDM_SAVEAS:
                    OnMenuSaveAs(win);
                    break;

                case IDT_FILE_PRINT:
                case IDM_PRINT:
                    OnMenuPrint(win);
                    break;

                case IDT_FILE_EXIT:
                case IDM_CLOSE:
                    CloseWindow(win, FALSE);
                    break;

                case IDM_EXIT:
                    OnMenuExit();
                    break;

                case IDM_REFRESH:
                    win->Reload();
                    break;

                case IDM_SAVEAS_BOOKMARK:
                    OnMenuSaveBookmark(win);
                    break;

                case IDT_VIEW_FIT_WIDTH:
                    if (win->dm)
                        OnMenuFitWidthContinuous(win);
                    break;

                case IDT_VIEW_FIT_PAGE:
                    if (win->dm)
                        OnMenuFitSinglePage(win);
                    break;

                case IDT_VIEW_ZOOMIN:
                    if (win->dm)
                        win->ZoomToSelection(ZOOM_IN_FACTOR, true);
                    break;

                case IDT_VIEW_ZOOMOUT:
                    if (win->dm)
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
                    OnMenuZoom(win, (UINT)wmId);
                    break;

                case IDM_ZOOM_CUSTOM:
                    OnMenuCustomZoom(win);
                    break;

                case IDM_VIEW_SINGLE_PAGE:
                    OnMenuViewSinglePage(win);
                    break;

                case IDM_VIEW_FACING:
                    OnMenuViewFacing(win);
                    break;

                case IDM_VIEW_BOOK:
                    OnMenuViewBook(win);
                    break;

                case IDM_VIEW_CONTINUOUS:
                    OnMenuViewContinuous(win);
                    break;

                case IDM_VIEW_SHOW_HIDE_TOOLBAR:
                    OnMenuViewShowHideToolbar(win);
                    break;

                case IDM_CHANGE_LANGUAGE:
                    OnMenuChangeLanguage(win);
                    break;

                case IDM_VIEW_BOOKMARKS:
                    if (win)
                        win->ToggleTocBox();
                    break;

                case IDM_GOTO_NEXT_PAGE:
                    OnMenuGoToNextPage(win);
                    break;

                case IDM_GOTO_PREV_PAGE:
                    OnMenuGoToPrevPage(win);
                    break;

                case IDM_GOTO_FIRST_PAGE:
                    OnMenuGoToFirstPage(win);
                    break;

                case IDM_GOTO_LAST_PAGE:
                    OnMenuGoToLastPage(win);
                    break;

                case IDM_GOTO_PAGE:
                    OnMenuGoToPage(win);
                    break;

                case IDM_VIEW_PRESENTATION_MODE:
                    OnMenuViewPresentation(win);
                    break;

                case IDM_VIEW_FULLSCREEN:
                    OnMenuViewFullscreen(win);
                    break;

                case IDM_VIEW_ROTATE_LEFT:
                    OnMenuViewRotateLeft(win);
                    break;

                case IDM_VIEW_ROTATE_RIGHT:
                    OnMenuViewRotateRight(win);
                    break;

                case IDM_FIND_FIRST:
                    OnMenuFind(win);
                    break;

                case IDM_FIND_NEXT:
                    OnMenuFindNext(win);
                    break;

                case IDM_FIND_PREV:
                    OnMenuFindPrev(win);
                    break;

                case IDM_FIND_MATCH:
                    OnMenuFindMatchCase(win);
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
                    OnMenuCheckUpdate(win);
                    break;

                case IDM_SETTINGS:
                    OnMenuSettings(win);
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
                    OnMenuProperties(win);
                    break;

                case IDM_MOVE_FRAME_FOCUS:
                    if (win->hwndFrame != GetFocus())
                        SetFocus(win->hwndFrame);
                    else if (win->tocShow)
                        SetFocus(win->hwndTocTree);
                    break;

                case IDM_GOTO_NAV_BACK:
                    if (win->dm)
                        win->dm->navigate(-1);
                    break;
                    
                case IDM_GOTO_NAV_FORWARD:
                    if (win->dm)
                        win->dm->navigate(1);
                    break;

                case IDM_COPY_SELECTION:
                    // Don't break the shortcut for text boxes
                    if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
                        SendMessage(GetFocus(), WM_COPY, 0, 0);
                    else if (win->hwndPdfProperties == GetForegroundWindow())
                        CopyPropertiesToClipboard(win->hwndPdfProperties);
                    else if (win->selectionOnPage)
                        CopySelectionToClipboard(win);
                    else
                        WindowInfo_ShowMessage_Async(win, _TR("Select content with Ctrl+left mouse button"), true);
                    break;

                case IDM_SELECT_ALL:
                    if (win->dm)
                        OnSelectAll(win);
                    break;

                case IDM_CRASH_ME:
                    CrashMe();
                    break;

                case IDM_THREAD_STRESS:
                    ToggleThreadStress(win);
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

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
            OnChar(win, wParam);
            break;

        case WM_KEYDOWN:
            OnKeydown(win, wParam, lParam);
            break;

        case WM_SETTINGCHANGE:
InitMouseWheelInfo:
            SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
            // ulScrollLines usually equals 3 or 0 (for no scrolling)
            // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40
            if (ulScrollLines)
                gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
            else
                gDeltaPerLine = 0;
            return 0;

        case WM_MOUSEWHEEL:
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
    wcex.lpfnWndProc    = WndProcFindStatus;
    wcex.hCursor        = LoadCursor(NULL, IDC_APPSTARTING);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = FINDSTATUS_CLASS_NAME;
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
    gCursorScroll = LoadCursor(NULL, IDC_SIZEALL);
    gCursorDrag  = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gCursorSizeWE = LoadCursor(NULL, IDC_SIZEWE);
    gCursorNo    = LoadCursor(NULL, IDC_NO);
    gBrushBg     = CreateSolidBrush(COL_WINDOW_BG);
    gBrushWhite  = CreateSolidBrush(WIN_COL_WHITE);
    gBrushBlack  = CreateSolidBrush(WIN_COL_BLACK);
    gBrushShadow = CreateSolidBrush(COL_WINDOW_SHADOW);

    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirect(&ncm.lfMessageFont);
    gBitmapReloadingCue = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_RELOADING_CUE));
    
    return true;
}

static bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors=true)
{
    TCHAR       devstring[256];      // array for WIN.INI data 
    HANDLE      printer;
    LPDEVMODE   devMode = NULL;
    DWORD       structSize, returnCode;
    bool        success = false;

    ScopedMem<TCHAR> fileName2(Path::Normalize(fileName));
    PdfEngine *pdfEngine = PdfEngine::CreateFromFileName(fileName2);

    if (!pdfEngine || !pdfEngine->hasPermission(PDF_PERM_PRINT)) {
        if (displayErrors)
            MessageBox(NULL, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Retrieve the printer, printer driver, and 
    // output-port names from WIN.INI. 
    GetProfileString(_T("Devices"), printerName, _T(""), devstring, dimof(devstring));

    // Parse the string of names, setting ptrs as required 
    // If the string contains the required names, use them to 
    // create a device context. 
    TCHAR *driver = _tcstok (devstring, (const TCHAR *)_T(","));
    TCHAR *port = _tcstok((TCHAR *) NULL, (const TCHAR *)_T(","));

    if (!driver || !port) {
        if (displayErrors)
            MessageBox(NULL, _T("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return false;
    }
    
    bool fOk = OpenPrinter((LPTSTR)printerName, &printer, NULL);
    if (!fOk) {
        if (displayErrors)
            MessageBox(NULL, _TR("Could not open Printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    HDC  hdcPrint = NULL;
    structSize = DocumentProperties(NULL,
        printer,                /* Handle to our printer. */ 
        (LPTSTR)printerName,    /* Name of the printer. */ 
        NULL,                   /* Asking for size, so */ 
        NULL,                   /* these are not used. */ 
        0);                     /* Zero returns buffer size. */
    devMode = (LPDEVMODE)malloc(structSize);
    if (!devMode) goto Exit;

    // Get the default DevMode for the printer and modify it for your needs.
    returnCode = DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* The address of the buffer to fill. */ 
        NULL,           /* Not using the input buffer. */ 
        DM_OUT_BUFFER); /* Have the output buffer filled. */ 

    if (IDOK != returnCode) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _T("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
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
            MessageBox(NULL, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }
    if (CheckPrinterStretchDibSupport(NULL, hdcPrint)) {
        PRINTPAGERANGE pr = { 1, pdfEngine->pageCount() };
        PrintToDevice(pdfEngine, hdcPrint, devMode, 1, &pr);
        success = true;
    }
Exit:
    free(devMode);
    DeleteDC(hdcPrint);
    return success;
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

#ifdef DEBUG
    extern void BaseUtils_UnitTests(void);
    BaseUtils_UnitTests();
    extern void SumatraPDF_UnitTests(void);
    SumatraPDF_UnitTests();
#endif

    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    fz_accelerate();

    {
        ScopedMem<TCHAR> prefsFilename(Prefs_GetFileName());
        SerializableGlobalPrefs_Init();
        if (!Prefs::Load(prefsFilename, &gGlobalPrefs, &gFileHistory)) {
            // assume that this is because prefs file didn't exist
            // i.e. this could be the first time Sumatra is launched.
            const char *lang = GuessLanguage();
            CurrLangNameSet(lang);
        }
    }

    CommandLineInfo i;
    i.bgColor = gGlobalPrefs.m_bgColor;
    i.fwdsearchOffset = gGlobalPrefs.m_fwdsearchOffset;
    i.fwdsearchWidth = gGlobalPrefs.m_fwdsearchWidth;
    i.fwdsearchColor = gGlobalPrefs.m_fwdsearchColor;
    i.fwdsearchPermanent = gGlobalPrefs.m_fwdsearchPermanent;
    i.escToExit = gGlobalPrefs.m_escToExit;
    i.invertColors = gGlobalPrefs.m_invertColors;

    i.ParseCommandLine(GetCommandLine());

    if (i.showConsole)
        RedirectIOToConsole();
    if (i.makeDefault)
        AssociateExeWithPdfExtension();
    if (i.filesToBenchmark.Count() > 0) {
        Bench(i.filesToBenchmark);
        // TODO: allow to redirect stdout/stderr to file
        if (i.showConsole)
            system("pause");
    }
    if (i.exitImmediately)
        goto Exit;

    gGlobalPrefs.m_bgColor = i.bgColor;
    gGlobalPrefs.m_fwdsearchOffset = i.fwdsearchOffset;
    gGlobalPrefs.m_fwdsearchWidth = i.fwdsearchWidth;
    gGlobalPrefs.m_fwdsearchColor = i.fwdsearchColor;
    gGlobalPrefs.m_fwdsearchPermanent = i.fwdsearchPermanent;
    gGlobalPrefs.m_escToExit = i.escToExit;
    gGlobalPrefs.m_invertColors = i.invertColors;
    gRestrictedUse = i.restrictedUse;

    gRenderCache.invertColors = &gGlobalPrefs.m_invertColors;
    gRenderCache.useGdiRenderer = &gUseGdiRenderer;

    if (i.inverseSearchCmdLine) {
        free(gGlobalPrefs.m_inverseSearchCmdLine);
        gGlobalPrefs.m_inverseSearchCmdLine = i.inverseSearchCmdLine;
        i.inverseSearchCmdLine = NULL;
        gGlobalPrefs.m_enableTeXEnhancements = TRUE;
    }
    CurrLangNameSet(i.lang);

    {
        ScopedMem<TCHAR> crashDumpPath(GetUniqueCrashDumpPath());
        InstallCrashHandler(crashDumpPath);
    }

    msg.wParam = 1; // set an error code, in case we prematurely have to goto Exit
    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    if (i.hwndPluginParent) {
        if (!IsWindow(i.hwndPluginParent) || i.fileNames.Count() == 0)
            goto Exit;

        gPluginMode = true;
        assert(i.fileNames.Count() == 1);
        while (i.fileNames.Count() > 1)
            free(i.fileNames.Pop());
        i.reuseInstance = i.exitOnPrint = false;
        // always display the toolbar when embedded (as there's no menubar in that case)
        gGlobalPrefs.m_showToolbar = TRUE;
    }

    WindowInfo *win = NULL;
    bool firstDocLoaded = false;
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

    for (size_t n = 0; n < i.fileNames.Count(); n++) {
        if (i.reuseInstance && !i.printDialog) {
            // delegate file opening to a previously running instance by sending a DDE message 
            TCHAR fullpath[MAX_PATH];
            GetFullPathName(i.fileNames[n], dimof(fullpath), fullpath, NULL);
            ScopedMem<TCHAR> command(Str::Format(_T("[") DDECOMMAND_OPEN _T("(\"%s\", 0, 1, 0)]"), fullpath));
            DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            if (i.destName && !firstDocLoaded) {
                ScopedMem<TCHAR> command(Str::Format(_T("[") DDECOMMAND_GOTO _T("(\"%s\", \"%s\")]"), fullpath, i.destName));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
            else if (i.pageNumber > 0 && !firstDocLoaded) {
                ScopedMem<TCHAR> command(Str::Format(_T("[") DDECOMMAND_PAGE _T("(\"%s\", %d)]"), fullpath, i.pageNumber));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
            if ((i.startView != DM_AUTOMATIC || i.startZoom != INVALID_ZOOM ||
                 i.startScroll.x != -1 && i.startScroll.y != -1) && !firstDocLoaded) {
                ScopedMem<TCHAR> viewMode(Str::Conv::FromUtf8(DisplayModeNameFromEnum(i.startView)));
                ScopedMem<TCHAR> command(Str::Format(_T("[") DDECOMMAND_SETVIEW _T("(\"%s\", \"%s\", %.2f, %d, %d)]"),
                                         fullpath, viewMode, i.startZoom, i.startScroll.x, i.startScroll.y));
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
            }
        }
        else {
            bool showWin = !(i.printDialog && i.exitOnPrint) && !gPluginMode;
            win = LoadDocument(i.fileNames[n], NULL, showWin);
            if (!win || win->state != WS_SHOWING_PDF)
                msg.wParam++; // set an error code for the next goto Exit
            if (!win)
                goto Exit;
            if (WS_SHOWING_PDF == win->state && i.destName && !firstDocLoaded) {
                ScopedMem<char> tmp(Str::Conv::ToUtf8(i.destName));
                win->dm->goToNamedDest(tmp);
            }
            else if (WS_SHOWING_PDF == win->state && i.pageNumber > 0 && !firstDocLoaded) {
                if (win->dm->validPageNo(i.pageNumber))
                    win->dm->goToPage(i.pageNumber, 0);
            }
            if (i.hwndPluginParent)
                MakePluginWindow(win, i.hwndPluginParent);
            if (WS_SHOWING_PDF == win->state && !firstDocLoaded && (i.enterPresentation || i.enterFullscreen))
                WindowInfo_EnterFullscreen(win, i.enterPresentation);
            if (i.startView != DM_AUTOMATIC && !firstDocLoaded)
                win->SwitchToDisplayMode(i.startView);
            if (i.startZoom != INVALID_ZOOM && !firstDocLoaded)
                win->ZoomToSelection(i.startZoom, false);
            if (i.startScroll.x != -1 || i.startScroll.y != -1) {
                ScrollState ss;
                if (win->dm->getScrollState(&ss)) {
                    ss.x = i.startScroll.x;
                    ss.y = i.startScroll.y;
                    win->dm->setScrollState(&ss);
                }
            }
        }

        if (i.printDialog)
            OnMenuPrint(win);
        firstDocLoaded = true;
    }

    if (i.reuseInstance || i.printDialog && i.exitOnPrint)
        goto Exit;
 
    if (!firstDocLoaded) {
        bool enterFullscreen = (WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState);
        win = WindowInfo_CreateEmpty();
        if (!win) {
            msg.wParam = 1;
            goto Exit;
        }

        if (WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState ||
            WIN_STATE_MAXIMIZED == gGlobalPrefs.m_windowState)
            ShowWindow(win->hwndFrame, SW_MAXIMIZE);
        else
            ShowWindow(win->hwndFrame, SW_SHOW);
        UpdateWindow(win->hwndFrame);

        if (enterFullscreen)
            WindowInfo_EnterFullscreen(win);
    }

    if (!firstDocLoaded)
        UpdateToolbarAndScrollbarsForAllWindows();

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs.m_pdfAssociateShouldAssociate && win)
        RegisterForPdfExtentions(win->hwndFrame);

    if (gGlobalPrefs.m_enableAutoUpdate)
        DownloadSumatraUpdateInfo(gWindows[0], true);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SUMATRAPDF));
#ifndef THREAD_BASED_FILEWATCH
    const UINT_PTR timerID = SetTimer(NULL, -1, FILEWATCH_DELAY_IN_MS, NULL);
#endif

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
        // TODO: this isn't called during an inner message loop, so
        //       Execute() also has to be called from a WndProc
        gUIThreadMarshaller.Execute();
    }

#ifndef THREAD_BASED_FILEWATCH
    KillTimer(NULL, timerID);
#endif
    
Exit:
    while (gWindows.Count() > 0)
        WindowInfo_Delete(gWindows[0]);
    DeleteObject(gBrushBg);
    DeleteObject(gBrushWhite);
    DeleteObject(gBrushBlack);
    DeleteObject(gBrushShadow);
    DeleteObject(gDefaultGuiFont);
    DeleteBitmap(gBitmapReloadingCue);

    Translations_FreeData();
    SerializableGlobalPrefs_Deinit();

    return (int)msg.wParam;
}
