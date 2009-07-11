/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv2 */
   
#include "SumatraPDF.h"

#include "file_util.h"
#include "geom_util.h"
#include "str_strsafe.h"
#include "str_util.h"
#include "strlist_util.h"
#include "translations.h"
#include "utf_util.h"
#include "win_util.h"
#include "tstr_util.h"

#include "AppPrefs.h"
#include "SumatraDialogs.h"
#include "FileHistory.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <direct.h> /* for _mkdir() */

#include <shellapi.h>
#include <shlobj.h>

#include "WinUtil.hpp"
#include <windowsx.h>
#include <Wininet.h>

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST           IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST            IDM_VIEW_CONTINUOUS_FACING

#ifndef CURR_VERSION
#define CURR_VERSION "0.9.4"
#endif

// #define SVN_PRE_RELEASE_VER 994

#define _QUOTEME(x) #x
#define QM(x) _QUOTEME(x)

#ifdef SVN_PRE_RELEASE_VER
#define UPDATE_CHECK_VER _T(QM(SVN_PRE_RELEASE_VER))
#else
#define UPDATE_CHECK_VER _T(CURR_VERSION)
#endif

// this sucks but I don't know any other way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

//#define FANCY_UI 1

/* Define if you want to conserve memory by always freeing cached bitmaps
   for pages not visible. Only enable for stress-testing the logic. On
   desktop machine we usually have plenty memory */
//#define CONSERVE_MEMORY 1

/* Next action for the benchmark mode */
#define MSG_BENCH_NEXT_ACTION WM_USER + 1

#define ZOOM_IN_FACTOR      1.2
#define ZOOM_OUT_FACTOR     1.0 / ZOOM_IN_FACTOR

/* if TRUE, we're in debug mode where we show links as blue rectangle on
   the screen. Makes debugging code related to links easier.
   TODO: make a menu item in DEBUG build to turn it on/off. */
#ifdef DEBUG
static BOOL             gDebugShowLinks = TRUE;
#else
static BOOL             gDebugShowLinks = FALSE;
#endif

/* default UI settings */

// Note: Only make changes to these values #ifndef BUILD_RM_VERSION
#define DEFAULT_DISPLAY_MODE    DM_CONTINUOUS
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_ROTATION        0

/* define if want to use double-buffering for rendering the PDF. Takes more memory!. */
#define DOUBLE_BUFFER 1

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

#define MAX_LOADSTRING 100

#define WM_CREATE_FAILED -1
#define WM_CREATE_OK 0
#define WM_NCPAINT_HANDLED 0
#define WM_VSCROLL_HANDLED 0
#define WM_HSCROLL_HANDLED 0

#define WM_APP_REPAINT_DELAYED (WM_APP + 10)
#define WM_APP_REPAINT_NOW     (WM_APP + 11)
#define WM_APP_URL_DOWNLOADED  (WM_APP + 12)

#define COL_WHITE RGB(0xff,0xff,0xff)
#define COL_BLACK RGB(0,0,0)
#define COL_BLUE_LINK RGB(0,0x20,0xa0)
#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)
#define COL_WINDOW_SHADOW RGB(0x40, 0x40, 0x40)

#ifdef SVN_PRE_RELEASE_VER
#define ABOUT_BG_COLOR          RGB(255,0,0)
#else
#define ABOUT_BG_COLOR          RGB(255,242,0)
#endif

#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")
#define CANVAS_CLASS_NAME       _T("SUMATRA_PDF_CANVAS")
#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")
#define SPLITER_CLASS_NAME      _T("Spliter")
#define FINDSTATUS_CLASS_NAME   _T("FindStatus")
#define PDF_DOC_NAME            _T("Adobe PDF Document")
#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")
#define PREFS_FILE_NAME         _T("sumatrapdfprefs.dat")
#define APP_SUB_DIR             _T("SumatraPDF")
#define APP_NAME_STR            _T("SumatraPDF")

#define DEFAULT_INVERSE_SEARCH_COMMANDLINE _T("winedt.exe \"[Open(|%f|);SelPar(%l,8)]\"")

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_DX 612
#define DEF_PAGE_DY 792

#define SPLITTER_DX  5

#define REPAINT_TIMER_ID    1
#define REPAINT_DELAY_IN_MS 400

#define SMOOTHSCROLL_TIMER_ID       2
#define SMOOTHSCROLL_DELAY_IN_MS    20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

#define FIND_STATUS_WIDTH       200 // Default width for the find status window
#define FIND_STATUS_MARGIN      8

/* A special "pointer" vlaue indicating that we tried to render this bitmap
   but couldn't (e.g. due to lack of memory) */
#define BITMAP_CANNOT_RENDER (RenderedBitmap*)NULL

#define WS_REBAR (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | \
                  RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)

static FileHistoryList *            gFileHistoryRoot = NULL;

static HINSTANCE                    ghinst = NULL;
TCHAR                               gWindowTitle[MAX_LOADSTRING];

static WindowInfo*                  gWindowList;

static HCURSOR                      gCursorArrow;
static HCURSOR                      gCursorHand;
static HCURSOR                      gCursorDrag;
static HCURSOR                      gCursorIBeam;
static HCURSOR                      gCursorScroll;
static HBRUSH                       gBrushBg;
static HBRUSH                       gBrushWhite;
static HBRUSH                       gBrushShadow;
static HBRUSH                       gBrushLinkDebug;

static HBITMAP                      gBitmapCloseToc;

static TCHAR *                      gBenchFileName;
static int                          gBenchPageNum = INVALID_PAGE_NO;

#ifdef DOUBLE_BUFFER
static bool                         gUseDoubleBuffer = true;
#else
static bool                         gUseDoubleBuffer = false;
#endif

#define MAX_PAGE_REQUESTS 8
static PageRenderRequest            gPageRenderRequests[MAX_PAGE_REQUESTS];
static int                          gPageRenderRequestsCount = 0;

static HANDLE                       gPageRenderThreadHandle;
static HANDLE                       gPageRenderSem;
static HANDLE                       gPageRenderClearQueue;
static HANDLE                       gPageRenderQueueCleared;
static PageRenderRequest *          gCurPageRenderReq;

static int                          gReBarDy;
static int                          gReBarDyFrame;
static HWND                         gHwndAbout;

static bool                         gRestrictedUse = false;

#ifdef BUILD_RM_VERSION
static bool                         gDeleteFileOnClose = false; // Delete the file which was passed into the program by command line.
#endif

SerializableGlobalPrefs             gGlobalPrefs = {
    TRUE, // BOOL m_showToolbar
    FALSE, // BOOL m_pdfAssociateDontAskAgain
    TRUE, // BOOL m_pdfAssociateShouldAssociate
#ifndef BUILD_RM_VERSION
    TRUE, // BOOL m_enableAutoUpdate
#else
    FALSE,
#endif
    TRUE, // BOOL m_rememberOpenedFiles
    ABOUT_BG_COLOR, // int  m_bgColor
    FALSE, // BOOL m_escToExit
    NULL, // TCHAR *m_inverseSearchCmdLine
    NULL, // TCHAR *m_versionToSkip
    NULL, // char *m_lastUpdateTime
    DEFAULT_DISPLAY_MODE, // DisplayMode m_defaultDisplayMode
    DEFAULT_ZOOM, // double m_defaultZoom
    WIN_STATE_NORMAL, // int  m_windowState
    DEFAULT_WIN_POS, // int  m_windowPosX
    DEFAULT_WIN_POS, // int  m_windowPosY
    DEFAULT_WIN_POS, // int  m_windowDx
    DEFAULT_WIN_POS, // int  m_windowDy
    1, // int  m_showToc
    0, // int  m_globalPrefsOnly
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
    { 3,   IDT_VIEW_ZOOMOUT,      _TRN("Zoom Out"),       0,             },
    { 4,   IDT_VIEW_ZOOMIN,       _TRN("Zoom In"),        0,             },
    { -1,  IDM_FIND_FIRST,        NULL,                   0,             },
    { 5,   IDM_FIND_PREV,         _TRN("Find Previous"),  0,             },
    { 6,   IDM_FIND_NEXT,         _TRN("Find Next"),      0,             },
    { 7,   IDM_FIND_MATCH,        _TRN("Match case"),     0,             },
};

#define DEFAULT_LANGUAGE "en"

#define TOOLBAR_BUTTONS_COUNT dimof(gToolbarButtons)

static const char *g_currLangName;

static bool ReadRegStr(HKEY keySub, TCHAR *keyName, TCHAR *valName, TCHAR *buffer, DWORD bufLen);
static bool WriteRegStr(HKEY keySub, TCHAR *keyName, TCHAR *valName, TCHAR *value);

static void CreateToolbar(WindowInfo *win, HINSTANCE hInst);
static void CreateTocBox(WindowInfo *win, HINSTANCE hInst);
static void RebuildProgramMenus(void);
static void UpdateToolbarFindText(WindowInfo *win);
static void UpdateToolbarPageText(WindowInfo *win, int pageCount);
static void UpdateToolbarToolText(void);
static void OnMenuFindMatchCase(WindowInfo *win);
static bool LoadPdfIntoWindow(const TCHAR *fileName, WindowInfo *win, 
    const DisplayState *state, bool is_new_window, bool tryrepair, 
    bool showWin, bool placeWindow);
static void WindowInfo_ShowMessage_Asynch(WindowInfo *win, const TCHAR *message, bool resize);

void Find(HWND hwnd, WindowInfo *win, PdfSearchDirection direction = FIND_FORWARD);
static void ClearSearch(WindowInfo *win);
void WindowInfo_EnterFullscreen(WindowInfo *win);
void WindowInfo_ExitFullscreen(WindowInfo *win);
static bool GetAcrobatPath(TCHAR * buffer=NULL, int bufSize=0);

#define SEP_ITEM "-----"

/* according to http://wiki.snap.com/index.php/User_talk:Snap, Serbian (latin) should 
   be sp-rs and Serbian (Cyrillic) should be sr-rs */
#include "LangMenuDef.h"

//#define NEW_LANG_DETECTION

#ifdef NEW_LANG_DETECTION
// based on http://msdn.microsoft.com/en-us/library/dd318693%28VS.85%29.aspx
static struct {
    const char *lang;
    WORD primaryLangId;
    WORD subLangId;
} g_lcidLangMap[] = {
    { "ar", LANG_ARABIC, 0 },
    { "ca", LANG_CATALAN, 0 },
    { "fr", LANG_FRENCH, 0 },
    { "de", LANG_GERMAN, 0 },
    { "hu", LANG_HUNGARIAN, 0 },
    { "pl", LANG_POLISH, 0 },
    { "cy", LANG_WELSH, 0 },
    { "en", LANG_ENGLISH, 0 },
    { NULL, 0, 0 }

/*
    "cn", NULL, // Chinese Simplified
    "tr", "041f", NULL, // Turkish
    "by", "0423", NULL, // Belarusian
    "ja", "0411", NULL, // Japanese
    "fa", "0429", NULL, // Persian
    "dk", "0406", NULL, // Danish
    "it", "0410", NULL, // Italian
    "nl", "0813", "0413", NULL, // Dutch
    "ta", "0449", NULL, // Tamil
    "es", "0c0a", "040a", "500a", "280a", "3c0a", "180a", "080a", "2c0a", NULL, // Spanish
    "hr", "101a", "041a", NULL, // Croatian
    "ru", "0419", NULL, // Russian
    "sv", "081d", "041d", NULL, // Swedish
    "cz", "0405", NULL, // Czech
    "gr", "0408", NULL, // Greek
    "th", "041e", NULL, // Thai
    "pt", "0816", NULL, // Portuguese (Portugal)
    "br", "0416", NULL, // Portuguese (Brazillian)
    "no", "0414", "0814", NULL, // Norwegian
    "sk", "041b", NULL, // Slovak
    "vn", "042a", NULL, // Vietnamese
    "lt", NULL, NULL, // Lithuanian
    "my", NULL, NULL, // Malaysian
    "fi", NULL, NULL, // Finnish
    "si", NULL, NULL, // Slovenian
    "tw", NULL, NULL, // Chinese Traditional
    "ml", NULL, NULL, // Malayalam
    "he", NULL, NULL, // Hebrew
    "sp-rs", NULL, NULL, // Serbian (Latin)
    "id", NULL, NULL, // Indonesian
    "mk", NULL, NULL, // Macedonian
    "ro", NULL, NULL, // Romanian
    "sr-rs", NULL, NULL, // Serbian (Cyrillic)
    "kr", NULL, NULL, // Korean
    "gl", NULL, NULL, // Galician
    "bg", NULL, NULL, // Bulgarian
    "uk", NULL, NULL, // Ukrainian
    NULL
*/
};
#else
// based on http://msdn2.microsoft.com/en-us/library/ms776260.aspx
static const char *g_lcidLangMap[] = {
    "en", "0409", NULL, // English
    "pl", "0415", NULL, // Polish
    "fr", "080c", "0c0c", "040c", "140c", "180c", "100c", NULL, // French
    "de", "0407", "0c07", "1407", "1007", "0807", NULL, // German
    "tr", "041f", NULL, // Turkish
    "by", "0423", NULL, // Belarusian
    "ja", "0411", NULL, // Japanese
    "hu", "040e", NULL, // Hungarian
    "fa", "0429", NULL, // Persian
    "dk", "0406", NULL, // Danish
    "it", "0410", NULL, // Italian
    "nl", "0813", "0413", NULL, // Dutch
    "ta", "0449", NULL, // Tamil
    "es", "0c0a", "040a", "500a", "280a", "3c0a", "180a", "080a", "2c0a", NULL, // Spanish
    "hr", "101a", "041a", NULL, // Croatian
    "ru", "0419", NULL, // Russian
    "ar", "1401", "3c01", "0c01", "0801", "2c01", "3401", "3001", "1001", "1801", "2001", "4001", "0401", "2801", "1c01", "3801", "2401", NULL, // Arabic
    "cn", NULL, // Chinese Simplified
    "sv", "081d", "041d", NULL, // Swedish
    "cz", "0405", NULL, // Czech
    "gr", "0408", NULL, // Greek
    "th", "041e", NULL, // Thai
    "pt", "0816", NULL, // Portuguese (Portugal)
    "br", "0416", NULL, // Portuguese (Brazillian)
    "no", "0414", "0814", NULL, // Norwegian
    "sk", "041b", NULL, // Slovak
    "vn", "042a", NULL, // Vietnamese
    "lt", NULL, NULL, // Lithuanian
    "my", NULL, NULL, // Malaysian
    "fi", NULL, NULL, // Finnish
    "ca", NULL, NULL, // Catalan
    "si", NULL, NULL, // Slovenian
    "tw", NULL, NULL, // Chinese Traditional
    "ml", NULL, NULL, // Malayalam
    "he", NULL, NULL, // Hebrew
    "sp-rs", NULL, NULL, // Serbian (Latin)
    "id", NULL, NULL, // Indonesian
    "mk", NULL, NULL, // Macedonian
    "ro", NULL, NULL, // Romanian
    "sr-rs", NULL, NULL, // Serbian (Cyrillic)
    "kr", NULL, NULL, // Korean
    "gl", NULL, NULL, // Galician
    "bg", NULL, NULL, // Bulgarian
    "uk", NULL, NULL, // Ukrainian
    NULL
};
#endif

const char* CurrLangNameGet() {
    if (!g_currLangName)
        return DEFAULT_LANGUAGE;
    return g_currLangName;
}

bool CurrLangNameSet(const char* langName) {
    bool validLang = false;
    for (int i=0; i < LANGS_COUNT; i++) {
        if (str_eq(langName, g_langs[i]._langName)) {
            validLang = true;
            break;
        }
    }
    if (!validLang) 
        return false;
    free((void*)g_currLangName);
    g_currLangName = str_dup(langName);

    bool ok = Translations_SetCurrentLanguage(langName);
    assert(ok);
    return true;
}

static void CurrLangNameFree() {
    free((void*)g_currLangName);
    g_currLangName = NULL;
}

#ifdef NEW_LANG_DETECTION
static const char *GetLangNameFromLang(WORD primaryLangId, WORD subLangId)
{
    int i = 0;
    while (g_lcidLangMap[i].lang) {
        if (primaryLangId == g_lcidLangMap[i].primaryLangId)
            break;
        ++i;
    }
    if (!g_lcidLangMap[i].lang)
        return NULL;

    // either find the exact primary/sub lang id match, or return the
    // first entry where primary lang matches
    int firstPrimaryLangMatch = i;
    for (;;) {
        if (g_lcidLangMap[i].subLangId == subLangId)
            return g_lcidLangMap[i].lang;
        ++i;
        if (!g_lcidLangMap[i].lang || (g_lcidLangMap[i].primaryLangId != primaryLangId))
            return g_lcidLangMap[firstPrimaryLangMatch].lang;
    }
}

static void GuessLanguage()
{
    LANGID lang = GetUserDefaultUILanguage();
    const char *langName = GetLangNameFromLang(PRIMARYLANGID(lang), SUBLANGID(lang));
    if (NULL != langName)
        CurrLangNameSet(langName);
}
#else
static const char *GetLangFromLcid(const char *lcid)
{
    const char *lang;
    const char *langLcid;
    int i = 0;
    for (;;) {
        lang = g_lcidLangMap[i++];
        if (NULL == lang)
            return NULL;
        for (;;) {
            langLcid = g_lcidLangMap[i++];
            if (NULL == langLcid)
                break;
            if (str_eq(lcid, langLcid))
                return lang;
        }
    }
    assert(0);
    return NULL;
}

static void GuessLanguage()
{
    char langBuf[20];
    int res = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, langBuf, sizeof(langBuf));
    assert(0 != res);
    if (0 == res) return;
    const char *lang = GetLangFromLcid((const char*)&langBuf[0]);
    if (NULL != lang)
        CurrLangNameSet(lang);
}

#endif

/* Convert FILETIME to a string.
   Caller needs to free() the result. */
char *FileTimeToStr(FILETIME* ft)
{
    return mem_to_hexstr((unsigned char*)ft, sizeof(*ft));
}

/* Reverse of FileTimeToStr: convert string <s> to <ft>. */
void StrToFileTime(char *s, FILETIME* ft)
{
    hexstr_to_mem(s, (unsigned char*)ft, sizeof(*ft));
}

/* Get current UTS cystem time as string.
   Caller needs to free() the result. */
char *GetSystemTimeAsStr()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return FileTimeToStr(&ft);
}

static void FileTimeToLargeInteger(FILETIME *ft, LARGE_INTEGER *lt)
{
    lt->LowPart = ft->dwLowDateTime;
    lt->HighPart = ft->dwHighDateTime;
}

/* Return <ft1> - <ft2> in seconds */
DWORD FileTimeDiffInSecs(FILETIME *ft1, FILETIME *ft2)
{
    LARGE_INTEGER t1;
    LARGE_INTEGER t2;
    FileTimeToLargeInteger(ft1, &t1);
    FileTimeToLargeInteger(ft2, &t2);
    // diff is in 100 nanoseconds
    LONGLONG diff = t1.QuadPart - t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (DWORD)diff;
}

class MemSegment {
private:
    class MemSegment *next;

public:
    MemSegment(const void *buf, DWORD size) {
        next = NULL;
        data = NULL;
        add(buf, size);
    };

    MemSegment() {
        next = NULL;
        data = NULL;
    }

    bool add(const void *buf, DWORD size) {
        assert(size > 0);
        if (!data) {
            dataSize = size;
            data = malloc(size);
            if (!data)
                return false;
            memcpy(data, buf, size);
        } else {
            MemSegment *ms = new MemSegment(buf, size);
            if (!ms)
                return false;
            if (!ms->data) {
                delete ms;
                return false;
            }
            ms->next = next;
            next = ms;
        }
        return true;
    }

    void freeAll() {
        free(data);
        data = NULL;
        // clever trick: each segment will delete the next segment
        if (next) {
            delete next;
            next = NULL;
        }
    }
    ~MemSegment() {
        freeAll();
    }
    void *getData(DWORD *sizeOut);
    void *data;
    DWORD dataSize;
};

void *MemSegment::getData(DWORD *sizeOut)
{
    DWORD totalSize = dataSize;
    MemSegment *curr = next;
    while (curr) {
        totalSize += curr->dataSize;
        curr = curr->next;
    }
    if (0 == dataSize)
        return NULL;
    char *buf = (char*)malloc(totalSize + 1); // +1 for 0 termination
    if (!buf)
        return NULL;
    buf[totalSize] = 0;
    // the chunks are linked in reverse order, so we must reassemble them properly
    char *end = buf + totalSize;
    curr = next;
    while (curr) {
        end -= curr->dataSize;
        memcpy(end, curr->data, curr->dataSize);
        curr = curr->next;
    }
    end -= dataSize;
    memcpy(end, data, dataSize);
    assert(end == buf);
    *sizeOut = totalSize;
    return (void*)buf;
}

#ifdef DEBUG
void u_hexstr()
{
    unsigned char buf[6] = {1, 2, 33, 255, 0, 18};
    unsigned char buf2[6] = {0};
    char *s = mem_to_hexstr(buf, sizeof(buf));
    BOOL ok = hexstr_to_mem(s, buf2, sizeof(buf2));
    assert(ok);
    for (int i=0; i<sizeof(buf); i++) {
        assert(buf[i] == buf2[i]);
    }
    free(s);
    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s = FileTimeToStr(&ft1);
    StrToFileTime(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(&ft1, &ft2);
    assert(0 == diff);
    assert(ft1.dwLowDateTime == ft2.dwLowDateTime);
    assert(ft1.dwHighDateTime == ft2.dwHighDateTime);
    free(s);
}

void u_testMemSegment()
{
    MemSegment *ms;
    DWORD size;
    char *data;

    char buf[2] = {'a', '\0'};
    ms = new MemSegment();
    for (int i=0; i<7; i++) {
        ms->add(buf, 1);
        buf[0] = buf[0] + 1;
    }
    data = (char*)ms->getData(&size);
    delete ms;
    assert(str_eq("abcdefg", data));
    assert(7 == size);
    free(data);

    ms = new MemSegment("a", 1);
    data = (char*)ms->getData(&size);
    ms->freeAll();
    delete ms;
    assert(str_eq("a", data));
    assert(1 == size);
    free(data);
}
#endif

// based on information in http://www.codeproject.com/KB/IP/asyncwininet.aspx
class HttpReqCtx {
public:
    // the window to which we'll send notification about completed download
    HWND          hwndToNotify;
    // message to send when download is complete
    UINT          msg;
    // handle for connection during request processing
    HINTERNET     httpFile;

    TCHAR *       url;
    MemSegment    data;
    /* true for automated check, false for check triggered from menu */
    bool          autoCheck;

    HttpReqCtx(TCHAR *_url, HWND _hwnd, UINT _msg) {
        assert(_url);
        hwndToNotify = _hwnd;
        url = tstr_dup(_url);
        msg = _msg;
        autoCheck = false;
        httpFile = 0;
    }
    ~HttpReqCtx() {
        free(url);
        data.freeAll();
    }
};

void __stdcall InternetCallbackProc(HINTERNET hInternet,
                        DWORD_PTR dwContext,
                        DWORD dwInternetStatus,
                        LPVOID statusInfo,
                        DWORD statusLen)
{
    char buf[256];
    INTERNET_ASYNC_RESULT* res;
    HttpReqCtx *ctx = (HttpReqCtx*)dwContext;

    switch (dwInternetStatus)
    {
        case INTERNET_STATUS_HANDLE_CREATED:
            res = (INTERNET_ASYNC_RESULT*)statusInfo;
            ctx->httpFile = (HINTERNET)(res->dwResult);

            _snprintf(buf, 256, "HANDLE_CREATED (%d)", statusLen );
            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
        {
            // Check for errors.
            if (LPINTERNET_ASYNC_RESULT(statusInfo)->dwError != 0)
            {
                _snprintf(buf, 256, "REQUEST_COMPLETE (%d) Error (%d) encountered", statusLen, GetLastError());
                break;
            }

            // Set the resource handle to the HINTERNET handle returned in the callback.
            HINTERNET hInt = HINTERNET(LPINTERNET_ASYNC_RESULT(statusInfo)->dwResult);
            assert(hInt == ctx->httpFile);

            _snprintf(buf, 256, "REQUEST_COMPLETE (%d)", statusLen);

            INTERNET_BUFFERS ib = {0};
            ib.dwStructSize = sizeof(ib);
            ib.lpvBuffer = malloc(1024);

            // This is not exactly async, but we're assuming it'll complete quickly
            // because the update file is small and we now that connection is working
            // since we already got headers back
            BOOL ok;
            while (TRUE) {
                ib.dwBufferLength = 1024;
                ok = InternetReadFileEx(ctx->httpFile, &ib, IRF_ASYNC, (LPARAM)ctx);
                if (ok || (!ok && GetLastError()==ERROR_IO_PENDING)) {
                    DWORD readSize = ib.dwBufferLength;
                    if (readSize > 0) {
                        ctx->data.add(ib.lpvBuffer, readSize);
                    }
                }
                if (ok || GetLastError()!=ERROR_IO_PENDING)
                    break; // read the whole file or error
            }
            free(ib.lpvBuffer);
            InternetCloseHandle(ctx->httpFile);
            ctx->httpFile = 0;
            if (ok) {
                // read the whole file
                PostMessage(ctx->hwndToNotify, ctx->msg, (WPARAM) ctx, 0);
            } else {
                delete ctx;
            }
        }
        break;

#if 0
        case INTERNET_STATUS_CLOSING_CONNECTION:
            _snprintf(buf, 256, "CLOSING_CONNECTION (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTED_TO_SERVER:
            _snprintf(buf, 256, "CONNECTED_TO_SERVER (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTING_TO_SERVER:
            _snprintf(buf, 256, "CONNECTING_TO_SERVER (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTION_CLOSED:
            _snprintf(buf, 256, "CONNECTION_CLOSED (%d)", statusLen);
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            _snprintf(buf, 256, "HANDLE_CLOSING (%d)", statusLen);
            break;

        case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
            _snprintf(buf, 256, "INTERMEDIATE_RESPONSE (%d)", statusLen );
            break;

        case INTERNET_STATUS_NAME_RESOLVED:
            _snprintf(buf, 256, "NAME_RESOLVED (%d)", statusLen);
            break;

        case INTERNET_STATUS_RECEIVING_RESPONSE:
            _snprintf(buf, 256, "RECEIVING_RESPONSE (%d)",statusLen);
            break;

        case INTERNET_STATUS_RESPONSE_RECEIVED:
            _snprintf(buf, 256, "RESPONSE_RECEIVED (%d)", statusLen);
            break;

        case INTERNET_STATUS_REDIRECT:
            _snprintf(buf, 256, "REDIRECT (%d)", statusLen);
            break;

        case INTERNET_STATUS_REQUEST_SENT:
            _snprintf(buf, 256, "REQUEST_SENT (%d)", statusLen);
            break;

        case INTERNET_STATUS_RESOLVING_NAME:
            _snprintf(buf, 256, "RESOLVING_NAME (%d)", statusLen);
            break;

        case INTERNET_STATUS_SENDING_REQUEST:
            _snprintf(buf, 256, "SENDING_REQUEST (%d)", statusLen);
            break;

        case INTERNET_STATUS_STATE_CHANGE:
            _snprintf(buf, 256, "STATE_CHANGE (%d)", statusLen);
            break;

        default:
            _snprintf(buf, 256, "Unknown: Status %d Given", dwInternetStatus);
            break;
#endif
    }

    DBG_OUT(buf);
    DBG_OUT("\n");
}

static HINTERNET g_hOpen = NULL;

#ifndef SUMATRA_UPDATE_INFO_URL
#ifdef SVN_PRE_RELEASE_VER
#define SUMATRA_UPDATE_INFO_URL _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-prerelease-latest.txt")
#else
#define SUMATRA_UPDATE_INFO_URL _T("http://blog.kowalczyk.info/software/sumatrapdf/sumpdf-latest.txt")
#endif
#endif

#ifndef SVN_UPDATE_LINK
#ifdef SVN_PRE_RELEASE_VER
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf/prerelase.html")
#else
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf")
#endif
#endif


bool WininetInit()
{
    if (!g_hOpen)
        g_hOpen = InternetOpen(_T("SumatraPDF"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
    if (NULL == g_hOpen) {
        DBG_OUT("InternetOpen() failed\n");
        return false;
    }
    return true;
}

void WininetDeinit()
{
    if (g_hOpen)
        InternetCloseHandle(g_hOpen);
}

#define SECS_IN_DAY 60*60*24

void DownloadSumatraUpdateInfo(WindowInfo *win, bool autoCheck)
{
    if (!WininetInit())
        return;
    assert(win);
    HWND hwndToNotify = win->hwndFrame;

    /* For auto-check, only check if at least a day passed since last check */
    if (autoCheck && gGlobalPrefs.m_lastUpdateTime) {
        FILETIME lastUpdateTimeFt;
        StrToFileTime(gGlobalPrefs.m_lastUpdateTime, &lastUpdateTimeFt);
        FILETIME currentTimeFt;
        GetSystemTimeAsFileTime(&currentTimeFt);
        int secs = FileTimeDiffInSecs(&currentTimeFt, &lastUpdateTimeFt);
        assert(secs >= 0);
        // if secs < 0 => somethings wrong, so ignore that case
        if ((secs > 0) && (secs < SECS_IN_DAY))
            return;
    }

    TCHAR *url = SUMATRA_UPDATE_INFO_URL _T("?v=") UPDATE_CHECK_VER;
    HttpReqCtx *ctx = new HttpReqCtx(url, hwndToNotify, WM_APP_URL_DOWNLOADED);
    ctx->autoCheck = autoCheck;

    InternetSetStatusCallback(g_hOpen, (INTERNET_STATUS_CALLBACK)InternetCallbackProc);
    HINTERNET urlHandle;
    urlHandle = InternetOpenUrl(g_hOpen, url, NULL, 0, 
      INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | 
      INTERNET_FLAG_NO_CACHE_WRITE, (LPARAM)ctx);
    /* MSDN says NULL result from InternetOpenUrl() means an error, but in my testing
       in async mode InternetOpenUrl() returns NULL and error is ERROR_IO_PENDING */
    if (!urlHandle && (GetLastError() != ERROR_IO_PENDING)) {
        DBG_OUT("InternetOpenUrl() failed\n");
        delete ctx;
    }
    free(gGlobalPrefs.m_lastUpdateTime);
    gGlobalPrefs.m_lastUpdateTime = GetSystemTimeAsStr();
}

static void SerializableGlobalPrefs_Init() {
    gGlobalPrefs.m_inverseSearchCmdLine = tstr_dup(DEFAULT_INVERSE_SEARCH_COMMANDLINE);
}

static void SerializableGlobalPrefs_Deinit()
{
    free(gGlobalPrefs.m_versionToSkip);
    free(gGlobalPrefs.m_inverseSearchCmdLine);
    free(gGlobalPrefs.m_lastUpdateTime);
}

void LaunchBrowser(const TCHAR *url)
{
    if (gRestrictedUse)
        return;
    launch_url(url);
}

static BOOL pageRenderAbortCb(void *data)
{
    PageRenderRequest *req = (PageRenderRequest*)data;
    if (!req->abort)
        return FALSE;

    DBG_OUT("Rendering of page %d aborted\n", req->pageNo);
    return TRUE;
}

void RenderQueue_RemoveForDisplayModel(DisplayModel *dm) {
    LockCache();
    int reqCount = gPageRenderRequestsCount;
    int curPos = 0;
    for (int i = 0; i < reqCount; i++) {
        PageRenderRequest *req = &(gPageRenderRequests[i]);
        bool shouldRemove = (req->dm == dm);
        if (i != curPos)
            gPageRenderRequests[curPos] = gPageRenderRequests[i];
        if (shouldRemove)
            --gPageRenderRequestsCount;
        else
            ++curPos;
    }
    UnlockCache();
}

/* Wait until rendering of a page beloging to <dm> has finished. */
/* TODO: this might take some time, would be good to show a dialog to let the
   user know he has to wait until we finish */
void cancelRenderingForDisplayModel(DisplayModel *dm) {

    DBG_OUT("cancelRenderingForDisplayModel()\n");
    bool renderingFinished = false;;
    for (;;) {
        LockCache();
        if (!gCurPageRenderReq || (gCurPageRenderReq->dm != dm))
            renderingFinished = true;
        else
            gCurPageRenderReq->abort = TRUE;
        UnlockCache();
        if (renderingFinished)
            break;
        /* TODO: busy loop is not good, but I don't have a better idea */
        sleep_milliseconds(500);
    }
}

/* Render a bitmap for page <pageNo> in <dm>. */
void RenderQueue_Add(DisplayModel *dm, int pageNo) {
    DBG_OUT("RenderQueue_Add(pageNo=%d)\n", pageNo);
    assert(dm);
    if (!dm) goto Exit;

    LockCache();
    int rotation = dm->rotation();
    normalizeRotation(&rotation);
    double zoomLevel = dm->zoomReal();

    if (BitmapCache_Exists(dm, pageNo, zoomLevel, rotation)) {
        goto LeaveCsAndExit;
    }

    if (gCurPageRenderReq && 
        (gCurPageRenderReq->pageNo == pageNo) && (gCurPageRenderReq->dm == dm)) {
        if ((gCurPageRenderReq->zoomLevel != zoomLevel) || (gCurPageRenderReq->rotation != rotation)) {
            /* Currently rendered page is for the same page but with different zoom
            or rotation, so abort it */
            DBG_OUT("  aborting rendering\n");
            gCurPageRenderReq->abort = TRUE;
        } else {
            /* we're already rendering exactly the same page */
            DBG_OUT("  already rendering this page\n");
            goto LeaveCsAndExit;
        }
    }

    for (int i=0; i < gPageRenderRequestsCount; i++) {
        PageRenderRequest* req = &(gPageRenderRequests[i]);
        if ((req->pageNo == pageNo) && (req->dm == dm)) {
            if ((req->zoomLevel == zoomLevel) && (req->rotation == rotation)) {
                /* Request with exactly the same parameters already queued for
                   rendering. Move it to the top of the queue so that it'll
                   be rendered faster. */
                PageRenderRequest tmp;
                tmp = gPageRenderRequests[gPageRenderRequestsCount-1];
                gPageRenderRequests[gPageRenderRequestsCount-1] = *req;
                *req = tmp;
                DBG_OUT("  already queued\n");
                goto LeaveCsAndExit;
            } else {
                /* There was a request queued for the same page but with different
                   zoom or rotation, so only replace this request */
                DBG_OUT("Replacing request for page %d with new request\n", req->pageNo);
                req->zoomLevel = zoomLevel;
                req->rotation = rotation;
                goto LeaveCsAndExit;
            
            }
        }
    }

    PageRenderRequest* newRequest;
    /* add request to the queue */
    if (gPageRenderRequestsCount == MAX_PAGE_REQUESTS) {
        /* queue is full -> remove the oldest items on the queue */
        memmove(&(gPageRenderRequests[0]), &(gPageRenderRequests[1]), sizeof(PageRenderRequest)*(MAX_PAGE_REQUESTS-1));
        newRequest = &(gPageRenderRequests[MAX_PAGE_REQUESTS-1]);
    } else {
        newRequest = &(gPageRenderRequests[gPageRenderRequestsCount]);
        gPageRenderRequestsCount++;
    }
    assert(gPageRenderRequestsCount <= MAX_PAGE_REQUESTS);
    newRequest->dm = dm;
    newRequest->pageNo = pageNo;
    newRequest->zoomLevel = zoomLevel;
    newRequest->rotation = rotation;
    newRequest->abort = FALSE;

    UnlockCache();
    /* tell rendering thread there's a new request to render */
    LONG  prevCount;
    ReleaseSemaphore(gPageRenderSem, 1, &prevCount);
Exit:
    return;
LeaveCsAndExit:
    UnlockCache();
    return;
}

void RenderQueue_Pop(PageRenderRequest *req)
{
    LockCache();
    assert(gPageRenderRequestsCount > 0);
    assert(gPageRenderRequestsCount <= MAX_PAGE_REQUESTS);
    --gPageRenderRequestsCount;
    *req = gPageRenderRequests[gPageRenderRequestsCount];
    assert(gPageRenderRequestsCount >= 0);
    UnlockCache();
}

void RenderQueue_Clear()
{
    LockCache();
    gPageRenderRequestsCount = 0;
    UnlockCache();
}

static void MenuUpdateDisplayMode(WindowInfo *win)
{
    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    if (win->dm)
        displayMode = win->dm->displayMode();

    HMENU menuMain = win->hMenu;
    UINT enableState = win->dm ? MF_ENABLED : MF_GRAYED;
    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++) {
        CheckMenuItem(menuMain, id, MF_BYCOMMAND | MF_UNCHECKED);
        EnableMenuItem(menuMain, id, MF_BYCOMMAND | enableState);
    }

    UINT    id;
    if (DM_SINGLE_PAGE == displayMode) {
        id = IDM_VIEW_SINGLE_PAGE;
    } else if (DM_FACING == displayMode) {
        id =  IDM_VIEW_FACING;
    } else if (DM_CONTINUOUS == displayMode) {
        id =  IDM_VIEW_CONTINUOUS;
    } else if (DM_CONTINUOUS_FACING == displayMode) {
        id =  IDM_VIEW_CONTINUOUS_FACING;
    } else
        assert(0);

    CheckMenuItem(menuMain, id, MF_BYCOMMAND | MF_CHECKED);
}

static void SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode)
{
    if (!win->dm)
        return;

    win->dm->changeDisplayMode(displayMode);
    MenuUpdateDisplayMode(win);
}

static UINT AllocNewMenuId(void)
{
    static UINT firstId = 1000;
    ++firstId;
    return firstId;
}

enum menuFlags {
    MF_NOT_IN_RESTRICTED = 0x1 
};

MenuDef menuDefFile[] = {
    { _TRN("&Open\tCtrl-O"),                        IDM_OPEN ,                  MF_NOT_IN_RESTRICTED },
    { _TRN("&Close\tCtrl-W"),                       IDM_CLOSE,                  MF_NOT_IN_RESTRICTED },
    { _TRN("&Save as\tCtrl-S"),                     IDM_SAVEAS,                 MF_NOT_IN_RESTRICTED },
    { _TRN("&Print\tCtrl-P"),                       IDM_PRINT,                  MF_NOT_IN_RESTRICTED },
    { SEP_ITEM,                                     0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("Open in &Adobe Reader"),                IDM_VIEW_WITH_ACROBAT,      MF_NOT_IN_RESTRICTED },
    { SEP_ITEM ,                                    0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("E&xit\tCtrl-Q"),                        IDM_EXIT,                   0 }
};

MenuDef menuDefView[] = {
    { _TRN("Single page"),                 IDM_VIEW_SINGLE_PAGE,        0  },
    { _TRN("Facing"),                      IDM_VIEW_FACING,             0  },
    { _TRN("Continuous"),                  IDM_VIEW_CONTINUOUS,         0  },
    { _TRN("Continuous facing"),           IDM_VIEW_CONTINUOUS_FACING,  0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Rotate left\tCtrl-Shift--"),   IDM_VIEW_ROTATE_LEFT,        0  },
    { _TRN("Rotate right\tCtrl-Shift-+"),  IDM_VIEW_ROTATE_RIGHT,       0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Bookmarks\tF12"),              IDM_VIEW_BOOKMARKS,          0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Fullscreen\tCtrl-L"),          IDM_VIEW_FULLSCREEN,         0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Show toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR,  0  },
};

MenuDef menuDefGoTo[] = {
    { _TRN("Next Page"),                   IDM_GOTO_NEXT_PAGE,          0  },
    { _TRN("Previous Page"),               IDM_GOTO_PREV_PAGE,          0  },
    { _TRN("First Page\tHome"),            IDM_GOTO_FIRST_PAGE,         0  },
    { _TRN("Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,          0  },
    { _TRN("Page...\tCtrl-G"),             IDM_GOTO_PAGE,               0  },
    { SEP_ITEM ,                           0,                           0  },
    { _TRN("Find...\tCtrl-F"),             IDM_FIND_FIRST,              0  },
};

MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl-0"),           IDM_ZOOM_FIT_PAGE,           0  },
    { _TRN("Act&ual Size\tCtrl-1"),        IDM_ZOOM_ACTUAL_SIZE,        0  },
    { _TRN("Fit Widt&h\tCtrl-2"),          IDM_ZOOM_FIT_WIDTH,          0  },
    { SEP_ITEM },
#ifndef BUILD_RM_VERSION
    { _TRN("6400%"),                       IDM_ZOOM_6400,               0  },
    { _TRN("3200%"),                       IDM_ZOOM_3200,               0  },
    { _TRN("1600%"),                       IDM_ZOOM_1600,               0  },
    { _TRN("800%"),                        IDM_ZOOM_800,                0  },
#endif
    { _TRN("400%"),                        IDM_ZOOM_400,                0  },
    { _TRN("200%"),                        IDM_ZOOM_200,                0  },
    { _TRN("150%"),                        IDM_ZOOM_150,                0  },
    { _TRN("125%"),                        IDM_ZOOM_125,                0  },
    { _TRN("100%"),                        IDM_ZOOM_100,                0  },
    { _TRN("50%"),                         IDM_ZOOM_50,                 0  },
    { _TRN("25%"),                         IDM_ZOOM_25,                 0  },
    { _TRN("12.5%"),                       IDM_ZOOM_12_5,               0  },
    { _TRN("8.33%"),                       IDM_ZOOM_8_33,               0  },
};

MenuDef menuDefLang[] = {
    { _TRN("Change language"),             IDM_CHANGE_LANGUAGE,         0  },
#if 0
    { _TRN("Contribute translation"),      IDM_CONTRIBUTE_TRANSLATION,  MF_NOT_IN_RESTRICTED },
    { SEP_ITEM ,                           0,                           MF_NOT_IN_RESTRICTED },
#endif
    { _TRN("&Options..."),                 IDM_SETTINGS,                MF_NOT_IN_RESTRICTED }
};

MenuDef menuDefHelp[] = {
    { _TRN("&Visit website"),              IDM_VISIT_WEBSITE,       MF_NOT_IN_RESTRICTED },
    { _TRN("&Manual"),                     IDM_MANUAL,              MF_NOT_IN_RESTRICTED },
    { _TRN("&Check for new version"),      IDM_CHECK_UPDATE,        MF_NOT_IN_RESTRICTED },
    { SEP_ITEM ,                           0,                       MF_NOT_IN_RESTRICTED },
    { _TRN("&About"),                      IDM_ABOUT,               0  }
};

static void AddFileMenuItem(HMENU menuFile, FileHistoryList *node)
{
    assert(node);
    if (!node) return;
    assert(menuFile);
    if (!menuFile) return;

    UINT newId = node->menuId;
    if (INVALID_MENU_ID == node->menuId)
        newId = AllocNewMenuId();
    AppendMenu(menuFile, MF_ENABLED | MF_STRING, newId, FilePathW_GetBaseName(node->state.filePath));
    node->menuId = newId;
}

static HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuItems)
{
    HMENU m = CreateMenu();
    if (NULL == m) 
        return NULL;

    for (int i=0; i < menuItems; i++) {
        MenuDef md = menuDefs[i];
        if (!gRestrictedUse || ~md.m_flags & MF_NOT_IN_RESTRICTED) {
            const char *title = md.m_title;
            if (!title)
                continue; // the menu item was dynamically removed
            if (str_eq(title, SEP_ITEM)) {
                AppendMenu(m, MF_SEPARATOR, 0, NULL);
                continue;
            }
            const TCHAR *ttitle =  Translations_GetTranslation(title);
            AppendMenu(m, MF_STRING, (UINT_PTR)md.m_id, ttitle);
        }
    }
    return m;
}

static void AppendRecentFilesToMenu(HMENU m)
{
    if (!gFileHistoryRoot) return;

    AppendMenu(m, MF_SEPARATOR, 0, NULL);

    int  itemsAdded = 0;
    FileHistoryList *curr = gFileHistoryRoot;
    while (curr) {
        assert(curr->state.filePath);
        if (curr->state.filePath) {
            AddFileMenuItem(m, curr);
            assert(curr->menuId != INVALID_MENU_ID);
            ++itemsAdded;
            if (itemsAdded >= MAX_RECENT_FILES_IN_MENU) {
                DBG_OUT("  not adding, reached max %d items\n", MAX_RECENT_FILES_IN_MENU);
                return;
            }
        }
        curr = curr->next;
    }
}

static void WindowInfo_RebuildMenu(WindowInfo *win)
{
    if (win->hMenu) {
        DestroyMenu(win->hMenu);
        win->hMenu = NULL;
    }
    
    HMENU mainMenu = CreateMenu();
    // Don't display the Acrobat option, if the program couldn't be found
    if (!GetAcrobatPath())
        for (int i = 0; i < dimof(menuDefFile); i++)
            if (IDM_VIEW_WITH_ACROBAT == menuDefFile[i].m_id)
                menuDefFile[i].m_title = menuDefFile[i - 1].m_title = NULL;

    HMENU tmp = BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile));
    if (!gRestrictedUse)
        AppendRecentFilesToMenu(tmp);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&File"));
    tmp = BuildMenuFromMenuDef(menuDefView, dimof(menuDefView));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&View"));
    tmp = BuildMenuFromMenuDef(menuDefGoTo, dimof(menuDefGoTo));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&Go To"));
    tmp = BuildMenuFromMenuDef(menuDefZoom, dimof(menuDefZoom));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&Zoom"));
    tmp = BuildMenuFromMenuDef(menuDefLang, dimof(menuDefLang));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&Settings"));
    tmp = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp));
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)tmp, _TR("&Help"));
    win->hMenu = mainMenu;
}

/* Return the full exe path of my own executable.
   Caller needs to free() the result. */
static TCHAR *ExePathGet(void)
{
    TCHAR buf[MAX_PATH];
    buf[0] = 0;
    GetModuleFileName(NULL, buf, dimof(buf));
    return tstr_dup(buf);
}

static void AddFileToHistory(const TCHAR *filePath)
{
    FileHistoryList *   node;
    uint32_t            oldMenuId = INVALID_MENU_ID;

    assert(filePath);
    if (!filePath) return;

    /* if a history entry with the same name already exists, then delete it.
       That way we don't have duplicates and the file moves to the front of the list */
    node = FileHistoryList_Node_FindByFilePath(&gFileHistoryRoot, filePath);
    if (node) {
        oldMenuId = node->menuId;
        FileHistoryList_Node_RemoveAndFree(&gFileHistoryRoot, node);
    }
    node = FileHistoryList_Node_CreateFromFilePath(filePath);
    if (!node)
        return;
    node->menuId = oldMenuId;
    FileHistoryList_Node_InsertHead(&gFileHistoryRoot, node);
}

extern "C" char *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName);

/* Get password for a given 'fileName', can be NULL if user cancelled the
   dialog box.
   Caller needs to free() the result. */
char *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName)
{
    fileName = FilePathW_GetBaseName(fileName);
    TCHAR *pass = Dialog_GetPassword(win, fileName);
    // TODO: Can we make GetPasswordForFile return a (TCHAR *)?
    char *passA = wstr_to_multibyte(pass, CP_ACP);
    free(pass);
    return passA;
}

/* Return true if this program has been started from "Program Files" directory
   (which is an indicator that it has been installed */
static bool runningFromProgramFiles(void)
{
    TCHAR programFilesDir[MAX_PATH];
    BOOL fOk = SHGetSpecialFolderPath(NULL, programFilesDir, CSIDL_PROGRAM_FILES, FALSE);
    TCHAR *exePath = ExePathGet();
    if (!exePath) return true; // again, assume it is
    bool fromProgramFiles = false;
    if (fOk)
        if (tstr_startswithi(exePath, programFilesDir))
            fromProgramFiles = true;
    free(exePath);
    return fromProgramFiles;
}

bool IsRunningInPortableMode(void)
{
    return !runningFromProgramFiles();
}

/* Caller needs to free() the result. */
static TCHAR * AppGetAppDir(void)
{
    TCHAR dir[MAX_PATH];
    TCHAR * appDir;

    SHGetSpecialFolderPath(NULL, dir, CSIDL_APPDATA, TRUE);
    appDir = tstr_printf(_T("%s/%s"), dir, APP_SUB_DIR);
    if (appDir)
        _tmkdir(appDir);

    return appDir;
}

/* Generate the full path for a filename used by the app in the userdata path. */
/* Caller needs to free() the result. */
static TCHAR * AppGenDataFilename(TCHAR *pFilename)
{
    assert(pFilename);
    if (!pFilename) return NULL;

    TCHAR * path = NULL;
    bool portable = IsRunningInPortableMode();
    if (portable) {
        /* Use the same path as the binary */
        TCHAR *exePath = ExePathGet();
        if (exePath) {
            assert(exePath[0]);
            path = FilePathW_GetDir(exePath);
            free(exePath);
        }
    } else {
        path = AppGetAppDir();
    }
    if (!path)
        return NULL;

    bool needsSep = !char_is_dir_sep(path[lstrlen(path) - 1]) && !char_is_dir_sep(pFilename[0]);
    TCHAR * filename = tstr_printf(_T("%s%s%s"), path, needsSep ? _T(DIR_SEP_STR) : _T(""), pFilename);
    free(path);

    return filename;
}

/* Caller needs to free() the result. */
static TCHAR * Prefs_GetFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

/* Load preferences from the preferences file.
   Returns true if preferences file was loaded, false if there was an error.
*/
static bool Prefs_Load(void)
{
    char *          prefsTxt;
    bool            ok = false;

#ifdef DEBUG
    static bool     loaded = false;
    assert(!loaded);
    loaded = true;
#endif

    TCHAR * prefsFilename = Prefs_GetFileName();
    assert(prefsFilename);
    const char * prefsFilenameA = wstr_to_multibyte(prefsFilename, CP_ACP);
    assert(prefsFilenameA);
    uint64_t prefsFileLen;
    prefsTxt = file_read_all(prefsFilenameA, &prefsFileLen);
    if (!str_empty(prefsTxt)) {
        ok = Prefs_Deserialize(prefsTxt, prefsFileLen, &gFileHistoryRoot);
        assert(ok);
    }

    free((void *)prefsFilenameA);
    free(prefsFilename);
    free(prefsTxt);
    return ok;
}

unsigned short gItemId[] = {
    IDM_ZOOM_6400, IDM_ZOOM_3200, IDM_ZOOM_1600, IDM_ZOOM_800, IDM_ZOOM_400,
    IDM_ZOOM_200, IDM_ZOOM_150, IDM_ZOOM_125, IDM_ZOOM_100, IDM_ZOOM_50,
    IDM_ZOOM_25, IDM_ZOOM_12_5, IDM_ZOOM_8_33, IDM_ZOOM_FIT_PAGE, 
    IDM_ZOOM_FIT_WIDTH, IDM_ZOOM_ACTUAL_SIZE };

double gItemZoom[] = { 6400.0, 3200.0, 1600.0, 800.0, 400.0, 200.0, 150.0, 
    125.0, 100.0, 50.0, 25.0, 12.5, 8.33, ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH, 100.0 };

static UINT MenuIdFromVirtualZoom(double virtualZoom)
{
    for (size_t i=0; i < dimof(gItemZoom); i++) {
        if (virtualZoom == gItemZoom[i])
            return gItemId[i];
    }
    return IDM_ZOOM_ACTUAL_SIZE;
}

static double ZoomMenuItemToZoom(UINT menuItemId)
{
    for (size_t i=0; i<dimof(gItemId); i++) {
        if (menuItemId == gItemId[i]) {
            return gItemZoom[i];
        }
    }
    assert(0);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU hmenu, UINT menuItemId, BOOL canZoom)
{
    BOOL found = FALSE;
    if (IDM_ZOOM_100 == menuItemId)
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;

    for (size_t i=0; i<dimof(gItemId); i++) {
        UINT checkState = MF_BYCOMMAND | MF_UNCHECKED;
        if (menuItemId == gItemId[i]) {
            assert(!found);
            found = TRUE;
            checkState = MF_BYCOMMAND | MF_CHECKED;
        }
        else if (IDM_ZOOM_ACTUAL_SIZE == menuItemId && IDM_ZOOM_100 == gItemId[i]) {
            checkState = MF_BYCOMMAND | MF_CHECKED;
        }
        CheckMenuItem(hmenu, gItemId[i], checkState);
        UINT enableState = canZoom ? MF_ENABLED : MF_GRAYED;
        EnableMenuItem(hmenu, gItemId[i], MF_BYCOMMAND | enableState);
    }
    assert(found);
}

static void MenuUpdateZoom(WindowInfo* win)
{
    double zoomVirtual = gGlobalPrefs.m_defaultZoom;
    if (win->dm)
        zoomVirtual = win->dm->zoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->hMenu, menuId, NULL != win->dm);
}

static void SeeLastError(void) {
    TCHAR *msgBuf = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msgBuf, 0, NULL);
    if (!msgBuf) return;
    _tprintf(_T("SeeLastError(): %s\n"), msgBuf);
    OutputDebugString(msgBuf);
    LocalFree(msgBuf);
}

static void UpdateDisplayStateWindowRect(WindowInfo *win, DisplayState *ds)
{
    // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
    //       the non-maximized dimensions for proper restoration)
    if (IsZoomed(win->hwndFrame) || IsIconic(win->hwndFrame) || win->fullScreen)
        return;

    RECT r;
    if (!GetWindowRect(win->hwndFrame, &r))
        return;

    ds->windowX = r.left;
    ds->windowY = r.top;
    ds->windowDx = rect_dx(&r);
    ds->windowDy = rect_dy(&r);
}

static void UpdateCurrentFileDisplayStateForWin(WindowInfo *win)
{
    DisplayState     ds;
    const TCHAR *    fileName = NULL;
    FileHistoryList* node = NULL;

    if (!win)
        return;

    if (WS_ABOUT == win->state || gGlobalPrefs.m_globalPrefsOnly)
    {
        // update global windowState for next default launch when no pdf opened
        if (win->fullScreen)
            gGlobalPrefs.m_windowState = WIN_STATE_FULLSCREEN;
        else if (IsZoomed(win->hwndFrame))
            gGlobalPrefs.m_windowState = WIN_STATE_MAXIMIZED;
        else
            gGlobalPrefs.m_windowState = WIN_STATE_NORMAL;
    }

    if (WS_SHOWING_PDF != win->state)
        return;
    if (!win->dm)
        return;

    fileName = win->dm->fileName();
    assert(fileName);
    if (!fileName)
        return;

    node = FileHistoryList_Node_FindByFilePath(&gFileHistoryRoot, fileName);
    assert(node || !gGlobalPrefs.m_rememberOpenedFiles);
    if (!node)
        return;

    DisplayState_Init(&ds);
    ds.useGlobalValues = gGlobalPrefs.m_globalPrefsOnly;

    // Update pdf-specific windowState
    if (win->fullScreen)
        ds.windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win->hwndFrame))
        ds.windowState = WIN_STATE_MAXIMIZED;
    else
        ds.windowState = WIN_STATE_NORMAL;

    if (!displayStateFromDisplayModel(&ds, win->dm))
        return;

    UpdateDisplayStateWindowRect(win, &ds);
    DisplayState_Free(&(node->state));
    node->state = ds;
    node->state.visible = TRUE;
}

static void UpdateCurrentFileDisplayState(void)
{
    WindowInfo *        currWin;
    FileHistoryList *   currFile;

    currFile = gFileHistoryRoot;
    while (currFile) {
        currFile->state.visible = FALSE;
        currFile = currFile->next;
    }

    currWin = gWindowList;
    while (currWin) {
        UpdateCurrentFileDisplayStateForWin(currWin);
        currWin = currWin->next;
    }
}

static bool Prefs_Save(void)
{
    TCHAR *     path;
    size_t      dataLen;
    bool        ok = false;

    /* mark currently shown files as visible */
    UpdateCurrentFileDisplayState();

    const char *data = Prefs_Serialize(&gFileHistoryRoot, &dataLen);
    if (!data)
        goto Exit;

    assert(dataLen > 0);
    path = Prefs_GetFileName();
    assert(path);
    /* TODO: consider 2-step process:
        * write to a temp file
        * rename temp file to final file */
    if (write_to_file(path, (void*)data, dataLen))
        ok = true;

Exit:
    free((void*)data);
    free(path);
    return ok;
}

static void WindowInfo_Refresh(WindowInfo* win, bool autorefresh) {
    if (win->pdfsync)
        win->pdfsync->discard_index();
    DisplayState ds;
    DisplayState_Init(&ds);
    ds.useGlobalValues = gGlobalPrefs.m_globalPrefsOnly;
    if (!win->dm || !displayStateFromDisplayModel(&ds, win->dm))
        return;
    UpdateDisplayStateWindowRect(win, &ds);
    // Set the windows state based on the actual window's placement
    ds.windowState =  win->fullScreen ? WIN_STATE_FULLSCREEN
                    : IsZoomed(win->hwndFrame) ? WIN_STATE_MAXIMIZED 
                    : IsIconic(win->hwndFrame) ? WIN_STATE_MINIMIZED
                    : WIN_STATE_NORMAL ;
    LoadPdfIntoWindow(win->watcher.filepath(), win, &ds, false,
                        !autorefresh, // We don't allow PDF-repair if it is an autorefresh because
                                      // a refresh event can occur before the file is finished being written,
                                      // in which case the repair could fail. Instead, if the file is broken, 
                                      // we postpone the reload until the next autorefresh event
                        true,
                        false);
}

#ifndef THREAD_BASED_FILEWATCH
static void WindowInfo_RefreshUpdatedFiles(bool autorefresh) {
    WindowInfo* curr = gWindowList;
    while (curr) {
        if (curr->watcher.HasChanged())
            WindowInfo_Refresh(curr, autorefresh);
        curr = curr->next;
    }
}
#endif

static bool WindowInfo_Dib_Init(WindowInfo *win) {
    assert(NULL == win->dibInfo);
    win->dibInfo = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 12);
    if (!win->dibInfo)
        return false;
    win->dibInfo->bmiHeader.biSize = sizeof(win->dibInfo->bmiHeader);
    win->dibInfo->bmiHeader.biPlanes = 1;
    win->dibInfo->bmiHeader.biBitCount = 24;
    win->dibInfo->bmiHeader.biCompression = BI_RGB;
    win->dibInfo->bmiHeader.biXPelsPerMeter = 2834;
    win->dibInfo->bmiHeader.biYPelsPerMeter = 2834;
    win->dibInfo->bmiHeader.biClrUsed = 0;
    win->dibInfo->bmiHeader.biClrImportant = 0;
    return true;
}

static void WindowInfo_Dib_Deinit(WindowInfo *win) {
    free((void*)win->dibInfo);
    win->dibInfo = NULL;
}

static void WindowInfo_DoubleBuffer_Delete(WindowInfo *win) {
    if (win->bmpDoubleBuffer) {
        DeleteObject(win->bmpDoubleBuffer);
        win->bmpDoubleBuffer = NULL;
    }

    if (win->hdcDoubleBuffer) {
        DeleteDC(win->hdcDoubleBuffer);
        win->hdcDoubleBuffer = NULL;
    }
    win->hdcToDraw = NULL;
}

static bool WindowInfo_DoubleBuffer_New(WindowInfo *win)
{
    WindowInfo_DoubleBuffer_Delete(win);

    win->hdc = GetDC(win->hwndCanvas);
    win->hdcToDraw = win->hdc;
    win->GetCanvasSize();
    if (!gUseDoubleBuffer || (0 == win->winDx()) || (0 == win->winDy()))
        return true;

    win->hdcDoubleBuffer = CreateCompatibleDC(win->hdc);
    if (!win->hdcDoubleBuffer)
        return false;

    win->bmpDoubleBuffer = CreateCompatibleBitmap(win->hdc, win->winDx(), win->winDy());
    if (!win->bmpDoubleBuffer) {
        WindowInfo_DoubleBuffer_Delete(win);
        return false;
    }
    /* TODO: do I need this ? */
    SelectObject(win->hdcDoubleBuffer, win->bmpDoubleBuffer);
    /* fill out everything with background color */
    RECT r = {0};
    r.bottom = win->winDy();
    r.right = win->winDx();
    FillRect(win->hdcDoubleBuffer, &r, gBrushBg);
    win->hdcToDraw = win->hdcDoubleBuffer;
    return TRUE;
}

static void WindowInfo_DoubleBuffer_Show(WindowInfo *win, HDC hdc)
{
    if (win->hdc != win->hdcToDraw) {
        assert(win->hdcToDraw == win->hdcDoubleBuffer);
        BitBlt(hdc, 0, 0, win->winDx(), win->winDy(), win->hdcDoubleBuffer, 0, 0, SRCCOPY);
    }
}

static void WindowInfoList_Remove(WindowInfo *to_remove);

static void WindowInfo_Delete(WindowInfo *win)
{
    WindowInfoList_Remove(win);

    if (win->dm) {
        RenderQueue_RemoveForDisplayModel(win->dm);
        cancelRenderingForDisplayModel(win->dm);
    }
    delete win->dm;
    if (win->pdfsync) {
      delete win->pdfsync;
      win->pdfsync = NULL;
    }
    if (win->stopFindStatusThreadEvent) {
        CloseHandle(win->stopFindStatusThreadEvent);
        win->stopFindStatusThreadEvent = NULL;
    }
    if (win->findStatusThread) {
        CloseHandle(win->findStatusThread);
        win->findStatusThread = NULL;
    }
    win->dm = NULL;
    WindowInfo_Dib_Deinit(win);
    WindowInfo_DoubleBuffer_Delete(win);
    DragAcceptFiles(win->hwndCanvas, FALSE);

    free(win->title);
    win->title = NULL;

    delete win;
}

static WindowInfo* WindowInfo_FindByHwnd(HWND hwnd)
{
    WindowInfo  *win = gWindowList;
    while (win) {
        if (hwnd == win->hwndFrame)
            return win;
        if (hwnd == win->hwndCanvas)
            return win;
        if (hwnd == win->hwndReBar)
            return win;
        if (hwnd == win->hwndFindBox)
            return win;
        if (hwnd == win->hwndFindStatus)
            return win;
        if (hwnd == win->hwndPageBox)
            return win;
        if (hwnd == win->hwndTocBox)
            return win;
        if (hwnd == win->hwndSpliter)
            return win;
        win = win->next;
    }
    return NULL;
}

static WindowInfo *WindowInfo_New(HWND hwndFrame) {
    WindowInfo * win = WindowInfo_FindByHwnd(hwndFrame);
    assert(!win);

    win = new WindowInfo();
    if (!win)
        return NULL;

    if (!WindowInfo_Dib_Init(win))
        goto Error;

    win->state = WS_ABOUT;
    win->hwndFrame = hwndFrame;
    win->mouseAction = MA_IDLE;
    return win;
Error:
    WindowInfo_Delete(win);
    return NULL;
}

static void WindowInfoList_Add(WindowInfo *win) {
    win->next = gWindowList;
    gWindowList = win;
}

static void WindowInfoList_Remove(WindowInfo *to_remove) {
    assert(to_remove);
    if (!to_remove)
        return;
    if (gWindowList == to_remove) {
        gWindowList = to_remove->next;
        return;
    }
    WindowInfo* curr = gWindowList;
    while (curr) {
        if (to_remove == curr->next) {
            curr->next = to_remove->next;
            return;
        }
        curr = curr->next;
    }
}

static void WindowInfoList_DeleteAll(void) {
    WindowInfo* curr = gWindowList;
    while (curr) {
        WindowInfo* next = curr->next;
        WindowInfo_Delete(curr);
        curr = next;
    }
    gWindowList = NULL;
}

static int WindowInfoList_Len(void) {
    int len = 0;
    WindowInfo* curr = gWindowList;
    while (curr) {
        ++len;
        curr = curr->next;
    }
    return len;
}

// Find the first windows showing a given PDF file 
WindowInfo* WindowInfoList_Find(TCHAR * file) {
    TCHAR * normFile = FilePathW_Normalize(file, FALSE);
    if(!normFile)
        return NULL;

    WindowInfo* curr = gWindowList;
    while (curr) {
        if (tstr_ieq(curr->watcher.filepath(), normFile)) {
            free(normFile);
            return curr;
        }
        curr = curr->next;
    }
    free(normFile);
    return NULL;
}

static void WindowInfo_UpdateFindbox(WindowInfo *win) {
    InvalidateRect(win->hwndToolbar, NULL, true);
    if (!win->dm) {  // Avoid focus on Find box
        SetClassLong(win->hwndFindBox, GCL_HCURSOR, (LONG)gCursorArrow);
        HideCaret(NULL);
    }
    else {
        SetClassLong(win->hwndFindBox, GCL_HCURSOR, (LONG)gCursorIBeam);
        ShowCaret(NULL);
    }
}

static void WindowInfo_RedrawAll(WindowInfo *win, bool update=false) {
    InvalidateRect(win->hwndCanvas, NULL, false);
    if (update)
        UpdateWindow(win->hwndCanvas);
}

static bool FileCloseMenuEnabled(void) {
    WindowInfo* win = gWindowList;
    while (win) {
        if (win->state != WS_ABOUT)
            return true;
        win = win->next;
    }
    return false;
}

bool TbIsSepId(int bmpIndex) {
    return bmpIndex < 0;
}
 
static void ToolbarUpdateStateForWindow(WindowInfo *win) {
    const LPARAM enable = (LPARAM)MAKELONG(1,0);
    const LPARAM disable = (LPARAM)MAKELONG(0,0);

    for (size_t i=0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        if (TbIsSepId(gToolbarButtons[i].bmpIndex))
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
            switch( cmdId )
            {
                case IDM_FIND_NEXT:
                case IDM_FIND_PREV: 
                    // TODO: Update on whether there's more to find, not just on whether there is text.
                    if (win_get_text_len(win->hwndFindBox) == 0)
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

static BOOL WindowInfo_PdfLoaded(WindowInfo *win)
{
    assert(win);
    if (!win) return FALSE;
    if (!win->dm) return FALSE;
    return TRUE;
}

static void MenuUpdateBookmarksStateForWindow(WindowInfo *win) {
    HMENU hmenu = win->hMenu;
    BOOL documentSpecific = WindowInfo_PdfLoaded(win);
    BOOL enabled = WS_SHOWING_PDF == win->state && win->dm && win->dm->hasTocTree();

    if (documentSpecific ? win->dm->_showToc : gGlobalPrefs.m_showToc)
        CheckMenuItem(hmenu, IDM_VIEW_BOOKMARKS, MF_BYCOMMAND | MF_CHECKED);
    else
        CheckMenuItem(hmenu, IDM_VIEW_BOOKMARKS, MF_BYCOMMAND | MF_UNCHECKED);
    
    if (enabled)
        EnableMenuItem(hmenu, IDM_VIEW_BOOKMARKS, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_VIEW_BOOKMARKS, MF_BYCOMMAND | MF_GRAYED);
}

static void MenuUpdateShowToolbarStateForWindow(WindowInfo *win) {
    if (gGlobalPrefs.m_showToolbar)
        CheckMenuItem(win->hMenu, IDM_VIEW_SHOW_HIDE_TOOLBAR, MF_BYCOMMAND | MF_CHECKED);
    else
        CheckMenuItem(win->hMenu, IDM_VIEW_SHOW_HIDE_TOOLBAR, MF_BYCOMMAND | MF_UNCHECKED);
}

// show which language is being used via check in Language/* menu
static void MenuUpdateLanguage(WindowInfo *win) {
    HMENU hmenu = win->hMenu;
    for (int i = 0; i < LANGS_COUNT; i++) {
        const char *langName = g_langs[i]._langName;
        int langMenuId = g_langs[i]._langId;
        if (str_eq(CurrLangNameGet(), langName))
            CheckMenuItem(hmenu, langMenuId, MF_BYCOMMAND | MF_CHECKED);
        else
            CheckMenuItem(hmenu, langMenuId, MF_BYCOMMAND | MF_UNCHECKED);
    }
}

static void MenuUpdateStateForWindow(WindowInfo *win) {
    static UINT menusToDisableIfNoPdf[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS,
        IDM_VIEW_WITH_ACROBAT };

    bool fileCloseEnabled = FileCloseMenuEnabled();
    HMENU hmenu = win->hMenu;
    if (fileCloseEnabled)
        EnableMenuItem(hmenu, IDM_CLOSE, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_CLOSE, MF_BYCOMMAND | MF_GRAYED);

    bool filePrintEnabled = false;
    if (win->dm && win->dm->pdfEngine && win->dm->pdfEngine->printingAllowed())
        filePrintEnabled = true;
    if (filePrintEnabled)
        EnableMenuItem(hmenu, IDM_PRINT, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);

    MenuUpdateBookmarksStateForWindow(win);
    MenuUpdateShowToolbarStateForWindow(win);
    MenuUpdateLanguage(win);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    for (size_t i = 0; i < dimof(menusToDisableIfNoPdf); i++) {
        UINT menuId = menusToDisableIfNoPdf[i];
        if (WS_SHOWING_PDF == win->state)
            EnableMenuItem(hmenu, menuId, MF_BYCOMMAND | MF_ENABLED);
        else
            EnableMenuItem(hmenu, menuId, MF_BYCOMMAND | MF_GRAYED);
    }
    /* Hide scrollbars if not showing a PDF */
    /* TODO: doesn't really fit the name of the function */
    if (WS_SHOWING_PDF == win->state) {
        if (win->dm->needHScroll())
            ShowScrollBar(win->hwndCanvas, SB_HORZ, TRUE);
        if (win->dm->needVScroll() || (DM_SINGLE_PAGE == win->dm->displayMode() && win->dm->pageCount() > 1))
            ShowScrollBar(win->hwndCanvas, SB_VERT, TRUE);
    }
    else {
        ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
        if (WS_ABOUT == win->state)
            win_set_text(win->hwndFrame, gWindowTitle);
    }
}

/* Disable/enable menu items and toolbar buttons depending on wheter a
   given window shows a PDF file or not. */
static void MenuToolbarUpdateStateForAllWindows(void) {
    WindowInfo* win = gWindowList;
    while (win) {
        MenuUpdateStateForWindow(win);
        ToolbarUpdateStateForWindow(win);
        win = win->next;
    }
}

#define MIN_WIN_DX 50
#define MAX_WIN_DX 4096
#define MIN_WIN_DY 50
#define MAX_WIN_DY 4096

static bool IsWindowVisibleOnAMonitor(int x, int y, int dx, int dy)
{
    // check whether the lower half of the window's title bar is
    // inside a visible area (supports multiple monitors)
    RECT caption;
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    SetRect(&caption, x, y + captionDy / 2, x + dx, y + captionDy);

    return NULL != MonitorFromRect(&caption, MONITOR_DEFAULTTONULL);
}

static WindowInfo* WindowInfo_CreateEmpty(void) {
    HWND        hwndFrame, hwndCanvas;
    WindowInfo* win;

    /* TODO: maybe adjustement of size and position should be outside of this function */
    int winX = CW_USEDEFAULT;
    int winY = CW_USEDEFAULT;
    if (DEFAULT_WIN_POS != gGlobalPrefs.m_windowPosX) {
        winX = gGlobalPrefs.m_windowPosX;
        winY = gGlobalPrefs.m_windowPosY;
    }

    int winDx = DEF_PAGE_DX;
    if (DEFAULT_WIN_POS != gGlobalPrefs.m_windowDx) {
        winDx = gGlobalPrefs.m_windowDx;
        if (winDx < MIN_WIN_DX || winDx > MAX_WIN_DX)
            winDx = DEF_PAGE_DX;
    }
    
    int winDy = DEF_PAGE_DY;
    if (DEFAULT_WIN_POS != gGlobalPrefs.m_windowDy) {
        winDy = gGlobalPrefs.m_windowDy;
        if (winDy < MIN_WIN_DY || winDy > MAX_WIN_DY)
            winDy = DEF_PAGE_DY;
    }
    
    if (winX != CW_USEDEFAULT && winY != CW_USEDEFAULT) {
        if (!IsWindowVisibleOnAMonitor(winX, winY, winDx, winDy)) {
            winX = CW_USEDEFAULT;
            winY = CW_USEDEFAULT;
        }
    }

#if FANCY_UI
    hwndFrame = CreateWindowEx(
//            WS_EX_TOOLWINDOW,
        0,
//            WS_OVERLAPPEDWINDOW,
//            WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE,
        //WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_HSCROLL | WS_VSCROLL,
        FRAME_CLASS_NAME, gWindowTitle,
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winDx, winDy,
        NULL, NULL,
        ghinst, NULL);
#else
    hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, gWindowTitle,
            WS_OVERLAPPEDWINDOW,
            winX, winY, winDx, winDy,
            NULL, NULL,
            ghinst, NULL);
#endif

    if (!hwndFrame)
        return NULL;

    win = WindowInfo_New(hwndFrame);

    hwndCanvas = CreateWindowEx(
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
    WindowInfo_RebuildMenu(win);
    assert(win->hMenu);
    BOOL ok = SetMenu(hwndFrame, win->hMenu);
    assert(ok);

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    CreateToolbar(win, ghinst);
    CreateTocBox(win, ghinst);
    WindowInfo_UpdateFindbox(win);
    DragAcceptFiles(win->hwndCanvas, TRUE);

    win->stopFindStatusThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    WindowInfoList_Add(win);
    return win;
}

static void RecalcSelectionPosition (WindowInfo *win) {
    SelectionOnPage *   selOnPage = win->selectionOnPage;
    RectD               selD;
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
        } else {//page is visible
            RectD_Copy (&selD, &selOnPage->selectionPage);
            win->dm->rectCvtUserToScreen (selOnPage->pageNo, &selD);
            RectI_FromRectD (&selOnPage->selectionCanvas, &selD);
        }
        selOnPage = selOnPage->next;
    }
}
// Clear all the requests from the PageRender queue.
static void ClearPageRenderRequests()
{
    SetEvent(gPageRenderClearQueue);
    WaitForSingleObject(gPageRenderQueueCleared, INFINITE);
}

static bool LoadPdfIntoWindow(
    const TCHAR *fileName, // path to the PDF
    WindowInfo *win,       // destination window
    const DisplayState *state,   // state
    bool is_new_window,    // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool tryrepair,        // if true then try to repair the PDF if it is broken
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (is_new_window=false)
{
    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.m_globalPrefsOnly || state && state->useGlobalValues))
        state = NULL;

    /* In theory I should get scrollbars sizes using Win32_GetScrollbarSize(&scrollbarYDx, &scrollbarXDy);
       but scrollbars are not part of the client area on windows so it's better
       not to have them taken into account by DisplayModelSplash code.
       TODO: I think it's broken anyway and DisplayModelSplash needs to know if
             scrollbars are part of client area in order to accomodate windows
             UI properly */
    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    int startPage = 1;
    ScrollState ss = { 1, 0.0, 0.0 };
    int scrollbarYDx = 0;
    int scrollbarXDy = 0;
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
    SizeD totalDrawAreaSize(win->winSize());

    DisplayModel *previousmodel = win->dm;

    win->dm = DisplayModel_CreateFromFileName(fileName,
        totalDrawAreaSize, scrollbarYDx, scrollbarXDy, displayMode, startPage, win, tryrepair);

    if (!win->dm) {
        //DBG_OUT("failed to load file %s\n", fileName); <- fileName is now Unicode
        win->needrefresh = true;
        // if there is an error while reading the pdf and pdfrepair is not requested
        // then fallback to the previous state
        if (!tryrepair) {
            win->dm = previousmodel;
        } else {
            ClearPageRenderRequests(); // This is necessary because the PageRenderThread may still try to access the 'previousmodel'
            delete previousmodel;
            win->state = WS_ERROR_LOADING_PDF;
            win_set_text(win->hwndFrame, FilePathW_GetBaseName(fileName));
            goto Error;
        }
    } else {
        ClearPageRenderRequests(); // This is necessary because the PageRenderThread may still try to access the 'previousmodel'
        delete previousmodel;
        win->needrefresh = false;
    }

    win->dm->setAppData((void*)win);

    double zoomVirtual = gGlobalPrefs.m_defaultZoom;
    int rotation = DEFAULT_ROTATION;

    win->state = WS_SHOWING_PDF;
    if (state) {
        if (win->dm->validPageNo(startPage)) {
            ss.page = startPage;
            /* TODO: make sure scrollX isn't bogus */
            ss.x = state->scrollX;
            ss.y = state->scrollY;
        }
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;
        win->dm->_showToc = state->showToc;
    }
    else {
        win->dm->_showToc = gGlobalPrefs.m_showToc;
    }

    // Review needed: Is the following block really necessary?
    /*
    // The WM_SIZE message must be sent *after* updating win->dm->_showToc
    // otherwise the bookmark window reappear even if state->showToc=false.
    RECT rect;
    GetClientRect(win->hwndFrame, &rect);
    SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect_dx(&rect),rect_dy(&rect)));
    */

    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->hMenu, menuId, TRUE);

    win->dm->relayout(zoomVirtual, rotation);
    win->dm->setScrollState(&ss);

    if (!is_new_window) {
        WindowInfo_RedrawAll(win);
        OnMenuFindMatchCase(win); 
    }
    WindowInfo_UpdateFindbox(win);

    int pageCount = win->dm->pageCount();
    const TCHAR *baseName = FilePathW_GetBaseName(win->dm->fileName());
    if (pageCount <= 0)
        win_set_text(win->hwndFrame, baseName);
    else {
        UpdateToolbarPageText(win, pageCount);
        UpdateToolbarFindText(win);

        const TCHAR *title = baseName;
        if (win->title)
            title = win->title;

        if (win->needrefresh) {
            TCHAR buf[256];
            StringCchPrintf(buf, dimof(buf), _TR("[Changes detected; refreshing] %s"), title);
            win_set_text(win->hwndFrame, buf);
        }
        else
            win_set_text(win->hwndFrame, title);
    }
Error:
    if (is_new_window || placeWindow && state) {
        assert(win);
        if (is_new_window && state && 0 != state->windowDx && 0 != state->windowDy) {
            RECT rect;
            rect.top = state->windowY;
            rect.left = state->windowX;
            rect.bottom = rect.top + state->windowDy;
            rect.right = rect.left + state->windowDx;
            
            // Make sure it doesn't have a position like outside of the screen etc.
            rect_shift_to_work_area(&rect);
            
            // This shouldn't happen until win->state != WS_ABOUT, so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame,
                rect.left, rect.top, rect_dx(&rect), rect_dy(&rect), TRUE);
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
    if (win->dm && win->dm->_showToc) {
        if (win->dm->hasTocTree()) {
            win->ClearTocBox();
            win->ShowTocBox();
        }
        else
        {
            // Hide the now useless ToC sidebar and force an update afterwards
            win->HideTocBox();
            WindowInfo_RedrawAll(win, true);
        }
    }
    MenuToolbarUpdateStateForAllWindows();
    if (win->state == WS_ERROR_LOADING_PDF) {
        WindowInfo_RedrawAll(win);
        return false;
    }
    // This should only happen after everything else is ready
    if ((is_new_window || placeWindow) && showWin && showAsFullScreen)
        WindowInfo_EnterFullscreen(win);
    return true;
}

// This function is executed within the watching thread
static void OnFileChange(const TCHAR * filename, LPARAM param)
{
    // We cannot called WindowInfo_Refresh directly as it could cause race conditions between the watching thread and the main thread
    // Instead we just post a message to the main thread to trigger a reload
    PostMessage(((WindowInfo *)param)->hwndFrame, WM_COMMAND, IDM_REFRESH, 0);
}

static void CheckPositionAndSize(DisplayState* ds)
{
    if (!ds)
        return;

    if (0 == ds->windowDx && 0 == ds->windowDy) {
        ds->windowX = gGlobalPrefs.m_windowPosX;
        ds->windowY = gGlobalPrefs.m_windowPosY;
        ds->windowDx = gGlobalPrefs.m_windowDx;
        ds->windowDy = gGlobalPrefs.m_windowDy;
    }

    if (ds->windowDx < MIN_WIN_DX || ds->windowDx > MAX_WIN_DX)
        ds->windowDx = DEF_PAGE_DX;
    if (ds->windowDy < MIN_WIN_DY || ds->windowDy > MAX_WIN_DY)
        ds->windowDy = DEF_PAGE_DY;
    
    if (!IsWindowVisibleOnAMonitor(ds->windowX, ds->windowY, ds->windowDx, ds->windowDy)) {
        ds->windowX = CW_USEDEFAULT;
        ds->windowY = CW_USEDEFAULT;
    }
}

WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win, bool showWin, TCHAR *windowTitle)
{
    assert(fileName);
    if (!fileName) return NULL;

    bool is_new_window = false;
    if (!win && 1 == WindowInfoList_Len() && WS_ABOUT == gWindowList->state) {
        win = gWindowList;
    }
    else if (!win || WS_SHOWING_PDF == win->state) {
        is_new_window = true;
        win = WindowInfo_CreateEmpty();
        if (!win)
            return NULL;
    }

    if (windowTitle)
        win->title = windowTitle;

    // TODO: fileName might not exist.
    // Normalize the file path    
    TCHAR *pFullpath = FilePathW_Normalize(fileName, FALSE);
    if (!pFullpath)
        goto exit;

    FileHistoryList *fileFromHistory = FileHistoryList_Node_FindByFilePath(&gFileHistoryRoot, pFullpath);
    DisplayState *ds = NULL;
    if (fileFromHistory)
        ds = &fileFromHistory->state;

    CheckPositionAndSize(ds);
    if (!LoadPdfIntoWindow(pFullpath, win, ds, is_new_window, true, showWin, true)) {
        /* failed to open */
        goto exit;
    }

    // Define THREAD_BASED_FILEWATCH to use the thread-based implementation of file change detection.
#ifdef THREAD_BASED_FILEWATCH
    // TODO: passing fullPathUtf8 won't work for non-ascii files. First,
    // it should be converted to filesystem encoding. Second, even then it won't
    // work for unicode files names. We use utf8 so that we can safely
    // round-trip the string.
    // The right fix is to convert FileWatcher and PdfSync to handle Unicode
    // file names
    if (!win->watcher.IsThreadRunning())
        win->watcher.StartWatchThread(pFullpath, &OnFileChange, (LPARAM)win);
#else
        win->watcher.Init(pFullpath);
#endif

    win->pdfsync = CreateSynchronizer(pFullpath);

    if (gGlobalPrefs.m_rememberOpenedFiles) {
        AddFileToHistory(pFullpath);
        RebuildProgramMenus();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    SHAddToRecentDocs(SHARD_PATH, pFullpath);

exit:
    if (pFullpath)
        free(pFullpath);
    return win;
}

static HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize)
{
    HFONT       font_dc;
    HFONT       font;
    LOGFONT     lf = {0};

    font_dc = (HFONT)GetStockObject(SYSTEM_FONT);
    if (!GetObject(font_dc, sizeof(LOGFONT), &lf))
        return NULL;

    lf.lfHeight = (LONG)-fontSize;
    lf.lfWidth = 0;
    //lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    //lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;    
    _tcscpy_s(lf.lfFaceName, LF_FACESIZE, fontName);
    lf.lfWeight = FW_DONTCARE;
    font = CreateFontIndirect(&lf);
    return font;
}

static void Win32_Font_Delete(HFONT font)
{
    DeleteObject(font);
}

// The current page edit box is updated with the current page number
void DisplayModel::pageChanged()
{
    WindowInfo *win = (WindowInfo*)appData();
    assert(win);
    if (!win) return;

    int currPageNo = currentPageNo();
    int pageCount = win->dm->pageCount();
    if (pageCount > 0) {
        TCHAR buf[256];
        if (INVALID_PAGE_NO != currPageNo) {
            HRESULT hr = StringCchPrintf(buf, dimof(buf), _T("%d"), currPageNo);
            SetWindowText(win->hwndPageBox, buf);
            ToolbarUpdateStateForWindow(win);
        }
    }
}

/* Call from non-UI thread to cause repainting of the display */
static void triggerRepaintDisplayPotentiallyDelayed(WindowInfo *win, bool delayed)
{
    assert(win);
    if (!win) return;
    if (delayed)
        PostMessage(win->hwndCanvas, WM_APP_REPAINT_DELAYED, 0, 0);
    else
        PostMessage(win->hwndCanvas, WM_APP_REPAINT_NOW, 0, 0);
}

static void triggerRepaintDisplayNow(WindowInfo* win)
{
    triggerRepaintDisplayPotentiallyDelayed(win, false);
}

void DisplayModel::repaintDisplay(bool delayed)
{
    WindowInfo* win = (WindowInfo*)appData();
    triggerRepaintDisplayPotentiallyDelayed(win, delayed);
}

void DisplayModel::setScrollbarsState(void)
{
    WindowInfo *win = (WindowInfo*)this->appData();
    assert(win);
    if (!win) return;

    SCROLLINFO      si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    int canvasDx = _canvasSize.dxI();
    int canvasDy = _canvasSize.dyI();
    int drawAreaDx = drawAreaSize.dxI();
    int drawAreaDy = drawAreaSize.dyI();

    if (drawAreaDx >= canvasDx) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = (int)areaOffset.x;
        si.nMin = 0;
        si.nMax = canvasDx-1;
        si.nPage = drawAreaDx;
    }
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);

    if (drawAreaDy >= canvasDy) {
        si.nMin = 0;
        if (DM_SINGLE_PAGE == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual()) {
            si.nPos = win->dm->currentPageNo() - 1;
            si.nMax = win->dm->pageCount() - 1;
            si.nPage = 1;
        }
        else {
            si.nPos = 0;
            si.nMax = 99;
            si.nPage = 100;
        }
    } else {
        si.nPos = (int)areaOffset.y;
        si.nMin = 0;
        si.nMax = canvasDy-1;
        si.nPage = drawAreaDy;
    }
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
}

static void WindowInfo_ResizeToWindow(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    assert(win->dm);
    if (!win->dm) return;

    win->dm->changeTotalDrawAreaSize(win->winSize());
}

static void WindowInfo_ToggleZoom(WindowInfo *win)
{
    DisplayModel *  dm;

    assert(win);
    if (!win) return;

    dm = win->dm;
    assert(dm);
    if (!dm) return;

    if (ZOOM_FIT_PAGE == dm->zoomVirtual())
        dm->zoomTo(ZOOM_FIT_WIDTH);
    else if (ZOOM_FIT_WIDTH == dm->zoomVirtual())
        dm->zoomTo(ZOOM_FIT_PAGE);
}

static bool ReadRegStr(HKEY keySub, TCHAR *keyName, TCHAR *valName, TCHAR *buffer, DWORD bufLen)
{
    HKEY keyTmp = NULL;
    LONG res = RegCreateKeyEx(keySub, keyName, 0, NULL, 0, KEY_READ, NULL, &keyTmp, NULL);
    if (ERROR_SUCCESS != res) {
        SeeLastError();
        goto Exit;
    }
    res = RegQueryValueEx(keyTmp, valName, NULL, NULL, (BYTE *)buffer, &bufLen);
    if (ERROR_SUCCESS != res)
        SeeLastError();
Exit:
    if (NULL != keyTmp)
        RegCloseKey(keyTmp);

    return ERROR_SUCCESS == res;
}

static bool WriteRegStr(HKEY keySub, TCHAR *keyName, TCHAR *valName, TCHAR *value)
{
    HKEY keyTmp = NULL;
    LONG res = RegCreateKeyEx(keySub, keyName, 0, NULL, 0, KEY_WRITE, NULL, &keyTmp, NULL);
    if (ERROR_SUCCESS != res) {
        SeeLastError();
        goto Exit;
    }
    res = RegSetValueEx(keyTmp, valName, 0, REG_SZ, (const BYTE*)value, (lstrlen(value)+1) * sizeof(TCHAR));
    if (ERROR_SUCCESS != res)
        SeeLastError();
Exit:
    if (NULL != keyTmp)
        RegCloseKey(keyTmp);

    return ERROR_SUCCESS == res;
}

static bool DoAssociateExeWithPdfExtension(bool associateGlobally)
{
    TCHAR exePath[MAX_PATH], tmp[MAX_PATH + 8];
    bool success;

    HKEY hkeyToUse = HKEY_CURRENT_USER;
    if (associateGlobally)
        hkeyToUse = HKEY_LOCAL_MACHINE;

    success = WriteRegStr(hkeyToUse, _T("Software\\Classes\\.pdf"), NULL, APP_NAME_STR);
    if (!success) {
        // At least register for the user if we can't do so for the whole machine
        if (associateGlobally)
            return DoAssociateExeWithPdfExtension(false);
        return false;
    }

    GetModuleFileName(NULL, exePath, dimof(exePath));
    WriteRegStr(hkeyToUse, _T("Software\\Classes\\") APP_NAME_STR, NULL, (TCHAR *)_TR("PDF Document"));
    _sntprintf(tmp, dimof(tmp), _T("%s,1"), exePath);
    WriteRegStr(hkeyToUse, _T("Software\\Classes\\") APP_NAME_STR _T("\\DefaultIcon"), NULL, tmp);
    _sntprintf(tmp, dimof(tmp), _T("\"%s\" \"%%1\""), exePath);
    success = WriteRegStr(hkeyToUse, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell\\open\\command"), NULL, tmp);
    WriteRegStr(hkeyToUse, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell"), NULL, _T("open"));

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, 0, 0);
    return success;
}

bool IsExeAssociatedWithPdfExtension(void)
{
    TCHAR keyName[MAX_PATH + 2], tmp[MAX_PATH + 8];
    bool success;

    // Get the document name for PDFs (don't trust it to be APP_NAME_STR,
    // in case the user has manually associated PDFs with SumatraPDF)
    success = ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL, tmp, sizeof(tmp));
    if (!success)
        return false;
    lstrcpyn(keyName, tmp, dimof(keyName) - 19); // 19 = lstrlen(_T("\\Shell\\open\\command"))
    
    // Make sure that "open" is the default verb for PDFs
    lstrcat(keyName, _T("\\Shell"));
    success = ReadRegStr(HKEY_CLASSES_ROOT, keyName, NULL, tmp, sizeof(tmp));
    if (success && !tstr_ieq(tmp, _T("open")))
        return false;
    
    // Finally, SumatraPDF should be the handler for the "open" verb
    lstrcat(keyName, _T("\\open\\command"));
    success = ReadRegStr(HKEY_CLASSES_ROOT, keyName, NULL, tmp, sizeof(tmp));
    lstrcpy(keyName, _T("\""));
    GetModuleFileName(NULL, keyName + 1, MAX_PATH);
    lstrcat(keyName, _T("\""));
    if (success && tstr_startswith(tmp, keyName))
        return true;
    
    return false;
}

static BOOL RunMyselfAsAdmin(TCHAR *cmdline)
{
    assert(WindowsVer2000OrGreater());
    TCHAR *exePath = ExePathGet();
    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = _T("runas");
    sei.lpFile = exePath;
    sei.lpParameters = cmdline;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    BOOL ok = ShellExecuteEx(&sei);
    return ok;
}

void AssociateExeWithPdfExtension(void)
{
    if (WindowsVerVistaOrGreater())
        RunMyselfAsAdmin(_T("-register-for-pdf"));
    else
        DoAssociateExeWithPdfExtension(true);
}

static bool RegisterForPdfExtentions(HWND hwnd)
{
    if (IsExeAssociatedWithPdfExtension())
        return true;

    if (IsRunningInPortableMode()) {
        MessageBox(hwnd, _TR("This option is not available in portable mode."), _TR("Warning"), MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs.m_pdfAssociateDontAskAgain) {
        int result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs.m_pdfAssociateDontAskAgain);
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

static bool ResolveLnk(TCHAR * path)
{
    CoInitialize(NULL);

    IShellLink *lnk;
    HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID *)&lnk);
    if (SUCCEEDED(hRes)) {
        IPersistFile *file;
        hRes = lnk->QueryInterface(IID_IPersistFile, (LPVOID *)&file);
        if (SUCCEEDED(hRes)) {
            hRes = file->Load(path, STGM_READ);
            if (SUCCEEDED(hRes)) {
                hRes = lnk->Resolve(NULL, SLR_UPDATE);
                if (SUCCEEDED(hRes)) {
                    TCHAR newPath[MAX_PATH];
                    hRes = lnk->GetPath(newPath, MAX_PATH, NULL, 0);
                    if (SUCCEEDED(hRes)) {
                        lstrcpyn(path, newPath, MAX_PATH);
                    }
                }
            }
            file->Release();
        }
        lnk->Release();
    }
    CoUninitialize();

    return S_OK == hRes;
}

static void OnDropFiles(WindowInfo *win, HDROP hDrop)
{
    int         i;
    TCHAR       filename[MAX_PATH];
    const int   files_count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (i = 0; i < files_count; i++)
    {
        DragQueryFile(hDrop, i, filename, MAX_PATH);
        if (tstr_endswithi(filename, _T(".lnk")))
            ResolveLnk(filename);
        // The first dropped document may override the current window
        LoadPdf(filename, i == 0 ? win : NULL);
    }
    DragFinish(hDrop);

    if (files_count > 0)
        WindowInfo_RedrawAll(win);
}

static void DrawLineSimple(HDC hdc, int sx, int sy, int ex, int ey)
{
    MoveToEx(hdc, sx, sy, NULL);
    LineTo(hdc, ex, ey);
}

static void WinResizeIfNeeded(WindowInfo *win, bool resizeWindow=true)
{
    RECT    rc;
    GetClientRect(win->hwndCanvas, &rc);
    int win_dx = rect_dx(&rc);
    int win_dy = rect_dy(&rc);

    if (win->hdcToDraw &&
        (win_dx == win->winDx()) &&
        (win_dy == win->winDy()))
    {
        return;
    }

    WindowInfo_DoubleBuffer_New(win);
    if (resizeWindow)
        WindowInfo_ResizeToWindow(win);
}

static void PostBenchNextAction(HWND hwnd)
{
    PostMessage(hwnd, MSG_BENCH_NEXT_ACTION, 0, 0);
}

static void OnBenchNextAction(WindowInfo *win)
{
    if (!win->dm)
        return;

    if (win->dm->goToNextPage(0))
        PostBenchNextAction(win->hwndFrame);
}

#ifdef SVN_PRE_RELEASE_VER
int CompareVersion(TCHAR *txt1, TCHAR *txt2)
{
    int num1 = _ttoi(txt1);
    int num2 = _ttoi(txt2);
    if (num1 > num2)
        return 1;
    if (num1 == num2)
        return 0;
    return -1;
}
#else
// extract the next (positive) number from the string *txt
static int ExtractNextNumber(TCHAR **txt)
{
    // skip non numeric characters
    int val = -1;
    while(**txt && ((val < 0) || (val > 9)))
        val = *((*txt)++) - '0';
    if( val == -1 )
        return -1;

    TCHAR c;
    int n;
    while(**txt){
        c = *((*txt)++);
        n = c - '0';
        if ((n < 0) || (n > 9))
            break;
        val = 10 * val + n;
    }
    return val;
}
// compare two version string. Return 0 if they are the same, 1 if the first is greater than the second and
// -1 otherwise.
// e.g. 
//   0.9.3.900 is greater than 0.9.3
//   1/|09@300 is greater than 1/|09@3 which is greater than 1$%9)1
int CompareVersion(TCHAR *txt1, TCHAR *txt2)
{
    int v1, v2;
    while(1) {
        v1 = ExtractNextNumber(&txt1);
        v2 = ExtractNextNumber(&txt2);
        if (v1 == v2) {
            if (v1==-1)
                return 0;
        }
        else if (v1 > v2)
            return 1;
        else
            return -1;
    }
}
#endif

static BOOL ShowNewVersionDialog(WindowInfo *win, const TCHAR *newVersion)
{
    Dialog_NewVersion_Data data = {0};
    data.currVersion = UPDATE_CHECK_VER;
    data.newVersion = newVersion;
    data.skipThisVersion = FALSE;
    int res = Dialog_NewVersionAvailable(win->hwndFrame, &data);
    if (data.skipThisVersion) {
        tstr_dup_replace(&gGlobalPrefs.m_versionToSkip, newVersion);
    }
    return DIALOG_OK_PRESSED == res;
}

static void OnUrlDownloaded(WindowInfo *win, HttpReqCtx *ctx)
{
    DWORD dataSize;
    char *txt = (char*)ctx->data.getData(&dataSize);
    TCHAR *url = ctx->url;
    if (tstr_startswith(url, SUMATRA_UPDATE_INFO_URL)) {
        TCHAR *verTxt = multibyte_to_wstr(txt, CP_ACP);
        /* TODO: too hackish */
        tstr_trans_chars(verTxt, _T("\r\n"), _T("\0\0"));
        if (CompareVersion(verTxt, UPDATE_CHECK_VER)>0){
            bool showDialog = true;
            // if automated, respect gGlobalPrefs.m_versionToSkip
            if (ctx->autoCheck && gGlobalPrefs.m_versionToSkip) {
                if (tstr_ieq(gGlobalPrefs.m_versionToSkip, verTxt)) {
                    showDialog = false;
                }
            }
            if (showDialog) {
                BOOL download = ShowNewVersionDialog(win, verTxt);
                if (download) {
                    LaunchBrowser(SVN_UPDATE_LINK);
                }
            }
        } else {
            /* if automated => don't notify that there is no new version */
            if (!ctx->autoCheck) {
                MessageBox(win->hwndFrame, _TR("You have the latest version."), _TR("No new version available."), MB_ICONEXCLAMATION | MB_OK);
            }
        }
        free(verTxt);
    }
    free(txt);
    delete ctx;
}

static void DrawCenteredText(HDC hdc, RECT *r, const TCHAR *txt)
{    
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, lstrlen(txt), r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void PaintTransparentRectangle(WindowInfo *win, HDC hdc, RectI *rect, DWORD selectionColor) {
    HBITMAP hbitmap;       // bitmap handle
    BITMAPINFO bmi;        // bitmap header
    VOID *pvBits;          // pointer to DIB section
    BLENDFUNCTION bf;      // structure for alpha blending
    HDC rectDC = CreateCompatibleDC(hdc);
    const DWORD selectionColorBlack = 0xff000000;
    const int margin = 1;

    ZeroMemory(&bmi, sizeof(BITMAPINFO));

    bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect->dx;
    bmi.bmiHeader.biHeight = rect->dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = rect->dx * rect->dy * 4;

    hbitmap = CreateDIBSection (rectDC, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0x0);
    SelectObject(rectDC, hbitmap);

    for (int y = 0; y < rect->dy; y++) {
        for (int x = 0; x < rect->dx; x++) {
            if (x < margin || x > rect->dx - margin - 1 
                    || y < margin || y > rect->dy - margin - 1)
                ((UINT32 *)pvBits)[x + y * rect->dx] = selectionColorBlack;
            else
                ((UINT32 *)pvBits)[x + y * rect->dx] = selectionColor;
        }
    }
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0x5f;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(hdc, rect->x, rect->y, rect->dx, rect->dy, rectDC, 0, 0, rect->dx, rect->dy, bf);
    DeleteObject (hbitmap);
    DeleteDC (rectDC);
}

static void PaintSelection (WindowInfo *win, HDC hdc) {
    const DWORD selectionColorYellow = 0xfff5fc0c;
    if (win->mouseAction == MA_SELECTING) {
        // during selecting
        RectI selRect;

        selRect.x = min (win->selectionRect.x, 
            win->selectionRect.x + win->selectionRect.dx);
        selRect.y = min (win->selectionRect.y, 
            win->selectionRect.y + win->selectionRect.dy);
        selRect.dx = abs (win->selectionRect.dx);
        selRect.dy = abs (win->selectionRect.dy);

        if (selRect.dx != 0 && selRect.dy != 0)
            PaintTransparentRectangle (win, hdc, &selRect, selectionColorYellow);
    } else {
        // after selection is done
        SelectionOnPage *selOnPage = win->selectionOnPage;
        // TODO: Move recalcing to better place
        RecalcSelectionPosition(win);
        while (selOnPage != NULL) {
            if (selOnPage->selectionCanvas.dx != 0 && selOnPage->selectionCanvas.dy != 0)
                PaintTransparentRectangle(win, hdc, &selOnPage->selectionCanvas, selectionColorYellow);
            selOnPage = selOnPage->next;
        }
    }
}

static void PaintForwardSearchMark(WindowInfo *win, HDC hdc) {
    PdfPageInfo *pageInfo = win->dm->getPageInfo(win->fwdsearchmarkPage);
    if (!pageInfo->visible)
        return;
    
    const DWORD selectionColorBlue = 0xff0000FF;
    const DWORD selectionColorRed = 0xffFF0000;

    RectD recD;
    RectI recI;
    // draw the mark
    recD.x = win->fwdsearchmarkLoc.x-MARK_SIZE/2;
    recD.y = win->fwdsearchmarkLoc.y-MARK_SIZE/2;
    recD.dx = MARK_SIZE;
    recD.dy = MARK_SIZE;
    if (!win->dm->rectCvtUserToScreen (win->fwdsearchmarkPage, &recD))
        return;
    RectI_FromRectD (&recI, &recD);
    PaintTransparentRectangle(win, hdc, &recI, selectionColorRed);

    // draw the line
    recD.x = 0;
    recD.y = win->fwdsearchmarkLoc.y-MARK_SIZE;
    recD.dx = pageInfo->pageDx;
    recD.dy = 2*MARK_SIZE;
    win->dm->rectCvtUserToScreen (win->fwdsearchmarkPage, &recD);
    RectI_FromRectD (&recI, &recD);
    PaintTransparentRectangle(win, hdc, &recI, selectionColorBlue);
}

static void WindowInfo_Paint(WindowInfo *win, HDC hdc, PAINTSTRUCT *ps)
{
    RECT                bounds;
    RenderedBitmap *    renderedBmp = NULL;

    assert(win);
    if (!win) return;
    DisplayModel* dm = win->dm;
    assert(dm);
    if (!dm) return;

    assert(win->hdcToDraw);
    hdc = win->hdcToDraw;

    FillRect(hdc, &(ps->rcPaint), gBrushBg);

    DBG_OUT("WindowInfo_Paint() ");
    for (int pageNo = 1; pageNo <= dm->pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = dm->getPageInfo(pageNo);
        if (!pageInfo->visible)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        LockCache();
        //BitmapCacheEntry *entry = BitmapCache_Find(dm, pageNo, dm->zoomReal(), dm->rotation());
        BitmapCacheEntry *entry = BitmapCache_Find(dm, pageNo);

        if (entry)
            renderedBmp = entry->bitmap;

        if (!renderedBmp)
            DBG_OUT("   missing bitmap on visible page %d\n", pageNo);

        int xSrc = (int)pageInfo->bitmapX;
        int ySrc = (int)pageInfo->bitmapY;
        int bmpDx = (int)pageInfo->bitmapDx;
        int bmpDy = (int)pageInfo->bitmapDy;
        int xDest = (int)pageInfo->screenX;
        int yDest = (int)pageInfo->screenY;

        if (!entry) {
            /* TODO: assert is queued for rendering ? */
            HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
            HFONT origFont = (HFONT)SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
            bounds.left = xDest;
            bounds.top = yDest;
            bounds.right = xDest + bmpDx;
            bounds.bottom = yDest + bmpDy;
            FillRect(hdc, &bounds, gBrushWhite);
            DrawCenteredText(hdc, &bounds, _TR("Please wait - rendering..."));
            DBG_OUT("drawing empty %d ", pageNo);
            if (origFont)
                SelectObject(hdc, origFont);
            Win32_Font_Delete(fontRightTxt);
            UnlockCache();
            continue;
        }

        if (BITMAP_CANNOT_RENDER == renderedBmp) {
            HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
            HFONT origFont = (HFONT)SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
            bounds.left = xDest;
            bounds.top = yDest;
            bounds.right = xDest + bmpDx;
            bounds.bottom = yDest + bmpDy;
            FillRect(hdc, &bounds, gBrushWhite);
            DrawCenteredText(hdc, &bounds, _TR("Couldn't render the page"));
            if (origFont)
                SelectObject(hdc, origFont);
            Win32_Font_Delete(fontRightTxt);
            UnlockCache();
            continue;
        }

        DBG_OUT("page %d ", pageNo);

        int renderedBmpDx = renderedBmp->dx();
        int renderedBmpDy = renderedBmp->dy();
        int currPageDx = (int)pageInfo->currDx;
        int currPageDy = (int)pageInfo->currDy;
        HBITMAP hbmp = renderedBmp->createDIBitmap(hdc);
        UnlockCache();
        if (!hbmp)
            continue;

        // Frame info
        int fx = xDest, fy = yDest, fw = bmpDx - 4, fh = bmpDy - 4;
        // Shadow info
        int sx = fx + 4, sy = fy + 4, sw = fw, sh = fh;
        // Adjust frame/shadow info base on page/bitmap size
        if (bmpDy < currPageDy) {
            if (yDest <= 0) {
                sy = fy;
                sh = sh + 4;
                fy = fy - 1;
                if (yDest + bmpDy < currPageDy) {
                    sh = sh + 5;
                    fh = fh + 6;
                }
            }
            else {
                sh = sh + 4;
                fh = fh + 6;
            }
        }
        if (bmpDx < currPageDx) {
            fw = sw = bmpDx + 1;
            if (xDest <= 0) {
                fx = fx - 1;
            }
        }
        // Draw shadow
        RECT rc;
        HBRUSH br = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
        rect_set(&rc, sx, sy, sw, sh);
        FillRect(hdc, &rc, br);
        DeleteBrush(br);

        // Draw frame
        HPEN pe = CreatePen(PS_SOLID, 1, RGB(0x88, 0x88, 0x88));
        SelectObject(hdc, pe);
        DrawLineSimple(hdc, fx, fy, fx+fw-1, fy);
        DrawLineSimple(hdc, fx, fy, fx, fy+fh-1);
        DrawLineSimple(hdc, fx+fw-1, fy, fx+fw-1, fy+fh-1);
        DrawLineSimple(hdc, fx, fy+fh-1, fx+fw-1, fy+fh-1);
        DeletePen(pe);

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            SelectObject(bmpDC, hbmp);
            if ((renderedBmpDx < currPageDx) || (renderedBmpDy < currPageDy))
                StretchBlt(hdc, fx+1, fy+1, fw-2, fh-2, bmpDC, xSrc, ySrc, renderedBmpDx, renderedBmpDy, SRCCOPY);
            else
                BitBlt(hdc, fx+1, fy+1, fw-2, fh-2, bmpDC, xSrc, ySrc, SRCCOPY);
            DeleteDC(bmpDC);
        }
        DeleteObject(hbmp);
    }

    if (win->showSelection)
        PaintSelection(win, hdc);
    
    if (win->showForwardSearchMark)
        PaintForwardSearchMark(win, hdc);

    DBG_OUT("\n");
    if (!gDebugShowLinks)
        return;

    dm->recalcLinksCanvasPos();
    RectI drawAreaRect;
    /* debug code to visualize links */
    drawAreaRect.x = 0;
    drawAreaRect.y = 0;
    drawAreaRect.dx = dm->drawAreaSize.dxI();
    drawAreaRect.dy = dm->drawAreaSize.dyI();

    int linkCount = dm->_linksCount;
    for (int linkNo = 0; linkNo < linkCount; ++linkNo) {
        PdfLink *pdfLink = &dm->_links[linkNo];

        RectI rectLink, intersect;
        rectLink.x = pdfLink->rectCanvas.x;
        rectLink.y = pdfLink->rectCanvas.y;
        rectLink.dx = pdfLink->rectCanvas.dx;
        rectLink.dy = pdfLink->rectCanvas.dy;

        if (RectI_Intersect(&rectLink, &drawAreaRect, &intersect)) {
            RECT rectScreen;
            rectScreen.left = (LONG) ((double)intersect.x);
            rectScreen.top = (LONG) ((double)intersect.y);
            rectScreen.right = rectScreen.left + (LONG) ((double)intersect.dx);
            rectScreen.bottom = rectScreen.top + (LONG) ((double)intersect.dy);

            HPEN pe = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
            SelectObject(hdc, pe);
            DrawLineSimple(hdc, rectScreen.left+1, rectScreen.top+1, 
            rectScreen.right, rectScreen.top+1);
            DrawLineSimple(hdc, rectScreen.left+1, rectScreen.top+1, 
            rectScreen.left+1, rectScreen.bottom);
            DrawLineSimple(hdc, rectScreen.right, rectScreen.top+1, 
            rectScreen.right, rectScreen.bottom);
            DrawLineSimple(hdc, rectScreen.left+1, rectScreen.bottom, 
            rectScreen.right, rectScreen.bottom);
            DeletePen(pe);
        }
    }
}

/* TODO: change the name to DrawAbout.
   Draws the about screen a remember some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
#define ABOUT_RECT_PADDING          8
#define ABOUT_LINE_OUTER_SIZE       2
#define ABOUT_LINE_SEP_SIZE         1
#define ABOUT_LEFT_RIGHT_SPACE_DX   8
#define ABOUT_MARGIN_DX            10
#define ABOUT_BOX_MARGIN_DY         6

#define ABOUT_BORDER_COL            COL_BLACK

#ifndef SUMATRA_TXT
#define SUMATRA_TXT             _T("Sumatra PDF")
#endif
#define SUMATRA_TXT_FONT        _T("Arial Black")
#define SUMATRA_TXT_FONT_SIZE   24
#define TXTFY(val) #val

#ifdef SVN_PRE_RELEASE_VER
 #define BETA_TXT                _T("Pre-Release")
#else
 #ifdef DEBUG
 #define BETA_TXT                _T("Beta v") _T(CURR_VERSION) _T(" (dbg)")
 #else
 #define BETA_TXT                _T("Beta v") _T(CURR_VERSION)
 #endif
#endif

#define BETA_TXT_FONT           _T("Arial Black")
#define BETA_TXT_FONT_SIZE      12
#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

#define ABOUT_TXT_DY            6

typedef struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;
    const TCHAR *   url;

    /* data calculated by the layout */
    int             leftTxtPosX;
    int             leftTxtPosY;
    int             leftTxtDx;
    int             leftTxtDy;

    int             rightTxtPosX;
    int             rightTxtPosY;
    int             rightTxtDx;
    int             rightTxtDy;
} AboutLayoutInfoEl;

AboutLayoutInfoEl gAboutLayoutInfo[] = {
#ifdef _TEX_ENHANCEMENT
    { _T("note"), _T("TeX build"), _T("http://william.famille-blum.org/software/sumatra/index.html"),
    0, 0, 0, 0, 0, 0, 0, 0 },
#endif 
#ifdef SVN_PRE_RELEASE_VER
    { _T("a note"), _T("Pre-release version, for testing only!"), NULL,
    0, 0, 0, 0, 0, 0, 0, 0 },
#endif
    { _T("programming"), _T("Krzysztof Kowalczyk"), _T("http://blog.kowalczyk.info"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("pdf rendering"), _T("MuPDF"), _T("http://ccxvii.net/fitz/"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("website"), _T("http://blog.kowalczyk.info/software/sumatrapdf"), _T("http://blog.kowalczyk.info/software/sumatrapdf"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("forums"), _T("http://blog.kowalczyk.info/forum_sumatra"), _T("http://blog.kowalczyk.info/forum_sumatra"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("program icon"), _T("Zenon"), _T("http://www.flashvidz.tk/"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("toolbar icons"), _T("Mark James"), _T("http://www.famfamfam.com/lab/icons/silk/"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("ui polishing"), _T("Simon B\xFCnzli"), _T("http://www.zeniko.ch/"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("translators"), _T("The Translators"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translators.html"),
    0, 0, 0, 0, 0, 0, 0, 0 },

    { _T("translations"), _T("Contribute translation"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html"),
    0, 0, 0, 0, 0, 0, 0, 0 },

#ifdef _TEX_ENHANCEMENT
    { _T("TeX enhancements"), _T("William Blum"), _T("http://william.famille-blum.org/"),
    0, 0, 0, 0, 0, 0, 0, 0 },
    { _T("SyncTeX"), _T("Jérome Laurens"), _T("http://itexmac.sourceforge.net/SyncTeX.html"),
    0, 0, 0, 0, 0, 0, 0, 0 },
#endif 

    { NULL, NULL, NULL,
    0, 0, 0, 0, 0, 0, 0, 0 }
};

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RECT * rect);
static void OnPaintAbout(HWND hwnd);

static const TCHAR *AboutGetLink(WindowInfo *win, int x, int y)
{
    if (gRestrictedUse)
        return NULL;

    // Update the link location information
    if (win)
        UpdateAboutLayoutInfo(win->hwndCanvas, win->hdcToDraw, NULL);
    else
        OnPaintAbout(gHwndAbout);

    for (int i = 0; gAboutLayoutInfo[i].leftTxt; i++) {
        if ((x < gAboutLayoutInfo[i].rightTxtPosX) ||
            (x > gAboutLayoutInfo[i].rightTxtPosX + gAboutLayoutInfo[i].rightTxtDx))
            continue;
        if ((y < gAboutLayoutInfo[i].rightTxtPosY) ||
            (y > gAboutLayoutInfo[i].rightTxtPosY + gAboutLayoutInfo[i].rightTxtDy))
            continue;
        return gAboutLayoutInfo[i].url;
    }
    return NULL;
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RECT * rect)
{
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftDy, rightDy;
    int             leftLargestDx, rightLargestDx;
    int             linePosX, linePosY;
    int             currY;
    int             offX, offY;
    int             boxDy;

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);
    HFONT origFont = (HFONT)SelectObject(hdc, fontSumatraTxt);

    /* calculate top box height */
    const TCHAR *txt = SUMATRA_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    boxDy = txtSize.cy + ABOUT_BOX_MARGIN_DY * 2;

    /* calculate left text dimensions */
    (HFONT)SelectObject(hdc, fontLeftTxt);
    leftLargestDx = 0;
    leftDy = 0;
    for (int i = 0; gAboutLayoutInfo[i].leftTxt != NULL; i++) {
        txt = gAboutLayoutInfo[i].leftTxt;
        GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
        gAboutLayoutInfo[i].leftTxtDx = (int)txtSize.cx;
        gAboutLayoutInfo[i].leftTxtDy = (int)txtSize.cy;

        if (0 == i)
            leftDy = gAboutLayoutInfo[i].leftTxtDy;
        else
            assert(leftDy == gAboutLayoutInfo[i].leftTxtDy);
        if (leftLargestDx < gAboutLayoutInfo[i].leftTxtDx)
            leftLargestDx = gAboutLayoutInfo[i].leftTxtDx;
    }

    /* calculate right text dimensions */
    (HFONT)SelectObject(hdc, fontRightTxt);
    rightLargestDx = 0;
    rightDy = 0;
    for (int i = 0; gAboutLayoutInfo[i].leftTxt != NULL; i++) {
        txt = gAboutLayoutInfo[i].rightTxt;
        GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
        gAboutLayoutInfo[i].rightTxtDx = (int)txtSize.cx;
        gAboutLayoutInfo[i].rightTxtDy = (int)txtSize.cy;

        if (0 == i)
            rightDy = gAboutLayoutInfo[i].rightTxtDy;
        else
            assert(rightDy == gAboutLayoutInfo[i].rightTxtDy);
        if (rightLargestDx < gAboutLayoutInfo[i].rightTxtDx)
            rightLargestDx = gAboutLayoutInfo[i].rightTxtDx;
    }

    /* calculate total dimension and position */
    totalDx  = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx;
    totalDx += ABOUT_LEFT_RIGHT_SPACE_DX + ABOUT_LINE_SEP_SIZE + ABOUT_LEFT_RIGHT_SPACE_DX;
    totalDx += rightLargestDx + ABOUT_MARGIN_DX + ABOUT_LINE_OUTER_SIZE;

    totalDy  = boxDy;
    totalDy += ABOUT_LINE_OUTER_SIZE;
    totalDy += (dimof(gAboutLayoutInfo)-1) * (rightDy + ABOUT_TXT_DY);
    totalDy += ABOUT_LINE_OUTER_SIZE + 4;

    RECT rc;
    GetClientRect(hwnd, &rc);
    offX = (rect_dx(&rc) - totalDx) / 2;
    offY = (rect_dy(&rc) - totalDy) / 2;

    if (rect) {
        rect->left = offX;
        rect->top = offY;
        rect->right = offX + totalDx;
        rect->bottom = offY + totalDy;
    }

    /* calculate text positions */
    linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    linePosY = 4;

    currY = offY + boxDy + linePosY;
    for (int i = 0; gAboutLayoutInfo[i].leftTxt != NULL; i++) {
        gAboutLayoutInfo[i].leftTxtPosX = offX + linePosX - ABOUT_LEFT_RIGHT_SPACE_DX - gAboutLayoutInfo[i].leftTxtDx;
        gAboutLayoutInfo[i].leftTxtPosY = currY + (rightDy - leftDy) / 2;
        gAboutLayoutInfo[i].rightTxtPosX = offX + linePosX + ABOUT_LEFT_RIGHT_SPACE_DX;
        gAboutLayoutInfo[i].rightTxtPosY = currY;
        currY += rightDy + ABOUT_TXT_DY;
    }

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontSumatraTxt);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);
}

static void DrawAbout(HWND hwnd, HDC hdc, RECT * rect)
{
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftLargestDx;
    int             sumatraPdfTxtDx, sumatraPdfTxtDy;
    int             linePosX, linePosY, lineDy;
    int             offX, offY;
    int             x, y;
    int             boxDy;

    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);

    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontBetaTxt = Win32_Font_GetSimple(hdc, BETA_TXT_FONT, BETA_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HFONT origFont = (HFONT)SelectObject(hdc, fontSumatraTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, brushBg);

    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);

    offX = rect->left;
    offY = rect->top;
    totalDx = rect_dx(rect);
    totalDy = rect_dy(rect);

    /* render title */
    const TCHAR *txt = SUMATRA_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    sumatraPdfTxtDx = txtSize.cx;
    sumatraPdfTxtDy = txtSize.cy;

    boxDy = sumatraPdfTxtDy + ABOUT_BOX_MARGIN_DY * 2;

    Rectangle(hdc, offX, offY + ABOUT_LINE_OUTER_SIZE, offX + totalDx, offY + boxDy + ABOUT_LINE_OUTER_SIZE);

    SetTextColor(hdc, ABOUT_BORDER_COL);
    (HFONT)SelectObject(hdc, fontSumatraTxt);
    x = offX + (totalDx - sumatraPdfTxtDx) / 2;
    y = offY + (boxDy - sumatraPdfTxtDy) / 2;
    txt = SUMATRA_TXT;
    TextOut(hdc, x, y, txt, lstrlen(txt));

    (HFONT)SelectObject(hdc, fontBetaTxt);
    x = offX + (totalDx - sumatraPdfTxtDx) / 2 + sumatraPdfTxtDx + 6;
    y = offY + (boxDy - sumatraPdfTxtDy) / 2;
    txt = BETA_TXT;
    TextOut(hdc, x, y, txt, lstrlen(txt));

#ifdef BUILD_RM_VERSION
    txt = _T("Adapted by RM");
    TextOut(hdc, x, y + 16, txt, lstrlen(txt));
#endif

#ifdef SVN_PRE_RELEASE_VER
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    y += (int)txtSize.cy + 2;

    TCHAR buf[128];
    _sntprintf(buf, dimof(buf), _T("v%s svn %d"), _T(CURR_VERSION), SVN_PRE_RELEASE_VER);
    txt = &(buf[0]);
    TextOut(hdc, x, y, txt, lstrlen(txt));
#endif
    SetTextColor(hdc, ABOUT_BORDER_COL);

    offY += boxDy;
    Rectangle(hdc, offX, offY, offX + totalDx, offY + totalDy - boxDy);

    /* render text on the left*/
    leftLargestDx = 0;
    (HFONT)SelectObject(hdc, fontLeftTxt);
    for (int i = 0; gAboutLayoutInfo[i].leftTxt != NULL; i++) {
        txt = gAboutLayoutInfo[i].leftTxt;
        x = gAboutLayoutInfo[i].leftTxtPosX;
        y = gAboutLayoutInfo[i].leftTxtPosY;
        TextOut(hdc, x, y, txt, lstrlen(txt));

        if (leftLargestDx < gAboutLayoutInfo[i].leftTxtDx)
            leftLargestDx = gAboutLayoutInfo[i].leftTxtDx;
    }

    /* render text on the right */
    (HFONT)SelectObject(hdc, fontRightTxt);
    for (int i = 0; gAboutLayoutInfo[i].leftTxt != NULL; i++) {
        bool hasUrl = !gRestrictedUse && gAboutLayoutInfo[i].url;
        SetTextColor(hdc, hasUrl ? COL_BLUE_LINK : ABOUT_BORDER_COL);

        txt = gAboutLayoutInfo[i].rightTxt;
        x = gAboutLayoutInfo[i].rightTxtPosX;
        y = gAboutLayoutInfo[i].rightTxtPosY;
        TextOut(hdc, x, y, txt, lstrlen(txt));

        if (!hasUrl)
            continue;

        int underlineY = y + gAboutLayoutInfo[i].rightTxtDy - 3;
        SelectObject(hdc, penLinkLine);
        MoveToEx(hdc, x, underlineY, NULL);
        LineTo(hdc, x + gAboutLayoutInfo[i].rightTxtDx, underlineY);    
    }

    linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    linePosY = 4;
    lineDy = (dimof(gAboutLayoutInfo)-1) * (gAboutLayoutInfo[0].rightTxtDy + ABOUT_TXT_DY);

    SelectObject(hdc, penDivideLine);
    MoveToEx(hdc, linePosX + offX, linePosY + offY, NULL);
    LineTo(hdc, linePosX + offX, linePosY + lineDy + offY);

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontSumatraTxt);
    Win32_Font_Delete(fontBetaTxt);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);

    DeleteObject(brushBg);
    DeleteObject(penBorder);
    DeleteObject(penDivideLine);
    DeleteObject(penLinkLine);
}

static void WinMoveDocBy(WindowInfo *win, int dx, int dy)
{
    assert(win);
    if (!win) return;
    assert (WS_SHOWING_PDF == win->state);
    if (WS_SHOWING_PDF != win->state) return;
    assert(win->dm);
    if (!win->dm) return;
    assert(!win->linkOnLastButtonDown);
    if (win->linkOnLastButtonDown) return;
    if (0 != dx)
        win->dm->scrollXBy(dx);
    if (0 != dy)
        win->dm->scrollYBy(dy, FALSE);
}

static void CopySelectionTextToClipboard(WindowInfo *win)
{
    SelectionOnPage *   selOnPage;

    assert(win);
    if (!win) return;

    if (!win->selectionOnPage) return;

    HGLOBAL handle;
    TCHAR *ucsbuf;
    int ucsbuflen = 4096;

    if (!OpenClipboard(NULL)) return;

    EmptyClipboard();

    handle = GlobalAlloc(GMEM_MOVEABLE, ucsbuflen * sizeof(TCHAR));
    if (!handle) {
        CloseClipboard();
        return;
    }
    ucsbuf = (TCHAR *) GlobalLock(handle);

    selOnPage = win->selectionOnPage;

    int copied = 0;
    while (selOnPage != NULL) {
        int charCopied = win->dm->getTextInRegion(selOnPage->pageNo, 
            &selOnPage->selectionPage, (unsigned short *)ucsbuf + copied, ucsbuflen - copied - 1);
        copied += charCopied;
        if (ucsbuflen - copied == 1) 
            break;
        selOnPage = selOnPage->next;
    }
    ucsbuf[copied] = 0;

    GlobalUnlock(handle);

    if (copied > 0)
        if (!SetClipboardData(CF_UNICODETEXT, handle))
            SeeLastError();

    /* also copy a screenshot of the current selection to the clipboard */
    selOnPage = win->selectionOnPage;
    RectD * r = &selOnPage->selectionPage;
    fz_rect clipRegion;
    clipRegion.x0 = r->x; clipRegion.x1 = r->x + r->dx;
    clipRegion.y0 = r->y; clipRegion.y1 = r->y + r->dy;

    RenderedBitmap * bmp = win->dm->renderBitmap(selOnPage->pageNo, win->dm->zoomReal(),
        win->dm->rotation(), &clipRegion, NULL, NULL);
    if (bmp) {
        HDC hDC = GetDC(NULL);
        HBITMAP hBmp = bmp->createDIBitmap(hDC);
        if (hBmp) {
            if (!SetClipboardData(CF_BITMAP, hBmp))
                SeeLastError();
            DeleteObject(hBmp);
        }
        ReleaseDC(NULL, hDC);
        free(bmp);
    }

    CloseClipboard();
}

static void DeleteOldSelectionInfo (WindowInfo *win) {
    SelectionOnPage *selOnPage = win->selectionOnPage;
    while (selOnPage != NULL) {
        SelectionOnPage *tmp = selOnPage->next;
        free(selOnPage);
        selOnPage = tmp;
    }
    win->selectionOnPage = NULL;
}

static void ConvertSelectionRectToSelectionOnPage (WindowInfo *win) {
    RectI pageOnScreen, intersect;

    for (int pageNo = win->dm->pageCount(); pageNo >= 1; --pageNo) {
        PdfPageInfo *pageInfo = win->dm->getPageInfo(pageNo);
        if (!pageInfo->visible)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        pageOnScreen.x = pageInfo->screenX;
        pageOnScreen.y = pageInfo->screenY;
        pageOnScreen.dx = pageInfo->bitmapDx;
        pageOnScreen.dy = pageInfo->bitmapDy;

        if (!RectI_Intersect(&win->selectionRect, &pageOnScreen, &intersect))
            continue;

        /* selection intersects with a page <pageNo> on the screen */
        SelectionOnPage *selOnPage = (SelectionOnPage*)malloc(sizeof(SelectionOnPage));
        RectD_FromRectI(&selOnPage->selectionPage, &intersect);

        win->dm->rectCvtScreenToUser (&selOnPage->pageNo, &selOnPage->selectionPage);

        assert (pageNo == selOnPage->pageNo);

        selOnPage->next = win->selectionOnPage;
        win->selectionOnPage = selOnPage;
    }
}

static void OnInverseSearch(WindowInfo *win, UINT x, UINT y)
{
    assert(win);
    if (!win || !win->dm ) return;

    if (!win->pdfsync) {
        win->pdfsync = CreateSynchronizer(win->watcher.filepath());
        if (!win->pdfsync) {
            DBG_OUT("Pdfsync: Sync file cannot be loaded!\n");
            WindowInfo_ShowMessage_Asynch(win, _TR("Synchronization file cannot be opened"), true);
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
    win->pdfsync->convert_coord_to_internal(&x, &y, (UINT)pageInfo->pageDy, BottomLeft);
    UINT line, col;
    UINT err = win->pdfsync->pdf_to_source(pageNo, x, y, srcfilepath, dimof(srcfilepath),&line,&col); // record 101
    if (err != PDFSYNCERR_SUCCESS) {
        DBG_OUT("cannot sync from pdf to source!\n");
        WindowInfo_ShowMessage_Asynch(win, _TR("No synchronization info at this position"), true);
        return;
    }

    TCHAR cmdline[MAX_PATH];
    if (win->pdfsync->prepare_commandline(gGlobalPrefs.m_inverseSearchCmdLine,
      srcfilepath, line, col, cmdline, dimof(cmdline)) ) {
        //ShellExecute(NULL, NULL, cmdline, cmdline, NULL, SW_SHOWNORMAL);
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            DBG_OUT("CreateProcess failed (%d): '%s'.\n", GetLastError(), cmdline);
        }
    }
}

static void OnDraggingStart(WindowInfo *win, int x, int y)
{
    assert(win);
    if (!win) return;
    bool startDragging = (WS_SHOWING_PDF == win->state) && (win->mouseAction == MA_IDLE);
    if (!startDragging)
        return;

    assert(win->dm);
    if (!win->dm) return;
    win->linkOnLastButtonDown = win->dm->linkAtPosition(x, y);
    /* dragging mode only starts when we're not on a link */
    if (win->linkOnLastButtonDown)
        return;

    SetCapture(win->hwndCanvas);
    win->mouseAction = MA_DRAGGING;
    win->dragPrevPosX = x;
    win->dragPrevPosY = y;
    win->dragStartX = x;
    win->dragStartY = y;
    SetCursor(gCursorDrag);
    DBG_OUT(" dragging start, x=%d, y=%d\n", x, y);
}

static void OnDraggingStop(WindowInfo *win, int x, int y)
{
    PdfLink *       link;
    int             dragDx, dragDy;

    assert(win);
    if (!win) return;

    if (WS_SHOWING_PDF != win->state)
        return;

    assert(win->dm);
    if (!win->dm) return;

    if (win->mouseAction == MA_DRAGGING && (GetCapture() == win->hwndCanvas)) {
        dragDx = x - win->dragPrevPosX;
        dragDy = y - win->dragPrevPosY;
        DBG_OUT(" dragging ends, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
        assert(!win->linkOnLastButtonDown);
        WinMoveDocBy(win, dragDx, -dragDy*2);
        win->dragPrevPosX = x;
        win->dragPrevPosY = y;
        SetCursor(gCursorArrow);
        ReleaseCapture();
        /* if we had a selection and this was just a click, hide selection */
        if (win->showSelection) {
            bool hideSelection = (x == win->dragStartX) && (y == win->dragStartY);
            if (hideSelection)
                ClearSearch(win);
        }
        return;
    }

    if (!win->linkOnLastButtonDown)
        return;

    link = win->dm->linkAtPosition(x, y);
    if (link && (link == win->linkOnLastButtonDown)) {
        win->dm->handleLink(link);
        SetCursor(gCursorArrow);
    }
    win->linkOnLastButtonDown = NULL;
}

static void OnMouseMove(WindowInfo *win, int x, int y, WPARAM flags)
{
    assert(win);
    if (!win || WS_SHOWING_PDF != win->state)
        return;

    assert(win->dm);
    if (!win->dm) return;

    if (MA_SCROLLING == win->mouseAction) {
        win->yScrollSpeed = (y - win->dragPrevPosY) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win->xScrollSpeed = (x - win->dragPrevPosX) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
    } else if (MA_SELECTING == win->mouseAction) {
        win->selectionRect.dx = x - win->selectionRect.x;
        win->selectionRect.dy = y - win->selectionRect.y;
        triggerRepaintDisplayNow(win);
    } else if (MA_DRAGGING == win->mouseAction) {
        int dragDx = win->dragPrevPosX - x;
        int dragDy = win->dragPrevPosY - y;
        DBG_OUT(" drag move, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
        WinMoveDocBy(win, dragDx, dragDy);
        win->dragPrevPosX = x;
        win->dragPrevPosY = y;
        return;
    }
}

static void OnSelectionStart(WindowInfo *win, int x, int y)
{
    if (WS_SHOWING_PDF == win->state && win->mouseAction == MA_IDLE) {
        DeleteOldSelectionInfo (win);

        win->selectionRect.x = x;
        win->selectionRect.y = y;
        win->selectionRect.dx = 0;
        win->selectionRect.dy = 0;
        win->showSelection = true;
        win->mouseAction = MA_SELECTING;

        triggerRepaintDisplayNow(win);
    }
}

static void OnSelectionStop(WindowInfo *win, int x, int y)
{
    if (WS_SHOWING_PDF == win->state && win->mouseAction == MA_SELECTING) {
        assert (win->dm);
        if (!win->dm) return;

        win->selectionRect.dx = abs (x - win->selectionRect.x);
        win->selectionRect.dy = abs (y - win->selectionRect.y);
        win->selectionRect.x = min (win->selectionRect.x, x);
        win->selectionRect.y = min (win->selectionRect.y, y);

        if (win->selectionRect.dx == 0 || win->selectionRect.dy == 0) {
            win->showSelection = false;
        } else {
            ConvertSelectionRectToSelectionOnPage (win);
            CopySelectionTextToClipboard (win);
        }
        triggerRepaintDisplayNow(win);
    }
}

static void OnMouseLeftButtonDblClk(WindowInfo *win, int x, int y, int key)
{
    //DBG_OUT("Right button clicked on %d %d\n", x, y);
    assert (win);
    if (!win) return;
    OnInverseSearch(win, x, y);
}

static void OnMouseLeftButtonDown(WindowInfo *win, int x, int y, int key)
{
    //DBG_OUT("Right button clicked on %d %d\n", x, y);
    assert (win);
    if (!win) return;

    if (WS_ABOUT == win->state) {
        // remember a link under so that on mouse up we only activate
        // link if mouse up is on the same link as mouse down
        win->url = AboutGetLink(win, x, y);
        return;
    }

    SetFocus(win->hwndFrame);

    if (!gRestrictedUse && (key & MK_CONTROL) != 0)
        OnSelectionStart(win, x, y);
    else
        OnDraggingStart(win, x, y);
}

static void OnMouseLeftButtonUp(WindowInfo *win, int x, int y, int key)
{
    assert (win);
    if (!win) return;

    if (WS_ABOUT == win->state) {
        const TCHAR * url = AboutGetLink(win, x, y);
        if (url && url == win->url)
            LaunchBrowser(url);
        win->url = NULL;
        return;
    }

    if (!gRestrictedUse && (key & MK_CONTROL) != 0)
        OnSelectionStop(win, x, y);
    else
        OnDraggingStop(win, x, y);

    win->mouseAction = MA_IDLE;
}

static void OnMouseMiddleButtonDown(WindowInfo *win, int x, int y)
{
    assert(win);
    if (!win) return;

    // Handle message by recording placement then moving document as mouse moves.

    if (win->mouseAction == MA_IDLE) {
        win->mouseAction = MA_SCROLLING;

        // record current mouse position, distance mouse moves
        // from this poition is speed to shift document
        win->dragPrevPosY = y; 
        win->dragPrevPosX = x;
        SetCursor(gCursorScroll);
    } else {
        win->mouseAction = MA_IDLE;
    }
}

#define ABOUT_ANIM_TIMER_ID 15

static void AnimState_AnimStop(AnimState *state)
{
    KillTimer(state->hwnd, ABOUT_ANIM_TIMER_ID);
}

static void AnimState_NextFrame(AnimState *state)
{
    state->frame += 1;
    InvalidateRect(state->hwnd, NULL, FALSE);
    UpdateWindow(state->hwnd);
}

static void AnimState_AnimStart(AnimState *state, HWND hwnd, UINT freqInMs)
{
    assert(IsWindow(hwnd));
    AnimState_AnimStop(state);
    state->frame = 0;
    state->hwnd = hwnd;
    SetTimer(state->hwnd, ABOUT_ANIM_TIMER_ID, freqInMs, NULL);
    AnimState_NextFrame(state);
}

#define ANIM_FONT_NAME _T("Georgia")
#define ANIM_FONT_SIZE_START 20
#define SCROLL_SPEED 3

static void DrawAnim2(WindowInfo *win, HDC hdc, PAINTSTRUCT *ps)
{
    AnimState *     state = &(win->animState);
    RECT            rc;
    HFONT           fontArial24 = NULL;
    HFONT           origFont = NULL;
    int             curFontSize;
    static int      curTxtPosX = -1;
    static int      curTxtPosY = -1;
    static int      curDir = SCROLL_SPEED;

    GetClientRect(win->hwndCanvas, &rc);

    if (-1 == curTxtPosX)
        curTxtPosX = 40;
    if (-1 == curTxtPosY)
        curTxtPosY = 25;

    int areaDx = rect_dx(&rc);
    int areaDy = rect_dy(&rc);

#if 0
    if (state->frame % 24 <= 12) {
        curFontSize = ANIM_FONT_SIZE_START + (state->frame % 24);
    } else {
        curFontSize = ANIM_FONT_SIZE_START + 12 - (24 - (state->frame % 24));
    }
#else
    curFontSize = ANIM_FONT_SIZE_START;
#endif

    curTxtPosY += curDir;
    if (curTxtPosY < 20)
        curDir = SCROLL_SPEED;
    else if (curTxtPosY > areaDy - 40)
        curDir = -SCROLL_SPEED;

    fontArial24 = Win32_Font_GetSimple(hdc, ANIM_FONT_NAME, curFontSize);
    assert(fontArial24);

    origFont = (HFONT)SelectObject(hdc, fontArial24);
    
    SetBkMode(hdc, TRANSPARENT);
    FillRect(hdc, &rc, gBrushBg);
    //DStringSprintf(&txt, "Welcome to animation %d", state->frame);
    //DrawText (hdc, txt.pString, -1, &rc, DT_SINGLELINE);
    TCHAR * txt = _T("Welcome to animation");
    TextOut(hdc, curTxtPosX, curTxtPosY, txt, lstrlen(txt));
    WindowInfo_DoubleBuffer_Show(win, hdc);
    if (state->frame > 99)
        state->frame = 0;

    if (origFont)
        SelectObject(hdc, origFont);
    Win32_Font_Delete(fontArial24);
}

static void WindowInfo_DoubleBuffer_Resize_IfNeeded(WindowInfo *win)
{
    WinResizeIfNeeded(win, false);
}

static void OnPaintAbout(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, &rc);
    EndPaint(hwnd, &ps);
}

static void OnPaint(WindowInfo *win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    RECT rc;
    GetClientRect(win->hwndCanvas, &rc);

    if (WS_ABOUT == win->state) {
        WindowInfo_DoubleBuffer_Resize_IfNeeded(win);
        UpdateAboutLayoutInfo(win->hwndCanvas, win->hdcToDraw, &rc);
        DrawAbout(win->hwndCanvas, win->hdcToDraw, &rc);
        WindowInfo_DoubleBuffer_Show(win, hdc);
    } else if (WS_ERROR_LOADING_PDF == win->state) {
        HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
        HFONT origFont = (HFONT)SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
        SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &ps.rcPaint, gBrushBg);
        DrawText(hdc, _TR("Error loading PDF file."), -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER) ;
        if (origFont)
            SelectObject(hdc, origFont);
        Win32_Font_Delete(fontRightTxt);
    } else if (WS_SHOWING_PDF == win->state) {
        //TODO: it might cause infinite loop due to showing/hiding scrollbars
        WinResizeIfNeeded(win);
        WindowInfo_Paint(win, hdc, &ps);
        WindowInfo_DoubleBuffer_Show(win, hdc);
    } else
        assert(0);

    EndPaint(win->hwndCanvas, &ps);
}

static void OnMenuExit(void)
{
    Prefs_Save();
    PostQuitMessage(0);
}

/* Close the document associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
static void CloseWindow(WindowInfo *win, bool quitIfLast)
{
    assert(win);
    if (!win)  return;

    bool lastWindow = false;
    if (1 == WindowInfoList_Len())
        lastWindow = true;

    if (lastWindow)
        Prefs_Save();
    else
        UpdateCurrentFileDisplayStateForWin(win);

    win->state = WS_ABOUT;

#ifdef THREAD_BASED_FILEWATCH
    win->watcher.SynchronousAbort();
#else
    win->watcher.Clean();
#endif

    if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        if (win->dm && win->dm->_showToc) {
            win->HideTocBox();
            MenuUpdateBookmarksStateForWindow(win);
        }
        win->ClearTocBox();
        delete win->dm;
        win->dm = NULL;
        UpdateToolbarPageText(win, 0);
        UpdateToolbarFindText(win);
        WindowInfo_RedrawAll(win);
        WindowInfo_UpdateFindbox(win);
        DeleteOldSelectionInfo(win);
    } else {
        HWND hwndToDestroy = win->hwndFrame;
        WindowInfo_Delete(win);
        DestroyWindow(hwndToDestroy);
    }

    if (lastWindow && quitIfLast) {
        assert(0 == WindowInfoList_Len());
        DeleteBitmap(gBitmapCloseToc);
        PostQuitMessage(0);
    } else {
        MenuToolbarUpdateStateForAllWindows();
    }
}

/* Zoom document in window 'hwnd' to zoom level 'zoom'.
   'zoom' is given as a floating-point number, 1.0 is 100%, 2.0 is 200% etc.
*/
static void OnMenuZoom(WindowInfo *win, UINT menuId)
{
    if (!win->dm)
        return;

    double zoom = ZoomMenuItemToZoom(menuId);
    win->dm->zoomTo(zoom);
    ZoomMenuItemCheck(win->hMenu, menuId, TRUE);
}

static bool CheckPrinterStretchDibSupport(HWND hwndForMsgBox, HDC hdc)
{
    // most printers can support stretchdibits,
    // whereas a lot of printers do not support bitblt
    // quit if printer doesn't support StretchDIBits
    int rasterCaps = GetDeviceCaps(hdc, RASTERCAPS);
    int supportsStretchDib = rasterCaps & RC_STRETCHDIB;
    if (supportsStretchDib)
        return true;

    MessageBox(hwndForMsgBox, _T("This printer doesn't support the StretchDIBits function"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
    return false;
}

// TODO: make it run in a background thread by constructing new PdfEngine()
// from a file name - this should be thread safe
static void PrintToDevice(DisplayModel *dm, HDC hdc, LPDEVMODE devMode, int nPageRanges, LPPRINTPAGERANGE pr, SelectionOnPage *sel=NULL) {

    assert(dm);
    if (!dm) return;

    PdfEngine *pdfEngine = dm->pdfEngine;
    DOCINFO di = {0};
    di.cbSize = sizeof (DOCINFO);
    di.lpszDocName = pdfEngine->fileName();

    if (StartDoc(hdc, &di) <= 0)
        return;

    // rendering for the same DisplayModel is not thread-safe
    // TODO: in fitz, propably rendering anything might not be thread-safe
    RenderQueue_RemoveForDisplayModel(dm);
    cancelRenderingForDisplayModel(dm);

    SetMapMode(hdc, MM_TEXT);

    int printAreaWidth = GetDeviceCaps(hdc, HORZRES);
    int printAreaHeight = GetDeviceCaps(hdc, VERTRES);

    int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    // use pixel sizes for printer with non square pixels
    float fLogPixelsx= (float)GetDeviceCaps(hdc, LOGPIXELSX); 
    float fLogPixelsy= (float)GetDeviceCaps(hdc, LOGPIXELSY);

    bool bPrintPortrait=fLogPixelsx*printAreaWidth<fLogPixelsy*printAreaHeight;
    if (devMode->dmFields & DM_ORIENTATION)
        bPrintPortrait = DMORIENT_PORTRAIT == devMode->dmOrientation;
    // print all the pages the user requested unless
    // bContinue flags there is a problem.
    for (int i=0; i < nPageRanges; i++) {
        if (-1 == pr->nToPage && 0 < pr->nFromPage) {
            assert(1 == nPageRanges && sel && !sel->next);
            DBG_OUT(" printing:  drawing bitmap for selection\n");
            StartPage(hdc);

            RectD * r = &sel->selectionPage;
            fz_rect clipRegion;
            clipRegion.x0 = r->x; clipRegion.x1 = r->x + r->dx;
            clipRegion.y0 = r->y; clipRegion.y1 = r->y + r->dy;

            int rotation = pdfEngine->pageRotation(pr->nFromPage) + dm->rotation();
            double zoom;
            int printAreaDx, printAreaDy;
            if ((rotation % 180) == 0) {
                zoom = min((double)printAreaWidth / r->dx, (double)printAreaHeight / r->dy);
                printAreaDx = zoom * r->dx; printAreaDy = zoom * r->dy;
            } else {
                zoom = min((double)printAreaWidth / r->dy, (double)printAreaHeight / r->dx);
                printAreaDx = zoom * r->dy; printAreaDy = zoom * r->dx;
            }

            RenderedBitmap *bmp = pdfEngine->renderBitmap(pr->nFromPage, 100.0 * zoom, 0, &clipRegion, NULL, NULL);
            if (!bmp)
                goto Error; /* most likely ran out of memory */

            bmp->stretchDIBits(hdc, leftMargin + (printAreaWidth - printAreaDx) / 2,
                topMargin + (printAreaHeight - printAreaDy) / 2, printAreaDx, printAreaDy);
            delete bmp;
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            continue;
        }

        assert(pr->nFromPage <= pr->nToPage);
        for (DWORD pageNo = pr->nFromPage; pageNo <= pr->nToPage; pageNo++) {
            DBG_OUT(" printing:  drawing bitmap for page %d\n", pageNo);

            StartPage(hdc);
            // MM_TEXT: Each logical unit is mapped to one device pixel.
            // Positive x is to the right; positive y is down.

            // try to use a zoom that matches the size of the page in the
            // printer

            SizeD pSize = pdfEngine->pageSize(pageNo);
            int rotation = pdfEngine->pageRotation(pageNo);
            // Further rotate the page, so that 0° and 180° mean Portrait orientation
            if (pSize.dx() > pSize.dy())
                rotation += 90;

            if (90 == rotation || 270 == rotation)
                rotation = bPrintPortrait ? 270 : 0;
            else
                rotation = bPrintPortrait ? 0 : 90;

            double zoom;
            int printAreaDx, printAreaDy;
            if (0 == rotation) {
                zoom = min((double)printAreaWidth / pSize.dx(), (double)printAreaHeight / pSize.dy());
                printAreaDx = zoom * pSize.dx(); printAreaDy = zoom * pSize.dy();
            } else {
                zoom = min((double)printAreaWidth / pSize.dy(), (double)printAreaHeight / pSize.dx());
                printAreaDx = zoom * pSize.dy(); printAreaDy = zoom * pSize.dx();
            }

            RenderedBitmap *bmp = pdfEngine->renderBitmap(pageNo, 100.0 * zoom, rotation, NULL, NULL, NULL);
            if (!bmp)
                goto Error; /* most likely ran out of memory */

            bmp->stretchDIBits(hdc, leftMargin + (printAreaWidth - printAreaDx) / 2,
                topMargin + (printAreaHeight - printAreaDy) / 2, printAreaDx, printAreaDy);
            delete bmp;
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }
        }
        pr++;
    }

Error:
    EndDoc(hdc);
}

/* Show Print Dialog box to allow user to select the printer
and the pages to print.

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
    PRINTDLGEX       pd;
    LPPRINTPAGERANGE ppr = NULL;

    assert(win);
    if (!win) return;

    DisplayModel *dm = win->dm;
    assert(dm);
    if (!dm) return;

    bool hasSelection = win->selectionOnPage && !win->selectionOnPage->next;

    /* printing uses the WindowInfo win that is created for the
       screen, it may be possible to create a new WindowInfo
       for printing to so we don't mess with the screen one,
       but the user is not inconvenienced too much, and this
       way we only need to concern ourselves with one dm.
       TODO: don't re-use WindowInfo, use a different, synchronious
       way of creating a bitmap */
    ZeroMemory(&pd, sizeof(PRINTDLGEX));
    pd.lStructSize = sizeof(PRINTDLGEX);
    pd.hwndOwner   = win->hwndFrame;
    pd.hDevMode    = NULL;   
    pd.hDevNames   = NULL;   
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!hasSelection)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges =1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    ppr = (LPPRINTPAGERANGE)malloc(MAXPAGERANGES*sizeof(PRINTPAGERANGE));
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->pageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->pageCount();
    pd.nStartPage = START_PAGE_GENERAL;

    if (PrintDlgEx(&pd) == S_OK) {
        if (pd.dwResultAction==PD_RESULT_PRINT) {
            if (CheckPrinterStretchDibSupport(win->hwndFrame, pd.hDC)){
                if (pd.Flags & PD_CURRENTPAGE) {
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=dm->currentPageNo();
                    pd.lpPageRanges->nToPage  =dm->currentPageNo();
                } else if (hasSelection && (pd.Flags & PD_SELECTION)) {
                    // TODO: Implement printing multiple selections?
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=dm->currentPageNo();
                    pd.lpPageRanges->nToPage  =-1; // hint for PrintToDevice
                } else if (!(pd.Flags & PD_PAGENUMS)) {
                    pd.nPageRanges=1;
                    pd.lpPageRanges->nFromPage=1;
                    pd.lpPageRanges->nToPage  =dm->pageCount();
                }
                PrintToDevice(dm, pd.hDC, (LPDEVMODE)pd.hDevMode, pd.nPageRanges, pd.lpPageRanges, win->selectionOnPage);
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
    if (pd.hDC != NULL) DeleteDC(pd.hDC);
    if (pd.hDevNames != NULL) GlobalFree(pd.hDevNames);
    if (pd.hDevMode != NULL) GlobalFree(pd.hDevMode);
}

static void OnMenuSaveAs(WindowInfo *win)
{
    OPENFILENAME   ofn = {0};
    TCHAR          dstFileName[MAX_PATH] = {0};
    const TCHAR *  srcFileName = NULL;

    assert(win);
    if (!win) return;
    assert(win->dm);
    if (!win->dm) return;

    srcFileName = win->dm->fileName();
    assert(srcFileName);
    if (!srcFileName) return;

    // Prepare the file filters (slightly hacky because
    // translations can't contain the \0 character)
    TCHAR fileFilter[256] = {0};
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("PDF documents"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.pdf\1"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("All files"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.*\1"));
    tstr_trans_chars(fileFilter, _T("\1"), _T("\0"));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    tstr_copy(dstFileName, dimof(dstFileName), FilePathW_GetBaseName(srcFileName));
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE == GetSaveFileName(&ofn))
        return;

    TCHAR * realDstFileName = dstFileName;
    if (!tstr_endswithi(dstFileName, _T(".pdf"))) {
        realDstFileName = tstr_cat_s(dstFileName, dimof(dstFileName), _T(".pdf"));
    }
    BOOL cancelled = FALSE;
    BOOL ok = CopyFileEx(srcFileName, realDstFileName, NULL, NULL, &cancelled, COPY_FILE_FAIL_IF_EXISTS);
    if (!ok) {
        SeeLastError();
        MessageBox(win->hwndFrame, _TR("Failed to save a file"), _TR("Warning"), MB_OK | MB_ICONEXCLAMATION);
    }
    if (realDstFileName != dstFileName)
        free(realDstFileName);
}

static void OnMenuOpen(WindowInfo *win)
{
    OPENFILENAME  ofn = {0};
    TCHAR         fileName[260];

    // Prepare the file filters (slightly hacky because
    // translations can't contain the \0 character)
    TCHAR fileFilter[256] = {0};
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("PDF documents"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.pdf\1"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("All files"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.*\1"));
    tstr_trans_chars(fileFilter, _T("\1"), _T("\0"));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = fileName;

    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = L'\0';
    ofn.nMaxFile = dimof(fileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (FALSE != GetOpenFileName(&ofn))
        LoadPdf(fileName, win);
}
static void RotateLeft(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->rotateBy(-90);
}

static void RotateRight(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
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
    if (DM_SINGLE_PAGE == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
        lineHeight = 1;

    switch (LOWORD(wParam))
    {
        case SB_TOP:
           si.nPos = si.nMin;
           break;

        case SB_BOTTOM:
           si.nPos = si.nMax;
           break;

        case SB_LINEUP:
           si.nPos -= lineHeight;
           break;

        case SB_LINEDOWN:
           si.nPos += lineHeight;
           break;

        case SB_PAGEUP:
           si.nPos -= si.nPage;
           break;

        case SB_PAGEDOWN:
           si.nPos += si.nPage;
           break;

        case SB_THUMBTRACK:
           si.nPos = si.nTrackPos;
           break;

        default:
           break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);

    // If the position has changed, scroll the window and update it
    if (win->dm && (si.nPos != iVertPos)) {
        if (DM_SINGLE_PAGE == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
            win->dm->goToPage(si.nPos + 1, 0);
        else
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
        case SB_TOP:
           si.nPos = si.nMin;
           break;

        case SB_BOTTOM:
           si.nPos = si.nMax;
           break;

        case SB_LINEUP:
           si.nPos -= 16;
           break;

        case SB_LINEDOWN:
           si.nPos += 16;
           break;

        case SB_PAGEUP:
           si.nPos -= si.nPage;
           break;

        case SB_PAGEDOWN:
           si.nPos += si.nPage;
           break;

        case SB_THUMBTRACK:
           si.nPos = si.nTrackPos;
           break;

        default:
           break;
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

static bool GetAcrobatPath(TCHAR * buffer, int bufSize)
{
    TCHAR path[MAX_PATH];

    bool foundAcrobat = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe"), NULL, path, dimof(path));
    if (foundAcrobat && buffer)
        lstrcpyn(buffer, path, bufSize);

    return foundAcrobat;
}

static void ViewWithAcrobat(WindowInfo *win)
{
    if (!WindowInfo_PdfLoaded(win))
        return;

    TCHAR acrobatPath[MAX_PATH];
    if (!GetAcrobatPath(acrobatPath, dimof(acrobatPath)))
        return;

    TCHAR params[MAX_PATH + 2];
    wsprintf(params, _T("\"%s\""), win->dm->fileName());
    ShellExecute(NULL, _T("open"), acrobatPath, params, NULL, SW_NORMAL);
}

static void OnMenuViewSinglePage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_SINGLE_PAGE);
}

static void OnMenuViewFacing(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_FACING);
}

static void RememberWindowPosition(WindowInfo *win)
{
    /* If the window being moved or resized doesn't show a PDF document,
       remember its position so that it can be persisted (we assume that
       position of this window is what the user wants to be a position
       of all new windows) */
    if (win->state != WS_ABOUT && !gGlobalPrefs.m_globalPrefsOnly)
        return;
    
    // update global windowState for next default launch when no pdf opened
    if (win->fullScreen)
        gGlobalPrefs.m_windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_NORMAL;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL != gGlobalPrefs.m_windowState || IsIconic(win->hwndFrame))
        return;

    RECT rc;
    GetWindowRect(win->hwndFrame, &rc);
    gGlobalPrefs.m_windowPosX = rc.left;
    gGlobalPrefs.m_windowPosY = rc.top;
    gGlobalPrefs.m_windowDx = rect_dx(&rc);
    gGlobalPrefs.m_windowDy = rect_dy(&rc);
}

static void AdjustWindowEdge(WindowInfo *win)
{
    DWORD exStyle = GetWindowLong(win->hwndCanvas, GWL_EXSTYLE);
    DWORD newStyle = exStyle;

    // Remove the canvas' edge in the cases where the vertical scrollbar
    // would otherwise touch the screen's edge, making the scrollbar much
    // easier to hit with the mouse (cf. Fitts' law)
    if (IsZoomed(win->hwndFrame) || win->fullScreen)
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
    
    if (win->tocLoaded && win->dm->_showToc)
        win->ShowTocBox();
    else
        SetWindowPos(win->hwndCanvas, NULL, 0, rebBarDy, dx, dy-rebBarDy, SWP_NOZORDER);
    // Need this here, so that -page and -nameddest work correctly in continuous mode
    if (WS_SHOWING_PDF == win->state)
        WinResizeIfNeeded(win);
}

static void ReloadPdfDocument(WindowInfo *win)
{
    if (WS_SHOWING_PDF != win->state)
        return;
    const TCHAR *fileName = NULL;
    if (win->dm)
        fileName = (const TCHAR*)tstr_dup(win->dm->fileName());
    CloseWindow(win, false);
    if (fileName) {
        LoadPdf(fileName, win);
        free((void*)fileName);
    }
}

static void RebuildProgramMenus(void)
{
    WindowInfo *win = gWindowList;
    while (win) {
        WindowInfo_RebuildMenu(win);
        // Setting the menu for a full screen window messes things up
        if (!win->fullScreen)
            SetMenu(win->hwndFrame, win->hMenu);
        MenuUpdateStateForWindow(win);
        win = win->next;
    }
}

static void LanguageChanged(const char *langName)
{
    assert(!str_eq(langName, CurrLangNameGet()));

    CurrLangNameSet(langName);
    RebuildProgramMenus();
    UpdateToolbarToolText();
}

static int LangIdFromName(const char *name)
{
    for (int i=0; i < LANGS_COUNT; i++) {
        const char *langName = g_langs[i]._langName;
        if (str_eq(name, langName))
            return g_langs[i]._langId;
    }
    return -1;
}

static void OnMenuLanguage(int langId)
{
    const char *langName = NULL;
    for (int i=0; i < LANGS_COUNT; i++) {
        if (g_langs[i]._langId == langId) {
            langName = g_langs[i]._langName;
            break;
        }
    }

    assert(langName);
    if (!langName) return;
    if (str_eq(langName, CurrLangNameGet()))
        return;
    LanguageChanged(langName);
}

void OnMenuCheckUpdate(WindowInfo *win)
{
    DownloadSumatraUpdateInfo(win, false);
}

static void OnMenuChangeLanguage(WindowInfo *win)
{
    int langId = LangIdFromName(CurrLangNameGet());
    int newLangId = Dialog_ChangeLanguge(win->hwndFrame, langId);
    if (-1 == newLangId)
        return;
    if (langId != newLangId)
        OnMenuLanguage(newLangId);
}

static void OnMenuViewShowHideToolbar(WindowInfo *win)
{
    if (gGlobalPrefs.m_showToolbar)
        gGlobalPrefs.m_showToolbar = FALSE;
    else
        gGlobalPrefs.m_showToolbar = TRUE;

    // Move the focus out of the toolbar
    // TODO: do this for all windows
    if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
        SetFocus(win->hwndFrame);

    win = gWindowList;
    while (win) {
        if (gGlobalPrefs.m_showToolbar)
            ShowWindow(win->hwndReBar, SW_SHOW);
        else
            ShowWindow(win->hwndReBar, SW_HIDE);
        RECT rect;
        GetClientRect(win->hwndFrame, &rect);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect_dx(&rect),rect_dy(&rect)));
        MenuUpdateShowToolbarStateForWindow(win);
        win = win->next;
    }
}

static void OnMenuSettings(WindowInfo *win)
{
    if (DIALOG_OK_PRESSED != Dialog_Settings(win->hwndFrame, &gGlobalPrefs))
        return;

    if (!gGlobalPrefs.m_rememberOpenedFiles)
        FileHistoryList_Free(&gFileHistoryRoot);

    for (win = gWindowList; win; win = win->next) {
        RebuildProgramMenus();
        MenuUpdateBookmarksStateForWindow(win);
        MenuUpdateDisplayMode(win);
        MenuUpdateZoom(win);
    }
    Prefs_Save();
}

static void OnMenuViewContinuous(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_CONTINUOUS);
}

static void OnMenuViewContinuousFacing(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_CONTINUOUS_FACING);
}

static void OnMenuGoToNextPage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToNextPage(0);
}

static void OnMenuGoToPrevPage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToPrevPage(0);
}

static void OnMenuGoToLastPage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToLastPage();
}

static void OnMenuGoToFirstPage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToFirstPage();
}

void WindowInfo::FocusPageNoEdit()
{
    hwndTracker = NULL;
    SendMessage(hwndPageBox, EM_SETSEL, 0, -1);
    SetFocus(hwndPageBox);
}

static void OnMenuGoToPage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.m_showToolbar && !win->fullScreen) {
        win->FocusPageNoEdit();
        return;
    }

    int newPageNo = Dialog_GoToPage(win);
    if (win->dm->validPageNo(newPageNo))
        win->dm->goToPage(newPageNo, 0);
}

static void OnMenuFind(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!WindowInfo_PdfLoaded(win))
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.m_showToolbar && !win->fullScreen) {
        win->FindStart();
        return;
    }

    const TCHAR * previousFind = win_get_textw(win->hwndFindBox);
    DWORD state = SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;
    
    TCHAR * findString = Dialog_Find(win->hwndFrame, previousFind, &matchCase);
    if (findString) {
        win_set_text(win->hwndFindBox, findString);
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
        free(findString);

        Find(win->hwndFindBox, win, FIND_FORWARD);
    }
    free((void *)previousFind);
}

static void OnMenuViewRotateLeft(WindowInfo *win)
{
    RotateLeft(win);
}

static void OnMenuViewRotateRight(WindowInfo *win)
{
    RotateRight(win);
}

void WindowInfo_EnterFullscreen(WindowInfo *win)
{
    if (win->fullScreen || !IsWindowVisible(win->hwndFrame)) 
        return;
    win->fullScreen = true;

    if (win->dm) {
        // Remove TOC from full screen, add back later on exit fullscreen
        win->dm->_tocBeforeFullScreen = win->dm->_showToc;
        if (win->dm->_showToc)
            win->HideTocBox();
    }

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
        w = rect_dx(&mi.rcMonitor);
        h = rect_dy(&mi.rcMonitor);
    }
    long ws = win->prevStyle = GetWindowLong(win->hwndFrame, GWL_STYLE);
    ws &= ~(WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    GetWindowRect(win->hwndFrame, &win->frameRc);

    SetMenu(win->hwndFrame, NULL);
    ShowWindow(win->hwndReBar, SW_HIDE);
    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);
    SetWindowPos(win->hwndFrame, HWND_NOTOPMOST, x, y, w, h, SWP_FRAMECHANGED|SWP_NOZORDER);
    SetWindowPos(win->hwndCanvas, NULL, 0, 0, w, h, SWP_NOZORDER);
    // Make sure that no toolbar/sidebar keeps the focus
    SetFocus(win->hwndFrame);
}

void WindowInfo_ExitFullscreen(WindowInfo *win)
{
    if (!win->fullScreen) 
        return;
    win->fullScreen = false;

    if (win->dm && win->dm->_tocBeforeFullScreen)
        win->ShowTocBox();

    if (gGlobalPrefs.m_showToolbar)
        ShowWindow(win->hwndReBar, SW_SHOW);
    SetMenu(win->hwndFrame, win->hMenu);
    SetWindowLong(win->hwndFrame, GWL_STYLE, win->prevStyle);
    SetWindowPos(win->hwndFrame, HWND_NOTOPMOST,
                 win->frameRc.left, win->frameRc.top,
                 rect_dx(&win->frameRc), rect_dy(&win->frameRc),
                 SWP_FRAMECHANGED|SWP_NOZORDER);
}

static void OnMenuViewFullscreen(WindowInfo *win)
{
    assert(win);
    if (!win)
        return;

    if (!win->dm || gGlobalPrefs.m_globalPrefsOnly) {
        /* not showing a PDF document */
        if (gGlobalPrefs.m_windowState != WIN_STATE_FULLSCREEN)
            gGlobalPrefs.m_windowState = WIN_STATE_FULLSCREEN;
        else if (IsZoomed(win->hwndFrame))
            gGlobalPrefs.m_windowState = WIN_STATE_MAXIMIZED;
        else
            gGlobalPrefs.m_windowState = WIN_STATE_NORMAL;
    }

    if (win->fullScreen)
        WindowInfo_ExitFullscreen(win);
    else
        WindowInfo_EnterFullscreen(win);
}

static void WindowInfo_ShowSearchResult(WindowInfo *win, PdfSearchResult *result)
{
    RectI pageOnScreen;
    PdfPageInfo *pdfPage = win->dm->getPageInfo(result->page);
    pageOnScreen.x = pdfPage->screenX;
    pageOnScreen.y = pdfPage->screenY;
    pageOnScreen.dx = pdfPage->bitmapDx;
    pageOnScreen.dy = pdfPage->bitmapDy;

    RectI rect = {
        result->left,
        result->top,
        abs(result->right - result->left),
        abs(result->bottom - result->top)
    };
    // TODO: this should really be fixed by the upper layer and here
    // bottom should always be >= top
    // assert(result->bottom >= result->top);
    // assert(result->right >= result->left);
    RectI intersect;
    DeleteOldSelectionInfo(win);
    if (RectI_Intersect(&rect, &pageOnScreen, &intersect)) {
        SelectionOnPage *selOnPage = (SelectionOnPage*)malloc(sizeof(SelectionOnPage));
        RectD_FromRectI(&selOnPage->selectionPage, &intersect);
        win->dm->rectCvtScreenToUser(&selOnPage->pageNo, &selOnPage->selectionPage);
        selOnPage->next = win->selectionOnPage;
        win->selectionOnPage = selOnPage;
    }

    win->showSelection = true;
    win->TrackMouse();

    triggerRepaintDisplayNow(win);
}

// Show a message for 3000 millisecond at most
DWORD WINAPI ShowMessageThread(WindowInfo *win)
{
    ShowWindowAsync(win->hwndFindStatus, SW_SHOWNA);
    WaitForSingleObject(win->stopFindStatusThreadEvent, 3000);
    ShowWindowAsync(win->hwndFindStatus, SW_HIDE);
    return 0;
}

// Display the message 'message' asynchronously
// If resize = true then the window width is adjusted to the length of the text
static void WindowInfo_ShowMessage_Asynch(WindowInfo *win, const TCHAR *message, bool resize)
{
    win_set_text(win->hwndFindStatus, message);
    if (resize) {
        // compute the length of the message
        RECT rc = {0,0,FIND_STATUS_WIDTH,0};
        HDC hdc = GetDC(win->hwndFindStatus);
        HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
        DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        ReleaseDC(win->hwndFindStatus, hdc);
        rc.right += 15;
        rc.bottom += 12;
        AdjustWindowRectEx(&rc, GetWindowLong(win->hwndFindStatus, GWL_STYLE), FALSE, GetWindowLong(win->hwndFindStatus, GWL_EXSTYLE));
        MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN + rc.left, FIND_STATUS_MARGIN + rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    }

    // if a thread has previously been started then make sure it has ended
    if (win->findStatusThread) {
        SetEvent(win->stopFindStatusThreadEvent);
        WaitForSingleObject(win->findStatusThread, INFINITE);
        CloseHandle(win->findStatusThread);
    }
    ResetEvent(win->stopFindStatusThreadEvent);
    win->findStatusThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowMessageThread, (void*)win, 0, 0);
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
void WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, UINT x, UINT y)
{
    if (ret == PDFSYNCERR_SUCCESS) {
        // remember the position of the search result for drawing the rect later on
        const PdfPageInfo *pi = win->dm->getPageInfo(page);
        if (pi) {
            WindowInfo_HideMessage(win);

            win->pdfsync->convert_coord_from_internal(&x, &y, (int)pi->pageDy, BottomLeft);
            win->fwdsearchmarkLoc.set(x,y);
            win->fwdsearchmarkPage = page;
            win->showForwardSearchMark = true;

            // Scroll to show the rectangle highlighting the forward search result
            PdfSearchResult res;
            res.page = page;
            res.left = x - MARK_SIZE / 2;
            res.top = y - MARK_SIZE / 2;
            res.right = res.left + MARK_SIZE;
            res.bottom = res.top + MARK_SIZE;
            win->dm->goToPage(page, 0);
            win->dm->MapResultRectToScreen(&res);
            if (IsIconic(win->hwndFrame))
                ShowWindowAsync(win->hwndFrame, SW_RESTORE);
            return;
        }
    }

    TCHAR buf[MAX_PATH];
    if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
        _sntprintf(buf, dimof(buf), _TR("Synchronization file cannot be opened"));
    else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER)
        _sntprintf(buf, dimof(buf), _TR("Page number %u inexistant"), page);
    else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION)
        _sntprintf(buf, dimof(buf), _TR("No synchronization info at this position"));
    else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE)
        _sntprintf(buf, dimof(buf), _TR("Unknown source file (%s)"), srcfilename);
    else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE)
        _sntprintf(buf, dimof(buf), _TR("Source file %s has no synchronization point"), srcfilename);
    else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE)
        _sntprintf(buf, dimof(buf), _TR("No result found around line %u in file %s"), line, srcfilename);
    else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD)
        _sntprintf(buf, dimof(buf), _TR("No result found around line %u in file %s"), line, srcfilename);

    WindowInfo_ShowMessage_Asynch(win, buf, true);
}

static void WindowInfo_ShowFindStatus(WindowInfo *win)
{
    LPARAM disable = (LPARAM)MAKELONG(0,0);

    MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN, FIND_STATUS_MARGIN, FIND_STATUS_WIDTH, 36, false);
    ShowWindow(win->hwndFindStatus, SW_SHOWNA);
    win->findStatusVisible = true;

    EnableWindow(win->hwndFindBox, false);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, disable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, disable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, disable);
}

static void WindowInfo_HideFindStatus(WindowInfo *win)
{
    LPARAM enable = (LPARAM)MAKELONG(1,0);

    EnableWindow(win->hwndFindBox, true);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, enable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, enable);
    SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, enable);

    if (!win->dm->bFoundText)
        WindowInfo_ShowMessage_Asynch(win, _TR("No matches were found"), false);
    else {
        TCHAR buf[256];
        swprintf(buf, _TR("Found text at page %d"), win->dm->currentPageNo());
        WindowInfo_ShowMessage_Asynch(win, buf, false);
    }    
}

static void OnMenuFindNext(WindowInfo *win)
{
    Find(win->hwndFindBox, win, FIND_FORWARD);
}

static void OnMenuFindPrev(WindowInfo *win)
{
    Find(win->hwndFindBox, win, FIND_BACKWARD);
}

static void OnMenuFindMatchCase(WindowInfo *win)
{
    DWORD state = SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win->dm->SetFindMatchCase((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindBox, TRUE);
}

#define KEY_PRESSED_MASK 0x8000
static bool WasKeyDown(int virtKey)
{
    SHORT state = GetKeyState(virtKey);
    if (KEY_PRESSED_MASK & state)
        return true;
    return false;
}

static bool WasShiftPressed()
{
    return WasKeyDown(VK_LSHIFT) || WasKeyDown(VK_RSHIFT);
}

static void OnKeydown(WindowInfo *win, int key, LPARAM lparam, bool inTextfield=false)
{
    if (!win->dm)
        return;
    
    //DBG_OUT("key=%d,%c,shift=%d\n", key, (char)key, (int)WasShiftPressed());

    if (VK_PRIOR == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        SendMessage (win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToPrevPage(0);
    } else if (VK_NEXT == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToNextPage(0);
    } else if (VK_UP == key) {
        SendMessage (win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
    } else if (VK_DOWN == key) {
        SendMessage (win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
    } else if (inTextfield) {
        // The remaining keys have a different meaning
    } else if (VK_LEFT == key) {
        SendMessage (win->hwndCanvas, WM_HSCROLL, SB_PAGEUP, 0);
    } else if (VK_RIGHT == key) {
        SendMessage (win->hwndCanvas, WM_HSCROLL, SB_PAGEDOWN, 0);
    } else if (VK_SPACE == key) {
        bool forward = !WasShiftPressed();
        win->dm->scrollYByAreaDy(forward, true);
    } else if (VK_HOME == key) {
        win->dm->goToFirstPage();
    } else if (VK_END == key) {
        win->dm->goToLastPage();    
    }
}

static void ClearSearch(WindowInfo *win)
{
    win->showSelection = false;
    triggerRepaintDisplayNow(win);
}

static void OnChar(WindowInfo *win, int key)
{
//    DBG_OUT("char=%d,%c\n", key, (char)key);

    if (IsCharUpper((TCHAR)key))
        key = (TCHAR)CharLower((LPTSTR)(TCHAR)key);

    if (VK_ESCAPE == key) {
        if (win->fullScreen)
            OnMenuViewFullscreen(win);
        else if (gGlobalPrefs.m_escToExit)
            DestroyWindow(win->hwndFrame);
        else
            ClearSearch(win);
    } else if ('q' == key) {
        DestroyWindow(win->hwndFrame);
    }

    if (!win->dm)
        return;

    if (VK_BACK == key) {
        bool forward = !WasShiftPressed();
        win->dm->scrollYByAreaDy(!forward, true);
    } else if ('g' == key) {
        OnMenuGoToPage(win);
    } else if ('j' == key) {
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
    } else if ('k' == key) {
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
    } else if ('n' == key) {
        win->dm->goToNextPage(0);
    } else if ('c' == key) {
        DisplayMode newMode = DM_CONTINUOUS;
        if (displayModeFacing(win->dm->displayMode()))
            newMode = DM_CONTINUOUS_FACING;
        SwitchToDisplayMode(win, newMode);
    } else if ('p' == key) {
        win->dm->goToPrevPage(0);
    } else if ('z' == key) {
        WindowInfo_ToggleZoom(win);
    } else if ('+' == key) {
        win->dm->zoomBy(ZOOM_IN_FACTOR);
    } else if ('-' == key) {
        win->dm->zoomBy(ZOOM_OUT_FACTOR);
    } else if ('r' == key) {
        WindowInfo_Refresh(win, false);
    } else if ('/' == key) {
        OnMenuFind(win);
    }
}

static bool IsBenchMode(void)
{
    if (NULL != gBenchFileName)
        return true;
    return false;
}

/* Find a file in a file history list that has a given 'menuId'.
   Return a copy of filename or NULL if couldn't be found.
   It's used to figure out if a menu item selected by the user
   is one of the "recent files" menu items in File menu.
   Caller needs to free() the memory.
   */
static const TCHAR *RecentFileNameFromMenuItemId(UINT  menuId) {
    FileHistoryList* curr = gFileHistoryRoot;
    while (curr) {
        if (curr->menuId == menuId)
            return tstr_dup(curr->state.filePath);
        curr = curr->next;
    }
    return NULL;
}

static void OnMenuContributeTranslation()
{
    LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html"));
}

#define FRAMES_PER_SECS 60
#define ANIM_FREQ_IN_MS  1000 / FRAMES_PER_SECS

static void OnMenuAbout() {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    gHwndAbout = CreateWindow(
            ABOUT_CLASS_NAME, ABOUT_WIN_TITLE,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout)
        return;

    // get the dimensions required for the about box's content
    RECT rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    InflateRect(&rc, ABOUT_RECT_PADDING, ABOUT_RECT_PADDING);

    // resize the new window to just match these dimensions
    RECT wRc, cRc;
    GetWindowRect(gHwndAbout, &wRc);
    GetClientRect(gHwndAbout, &cRc);
    wRc.right += rect_dx(&rc) - rect_dx(&cRc);
    wRc.bottom += rect_dy(&rc) - rect_dy(&cRc);
    MoveWindow(gHwndAbout, wRc.left, wRc.top, rect_dx(&wRc), rect_dy(&wRc), FALSE);

    ShowWindow(gHwndAbout, SW_SHOW);
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    TBBUTTON tbButton = {0};
    tbButton.idCommand = gToolbarButtons[i].cmdId;
    if (TbIsSepId(gToolbarButtons[i].bmpIndex)) {
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
    TBBUTTONINFOW buttonInfo;
    HWND hwnd = win->hwndToolbar;
    LRESULT res;
    for (size_t i=0; i < TOOLBAR_BUTTONS_COUNT; i++) {
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
    WindowInfo *win = gWindowList;
    while (win) {
        UpdateToolbarPageText(win, -1);
        UpdateToolbarFindText(win);
        UpdateToolbarButtonsToolTipsForWindow(win);
        MenuUpdateStateForWindow(win);
        win = win->next;
    }        
}

static WNDPROC DefWndProcFindBox = NULL;
static LRESULT CALLBACK WndProcFindBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_CHAR == message) {
        if (VK_ESCAPE == wParam || VK_TAB == wParam)
        {
            SetFocus(win->hwndFrame);
            return 1;
        } 

        if (VK_RETURN == wParam)
        {
            Find(hwnd, win);
            SetFocus(hwnd); // Set focus back to Text box so return can be pressed again to find next one.
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
    else if (WM_SETFOCUS == message) {
        win->hwndTracker = NULL;
    }
    else if (WM_KEYDOWN == message) {
        OnKeydown(win, wParam, lParam, true);
    }

    int ret = CallWindowProc(DefWndProcFindBox, hwnd, message, wParam, lParam);

    if (WM_CHAR  == message ||
        WM_PASTE == message ||
        WM_CUT   == message ||
        WM_CLEAR == message ||
        WM_UNDO  == message     ) {
        ToolbarUpdateStateForWindow(win);
    }

    return ret;
}

void Find(HWND hwnd, WindowInfo *win, PdfSearchDirection direction)
{
    TCHAR text[256];
    GetWindowText(hwnd, text, sizeof(text));
    bool hasText = lstrlen(text) > 0;
    if (!hasText)
        return;

    BOOL wasModified = Edit_GetModify(hwnd);
    PdfSearchResult *rect;
    if (wasModified)
        rect = win->dm->Find(direction, text);
    else
        rect = win->dm->Find(direction);

    if (rect)
        WindowInfo_ShowSearchResult(win, rect);
    else
        ClearSearch(win);
    WindowInfo_HideFindStatus(win);

    Edit_SetModify(hwnd, FALSE);
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
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_ERASEBKGND == message) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        DrawFrameControl((HDC)wParam, &rect, DFC_BUTTON, DFCS_BUTTONPUSH);
        return true;
    } else if (WM_PAINT == message) {
        RECT rect;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HFONT oldfnt = SelectFont(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
        TCHAR text[256];

        GetClientRect(hwnd, &rect);
        GetWindowText(hwnd, text, 256);

        SetBkMode(hdc, TRANSPARENT);
        rect.left += 10;
        rect.top += 4;
        DrawText(hdc, text, lstrlen(text), &rect, DT_LEFT);
        
        rect.top += 20;
        rect.bottom = rect.top + 5;
        rect.right = rect.left + FIND_STATUS_WIDTH - 20;
        DrawLineSimple(hdc, rect.left, rect.top, rect.right, rect.top);
        DrawLineSimple(hdc, rect.left, rect.bottom, rect.right, rect.bottom);
        DrawLineSimple(hdc, rect.left, rect.top, rect.left, rect.bottom);
        DrawLineSimple(hdc, rect.right, rect.top, rect.right, rect.bottom);
        
        int percent = win->findPercent;
        if (percent > 100)
            percent = 100;
        rect.top += 2;
        rect.left += 2;
        rect.right = rect.left + (FIND_STATUS_WIDTH - 20) * percent / 100 - 3;
        rect.bottom -= 1;
        FillRect(hdc, &rect, gBrushShadow);

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
    int txtLen = lstrlen(txt);
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

#define FIND_BOX_WIDTH 160
static void UpdateToolbarFindText(WindowInfo *win)
{
    const TCHAR *text = _TR("Find:");
    win_set_text(win->hwndFindText, text);

    RECT findWndRect;
    GetWindowRect(win->hwndFindBox, &findWndRect);
    int findWndDy = rect_dy(&findWndRect);

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - findWndDy) / 2;

    SIZE size = TextSizeInHwnd(win->hwndFindText, text);
    size.cx += 6;

    MoveWindow(win->hwndFindText, pos_x, (findWndDy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    MoveWindow(win->hwndFindBox, pos_x + size.cx, pos_y, FIND_BOX_WIDTH, findWndDy, false);
    MoveWindow(win->hwndFindStatus, FIND_STATUS_MARGIN, FIND_STATUS_MARGIN, FIND_STATUS_WIDTH, 36, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = size.cx + FIND_BOX_WIDTH + 12;
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

static void CreateFindBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND find = CreateWindowEx(WS_EX_STATICEDGE, WC_EDIT, _T(""),
                            WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL,
                            0, 1, FIND_BOX_WIDTH, 20, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND status = CreateWindowEx(WS_EX_TOPMOST, FINDSTATUS_CLASS_NAME, _T(""), WS_CHILD|SS_CENTER,
                            0, 0, 0, 0, win->hwndCanvas, (HMENU)0, hInst, NULL);

    HFONT fnt = (HFONT)GetStockObject(DEFAULT_GUI_FONT);  // TODO: this might not work on win95/98
    SetWindowFont(label, fnt, true);
    SetWindowFont(find, fnt, true);
    SetWindowFont(status, fnt, true);

    if (!DefWndProcToolbar)
        DefWndProcToolbar = (WNDPROC)GetWindowLong(win->hwndToolbar, GWL_WNDPROC);
    SetWindowLong(win->hwndToolbar, GWL_WNDPROC, (LONG)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLong(find, GWL_WNDPROC);
    SetWindowLong(find, GWL_WNDPROC, (LONG)WndProcFindBox);

    win->hwndFindText = label;
    win->hwndFindBox = find;
    win->hwndFindStatus = status;

    UpdateToolbarFindText(win);
}

static WNDPROC DefWndProcPageBox = NULL;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_CHAR == message) {
        if (VK_RETURN == wParam) {
            TCHAR buf[256];
            int newPageNo;
            GetWindowText(win->hwndPageBox, buf, dimof(buf));
            newPageNo = _ttoi(buf);
            if (win->dm->validPageNo(newPageNo)) {
                win->dm->goToPage(newPageNo, 0);
                SetFocus(win->hwndFrame);
            }
            return 1;
        }
        else if (VK_ESCAPE == wParam || VK_TAB == wParam) {
            SetFocus(win->hwndFrame);
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
    } else if (WM_SETFOCUS == message) {
        win->hwndTracker = NULL;
    } else if (WM_KEYDOWN == message) {
        OnKeydown(win, wParam, lParam, true);
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, message, wParam, lParam);
}

#define PAGE_BOX_WIDTH 40
static void UpdateToolbarPageText(WindowInfo *win, int pageCount)
{
    const TCHAR *text = _TR("Page:");
    win_set_text(win->hwndPageText, text);
    SIZE size = TextSizeInHwnd(win->hwndPageText, text);
    size.cx += 6;

    RECT pageWndRect;
    GetWindowRect(win->hwndPageBox, &pageWndRect);
    int pageWndDy = rect_dy(&pageWndRect);

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDM_OPEN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - pageWndDy) / 2;

    TCHAR buf[256];
    if (0 == pageCount) {
        buf[0] = 0;
    } else if (-1 == pageCount) {
        GetWindowText(win->hwndPageTotal, buf, sizeof(buf));
    } else {
        StringCchPrintf(buf, dimof(buf), _T(" / %d"), pageCount);
    }
    win_set_text(win->hwndPageTotal, buf);
    SIZE size2 = TextSizeInHwnd(win->hwndPageTotal, buf);
    size2.cx += 6;

    MoveWindow(win->hwndPageText, pos_x, (pageWndDy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    MoveWindow(win->hwndPageBox, pos_x + size.cx, pos_y, PAGE_BOX_WIDTH, pageWndDy, false);
    MoveWindow(win->hwndPageTotal, pos_x + size.cx + PAGE_BOX_WIDTH, (pageWndDy - size.cy + 1) / 2 + pos_y, size2.cx, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = size.cx + PAGE_BOX_WIDTH + size2.cx + 12;
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
}

static void CreatePageBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND page = CreateWindowEx(WS_EX_STATICEDGE, WC_EDIT, _T("0"),
                            WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                            0, 1, PAGE_BOX_WIDTH, 20, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND total = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HFONT fnt = (HFONT)GetStockObject(DEFAULT_GUI_FONT);  // TODO: this might not work on win95/98
    SetWindowFont(label, fnt, true);
    SetWindowFont(page, fnt, true);
    SetWindowFont(total, fnt, true);

    if (!DefWndProcPageBox)
        DefWndProcPageBox = (WNDPROC)GetWindowLong(page, GWL_WNDPROC);
    SetWindowLong(page, GWL_WNDPROC, (LONG)WndProcPageBox);

    win->hwndPageText = label;
    win->hwndPageBox = page;
    win->hwndPageTotal = total;

    UpdateToolbarPageText(win, -1);
}

static HBITMAP LoadExternalBitmap(HINSTANCE hInst, TCHAR * filename, INT resourceId)
{
    TCHAR * path = AppGenDataFilename(filename);
    
    HBITMAP hBmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hBmp)
        hBmp = LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));

    free(path);
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

    HBITMAP hbmp = LoadExternalBitmap(hInst, _T("toolbar_8.bmp"), IDB_TOOLBAR);
    BITMAP bmp;
    GetObject(hbmp, sizeof(BITMAP), &bmp);
    // Assume square icons
    himl = ImageList_Create(bmp.bmHeight, bmp.bmHeight, ILC_COLORDDB | ILC_MASK, 0, 0);
    ImageList_AddMasked(himl, hbmp, RGB(255, 0, 255));
    DeleteObject(hbmp);

    for (size_t i=0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        tbButtons[i] = TbButtonFromButtonInfo(i);
        if (gToolbarButtons[i].cmdId == IDM_FIND_MATCH) {
            tbButtons[i].fsStyle = BTNS_CHECK;
        }
    }
    lres = SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    // TODO: construct disabled image list as well?
    //SendMessage(hwndToolbar, TB_SETDISABLEDIMAGELIST, 0, (LPARAM)himl);

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
    GetWindowRect(win->hwndReBar, &rc);
    gReBarDy = rect_dy(&rc);
    //TODO: this was inherited but doesn't seem to be right (makes toolbar
    // partially unpainted if using classic scheme on xp or vista
    //gReBarDyFrame = bIsAppThemed ? 0 : 2;
    gReBarDyFrame = 0;
    
    CreatePageBox(win, hInst);
    CreateFindBox(win, hInst);
}

static LRESULT CALLBACK WndProcSpliter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static POINT cur;
    static bool resizing = false;
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);

    switch (message)
    {
        case WM_MOUSEMOVE:
            if (resizing) {
                short dx, ty = 0, tw;
                POINT pcur;

                GetCursorPos(&pcur);
                dx = (short)(pcur.x - cur.x);
                cur = pcur;

                RECT r;
                GetWindowRect(win->hwndTocBox, &r);
                tw = rect_dx(&r) + dx;
                if (tw <= DEF_PAGE_DX / 4) break;

                GetClientRect(win->hwndFrame, &r);
                int width = rect_dx(&r) - tw - SPLITTER_DX;
                int height = rect_dy(&r);

                if (gGlobalPrefs.m_showToolbar && !win->fullScreen) {
                    ty = gReBarDy + gReBarDyFrame;
                    height -= ty;
                }

                MoveWindow(win->hwndTocBox, 0, ty, tw, height, true);
                MoveWindow(win->hwndCanvas, tw + SPLITTER_DX, ty, width, height, true);
                MoveWindow(hwnd, tw, ty, SPLITTER_DX, height, true);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            GetCursorPos(&cur);
            resizing = true;
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            resizing = false;
            break;
        case WM_COMMAND:
            if (HIWORD(wParam) == STN_CLICKED)
                win->ToggleTocBox();
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void WindowInfo::FindStart()
{
    hwndTracker = NULL;
    SendMessage(hwndFindBox, EM_SETSEL, 0, -1);
    SetFocus(hwndFindBox);
}

void WindowInfo::FindUpdateStatus(int current, int total)
{
    if (!findStatusVisible) {
        WindowInfo_ShowFindStatus(this);
    }

    TCHAR buf[256];
    wsprintf(buf, _TR("Searching %d of %d..."), current, total);
    win_set_text(hwndFindStatus, buf);

    findPercent = current * 100 / total;

    MSG msg = { 0 };
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowInfo::TrackMouse(HWND tracker)
{
    if (!tracker)
        tracker = hwndCanvas;
    else
    if (hwndFrame != GetActiveWindow() || hwndFindBox == GetFocus() || hwndPageBox == GetFocus() || hwndTracker == tracker)
        return;

    TRACKMOUSEEVENT tme = { sizeof(tme) };
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwndTracker = tracker;
    TrackMouseEvent(&tme);
    if (tracker == hwndCanvas)
        SetFocus(hwndFrame);
    else
        SetFocus(hwndTocBox);
}

static WNDPROC DefWndProcTocBox = NULL;
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    switch (message) {
        case WM_MOUSELEAVE:
            win->hwndTracker = NULL;
            return 0;
        case WM_MOUSEMOVE:
            win->TrackMouse(hwnd);
            break;
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.m_escToExit)
                DestroyWindow(win->hwndFrame);
            break;
    }
    return CallWindowProc(DefWndProcTocBox, hwnd, message, wParam, lParam);
}

static void CreateTocBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND spliter = CreateWindow(SPLITER_CLASS_NAME, _T(""), WS_CHILDWINDOW, 0, 0, 0, 0,
                                win->hwndFrame, (HMENU)0, hInst, NULL);
    win->hwndSpliter = spliter;
    
    HWND closeToc = CreateWindow(WC_STATIC, _T(""),
                        SS_BITMAP | SS_CENTERIMAGE | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                        0, 0, 5, 9, spliter, (HMENU)0, hInst, NULL);
    SendMessage(closeToc, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)gBitmapCloseToc);
    SetClassLong(closeToc, GCL_HCURSOR, (LONG)gCursorHand);

    win->hwndTocBox = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("TOC"),
                        TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                        TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_INFOTIP|TVS_FULLROWSELECT|
                        WS_TABSTOP|WS_CHILD,
                        0,0,0,0, win->hwndFrame, (HMENU)IDC_PDF_TOC_TREE, hInst, NULL);

    assert(win->hwndTocBox);
    if (!win->hwndTocBox)
        SeeLastError();
    else
        TreeView_SetUnicodeFormat(win->hwndTocBox, true);
        
    if (NULL == DefWndProcTocBox)
        DefWndProcTocBox = (WNDPROC)GetWindowLong(win->hwndTocBox, GWL_WNDPROC);
    SetWindowLong(win->hwndTocBox, GWL_WNDPROC, (LONG)WndProcTocBox);
}

static HTREEITEM AddTocItemToView(HWND hwnd, PdfTocItem *entry, HTREEITEM parent)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = (HTREEITEM)parent;
    tvinsert.hInsertAfter = TVI_LAST;
    if (parent == NULL) {
        tvinsert.itemex.state = TVIS_EXPANDED;
        tvinsert.itemex.stateMask = TVIS_EXPANDED;
    }
    else {
        tvinsert.itemex.state = 0;
        tvinsert.itemex.stateMask = 0;
    }
    tvinsert.itemex.mask = TVIF_TEXT|TVIF_PARAM|TVIF_STATE;
    tvinsert.itemex.lParam = (LPARAM)entry->link;
    // Replace unprintable whitespace with regular spaces
    tstr_trans_chars(entry->title, _T("\t\n\v\f\r"), _T("     "));
    tvinsert.itemex.pszText = entry->title;
    
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void PopluateTocTreeView(HWND hwnd, PdfTocItem *entry, HTREEITEM parent = NULL)
{
    while (entry) {
        HTREEITEM node = AddTocItemToView(hwnd, entry, parent);
        PopluateTocTreeView(hwnd, entry->child, node);
        entry = entry->next;
    }
}

void WindowInfo::LoadTocTree()
{
    if (tocLoaded)
        return;

    PdfTocItem *toc = dm->getTocTree();
    if (toc) {
        PopluateTocTreeView(hwndTocBox, toc);
        delete toc;
    }
    tocLoaded = true;
}

void WindowInfo::ToggleTocBox()
{
    if (!dm)
        return;
    if (!dm->_showToc)
        ShowTocBox();
    else
        HideTocBox();
    MenuUpdateBookmarksStateForWindow(this);
}

void WindowInfo::ShowTocBox()
{
    if (!dm->hasTocTree())
        goto Exit;

    LoadTocTree();

    RECT rtoc, rframe;
    int cw, ch, cx, cy;

    GetClientRect(hwndFrame, &rframe);
    GetWindowRect(hwndTocBox, &rtoc);

    if (gGlobalPrefs.m_showToolbar && !fullScreen)
        cy = gReBarDy + gReBarDyFrame;
    else
        cy = 0;
    ch = rect_dy(&rframe) - cy;

    cx = rect_dx(&rtoc);
    if (cx == 0) // first time
        cx = rect_dx(&rframe) / 4;
    cw = rect_dx(&rframe) - cx - SPLITTER_DX;

    SetWindowPos(hwndTocBox, NULL, 0, cy, cx, ch, SWP_NOZORDER|SWP_SHOWWINDOW);
    SetWindowPos(hwndSpliter, NULL, cx, cy, SPLITTER_DX, ch, SWP_NOZORDER|SWP_SHOWWINDOW);
    SetWindowPos(hwndCanvas, NULL, cx + SPLITTER_DX, cy, cw, ch, SWP_NOZORDER|SWP_SHOWWINDOW);
Exit:
    dm->_showToc = TRUE;
}

void WindowInfo::HideTocBox()
{
    RECT r;
    GetClientRect(hwndFrame, &r);

    int cy = 0;
    int cw = rect_dx(&r), ch = rect_dy(&r);

    if (gGlobalPrefs.m_showToolbar && !fullScreen)
        cy = gReBarDy + gReBarDyFrame;

    SetWindowPos(hwndCanvas, NULL, 0, cy, cw, ch - cy, SWP_NOZORDER);
    ShowWindow(hwndTocBox, SW_HIDE);
    ShowWindow(hwndSpliter, SW_HIDE);

    dm->_showToc = FALSE;
}

void WindowInfo::ClearTocBox()
{
    if (!tocLoaded) return;
    TreeView_DeleteAllItems(hwndTocBox);
    tocLoaded = false;
}

static LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const TCHAR * url;
    POINT pt;

    switch (message)
    {
        case WM_CREATE:
            assert(!gHwndAbout);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                url = AboutGetLink(NULL, pt.x, pt.y);
                if (url) {
                    SetCursor(gCursorHand);
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_LBUTTONDOWN:
            url = AboutGetLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            SetWindowLong(hwnd, GWL_USERDATA, (LONG)url);
            break;

        case WM_LBUTTONUP:
            url = AboutGetLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (url && url == (const TCHAR *)GetWindowLong(hwnd, GWL_USERDATA))
                LaunchBrowser(url);
            SetWindowLong(hwnd, GWL_USERDATA, 0);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            assert(gHwndAbout);
            gHwndAbout = NULL;
            break;

        /* TODO: handle mouse move/down/up so that links work */

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

/* TODO: gAccumDelta must be per WindowInfo */
static int      gDeltaPerLine, gAccumDelta;      // for mouse wheel logic

static LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *    win;
    win = WindowInfo_FindByHwnd(hwnd);
    switch (message)
    {
        case WM_APP_REPAINT_DELAYED:
            if (win)
                SetTimer(hwnd, REPAINT_TIMER_ID, REPAINT_DELAY_IN_MS, NULL);
            break;

        case WM_APP_REPAINT_NOW:
            if (win)
                WindowInfo_RedrawAll(win);
            break;

        case WM_VSCROLL:
            OnVScroll(win, wParam);
            return WM_VSCROLL_HANDLED;

        case WM_HSCROLL:
            OnHScroll(win, wParam);
            return WM_HSCROLL_HANDLED;

        case WM_MOUSELEAVE:
            win->hwndTracker = NULL;
            return 0;

        case WM_MOUSEMOVE:
            win->TrackMouse(hwnd);
            if (win)
                OnMouseMove(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDBLCLK:
            if (win)
                OnMouseLeftButtonDblClk(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDOWN:
            if (win)
                OnMouseLeftButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONUP:
            if (win)
                OnMouseLeftButtonUp(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_MBUTTONDOWN:
            if (win && WS_SHOWING_PDF == win->state)
            {
                SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);
                // TODO: Create window that shows location of initial click for reference
                OnMouseMiddleButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;

        case WM_SETCURSOR:
            if (win && WS_ABOUT == win->state) {
                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    const TCHAR * url = AboutGetLink(win, pt.x, pt.y);
                    if (url) {
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
            } else if (win && MA_DRAGGING == win->mouseAction) {
                SetCursor(gCursorDrag);
                return TRUE;
            } else if (win && MA_SCROLLING == win->mouseAction) {
                SetCursor(gCursorScroll);
                return TRUE;
            } else if (win && WS_SHOWING_PDF == win->state) {
                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    PdfLink * link = win->dm->linkAtPosition(pt.x, pt.y);
                    if (link) {
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
            }
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_TIMER:
            assert(win);
            if (win) {
                if (REPAINT_TIMER_ID == wParam)
                    WindowInfo_RedrawAll(win);
                else if (SMOOTHSCROLL_TIMER_ID == wParam) {
                    if (MA_SCROLLING == win->mouseAction)
                        WinMoveDocBy(win, win->xScrollSpeed, win->yScrollSpeed);
                    else
                    {
                        KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
                        win->yScrollSpeed = 0;
                        win->xScrollSpeed = 0;
                    }
                }
                else
                    AnimState_NextFrame(&win->animState);
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
                /* blindly kill the timer, just in case it's there */
                KillTimer(hwnd, REPAINT_TIMER_ID);
                OnPaint(win);
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int             wmId, wmEvent;
    WindowInfo *    win;
    ULONG           ulScrollLines;                   // for mouse wheel logic
    const TCHAR *   fileName;

    win = WindowInfo_FindByHwnd(hwnd);

    switch (message)
    {
        case WM_CREATE:
            // do nothing
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win) {
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

        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);

            fileName = RecentFileNameFromMenuItemId(wmId);
            if (fileName) {
                LoadPdf(fileName, win);
                free((void*)fileName);
                break;
            }

            switch (wmId)
            {
                case IDM_OPEN:
                case IDT_FILE_OPEN:
                    if (!gRestrictedUse)
                        OnMenuOpen(win);
                    break;
                case IDM_SAVEAS:
                    if (!gRestrictedUse)
                        OnMenuSaveAs(win);
                    break;

                case IDT_FILE_PRINT:
                case IDM_PRINT:
                    if (!gRestrictedUse)
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
                    WindowInfo_Refresh(win, true);
                    break;

                case IDT_VIEW_ZOOMIN:
                    if (win->dm)
                        win->dm->zoomBy(ZOOM_IN_FACTOR);
                    break;

                case IDT_VIEW_ZOOMOUT:
                    if (win->dm)
                        win->dm->zoomBy(ZOOM_OUT_FACTOR);
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
                case IDM_ZOOM_ACTUAL_SIZE:
                    OnMenuZoom(win, (UINT)wmId);
                    break;

                case IDM_VIEW_SINGLE_PAGE:
                    OnMenuViewSinglePage(win);
                    break;

                case IDM_VIEW_FACING:
                    OnMenuViewFacing(win);
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

                case IDM_VIEW_FULLSCREEN:
                    OnMenuViewFullscreen(win);
                    break;

                case IDM_VIEW_CONTINUOUS_FACING:
                    OnMenuViewContinuousFacing(win);
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
                    OnMenuContributeTranslation();
                    break;

                case IDM_ABOUT:
                    OnMenuAbout();
                    break;

                case IDM_CHECK_UPDATE:
                    OnMenuCheckUpdate(win);
                    break;

                case IDM_SETTINGS:
                    if (!gRestrictedUse)
                        OnMenuSettings(win);
                    break;

                case IDM_VIEW_WITH_ACROBAT:
                    if (!gRestrictedUse)
                        ViewWithAcrobat(win);
                    break;

                case IDM_MOVE_FRAME_FOCUS:
                    if (win->hwndFrame != GetFocus())
                        SetFocus(win->hwndFrame);
                    else if (win->dm && win->dm->_showToc)
                        SetFocus(win->hwndTocBox);
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_CHAR:
            if (win)
                OnChar(win, wParam);
            break;

        case WM_KEYDOWN:
            if (win)
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

        // TODO: I don't understand why WndProcCanvas() doesn't receive this message
        case WM_MOUSEWHEEL:
            if (!win || !win->dm) /* TODO: check for pdfDoc as well ? */
                break;

            if (LOWORD(wParam) == MK_CONTROL)
            {
                if ((short)HIWORD(wParam) < 0)
                    win->dm->zoomBy(ZOOM_OUT_FACTOR);
                else
                    win->dm->zoomBy(ZOOM_IN_FACTOR);
                return 0;
            }

            if (gDeltaPerLine == 0)
               break;

            if (DM_SINGLE_PAGE == win->dm->displayMode()) {
                if ((short) HIWORD (wParam) > 0)
                    win->dm->goToPrevPage(0);
                else
                    win->dm->goToNextPage(0);
                return 0;
            }

            gAccumDelta += (short) HIWORD (wParam);     // 120 or -120

            while (gAccumDelta >= gDeltaPerLine)
            {
                SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
                gAccumDelta -= gDeltaPerLine;
            }

            while (gAccumDelta <= -gDeltaPerLine)
            {
                SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
                gAccumDelta += gDeltaPerLine;
            }
            return 0;

        case WM_DESTROY:
            /* WM_DESTROY might be sent as a result of File\Close, in which case CloseWindow() has already been called */
            if (win)
                CloseWindow(win, TRUE);
            break;

        case WM_DDE_INITIATE:
            return OnDDEInitiate(hwnd, wParam, lParam);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wParam, lParam);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wParam, lParam);

        case MSG_BENCH_NEXT_ACTION:
            if (win)
                OnBenchNextAction(win);
            break;

        case WM_APP_URL_DOWNLOADED:
            assert(win);
            if (win)
                OnUrlDownloaded(win, (HttpReqCtx*)wParam);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_PDF_TOC_TREE) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
                switch (pnmtv->hdr.code) 
                {
                    case TVN_SELCHANGEDW: 
                    {
                        // When the focus is set to the toc window the first item in the treeview is automatically
                        // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action == 0x00001000.
                        // We have to ignore this message to prevent the current page to be changed.
                        if (pnmtv->action==TVC_BYKEYBOARD || pnmtv->action==TVC_BYMOUSE) {
                            if (win->dm && pnmtv->itemNew.lParam)
                                win->dm->goToTocLink((void *)pnmtv->itemNew.lParam);
                        }
                        // The case pnmtv->action==TVC_UNKNOWN is ignored because 
                        // it corresponds to a notification sent by
                        // the function TreeView_DeleteAllItems after deletion of the item.
                    }
                    break;
                    case TVN_KEYDOWN: {
                        TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)lParam;
                        if (VK_TAB == ptvkd->wVKey) {
                            SetFocus(win->hwndFrame);
                            return 1;
                        }
                    }
                    break;
                }
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcFrame;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = FRAME_CLASS_NAME;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = WndProcCanvas;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CANVAS_CLASS_NAME;
    wcex.hIconSm        = 0;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcAbout;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = ABOUT_CLASS_NAME;
    wcex.hIconSm        = 0;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcSpliter;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = SPLITER_CLASS_NAME;
    wcex.hIconSm        = 0;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcFindStatus;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_APPSTARTING);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = FINDSTATUS_CLASS_NAME;
    wcex.hIconSm        = 0;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    return TRUE;
}

#define IDC_HAND            MAKEINTRESOURCE(32649)
static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;

    gCursorArrow = LoadCursor(NULL, IDC_ARROW);
    gCursorIBeam = LoadCursor(NULL, IDC_IBEAM);
    gCursorHand  = LoadCursor(NULL, IDC_HAND); // apparently only available if WINVER >= 0x0500
    gCursorScroll = LoadCursor(NULL, IDC_SIZEALL);
    if (!gCursorHand)
        gCursorHand = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gCursorDrag  = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gBrushBg     = CreateSolidBrush(COL_WINDOW_BG);
    gBrushWhite  = CreateSolidBrush(COL_WHITE);
    gBrushShadow = CreateSolidBrush(COL_WINDOW_SHADOW);
    gBrushLinkDebug = CreateSolidBrush(RGB(0x00,0x00,0xff));
    
    gBitmapCloseToc = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_CLOSE_TOC));

    return TRUE;
}

static WStrList *WStrList_FromCmdLine(TCHAR *cmdLine, bool addExe=false)
{
    TCHAR *      exePath;
    WStrList *   strList = NULL;
    TCHAR *      txt;

    assert(cmdLine);

    if (!cmdLine)
        return NULL;

    if (addExe)
    {
        exePath = ExePathGet();
        if (!exePath)
            return NULL;
        if (!WStrList_InsertAndOwn(&strList, exePath)) {
            free((void*)exePath);
            return NULL;
        }
    }

    for (;;) {
        txt = wstr_parse_possibly_quoted(&cmdLine);
        if (!txt)
            break;
        if (!WStrList_InsertAndOwn(&strList, txt)) {
            free((void*)txt);
            break;
        }
    }
    WStrList_Reverse(&strList);
    return strList;
}

static void u_DoAllTests(void)
{
#ifdef DEBUG
    DBG_OUT("Running tests\n");
    u_RectI_Intersect();
    u_testMemSegment();
    u_hexstr();
#else
    printf("Not running tests\n");
#endif
}

#define CONSERVE_MEMORY 1

static DWORD WINAPI PageRenderThread(PVOID data)
{
    PageRenderRequest   req;
    RenderedBitmap *    bmp;

    DBG_OUT("PageRenderThread() started\n");
    while (1) {
        //DBG_OUT("Worker: wait\n");
        LockCache();
        gCurPageRenderReq = NULL;
        int count = gPageRenderRequestsCount;
        UnlockCache();
        if (0 == count) {
            HANDLE handles[2] = { gPageRenderSem, gPageRenderClearQueue };
            DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            // Is it a page render request?
            if (WAIT_OBJECT_0 == waitResult) {
            }
            // is it a 'clear requests' request?
            else if (WAIT_OBJECT_0+1 == waitResult) {
                RenderQueue_Clear();
                // Signal that the queue is cleared
                SetEvent(gPageRenderQueueCleared);
            }
            else {
                DBG_OUT("  WaitForSingleObject() failed\n");
                continue;
            }
        }
        if (0 == gPageRenderRequestsCount) {
            continue;
        }
        LockCache();
        RenderQueue_Pop(&req);
        gCurPageRenderReq = &req;
        UnlockCache();
        DBG_OUT("PageRenderThread(): dequeued %d\n", req.pageNo);
        if (!req.dm->pageVisibleNearby(req.pageNo)) {
            DBG_OUT("PageRenderThread(): not rendering because not visible\n");
            continue;
        }
        assert(!req.abort);
        MsTimer renderTimer;
        bmp = req.dm->renderBitmap(req.pageNo, req.zoomLevel, req.rotation, NULL, pageRenderAbortCb, (void*)&req);
        renderTimer.stop();
        LockCache();
        gCurPageRenderReq = NULL;
        UnlockCache();
        if (req.abort) {
            delete bmp;
            continue;
        }
        if (bmp)
            DBG_OUT("PageRenderThread(): finished rendering %d\n", req.pageNo);
        else
            DBG_OUT("PageRenderThread(): failed to render a bitmap of page %d\n", req.pageNo);
        double renderTime = renderTimer.timeInMs();
        BitmapCache_Add(req.dm, req.pageNo, req.zoomLevel, req.rotation, bmp, renderTime);
#ifdef CONSERVE_MEMORY
        BitmapCache_FreeNotVisible();
#endif
        WindowInfo* win = (WindowInfo*)req.dm->appData();
        triggerRepaintDisplayNow(win);
    }
    DBG_OUT("PageRenderThread() finished\n");
    return 0;
}

static void CreatePageRenderThread(void)
{
    LONG semMaxCount = 1000; /* don't really know what the limit should be */
    DWORD dwThread1ID = 0;
    assert(NULL == gPageRenderThreadHandle);

    gPageRenderSem = CreateSemaphore(NULL, 0, semMaxCount, NULL);
    gPageRenderClearQueue = CreateEvent(NULL, FALSE, FALSE, NULL);
    gPageRenderQueueCleared = CreateEvent(NULL, FALSE, FALSE, NULL);
    gPageRenderThreadHandle = CreateThread(NULL, 0, PageRenderThread, (void*)NULL, 0, &dwThread1ID);
    assert(NULL != gPageRenderThreadHandle);
}

static void FreePageRenderThread(void)
{
    CloseHandle(gPageRenderThreadHandle);
    CloseHandle(gPageRenderQueueCleared);
    CloseHandle(gPageRenderClearQueue);
    CloseHandle(gPageRenderSem);
}

static void PrintFile(WindowInfo *win, const TCHAR *printerName)
{
    TCHAR       devstring[256];      // array for WIN.INI data 
    HANDLE      printer;
    LPDEVMODE   devMode = NULL;
    DWORD       structSize, returnCode;

    if (!win->dm->pdfEngine->printingAllowed()) {
        MessageBox(win->hwndFrame, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return;
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
        MessageBox(win->hwndFrame, _T("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return;
    }
    
    BOOL fOk = OpenPrinter((LPTSTR)printerName, &printer, NULL);
    if (!fOk) {
        MessageBox(win->hwndFrame, _TR("Could not open Printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        return;
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
        MessageBox(win->hwndFrame, _T("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    PdfPageInfo * pageInfo = pageInfo = win->dm->getPageInfo(1);

    if (pageInfo->bitmapDx > pageInfo->bitmapDy) {
        devMode->dmOrientation = DMORIENT_LANDSCAPE;
    } else {
        devMode->dmOrientation = DMORIENT_PORTRAIT;
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
        MessageBox(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }
    PRINTPAGERANGE pr;
    pr.nFromPage =1;
    pr.nToPage =win->dm->pageCount();
    if (CheckPrinterStretchDibSupport(win->hwndFrame, hdcPrint))
        PrintToDevice(win->dm, hdcPrint, devMode, 1, &pr );
Exit:
    free(devMode);
    DeleteDC(hdcPrint);
}

static void EnumeratePrinters()
{
    PRINTER_INFO_5 *info5Arr = NULL;
    DWORD bufSize = 0, printersCount;
    BOOL fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
        5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    if (!fOk) {
        info5Arr = (PRINTER_INFO_5 *)malloc(bufSize);
        fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
        5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    }
    if (!info5Arr)
        return;
    assert(fOk);
    if (!fOk) return;
    printf("Printers: %ld\n", printersCount);
    for (DWORD i=0; i < printersCount; i++) {
        const TCHAR *printerName = info5Arr[i].pPrinterName;
        const TCHAR *printerPort = info5Arr[i].pPortName;
        bool fDefault = false;
        if (info5Arr[i].Attributes & PRINTER_ATTRIBUTE_DEFAULT)
            fDefault = true;
        _tprintf(_T("Name: %s, port: %s, default: %d\n"), printerName, printerPort, (int)fDefault);
    }
    TCHAR buf[512];
    bufSize = dimof(buf);
    fOk = GetDefaultPrinter(buf, &bufSize);
    if (!fOk) {
        if (ERROR_FILE_NOT_FOUND == GetLastError())
            printf("No default printer\n");
    }
    free(info5Arr);
}

/* Get the name of default printer or NULL if not exists.
   The caller needs to free() the result */
TCHAR *GetDefaultPrinterName()
{
    TCHAR buf[512];
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize))
        return tstr_dup(buf);
    return NULL;
}

#define is_arg(txt) tstr_ieq(_T(##txt), currArg->str)

/* Parse 'txt' as hex color and set it as background color */
static void ParseBgColor(const TCHAR* txt)
{
    if (tstr_startswith(txt, _T("0x")))
        txt += 2;
    else if (tstr_startswith(txt, _T("#")))
        txt += 1;
    int r = hex_wstr_decode_byte(&txt);
    if (-1 == r)
        return;
    int g = hex_wstr_decode_byte(&txt);
    if (-1 == g)
        return;
    int b = hex_wstr_decode_byte(&txt);
    if (-1 == b)
        return;
    if (*txt)
        return;
    int col = RGB(r,g,b);
    gGlobalPrefs.m_bgColor = col;
}

HDDEDATA CALLBACK DdeCallback(UINT uType,
    UINT uFmt,
    HCONV hconv,
    HSZ hsz1,
    HSZ hsz2,
    HDDEDATA hdata,
    ULONG_PTR dwData1,
    ULONG_PTR dwData2)
{
  return (0);
}

void DDEExecute (LPCTSTR server, LPCTSTR topic, LPCTSTR command)
{
    DBG_OUT("DDEExecute(\"%s\",\"%s\",\"%s\")", server, topic, command);
    unsigned long inst = 0;
    HSZ hszServer = NULL, hszTopic = NULL;
    HCONV hconv = NULL;
    HDDEDATA hddedata = NULL;
#ifdef UNICODE
    int codepage = CP_WINUNICODE;
    int dataFormat = CF_UNICODETEXT;
#else
    int codepage = CP_WINANSI;
    int dataFormat = CF_TEXT;
#endif

    UINT result = DdeInitialize(&inst, &DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        DBG_OUT("DDE communication could not be initiated %d.", result);
        goto exit;
    }
    hszServer = DdeCreateStringHandle(inst, server, codepage);
    if (hszServer == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hszTopic = DdeCreateStringHandle(inst, topic, codepage);
    if (hszTopic == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, 0);
    if (hconv == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hddedata = DdeCreateDataHandle(inst, (BYTE*)command, (tstr_len(command) + 1) * sizeof(TCHAR), 0, 0, dataFormat, 0);
    if (hddedata == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
    }
    if (DdeClientTransaction((BYTE*)hddedata, (DWORD)-1, hconv, 0, 0, XTYP_EXECUTE, 10000, 0) == 0) {
        DBG_OUT("DDE transaction failed %u.", DdeGetLastError(inst));
    }
exit:
    DdeFreeDataHandle(hddedata);
    DdeDisconnect(hconv);
    DdeFreeStringHandle(inst, hszTopic);
    DdeFreeStringHandle(inst, hszServer);
    DdeUninitialize(inst);
}

extern "C" void pdf_destoryfontlistMS(); // in pdf_fontfilems.c

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WStrList *          argListRoot;
    WStrList *          currArg;
    TCHAR *             benchPageNumStr = NULL;
    MSG                 msg = {0};
    HACCEL              hAccelTable;
    WindowInfo*         win;
    bool                exitOnPrint = false;
    bool                printToDefaultPrinter = false;
    bool                printDialog = false;
    TCHAR *             destName = NULL;
    int                 pageNumber = 0;
    TCHAR *             cmdLine;
    bool                firstDocLoaded = false;

#ifdef _DEBUG
    // Memory leak detection
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    //_CrtSetBreakAlloc(421);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);

    u_DoAllTests();

    INITCOMMONCONTROLSEX cex;
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);

    SerializableGlobalPrefs_Init();

    cmdLine  = GetCommandLine();

    argListRoot = WStrList_FromCmdLine(cmdLine);
    assert(argListRoot);
    if (!argListRoot)
        return 0;

#ifdef BUILD_RM_VERSION
    bool prefsLoaded = false;
#else
    bool prefsLoaded = Prefs_Load();
#endif

    if (!prefsLoaded) {
        // assume that this is because prefs file didn't exist i.e. this is
        // the first time Sumatra is launched.
        GuessLanguage();
    }

    /* parse argument list. If -bench was given, then we're in benchmarking mode. Otherwise
    we assume that all arguments are PDF file names.
    -bench can be followed by file or directory name. If file, it can additionally be followed by
    a number which we interpret as page number */
#ifdef BUILD_RM_VERSION
    bool registerForPdfExtentions = false;
#else
    bool registerForPdfExtentions = true;
    if (IsRunningInPortableMode())
        registerForPdfExtentions = false;
#endif

    bool reuse_instance = false;
    currArg = argListRoot->next;
    TCHAR *printerName = NULL;
    TCHAR *newWindowTitle = NULL;
    while (currArg) {
        if (is_arg("-register-for-pdf")) {
            DoAssociateExeWithPdfExtension(true);
            return 0;
        }

        if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            goto Exit;
        }

        if (is_arg( "-no-register-ext")) {
            currArg = currArg->next;
            registerForPdfExtentions = false;
            continue;
        }

        if (is_arg("-bench")) {
            currArg = currArg->next;
            if (currArg) {
                gBenchFileName = currArg->str;
                if (currArg->next)
                    benchPageNumStr = currArg->next->str;
            }
            break;
        }

        if (is_arg("-exit-on-print")) {
            currArg = currArg->next;
            exitOnPrint = true;
            continue;
        }

        if (is_arg("-print-to-default")) {
            currArg = currArg->next;
            printToDefaultPrinter = true;
            continue;
        }

        if (is_arg("-print-to")) {
            currArg = currArg->next;
            if (currArg) {
                printerName = tstr_dup(currArg->str);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-print-dialog")) {
            currArg = currArg->next;
            printDialog = true;
            continue;
        }

        if (is_arg("-bgcolor")) {
            currArg = currArg->next;
            if (currArg) {
                ParseBgColor(currArg->str);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-inverse-search")) {
            currArg = currArg->next;
            if (currArg) {
                free(gGlobalPrefs.m_inverseSearchCmdLine);
                gGlobalPrefs.m_inverseSearchCmdLine = tstr_dup(currArg->str);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-esc-to-exit")) {
            currArg = currArg->next;
            gGlobalPrefs.m_escToExit = TRUE;
            continue;
        }

        if (is_arg("-reuse-instance")) {
            currArg = currArg->next;
            // find the window handle of a running instance of SumatraPDF
            // TODO: there should be a mutex here to reduce possibility of
            // race condition and having more than one copy launch because
            // FindWindow() in one process is called before a window is created
            // in another process
            reuse_instance = FindWindow(FRAME_CLASS_NAME, 0) != NULL;
            continue;
        }

        if (is_arg("-lang")) {
            currArg = currArg->next;
            if (currArg) {
                char * s = wstr_to_multibyte(currArg->str, CP_ACP);
                CurrLangNameSet(s);
                free(s);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-nameddest")) {
            currArg = currArg->next;
            if (currArg) {
                destName = tstr_dup(currArg->str);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-page")) {
            currArg = currArg->next;
            if (currArg) {
                pageNumber = _ttoi(currArg->str);
                currArg = currArg->next;
            }
            continue;
        }

        if (is_arg("-restrict")) {
            currArg = currArg->next;
            gRestrictedUse = true;
            registerForPdfExtentions = false;
            continue;
        }


        if (is_arg("-title")) {
            currArg = currArg->next;
            if (currArg) {
                newWindowTitle = tstr_dup(currArg->str); 
                currArg = currArg->next;
            }
            continue;
        }

#ifdef BUILD_RM_VERSION
        if (is_arg("-delete-these-on-close")) {
            currArg = currArg->next;
            gDeleteFileOnClose = true;
            continue;
        }
#endif

        // we assume that switches come first and file names to open later
        // TODO: it would probably be better to collect all non-switches
        // in a separate list so that file names can be interspersed with
        // switches
        break;
    }

    if (benchPageNumStr) {
        gBenchPageNum = _ttoi(benchPageNumStr);
        if (gBenchPageNum < 1)
            gBenchPageNum = INVALID_PAGE_NO;
    }

    LoadString(hInstance, IDS_APP_TITLE, gWindowTitle, MAX_LOADSTRING);
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SUMATRAPDF));

    CreatePageRenderThread();
    /* remaining arguments are names of PDF files */
#ifdef BUILD_RM_VERSION
    WStrList *currArgFileNames = currArg;
#endif
    if (NULL != gBenchFileName) {
            win = LoadPdf(gBenchFileName);
            if (win && WS_SHOWING_PDF == win->state)
                firstDocLoaded = true;
    } else {
        while (currArg) {
            if (reuse_instance) {
                // delegate file opening to a previously running instance by sending a DDE message 
                TCHAR command[2 * MAX_PATH + 20];
                wsprintf(command, _T("[") DDECOMMAND_OPEN _T("(\"%s\", 0, 1, 0)]"), currArg->str);
                DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
                if (destName && !firstDocLoaded) {
                    wsprintf(command, _T("[") DDECOMMAND_GOTO _T("(\"%s\", \"%s\")]"), currArg->str, destName);
                    DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
                }
                else if (pageNumber > 0 && !firstDocLoaded) {
                    wsprintf(command, _T("[") DDECOMMAND_PAGE _T("(\"%s\", %d)]"), currArg->str, pageNumber);
                    DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, command);
                }
            }
            else {
                bool showWin = !exitOnPrint;
                win = LoadPdf(currArg->str, NULL, showWin, newWindowTitle);
                if (!win)
                    goto Exit;
                if (WS_SHOWING_PDF != win->state) {
                    // cancel printing, if there was a load error
                    exitOnPrint = printToDefaultPrinter = printDialog = FALSE;
                    if (printerName) {
                        free(printerName);
                        printerName = NULL;
                    }
                }
                else if (destName && !firstDocLoaded) {
                    char * destNameA = wstr_to_multibyte(destName, CP_ACP);
                    win->dm->goToNamedDest(destNameA);
                    free(destNameA);
                }
                else if (pageNumber > 0 && !firstDocLoaded) {
                    if (win->dm->validPageNo(pageNumber))
                        win->dm->goToPage(pageNumber, 0);
                }
            }

            if (exitOnPrint)
                ShowWindow(win->hwndFrame, SW_HIDE);

            if (printToDefaultPrinter) {
                printerName = GetDefaultPrinterName();
                if (printerName)
                    PrintFile(win, printerName);
                free(printerName);
            } else if (printerName) {
                // note: this prints all of PDF files. Another option would be to
                // print only the first one
                PrintFile(win, printerName);
            } else if (printDialog) {
                OnMenuPrint(win);
            }
            firstDocLoaded = true;
            currArg = currArg->next;
        }
    }

    if (((printerName || printDialog) && exitOnPrint)
          || reuse_instance)
        goto Exit;
 
    if (!firstDocLoaded) {
        /* disable benchmark mode if we couldn't open file to benchmark */
        gBenchFileName = 0;
        win = WindowInfo_CreateEmpty();
        if (!win)
            goto Exit;

        if (gGlobalPrefs.m_windowState == WIN_STATE_FULLSCREEN)
            ShowWindow(win->hwndFrame, SW_MAXIMIZE);
        else if (gGlobalPrefs.m_windowState == WIN_STATE_MAXIMIZED)
            ShowWindow(win->hwndFrame, SW_MAXIMIZE);
        else
            ShowWindow(win->hwndFrame, SW_SHOW);
        UpdateWindow(win->hwndFrame);

        if (WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState)
            WindowInfo_EnterFullscreen(win);
    }

    if (IsBenchMode()) {
        assert(win);
        assert(firstDocLoaded);
        if (win)
            PostBenchNextAction(win->hwndFrame);
    }

    if (!firstDocLoaded)
        MenuToolbarUpdateStateForAllWindows();

    if (registerForPdfExtentions && win)
        RegisterForPdfExtentions(win->hwndFrame);

    if (gGlobalPrefs.m_enableAutoUpdate)
        DownloadSumatraUpdateInfo(gWindowList, true);

#ifdef THREAD_BASED_FILEWATCH
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Make sure to dispatch the accelerator to the correct window
        win = WindowInfo_FindByHwnd(msg.hwnd);
        if (!TranslateAccelerator(win ? win->hwndFrame : msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#else
    while (1) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
            if (GetMessage(&msg, NULL, 0, 0)) {
                // Make sure to dispatch the accelerator to the correct window
                win = WindowInfo_FindByHwnd(msg.hwnd);
                if (!TranslateAccelerator(win ? win->hwndFrame : msg.hwnd, hAccelTable, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
        else {
            WindowInfo_RefreshUpdatedFiles();
            Sleep(50); // TODO: why is it here?
        }
    }
#endif
    
Exit:
    free(destName);
    free(printerName);
  
    FreePageRenderThread();

    WindowInfoList_DeleteAll();
    FileHistoryList_Free(&gFileHistoryRoot);
    DeleteObject(gBrushBg);
    DeleteObject(gBrushWhite);
    DeleteObject(gBrushShadow);
    DeleteObject(gBrushLinkDebug);

    Translations_FreeData();
    CurrLangNameFree();
    WininetDeinit();
    SerializableGlobalPrefs_Deinit();

#ifdef BUILD_RM_VERSION
    if (gDeleteFileOnClose)
    {
        // Delete the files which where passed to the command line.
        // This only really makes sense if we are in restricted use.
        while (currArgFileNames)
        {
            TCHAR fullpath[MAX_PATH];
            GetFullPathName(currArgFileNames->str, dimof(fullpath), fullpath, NULL);

            int error = DeleteFile(fullpath);

            // Sumatra holds the lock on the file (open stream), it should have lost it by the time
            // we reach here, but sometimes it's a little slow, so loop around till we can do it.
            while (error != 0)
            {
                error = GetLastError();
                if (error == 32)
                    error = DeleteFile(fullpath);
                else
                    error = 0;
            }

            currArgFileNames = currArgFileNames->next;
        }
    }
#endif // BUILD_RM_VERSION

    pdf_destoryfontlistMS();

    WStrList_Destroy(&argListRoot);
    //histDump();
    return (int) msg.wParam;
}

