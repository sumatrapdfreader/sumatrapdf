/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"

#include "AppPrefs.h"
#include "SumatraDialogs.h"
#include "FileHistory.h"

#include "WinUtil.hpp"

#include "Version.h"

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST           IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST            IDM_VIEW_CONTINUOUS
#define IDM_ZOOM_FIRST                  IDM_ZOOM_FIT_PAGE
#define IDM_ZOOM_LAST                   IDM_ZOOM_CUSTOM

// this sucks but I don't know any other way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

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
#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
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
#define WM_APP_FIND_UPDATE     (WM_APP + 13)
#define WM_APP_FIND_END        (WM_APP + 14)
#define WM_APP_REPAINT_TOC     (WM_APP + 15)

#ifdef SVN_PRE_RELEASE_VER
#define ABOUT_BG_COLOR          RGB(255,0,0)
#else
#define ABOUT_BG_COLOR          RGB(255,242,0)
#endif

#define COL_FWDSEARCH_BG        RGB(101,129,255)

#define FRAME_CLASS_NAME        _T("SUMATRA_PDF_FRAME")
#define CANVAS_CLASS_NAME       _T("SUMATRA_PDF_CANVAS")
#define SPLITER_CLASS_NAME      _T("Spliter")
#define FINDSTATUS_CLASS_NAME   _T("FindStatus")
#define PDF_DOC_NAME            _T("Adobe PDF Document")
#define PREFS_FILE_NAME         _T("sumatrapdfprefs.dat")

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO (612.0/792.0)

#define SPLITTER_DX  5
#define SPLITTER_MIN_WIDTH 150

#define REPAINT_TIMER_ID    1
/* Time to delay painting when going to a new page (prevents flickering) */
#define REPAINT_DELAY_IN_MS 200

#define SMOOTHSCROLL_TIMER_ID       2
#define SMOOTHSCROLL_DELAY_IN_MS    20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

#define FIND_STATUS_WIDTH       200 // Default width for the find status window
#define FIND_STATUS_MARGIN      8
#define FIND_STATUS_PROGRESS_HEIGHT 5

/* A special "pointer" vlaue indicating that we tried to render this bitmap
   but couldn't (e.g. due to lack of memory) */
#define BITMAP_CANNOT_RENDER (RenderedBitmap*)NULL

#define WS_REBAR (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | \
                  RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)

static FileHistoryList *            gFileHistoryRoot = NULL;

       HINSTANCE                    ghinst = NULL;
static TCHAR                        gWindowTitle[MAX_LOADSTRING];

static WindowInfo*                  gWindowList;

static HCURSOR                      gCursorArrow;
       HCURSOR                      gCursorHand;
static HCURSOR                      gCursorDrag;
static HCURSOR                      gCursorIBeam;
static HCURSOR                      gCursorScroll;
static HBRUSH                       gBrushBg;
static HBRUSH                       gBrushWhite;
static HBRUSH                       gBrushBlack;
static HBRUSH                       gBrushShadow;
static HFONT                        gDefaultGuiFont;
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
static bool                         gSuspendRendering = false;

static int                          gReBarDy;
static int                          gReBarDyFrame;

       bool                         gRestrictedUse = false;

SerializableGlobalPrefs             gGlobalPrefs = {
    TRUE, // BOOL m_showToolbar
    FALSE, // BOOL m_pdfAssociateDontAskAgain
    FALSE, // BOOL m_pdfAssociateShouldAssociate
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
    0, // int  m_fwdsearchOffset
    COL_FWDSEARCH_BG, // int  m_fwdsearchColor
    15, // int  m_fwdsearchWidth
    FALSE, // BOOL m_invertColors
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

static const char *g_currLangName = NULL;

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

static void Find(WindowInfo *win, PdfSearchDirection direction = FIND_FORWARD);
static void DeleteOldSelectionInfo(WindowInfo *win);
static void ClearSearch(WindowInfo *win);
static void WindowInfo_EnterFullscreen(WindowInfo *win, bool presentation=false);
static void WindowInfo_ExitFullscreen(WindowInfo *win);
static bool CanViewWithAcrobat(WindowInfo *win=NULL);
static bool CanSendAsEmailAttachment(WindowInfo *win=NULL);

#define SEP_ITEM "-----"

/* according to http://wiki.snap.com/index.php/User_talk:Snap, Serbian (latin) should 
   be sp-rs and Serbian (Cyrillic) should be sr-rs */
#include "LangMenuDef.h"

#define NEW_LANG_DETECTION

#ifdef NEW_LANG_DETECTION
#define _MAKELANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
// based on http://msdn.microsoft.com/en-us/library/dd318693%28VS.85%29.aspx
static struct {
    const char *lang;
    LANGID langId;
} g_lcidLangMap[] = {
    { "en", _MAKELANGID(LANG_ENGLISH) },
    { "af", _MAKELANGID(LANG_AFRIKAANS) },
    { "ar", _MAKELANGID(LANG_ARABIC) },
    { "eu", _MAKELANGID(LANG_BASQUE) },
    { "by", _MAKELANGID(LANG_BELARUSIAN) },
    { "bn", _MAKELANGID(LANG_BENGALI) },
    { "bg", _MAKELANGID(LANG_BULGARIAN) },
//  { "mm", Burmese },
    { "ca", _MAKELANGID(LANG_CATALAN) },
    { "cn", MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) },
    { "tw", MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL) },
    { "hr", _MAKELANGID(LANG_CROATIAN) },
    { "cz", _MAKELANGID(LANG_CZECH) },
    { "dk", _MAKELANGID(LANG_DANISH) },
    { "nl", _MAKELANGID(LANG_DUTCH) },
    { "fi", _MAKELANGID(LANG_FINNISH) },
    { "fr", _MAKELANGID(LANG_FRENCH) },
    { "gl", _MAKELANGID(LANG_GALICIAN) },
    { "de", _MAKELANGID(LANG_GERMAN) },
    { "gr", _MAKELANGID(LANG_GREEK) },
    { "he", _MAKELANGID(LANG_HEBREW) },
    { "hi", _MAKELANGID(LANG_HINDI) },
    { "hu", _MAKELANGID(LANG_HUNGARIAN) },
    { "id", _MAKELANGID(LANG_INDONESIAN) },
    { "ga", _MAKELANGID(LANG_IRISH) },
    { "it", _MAKELANGID(LANG_ITALIAN) },
    { "ja", _MAKELANGID(LANG_JAPANESE) },
    { "kr", _MAKELANGID(LANG_KOREAN) },
    { "lt", _MAKELANGID(LANG_LITHUANIAN) },
    { "mk", _MAKELANGID(LANG_MACEDONIAN) },
    { "ml", _MAKELANGID(LANG_MALAYALAM) },
    { "my", _MAKELANGID(LANG_MALAY) },
    { "no", MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL) },
    { "nn", MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK) },
    { "fa", _MAKELANGID(LANG_FARSI) },
    { "pl", _MAKELANGID(LANG_POLISH) },
    { "br", MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN) },
    { "pt", _MAKELANGID(LANG_PORTUGUESE) }, // SUBLANG_PORTUGUESE
    { "pa", _MAKELANGID(LANG_PUNJABI) },
    { "ro", _MAKELANGID(LANG_ROMANIAN) },
    { "ru", _MAKELANGID(LANG_RUSSIAN) },
    { "sp-rs", MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN) },
    { "sr-rs", MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC) },
    { "sk", _MAKELANGID(LANG_SLOVAK) },
    { "si", _MAKELANGID(LANG_SLOVENIAN) },
//  { "sn", Shona },
    { "es", _MAKELANGID(LANG_SPANISH) },
    { "sv", _MAKELANGID(LANG_SWEDISH) },
//  { "tl", Tagalog },
    { "ta", _MAKELANGID(LANG_TAMIL) },
    { "th", _MAKELANGID(LANG_THAI) },
    { "tr", _MAKELANGID(LANG_TURKISH) },
    { "uk", _MAKELANGID(LANG_UKRAINIAN) },
//  { "va", Valencian },
    { "vn", _MAKELANGID(LANG_VIETNAMESE) },
    { "cy", _MAKELANGID(LANG_WELSH) },
    { NULL, 0 }
};

static const char *GuessLanguage()
{
    LANGID langId = GetUserDefaultUILanguage();
    LANGID langIdNoSublang = _MAKELANGID(PRIMARYLANGID(langId));
    const char *langName = NULL;

    // Either find the exact primary/sub lang id match, or a neutral sublang if it exists
    // (don't return any sublang for a given language, it might be too different)
    for (int i = 0; g_lcidLangMap[i].lang; i++) {
        if (langId == g_lcidLangMap[i].langId) {
            langName = g_lcidLangMap[i].lang;
            break;
        }
        if (langIdNoSublang == g_lcidLangMap[i].langId)
            langName = g_lcidLangMap[i].lang;
            // continue searching after finding a match with a neutral sublanguage
    }

    return langName;
}
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

static const char *GuessLanguage()
{
    char langBuf[20];
    int res = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, langBuf, sizeof(langBuf));
    assert(0 != res);
    if (0 == res)
        return NULL;

    const char *langName;
    int i = 0;
    while ((langName = g_lcidLangMap[i++])) {
        const char *langLcid;
        while ((langLcid = g_lcidLangMap[i++])) {
            if (str_eq(langBuf, langLcid))
                return langName;
        }
    }
    return NULL;
}
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

#define KEY_PRESSED_MASK 0x8000
static bool WasKeyDown(int virtKey)
{
    SHORT state = GetKeyState(virtKey);
    if (KEY_PRESSED_MASK & state)
        return true;
    return false;
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
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf/prerelase.html")
#else
#define SVN_UPDATE_LINK         _T("http://blog.kowalczyk.info/software/sumatrapdf")
#endif
#endif

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

    const TCHAR *url = SUMATRA_UPDATE_INFO_URL _T("?v=") UPDATE_CHECK_VER;
    StartHttpDownload(url, hwndToNotify, WM_APP_URL_DOWNLOADED, autoCheck);

    free(gGlobalPrefs.m_lastUpdateTime);
    gGlobalPrefs.m_lastUpdateTime = GetSystemTimeAsStr();
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

static bool ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *buffer, DWORD bufLen)
{
    HKEY keyTmp = NULL;
    LONG res = RegOpenKeyEx(keySub, keyName, 0, KEY_READ, &keyTmp);

    if (ERROR_SUCCESS == res) {
        bufLen *= sizeof(TCHAR); // we need the buffer size in bytes not TCHARs
        res = RegQueryValueEx(keyTmp, valName, NULL, NULL, (BYTE *)buffer, &bufLen);
        RegCloseKey(keyTmp);
    }

    if (ERROR_SUCCESS != res)
        SeeLastError();
    return ERROR_SUCCESS == res;
}

static bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value)
{
    HKEY keyTmp = NULL;
    LONG res = RegCreateKeyEx(keySub, keyName, 0, NULL, 0, KEY_WRITE, NULL, &keyTmp, NULL);

    if (ERROR_SUCCESS == res) {
        res = RegSetValueEx(keyTmp, valName, 0, REG_SZ, (const BYTE*)value, (lstrlen(value)+1) * sizeof(TCHAR));
        RegCloseKey(keyTmp);
    }

    if (ERROR_SUCCESS != res)
        SeeLastError();
    return ERROR_SUCCESS == res;
}

// List of rules used to detect TeX editors.

// type of path information retrieved from the registy
typedef enum
{
    BinaryPath,         // full path to the editor's binary file
    BinaryDir,          // directory containing the editor's binary file
    SiblingPath,        // full path to a sibling file of the editor's binary file    
} EditorPathType;
typedef struct 
{
    PTSTR          Name;                // Editor name
    EditorPathType Type;                // Type of the path information obtained from the registry
    HKEY           RegRoot;             // Root of the regkey
    PTSTR          RegKey;              // Registry key path
    PTSTR          RegValue;            // Registry value name
    PTSTR          BinaryFilename;      // Editor's binary file name
    PTSTR          InverseSearchArgs;   // Parameters to be passed to the editor;
                                        // use placeholder '%f' for path to source file and '%l' for line number.
} EditorDetectionRules;
static EditorDetectionRules editor_rules[] =
{
    _T("WinEdt"),             BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\WinEdt.exe"), NULL,
                              _T("WinEdt.exe"), _T("\"[Open(|%f|);SelPar(%l,8)]\""),

    _T("WinEdt"),             BinaryDir, HKEY_CURRENT_USER, _T("Software\\WinEdt"), _T("Install Root"),
                              _T("WinEdt.exe"), _T("\"[Open(|%f|);SelPar(%l,8)]\""),

    _T("Notepad++"),          BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad++.exe"), NULL,
                              _T("WinEdt.exe"), _T("-n%l \"%f\""),

    _T("Notepad++"),          BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Notepad++"), NULL,
                              _T("notepad++.exe"), _T("-n%l \"%f\""),

    _T("Notepad++"),          BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Notepad++"), _T("DisplayIcon"),
                              _T("notepad++.exe"), _T("-n%l \"%f\""),

    _T("TeXnicCenter Alpha"), BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\ToolsCenter\\TeXnicCenterNT"), _T("AppPath"),
                              _T("TeXnicCenter.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter Alpha"), BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TeXnicCenter Alpha_is1"), _T("InstallLocation"),
                              _T("TeXnicCenter.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter"),       BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\ToolsCenter\\TeXnicCenter"), _T("AppPath"),
                              _T("TEXCNTR.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter"),       BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TeXnicCenter_is1"), _T("InstallLocation"),
                              _T("TEXCNTR.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("WinShell"),           BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinShell_is1"), _T("InstallLocation"),
                              _T("WinShell.exe"), _T("-c \"%f\" -l %l"),

    _T("Gvim"),               BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Vim\\Gvim"), _T("path"),
                              _T("gvim.exe"), _T("\"%f\" +%l"),
    
    // TODO: add this rule only if the latex-suite for ViM is installed (http://vim-latex.sourceforge.net/documentation/latex-suite.txt)
    _T("Gvim+latex-suite"),   BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Vim\\Gvim"), _T("path"),
                             _T("gvim.exe"), _T("-c \":RemoteOpen +%l %f\""),
                              
    _T("Texmaker"),           SiblingPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Texmaker"), _T("UninstallString"),
                              _T("texmaker.exe"), _T("\"%f\" -line %l"),

    // TODO: find a way to detect where emacs is installed
    //_T("ntEmacs"),            BinaryPath, HKEY_LOCAL_MACHINE, _T("???"), _T("???"),
    //                          _T("emacsclientw.exe"), _T("+%l \"%f\""),
};

// Detect TeX editors installed on the system and construct the
// corresponding inverse search commands.
//
// Parameters:
//      hwndCombo   -- (optional) handle to a combo list that will be filled with the list of possible inverse search commands.
// Returns:
//      the inverse search command of the first detected editor (the caller needs to free() the result).
LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo)
{
    LPTSTR firstEditor = NULL;
    TCHAR path[MAX_PATH];

    for (int i = 0; i < dimof(editor_rules); i++)
    {
        if (!ReadRegStr(editor_rules[i].RegRoot, editor_rules[i].RegKey, editor_rules[i].RegValue, path, dimof(path)))
        {
            continue;
        }

        PTSTR cmd;
        if (editor_rules[i].Type == SiblingPath)
        {
            // remove file part
            PTSTR dir = FilePath_GetDir(path);
            cmd = tstr_printf(_T("\"%s\\%s\" %s"), dir, editor_rules[i].BinaryFilename, editor_rules[i].InverseSearchArgs);
            free(dir);
        }
        else if (editor_rules[i].Type == BinaryDir)
        {
            // remove trailing path separator (TODO: move to function in file_util.c)
            int len = tstr_len(path);
            if (*path && char_is_dir_sep(path[len-1]))
                path[len-1] = 0;
            cmd = tstr_printf(_T("\"%s\\%s\" %s"), path, editor_rules[i].BinaryFilename, editor_rules[i].InverseSearchArgs);
        }
        else // if (editor_rules[i].Type == BinaryPath)
        {
            cmd = tstr_printf(_T("\"%s\" %s"), path, editor_rules[i].InverseSearchArgs);
        }

        if (!firstEditor)
            firstEditor = tstr_dup(cmd);
        if (!hwndCombo)
        {
            // no need to fill a combo box: return immeditately after finding an editor.
            free(cmd);
            return firstEditor;
        }

        SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)cmd);
        free(cmd);

        // skip the remaining rules for this editor
        while (i + 1 < dimof(editor_rules) && tstr_eq(editor_rules[i].Name, editor_rules[i+1].Name))
            i++;
    }

    // Fall back to notepad as a default handler
    if (!firstEditor) firstEditor = tstr_dup(_T("notepad %f"));
    return firstEditor;
}

static void SerializableGlobalPrefs_Init() {
    // Detect a text editor and use it as the default inverse search handler
    gGlobalPrefs.m_inverseSearchCmdLine = AutoDetectInverseSearchCommands();
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
    double zoomLevel = dm->zoomReal(pageNo);

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
    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++)
        EnableMenuItem(menuMain, id, MF_BYCOMMAND | enableState);

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

    CheckMenuRadioItem(menuMain, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    if (displayModeContinuous(displayMode))
        CheckMenuItem(menuMain, IDM_VIEW_CONTINUOUS, MF_BYCOMMAND | MF_CHECKED);

}

static void SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode, bool keepContinuous)
{
    if (!win->dm)
        return;

    if (keepContinuous && displayModeContinuous(win->dm->displayMode())) {
        switch (displayMode) {
            case DM_SINGLE_PAGE: displayMode = DM_CONTINUOUS; break;
            case DM_FACING: displayMode = DM_CONTINUOUS_FACING; break;
            case DM_BOOK_VIEW: displayMode = DM_CONTINUOUS_BOOK_VIEW; break;
        }
    }
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
    { _TRN("Send by &email..."),                    IDM_SEND_BY_EMAIL,          MF_NOT_IN_RESTRICTED },
    { SEP_ITEM ,                                    0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("P&roperties...\tCtrl-D"),               IDM_PROPERTIES,             0 },
    { SEP_ITEM ,                                    0,                          MF_NOT_IN_RESTRICTED },
    { _TRN("E&xit\tCtrl-Q"),                        IDM_EXIT,                   0 }
};

MenuDef menuDefView[] = {
    { _TRN("Single page"),                 IDM_VIEW_SINGLE_PAGE,        0  },
    { _TRN("Facing"),                      IDM_VIEW_FACING,             0  },
    { _TRN("Book view"),                   IDM_VIEW_BOOK,               0  },
    { _TRN("Show pages continuously"),     IDM_VIEW_CONTINUOUS,         0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Rotate left\tCtrl-Shift--"),   IDM_VIEW_ROTATE_LEFT,        0  },
    { _TRN("Rotate right\tCtrl-Shift-+"),  IDM_VIEW_ROTATE_RIGHT,       0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Fullscreen\tCtrl-L"),          IDM_VIEW_PRESENTATION_MODE,  0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Bookmarks\tF12"),              IDM_VIEW_BOOKMARKS,          0  },
    { _TRN("Show toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR,  0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Copy Selection\tCtrl-C"),      IDM_COPY_SELECTION,          0  },
};

MenuDef menuDefGoTo[] = {
    { _TRN("Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,          0  },
    { _TRN("Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,          0  },
    { _TRN("First Page\tHome"),            IDM_GOTO_FIRST_PAGE,         0  },
    { _TRN("Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,          0  },
    { _TRN("Page...\tCtrl-G"),             IDM_GOTO_PAGE,               0  },
    { SEP_ITEM ,                           0,                           0  },
    { _TRN("Back\tAlt+Left Arrow"),        IDM_GOTO_NAV_BACK,           0  },
    { _TRN("Forward\tAlt+Right Arrow"),    IDM_GOTO_NAV_FORWARD,        0  },
    { SEP_ITEM ,                           0,                           0  },
    { _TRN("Find...\tCtrl-F"),             IDM_FIND_FIRST,              0  },
};

MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl-0"),           IDM_ZOOM_FIT_PAGE,           0  },
    { _TRN("Act&ual Size\tCtrl-1"),        IDM_ZOOM_ACTUAL_SIZE,        0  },
    { _TRN("Fit Widt&h\tCtrl-2"),          IDM_ZOOM_FIT_WIDTH,          0  },
    { _TRN("&Custom Zoom..."),             IDM_ZOOM_CUSTOM,             0  },
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

static void AddFileMenuItem(HMENU menuFile, FileHistoryList *node, UINT index)
{
    assert(node);
    if (!node) return;
    assert(menuFile);
    if (!menuFile) return;

    TCHAR menuString[MAX_PATH + 4];
    wsprintf(menuString, _T("&%d) %s"), (index + 1) % 10, FilePath_GetBaseName(node->state.filePath));
    if (INVALID_MENU_ID == node->menuId)
        node->menuId = AllocNewMenuId();
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, node->menuId, menuString);
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
    if (gRestrictedUse) return;
    if (!gFileHistoryRoot) return;

    int itemsAdded = 0;
    FileHistoryList *curr = gFileHistoryRoot;
    while (curr && itemsAdded < MAX_RECENT_FILES_IN_MENU) {
        assert(curr->state.filePath);
        if (curr->state.filePath) {
            AddFileMenuItem(m, curr, itemsAdded);
            assert(curr->menuId != INVALID_MENU_ID);
            ++itemsAdded;
        }
        curr = curr->next;
    }
    if (curr) {
        DBG_OUT("  not adding, reached max %d items\n", MAX_RECENT_FILES_IN_MENU);
    }

    InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
}

static void WindowInfo_RebuildMenu(WindowInfo *win)
{
    if (win->hMenu) {
        DestroyMenu(win->hMenu);
        win->hMenu = NULL;
    }
    
    HMENU mainMenu = CreateMenu();
    // Don't display the Acrobat and email options, if the program couldn't be found
    bool noAcrobat = !CanViewWithAcrobat(), noEmail = !CanSendAsEmailAttachment();
    if (noAcrobat || noEmail) {
        for (int i = 0; i < dimof(menuDefFile); i++) {
            if (IDM_VIEW_WITH_ACROBAT == menuDefFile[i].m_id) {
                if (noAcrobat)
                    menuDefFile[i].m_title = NULL;
                if (noEmail)
                    menuDefFile[i + 1].m_title = NULL;
                if (noAcrobat && noEmail)
                    menuDefFile[i - 1].m_title = NULL;
            }
        }
    }

    HMENU tmp = BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile));
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

extern "C" TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName);

/* Get password for a given 'fileName', can be NULL if user cancelled the
   dialog box.
   Caller needs to free() the result. */
TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName)
{
    fileName = FilePath_GetBaseName(fileName);
    return Dialog_GetPassword(win, fileName);
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
    appDir = tstr_printf(_T("%s/%s"), dir, APP_NAME_STR);
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
            path = FilePath_GetDir(exePath);
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
    uint64_t prefsFileLen;
    prefsTxt = file_read_all(prefsFilename, &prefsFileLen);
    if (!str_empty(prefsTxt)) {
        ok = Prefs_Deserialize(prefsTxt, prefsFileLen, &gFileHistoryRoot);
        assert(ok);
    }

    free(prefsFilename);
    free(prefsTxt);
    return ok;
}

unsigned short gItemId[] = {
    IDM_ZOOM_6400, IDM_ZOOM_3200, IDM_ZOOM_1600, IDM_ZOOM_800, IDM_ZOOM_400,
    IDM_ZOOM_200, IDM_ZOOM_150, IDM_ZOOM_125, IDM_ZOOM_100, IDM_ZOOM_50,
    IDM_ZOOM_25, IDM_ZOOM_12_5, IDM_ZOOM_8_33, IDM_ZOOM_FIT_PAGE, 
    IDM_ZOOM_FIT_WIDTH, IDM_ZOOM_ACTUAL_SIZE, IDM_ZOOM_CUSTOM };

double gItemZoom[] = { 6400.0, 3200.0, 1600.0, 800.0, 400.0, 200.0, 150.0, 
    125.0, 100.0, 50.0, 25.0, 12.5, 8.33, ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH, ZOOM_ACTUAL_SIZE, 0 };

static UINT MenuIdFromVirtualZoom(double virtualZoom)
{
    for (size_t i=0; i < dimof(gItemZoom); i++) {
        if (virtualZoom == gItemZoom[i])
            return gItemId[i];
    }
    return IDM_ZOOM_CUSTOM;
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
    assert(IDM_ZOOM_FIRST <= menuItemId && menuItemId <= IDM_ZOOM_LAST);

    for (size_t i = 0; i < dimof(gItemId); i++)
        EnableMenuItem(hmenu, gItemId[i], MF_BYCOMMAND | (canZoom ? MF_ENABLED : MF_GRAYED));

    if (IDM_ZOOM_100 == menuItemId)
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;
    CheckMenuRadioItem(hmenu, IDM_ZOOM_FIRST, IDM_ZOOM_LAST, menuItemId, MF_BYCOMMAND);
    if (IDM_ZOOM_ACTUAL_SIZE == menuItemId)
        CheckMenuRadioItem(hmenu, IDM_ZOOM_100, IDM_ZOOM_100, IDM_ZOOM_100, MF_BYCOMMAND);
}

static void MenuUpdateZoom(WindowInfo* win)
{
    double zoomVirtual = gGlobalPrefs.m_defaultZoom;
    if (win->dm)
        zoomVirtual = win->dm->zoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->hMenu, menuId, NULL != win->dm);
}

static void UpdateDisplayStateWindowRect(WindowInfo *win, DisplayState *ds)
{
    // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
    //       the non-maximized dimensions for proper restoration)
    if (IsZoomed(win->hwndFrame) || IsIconic(win->hwndFrame) || win->fullScreen || win->presentation)
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
        if (win->presentation)
            gGlobalPrefs.m_windowState = win->_windowStateBeforePresentation;
        else if (win->fullScreen)
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
    if (win->presentation)
        ds.windowState = win->_windowStateBeforePresentation;
    else if (win->fullScreen)
        ds.windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win->hwndFrame))
        ds.windowState = WIN_STATE_MAXIMIZED;
    else
        ds.windowState = WIN_STATE_NORMAL;

    if (!win->dm->displayStateFromModel(&ds))
        return;

    UpdateDisplayStateWindowRect(win, &ds);
    DisplayState_Free(&(node->state));
    node->state = ds;
}

static void UpdateCurrentFileDisplayState(void)
{
    for (WindowInfo *currWin = gWindowList; currWin; currWin = currWin->next)
        UpdateCurrentFileDisplayStateForWin(currWin);
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
    if (!win->dm || !win->dm->displayStateFromModel(&ds)) {
        if (!autorefresh && !win->dm && win->loadedFilePath)
            LoadPdf(win->loadedFilePath, win);
        return;
    }
    UpdateDisplayStateWindowRect(win, &ds);
    // Set the windows state based on the actual window's placement
    ds.windowState =  win->fullScreen ? WIN_STATE_FULLSCREEN
                    : IsZoomed(win->hwndFrame) ? WIN_STATE_MAXIMIZED 
                    : IsIconic(win->hwndFrame) ? WIN_STATE_MINIMIZED
                    : WIN_STATE_NORMAL ;

    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken, 
    // we postpone the reload until the next autorefresh event
    bool tryrepair = !autorefresh;
    LoadPdfIntoWindow(win->watcher.filepath(), win, &ds, false, tryrepair, true, false);
    DisplayState_Free(&ds);
}

#ifndef THREAD_BASED_FILEWATCH
static void WindowInfo_RefreshUpdatedFiles(bool autorefresh) {
    for (WindowInfo* curr = gWindowList; curr; curr = curr->next) {
        if (curr->watcher.HasChanged())
            WindowInfo_Refresh(curr, autorefresh);
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
    RECT r;
    rect_set(&r, 0, 0, win->winDx(), win->winDy());
    FillRect(win->hdcDoubleBuffer, &r, win->presentation ? gBrushBlack : gBrushBg);
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

static void WindowInfo_AbortFinding(WindowInfo *win)
{
    if (win->findThread) {
        win->findCanceled = true;
        WaitForSingleObject(win->findThread, INFINITE);
    }
    win->findCanceled = false;
}

static void WindowInfoList_Remove(WindowInfo *to_remove);

static void WindowInfo_Delete(WindowInfo *win)
{
    // must DestroyWindow(win->hwndPdfProperties) before removing win from
    // the list of properties beacuse WM_DESTROY handler needs to find
    // WindowInfo for its HWND
    if (win->hwndPdfProperties) {
        DestroyWindow(win->hwndPdfProperties);
        assert(NULL == win->hwndPdfProperties);
    }
    WindowInfoList_Remove(win);

    if (win->dm) {
        RenderQueue_RemoveForDisplayModel(win->dm);
        cancelRenderingForDisplayModel(win->dm);
    }
    WindowInfo_AbortFinding(win);
    delete win->dm;
    win->dm = NULL;
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
    FreePdfProperties(win);
    WindowInfo_Dib_Deinit(win);
    WindowInfo_DoubleBuffer_Delete(win);
    DragAcceptFiles(win->hwndCanvas, FALSE);
    DeleteOldSelectionInfo(win);

    free(win->title);
    if (win->loadedFilePath)
        free(win->loadedFilePath);

    delete win;
}

WindowInfo* WindowInfo_FindByHwnd(HWND hwnd)
{
    for (WindowInfo *win = gWindowList; win; win = win->next) {
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
        if (hwnd == win->hwndPdfProperties)
            return win;
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
    
    HDC hdc = GetDC(hwndFrame);
    win->dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    win->uiDPIFactor = ceil(win->dpi * 4.0 / USER_DEFAULT_SCREEN_DPI) / 4.0;
    ReleaseDC(hwndFrame, hdc);

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
    for (WindowInfo* curr = gWindowList; curr; curr = curr->next) {
        if (to_remove == curr->next) {
            curr->next = to_remove->next;
            return;
        }
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
    for (WindowInfo* curr = gWindowList; curr; curr = curr->next)
        ++len;
    return len;
}

// Find the first windows showing a given PDF file 
WindowInfo* WindowInfoList_Find(TCHAR * file) {
    TCHAR * normFile = FilePath_Normalize(file, FALSE);
    if (!normFile)
        return NULL;

    for (WindowInfo* curr = gWindowList; curr; curr = curr->next) {
        if (curr->loadedFilePath && tstr_ieq(curr->loadedFilePath, normFile)) {
            free(normFile);
            return curr;
        }
    }
    free(normFile);
    return NULL;
}

static void UpdateToolbarBg(HWND hwnd, BOOL enabled)
{
    DWORD newStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (enabled)
        newStyle |= SS_WHITERECT;
    else
        newStyle &= ~SS_WHITERECT;
    SetWindowLong(hwnd, GWL_STYLE, newStyle);
}

static void WindowInfo_UpdateFindbox(WindowInfo *win) {
    UpdateToolbarBg(win->hwndFindBg, !!win->dm);
    UpdateToolbarBg(win->hwndPageBg, !!win->dm);

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
    for (WindowInfo* win = gWindowList; win; win = win->next) {
        if (win->state != WS_ABOUT)
            return true;
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
            switch (cmdId)
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

static void MenuUpdatePrintItem(WindowInfo *win) {
    HMENU hmenu = win->hMenu;
    bool filePrintEnabled = false;
    if (win->dm && win->dm->pdfEngine)
        filePrintEnabled = true;
    bool filePrintAllowed = !filePrintEnabled || win->dm->pdfEngine->hasPermission(PDF_PERM_PRINT);

    int ix;
    for (ix = 0; ix < dimof(menuDefFile) && menuDefFile[ix].m_id != IDM_PRINT; ix++);
    assert(ix < dimof(menuDefFile));
    if (ix < dimof(menuDefFile)) {
        const TCHAR *printItem = Translations_GetTranslation(menuDefFile[ix].m_title);
        if (!filePrintAllowed)
            printItem = _TR("&Print... (denied)");
        ModifyMenu(hmenu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
    }

    if (filePrintEnabled && filePrintAllowed)
        EnableMenuItem(hmenu, IDM_PRINT, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);
}

static void MenuUpdateStateForWindow(WindowInfo *win) {
    static UINT menusToDisableIfNoPdf[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SEND_BY_EMAIL,
        IDM_COPY_SELECTION, IDM_PROPERTIES };

    bool fileCloseEnabled = FileCloseMenuEnabled();
    assert(!fileCloseEnabled == !win->loadedFilePath);
    HMENU hmenu = win->hMenu;
    if (fileCloseEnabled)
        EnableMenuItem(hmenu, IDM_CLOSE, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_CLOSE, MF_BYCOMMAND | MF_GRAYED);

    if (CanViewWithAcrobat(win))
        EnableMenuItem(hmenu, IDM_VIEW_WITH_ACROBAT, MF_BYCOMMAND | MF_ENABLED);
    else
        EnableMenuItem(hmenu, IDM_VIEW_WITH_ACROBAT, MF_BYCOMMAND | MF_GRAYED);

    MenuUpdatePrintItem(win);
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
        if (win->dm->needVScroll() || (!displayModeContinuous(win->dm->displayMode()) && win->dm->pageCount() > 1))
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
    for (WindowInfo* win = gWindowList; win; win = win->next) {
        MenuUpdateStateForWindow(win);
        ToolbarUpdateStateForWindow(win);
    }
}

#define MIN_WIN_DX 50
#define MIN_WIN_DY 50

static void EnsureWindowVisibility(int *x, int *y, int *dx, int *dy)
{
    RECT rc = { *x, *y, *x + *dy, *y + *dy };

    // adjust to the work-area of the current monitor (not necessarily the primary one)
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST), &mi))
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    // make sure that the window is neither too small nor bigger than the monitor
    if (*dx < MIN_WIN_DX || *dx > rect_dx(&mi.rcWork))
        *dx = rect_dy(&mi.rcWork) * DEF_PAGE_RATIO;
    if (*dy < MIN_WIN_DY || *dy > rect_dy(&mi.rcWork))
        *dy = rect_dy(&mi.rcWork);

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    rc.bottom = rc.top + captionDy;
    rc.top += captionDy / 2;
    if (!IntersectRect(&mi.rcMonitor, &mi.rcWork, &rc)) {
        *x = mi.rcWork.left;
        *y = mi.rcWork.top;
    }
}

static WindowInfo* WindowInfo_CreateEmpty(void) {
    HWND        hwndFrame, hwndCanvas;
    WindowInfo* win;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int winX = CW_USEDEFAULT;
    int winY = CW_USEDEFAULT;
    int winDy = rect_dy(&workArea);
    int winDx = winDy * DEF_PAGE_RATIO;

    if (DEFAULT_WIN_POS != gGlobalPrefs.m_windowPosX) {
        winX = gGlobalPrefs.m_windowPosX;
        winY = gGlobalPrefs.m_windowPosY;
        winDx = gGlobalPrefs.m_windowDx;
        winDy = gGlobalPrefs.m_windowDy;

        EnsureWindowVisibility(&winX, &winY, &winDx, &winDy);
    }

    hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, gWindowTitle,
            WS_OVERLAPPEDWINDOW,
            winX, winY, winDx, winDy,
            NULL, NULL,
            ghinst, NULL);
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

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

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
    ScrollState ss = { 1, -1, -1 };
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
    WindowInfo_AbortFinding(win);

    if (win->loadedFilePath)
        free(win->loadedFilePath);
    win->loadedFilePath = tstr_dup(fileName);
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
            win_set_text(win->hwndFrame, FilePath_GetBaseName(fileName));
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
    // Only restore the scroll state when everything is visible
    // (otherwise we might have to relayout twice, which can take
    //  a while for longer documents)
    // win->dm->setScrollState(&ss);

    if (!is_new_window) {
        WindowInfo_RedrawAll(win);
        OnMenuFindMatchCase(win); 
    }
    WindowInfo_UpdateFindbox(win);

    int pageCount = win->dm->pageCount();
    const TCHAR *baseName = FilePath_GetBaseName(win->dm->fileName());
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
            rect_shift_to_work_area(&rect, FALSE);
            
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
    if (win->tocLoaded)
        win->ClearTocBox();
    if (win->dm)
        win->dm->setScrollState(&ss);
    if (win->dm && win->dm->_showToc) {
        if (win->dm->hasTocTree()) {
            win->ShowTocBox();
        } else {
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
    PostMessage(((WindowInfo *)param)->hwndFrame, WM_COMMAND, IDM_REFRESH, TRUE);
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

    EnsureWindowVisibility(&ds->windowX, &ds->windowY, &ds->windowDx, &ds->windowDy);
}

static void AdjustRemovableDriveLetter(TCHAR *path)
{
    TCHAR szDrive[] = _T("?:\\"), origDrive;
    UINT driveType;
    DWORD driveMask;

    // Don't bother if the file path is still valid
    if (file_exists(path))
        return;

    // Don't bother for invalid and non-removable drives
    szDrive[0] = toupper(path[0]);
    if (szDrive[0] < 'A' || szDrive[0] > 'Z')
        return;
    driveType = GetDriveType(szDrive);
    if (DRIVE_REMOVABLE != driveType && DRIVE_UNKNOWN != driveType && DRIVE_NO_ROOT_DIR != driveType)
        return;

    // Iterate through all (other) removable drives and try to find the file there
    szDrive[0] = 'A';
    origDrive = path[0];
    for (driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && GetDriveType(szDrive) == DRIVE_REMOVABLE) {
            path[0] = szDrive[0];
            if (file_exists(path))
                return;
        }
        szDrive[0]++;
    }
    path[0] = origDrive;
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
    TCHAR *fullpath = FilePath_Normalize(fileName, FALSE);
    if (!fullpath)
        goto exit;

    FileHistoryList *fileFromHistory = FileHistoryList_Node_FindByFilePath(&gFileHistoryRoot, fullpath);
    DisplayState *ds = NULL;
    if (fileFromHistory) {
        ds = &fileFromHistory->state;
        AdjustRemovableDriveLetter(fullpath);
    }

    CheckPositionAndSize(ds);
    if (!LoadPdfIntoWindow(fullpath, win, ds, is_new_window, true, showWin, true)) {
        /* failed to open */
        goto exit;
    }

    // Define THREAD_BASED_FILEWATCH to use the thread-based implementation of file change detection.
#ifdef THREAD_BASED_FILEWATCH
    if (!win->watcher.IsThreadRunning())
        win->watcher.StartWatchThread(fullpath, &OnFileChange, (LPARAM)win);
#else
        win->watcher.Init(fullpath);
#endif

    CreateSynchronizer(fullpath, &win->pdfsync);

    if (gGlobalPrefs.m_rememberOpenedFiles) {
        AddFileToHistory(fullpath);
        RebuildProgramMenus();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    SHAddToRecentDocs(SHARD_PATH, fullpath);

exit:
    free(fullpath);
    return win;
}

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize)
{
    HFONT       font_dc;
    HFONT       font;
    LOGFONT     lf = {0};

    font_dc = (HFONT)GetStockObject(SYSTEM_FONT);
    if (!GetObject(font_dc, sizeof(LOGFONT), &lf))
        return NULL;

    lf.lfWidth = 0;
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;    
    lstrcpyn(lf.lfFaceName, fontName, LF_FACESIZE);
    lf.lfWeight = FW_DONTCARE;
    font = CreateFontIndirect(&lf);
    return font;
}

void Win32_Font_Delete(HFONT font)
{
    DeleteObject(font);
}

static HTREEITEM WindowInfo_TreeItemForPageNo(WindowInfo *win, HTREEITEM hItem, int pageNo)
{
    HTREEITEM hCurrItem = NULL;

    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(win->hwndTocBox, &item);

        // return if this item is on the specified page (or on a latter page)
        if (item.lParam && PDF_LGOTO == ((pdf_link *)item.lParam)->kind) {
            int page = win->dm->pdfEngine->findPageNo(((pdf_link *)item.lParam)->dest);
            if (page <= pageNo)
                hCurrItem = hItem;
            if (page >= pageNo)
                break;
        }

        // find any child item closer to the specified page
        HTREEITEM hSubItem = NULL;
        if ((item.state & TVIS_EXPANDED))
            hSubItem = WindowInfo_TreeItemForPageNo(win, TreeView_GetChild(win->hwndTocBox, hItem), pageNo);
        if (hSubItem)
            hCurrItem = hSubItem;

        hItem = TreeView_GetNextSibling(win->hwndTocBox, hItem);
    }

    return hCurrItem;
}

static void WindowInfo_UpdateTocSelection(WindowInfo *win, int currPageNo)
{
    if (!win->dm || !win->dm->_showToc || !win->tocLoaded)
        return;
    if (GetFocus() == win->hwndTocBox)
        return;

    HTREEITEM hRoot = TreeView_GetRoot(win->hwndTocBox);
    if (!hRoot)
        return;
    HTREEITEM hCurrItem = WindowInfo_TreeItemForPageNo(win, hRoot, currPageNo);
    if (hCurrItem)
        TreeView_SelectItem(win->hwndTocBox, hCurrItem);
    win->currPageNo = currPageNo;
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
        if (currPageNo != win->currPageNo)
            WindowInfo_UpdateTocSelection(win, currPageNo);
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

    // When hiding the scroll bars and fitting width, it could be that we'd have to
    // display the scroll bars right again for the new width. Make sure we haven't just done
    // that - or if so, force the vertical scroll bar to remain visible.
    if (ZOOM_FIT_WIDTH == win->dm->zoomVirtual()) {
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

        if (!win->fullScreen && !win->presentation && ZOOM_FIT_PAGE == win->dm->zoomVirtual()) {
            switch (win->dm->displayMode()) {
                case DM_SINGLE_PAGE:
                    si.nPos = win->dm->currentPageNo() - 1;
                    si.nMax = win->dm->pageCount() - 1;
                    si.nPage = 1;
                    break;
                case DM_FACING:
                    si.nPos = (win->dm->currentPageNo() + 1) / 2 - 1;
                    si.nMax = (win->dm->pageCount() + 1) / 2 - 1;
                    si.nPage = 1;
                    break;
                case DM_BOOK_VIEW:
                    si.nPos = win->dm->currentPageNo() / 2;
                    si.nMax = win->dm->pageCount() / 2;
                    si.nPage = 1;
                    break;
            }
        }
    } else {
        si.nPos = (int)areaOffset.y;
        si.nMin = 0;
        si.nMax = canvasDy-1;
        si.nPage = drawAreaDy;

        if (ZOOM_FIT_PAGE != win->dm->zoomVirtual()) {
            // keep the top/bottom 5% of the previous page visible after paging down/up
            si.nPage *= 0.95;
            si.nMax -= drawAreaDy - si.nPage;
        }
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

/*
Structure of registry entries for associating Sumatra with PDF files.

The following paths exist under both HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER.
HKCU has precedence over HKLM.

Software\Classes\.pdf default key is name of reg entry describing the app
  handling opening PDF files. In our case it's SumatraPDF

Software\Classes\SumatraPDF\DefaultIcon = $exePath,1
  1 means the second icon resource within the executable
Software\Classes\SumatraPDF\shell\open\command = "$exePath" "%1"
  tells how to call sumatra to open PDF file. %1 is replaced by PDF file path
Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Progid
  should be SumatraPDF (FoxIt takes it over); only needed for HKEY_CURRENT_USER

HKEY_CLASSES_ROOT\.pdf\OpenWithList
  list of all apps that can be used to open PDF files. We don't touch that.

HKEY_CLASSES_ROOT\.pdf default comes from either HKCU\Software\Classes\.pdf or
HKLM\Software\Classes\.pdf (HKCU has priority over HKLM)

Note: When making changes below, please also adjust the installer.nsis script.
*/
static void DoAssociateExeWithPdfExtension(HKEY hkey)
{
    bool ok;
    TCHAR exePath[MAX_PATH];
    TCHAR previousPdfHandler[MAX_PATH + 8];

    // Remember the previous default app for the Uninstaller
    ok = ReadRegStr(hkey, _T("Software\\Classes\\.pdf"), NULL, previousPdfHandler, dimof(previousPdfHandler));
    if (ok && !tstr_eq(previousPdfHandler, APP_NAME_STR)) {
        WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR, _T("previous.pdf"), previousPdfHandler);
    }

    GetModuleFileName(NULL, exePath, dimof(exePath));
    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR, NULL, _TR("PDF Document"));
    TCHAR *icon_path = tstr_cat(exePath, _T(",1"));
    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\DefaultIcon"), NULL, icon_path);
    free(icon_path);

    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell"), NULL, _T("open"));

    TCHAR *cmd_path = tstr_cat3(_T("\""), exePath, _T("\" \"%1\"")); // "${exePath}" "%1"
    ok = WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell\\open\\command"), NULL, cmd_path);
    free(cmd_path);

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (ok) {
        WriteRegStr(hkey, _T("Software\\Classes\\.pdf"), NULL, APP_NAME_STR);
        if (hkey == HKEY_CURRENT_USER) {
            WriteRegStr(hkey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"), _T("Progid"), APP_NAME_STR);
        }
    }
}

// verify that all registry entries that need to be set in order to associate
// Sumatra with .pdf files exist and have the right values
bool IsExeAssociatedWithPdfExtension(void)
{
    TCHAR exePath[MAX_PATH];
    TCHAR tmp[MAX_PATH];
    bool ok;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    ok = ReadRegStr(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"), _T("Progid"), tmp, dimof(tmp));
    if (ok && !tstr_eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to APP_NAME_STR
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL, tmp, dimof(tmp));
    if (!ok || !tstr_eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell"), NULL, tmp, dimof(tmp));
    if (!ok || !tstr_ieq(tmp, _T("open")))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell\\open\\command"), NULL, tmp, dimof(tmp));
    if (!ok)
        return false;

    GetModuleFileName(NULL, exePath, dimof(exePath));
    TCHAR *cmd_path = tstr_cat3(_T("\""), exePath, _T("\" \"%1\"")); // "${exePath}" "%1"
    bool same = !!tstr_eq(tmp, cmd_path);
    free(cmd_path);
    
    return same;
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
    if (IsRunningInPortableMode() || gRestrictedUse)
        return false;

    if (IsExeAssociatedWithPdfExtension())
        return true;

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

static TCHAR * ResolveLnk(TCHAR * path)
{
    IShellLink *lnk = NULL;
    IPersistFile *file = NULL;
    TCHAR *resolvedPath = NULL;

    LPCOLESTR olePath = tstr_to_wstr(path);
    if (!olePath)
        return NULL;

    HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->QueryInterface(IID_IPersistFile, (LPVOID *)&file);
    if (FAILED(hRes))
        goto Exit;

    hRes = file->Load(olePath, STGM_READ);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->Resolve(NULL, SLR_UPDATE);
    if (FAILED(hRes))
        goto Exit;

    TCHAR newPath[MAX_PATH];
    hRes = lnk->GetPath(newPath, MAX_PATH, NULL, 0);
    if (FAILED(hRes))
        goto Exit;

    resolvedPath = tstr_dup(newPath);

Exit:
    if (file)
        file->Release();
    if (lnk)
        lnk->Release();
    free((void *)olePath);

    return resolvedPath;
}

static void OnDropFiles(WindowInfo *win, HDROP hDrop)
{
    int         i;
    TCHAR       filename[MAX_PATH];
    const int   files_count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (i = 0; i < files_count; i++)
    {
        DragQueryFile(hDrop, i, filename, MAX_PATH);
        if (tstr_endswithi(filename, _T(".lnk"))) {
            TCHAR *resolved = ResolveLnk(filename);
            if (resolved) {
                lstrcpyn(filename, resolved, MAX_PATH);
                free(resolved);
            }
        }
        // The first dropped document may override the current window
        LoadPdf(filename, i == 0 ? win : NULL);
    }
    DragFinish(hDrop);

    if (files_count > 0)
        WindowInfo_RedrawAll(win);
}

static void PaintRectangle(HDC hdc, RECT * rect)
{
    MoveToEx(hdc, rect->left, rect->top, NULL);
    LineTo(hdc, rect->right - 1, rect->top);
    LineTo(hdc, rect->right - 1, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->top);
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
    if (val == -1)
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
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
int CompareVersion(TCHAR *txt1, TCHAR *txt2)
{
    int v1, v2;
    while (1) {
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

static bool ValiProgramVersionChar(char c)
{
    if (c >= '0' && c <= '9')
        return true;
    if (c == '.' || c == '\r' || c == '\n')
        return true;
    return false;
}

// the only valid chars are 0-9, . and newlines. Return false if contains
// anything else
static bool ValidProgramVersion(char *txt)
{
    char c = *txt++;
    while (c != 0) {
        if (!ValiProgramVersionChar(c)) {
            return false;
        }
        c = *txt++;
    }
    return true;
}

static void OnUrlDownloaded(WindowInfo *win, HttpReqCtx *ctx)
{
    DWORD dataSize;
    char *txt = (char*)ctx->data.getData(&dataSize);
    TCHAR *url = ctx->url;
    if (!tstr_startswith(url, SUMATRA_UPDATE_INFO_URL)) {
        goto Exit;
    }

    // see http://code.google.com/p/sumatrapdf/issues/detail?id=725
    // if a user configures os-wide proxy that is not regular ie proxy
    // (which we pick up) we might get complete garbage in response to
    // our query and in might accidentally contain number which might
    // be bigger than our version number which will make program ask
    // to upgrade every time
    // to fix that, we reject text that doesn't look like comes from us
    if (!ValidProgramVersion(txt)) {
        goto Exit;
    }
        
    TCHAR *verTxt = multibyte_to_tstr(txt, CP_ACP);
    /* TODO: too hackish */
    tstr_trans_chars(verTxt, _T("\r\n"), _T("\0\0"));
    if (CompareVersion(verTxt, UPDATE_CHECK_VER) > 0){
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
Exit:
    free(txt);
    delete ctx;
}

static void DrawCenteredText(HDC hdc, RECT *r, const TCHAR *txt)
{    
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, lstrlen(txt), r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void PaintTransparentRectangle(WindowInfo *win, HDC hdc, RectI *rect, DWORD selectionColor, int margin = 1) {
    HBITMAP hbitmap;       // bitmap handle
    BITMAPINFO bmi;        // bitmap header
    VOID *pvBits;          // pointer to DIB section
    BLENDFUNCTION bf;      // structure for alpha blending
    HDC rectDC = CreateCompatibleDC(hdc);
    const DWORD selectionColorBlack = 0xff000000;

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
    
    const DWORD selectionColorBlue = 0xff000000 | 
        (GetRValue(gGlobalPrefs.m_fwdsearchColor) << 16) |
        (GetGValue(gGlobalPrefs.m_fwdsearchColor) << 8) | 
        GetBValue(gGlobalPrefs.m_fwdsearchColor);

    RectD recD;
    RectI recI;

    // Draw the rectangles highlighting the forward search results
    for (UINT i = 0; i < win->fwdsearchmarkRects.size(); i++)
    {
        RectD_FromRectI(&recD, &win->fwdsearchmarkRects[i]);
        win->dm->rectCvtUserToScreen(win->fwdsearchmarkPage, &recD);
        if (gGlobalPrefs.m_fwdsearchOffset > 0)
        {
            recD.x = pageInfo->screenX + (double)gGlobalPrefs.m_fwdsearchOffset * win->dm->zoomReal() / 100.0;
            recD.dx = (gGlobalPrefs.m_fwdsearchWidth > 0 ? (double)gGlobalPrefs.m_fwdsearchWidth : 15.0) * win->dm->zoomReal() / 100.0;
            recD.y -= 4;
            recD.dy += 8;
        }
        RectI_FromRectD(&recI, &recD);
        PaintTransparentRectangle(win, hdc, &recI, selectionColorBlue, 0);
    }
}

#define BORDER_SIZE   1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, PdfPageInfo * pageInfo, bool presentation, RECT * bounds)
{
    int xDest = pageInfo->screenX;
    int yDest = pageInfo->screenY;
    int bmpDx = pageInfo->bitmapDx;
    int bmpDy = pageInfo->bitmapDy;

    rect_set(bounds, xDest, yDest, bmpDx, bmpDy);

    // Frame info
    int fx = xDest - BORDER_SIZE, fy = yDest - BORDER_SIZE;
    int fw = bmpDx + 2 * BORDER_SIZE, fh = bmpDy + 2 * BORDER_SIZE;

    // Shadow info
    int sx = fx + SHADOW_OFFSET, sy = fy + SHADOW_OFFSET, sw = fw, sh = fh;
    if (xDest <= 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = min(pageInfo->bitmapX, SHADOW_OFFSET);
        sx -= diff; sw += diff;
    }
    if (yDest <= 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = min(pageInfo->bitmapY, SHADOW_OFFSET);
        sy -= diff; sh += diff;
    }

    // Draw shadow
    if (!presentation) {
        RECT rc;
        rect_set(&rc, sx, sy, sw, sh);
        FillRect(hdc, &rc, gBrushShadow);
    }

    // Draw frame
    HPEN pe = CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME);
    SelectObject(hdc, pe);
    SelectObject(hdc, gGlobalPrefs.m_invertColors ? gBrushBlack : gBrushWhite);
    Rectangle(hdc, fx, fy, fx + fw, fy + fh);
    DeletePen(pe);
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

    FillRect(hdc, &ps->rcPaint, win->presentation ? gBrushBlack : gBrushBg);

    DBG_OUT("WindowInfo_Paint() ");
    for (int pageNo = 1; pageNo <= dm->pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = dm->getPageInfo(pageNo);
        if (!pageInfo->visible)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        LockCache();
        BitmapCacheEntry *entry = BitmapCache_Find(dm, pageNo, dm->zoomReal(), dm->rotation());
        if (entry)
            renderedBmp = entry->bitmap;

        PaintPageFrameAndShadow(hdc, pageInfo, PM_ENABLED == win->presentation, &bounds);

        if (!entry || BITMAP_CANNOT_RENDER == renderedBmp) {
            HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
            HFONT origFont = (HFONT)SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
            SetTextColor(hdc, gGlobalPrefs.m_invertColors ? COL_WHITE : COL_BLACK);
            if (!entry) {
                /* TODO: assert is queued for rendering ? */
                DrawCenteredText(hdc, &bounds, _TR("Please wait - rendering..."));
                DBG_OUT("drawing empty %d ", pageNo);
            } else {
                DrawCenteredText(hdc, &bounds, _TR("Couldn't render the page"));
                DBG_OUT("   missing bitmap on visible page %d\n", pageNo);
            }
            SelectObject(hdc, origFont);
            Win32_Font_Delete(fontRightTxt);
            UnlockCache();
            continue;
        }

        DBG_OUT("page %d ", pageNo);

        HBITMAP hbmp = renderedBmp->createDIBitmap(hdc);
        int renderedBmpDx = renderedBmp->dx();
        int renderedBmpDy = renderedBmp->dy();
        renderedBmp = NULL; // could be invalid, as soon as the cache is unlocked

        UnlockCache();
        if (!hbmp)
            continue;

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            int xSrc = (int)pageInfo->bitmapX;
            int ySrc = (int)pageInfo->bitmapY;

            SelectObject(bmpDC, hbmp);
            if ((renderedBmpDx < pageInfo->currDx) || (renderedBmpDy < pageInfo->currDy))
                StretchBlt(hdc, bounds.left, bounds.top, rect_dx(&bounds), rect_dy(&bounds),
                    bmpDC, xSrc, ySrc, renderedBmpDx, renderedBmpDy, SRCCOPY);
            else
                BitBlt(hdc, bounds.left, bounds.top, rect_dx(&bounds), rect_dy(&bounds),
                    bmpDC, xSrc, ySrc, SRCCOPY);
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
            rect_set(&rectScreen, intersect.x, intersect.y, intersect.dx, intersect.dy);
            HPEN pe = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
            SelectObject(hdc, pe);
            PaintRectangle(hdc, &rectScreen);
            DeletePen(pe);
        }
    }
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

static void CopySelectionToClipboard(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    if (!win->selectionOnPage) return;
    if (!OpenClipboard(NULL)) return;

    if (win->dm->pdfEngine->hasPermission(PDF_PERM_COPY)) {
        TStrList *selections = NULL;
        for (SelectionOnPage *selOnPage = win->selectionOnPage; selOnPage; selOnPage = selOnPage->next)
            TStrList_InsertAndOwn(&selections, win->dm->getTextInRegion(selOnPage->pageNo, &selOnPage->selectionPage));

        TStrList_Reverse(&selections);
        TCHAR *selText = TStrList_Join(selections, NULL);
        TStrList_Destroy(&selections);

        EmptyClipboard();
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (lstrlen(selText) + 1) * sizeof(TCHAR));
        if (handle) {
            TCHAR *globalText = (TCHAR *)GlobalLock(handle);
            lstrcpy(globalText, selText);
            GlobalUnlock(handle);

#ifdef UNICODE
            if (!SetClipboardData(CF_UNICODETEXT, handle))
#else
            if (!SetClipboardData(CF_TEXT, handle))
#endif
                SeeLastError();
        }
        free(selText);
    }

    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage *selOnPage = win->selectionOnPage;
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
        delete bmp;
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
    win->showSelection = false;
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
    if (!win || !win->dm) return;

    // Clear the last forward-search result
    win->fwdsearchmarkRects.clear();
    InvalidateRect(win->hwndCanvas, NULL, FALSE);

    // No inverse search command configured
    if (!gGlobalPrefs.m_inverseSearchCmdLine || !*gGlobalPrefs.m_inverseSearchCmdLine)
    {
#ifdef _TEX_ENHANCEMENT
        WindowInfo_ShowMessage_Asynch(win, _TR("Cannot start inverse search command. Please check the command line in the settings."), true);
#endif
        return;
    }

    // On double-clicking no error message will be shown to the user if the PDF does not have a synchronization file is present.)

    if (!win->pdfsync) {
        UINT err = CreateSynchronizer(win->watcher.filepath(), &win->pdfsync);

        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND )
        {
            // In the official build to avoid confusion for non-LaTeX users
            // we do not show any error message if no synchronization file is present.
            DBG_OUT("Pdfsync: Sync file not found!\n");
#ifdef _TEX_ENHANCEMENT
            WindowInfo_ShowMessage_Asynch(win, _TR("No synchronization file found"), true);
#endif
            return;
        }
        else if (err != PDFSYNCERR_SUCCESS || !win->pdfsync)
        {
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
            WindowInfo_ShowMessage_Asynch(win, _TR("Cannot start inverse search command. Please check the command line in the settings."), true);
        }
    }
}

static void ChangePresentationMode(WindowInfo *win, PresentationMode mode)
{
    win->presentation = mode;
    WindowInfo_RedrawAll(win);
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

    SetCapture(win->hwndCanvas);
    win->mouseAction = MA_MAYBEDRAGGING;
    win->dragPrevPosX = x;
    win->dragPrevPosY = y;
    win->dragStartX = x;
    win->dragStartY = y;
    win->linkOnLastButtonDown = win->dm->linkAtPosition(x, y);
    SetCursor(gCursorDrag);
    DBG_OUT(" dragging start, x=%d, y=%d\n", x, y);
}

static void OnDraggingStop(WindowInfo *win, int x, int y)
{
    int             dragDx, dragDy;

    assert(win);
    if (!win) return;

    if (WS_SHOWING_PDF != win->state)
        return;

    assert(win->dm);
    if (!win->dm) return;

    if (MA_MAYBEDRAGGING != win->mouseAction && MA_DRAGGING != win->mouseAction)
        return;

    bool didDragMouse = MA_DRAGGING == win->mouseAction ||
        abs(x - win->dragStartX) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win->dragStartY) > GetSystemMetrics(SM_CYDRAG);

    if (GetCapture() == win->hwndCanvas) {
        if (didDragMouse) {
            win->linkOnLastButtonDown = NULL;
            dragDx = x - win->dragPrevPosX;
            dragDy = y - win->dragPrevPosY;
            DBG_OUT(" dragging ends, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
            WinMoveDocBy(win, dragDx, -dragDy*2);
            win->dragPrevPosX = x;
            win->dragPrevPosY = y;
        }
        SetCursor(gCursorArrow);
        ReleaseCapture();
    }

    if (win->linkOnLastButtonDown && win->dm->linkAtPosition(x, y) == win->linkOnLastButtonDown) {
        win->dm->handleLink(win->linkOnLastButtonDown);
        SetCursor(gCursorArrow);
    }
    else if (didDragMouse)
        /* pass */;
    /* if we had a selection and this was just a click, hide selection */
    else if (win->showSelection)
        ClearSearch(win);
    else if (win->fullScreen || PM_ENABLED == win->presentation) {
        if (WasKeyDown(VK_SHIFT))
            win->dm->goToPrevPage(0);
        else
            win->dm->goToNextPage(0);
    }
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
        ChangePresentationMode(win, PM_ENABLED);

    win->linkOnLastButtonDown = NULL;
}

static void OnMouseMove(WindowInfo *win, int x, int y, WPARAM flags)
{
    int dragDx, dragDy;

    assert(win);
    if (!win || WS_SHOWING_PDF != win->state)
        return;

    assert(win->dm);
    if (!win->dm) return;

    switch (win->mouseAction) {
    case MA_SCROLLING:
        win->yScrollSpeed = (y - win->dragPrevPosY) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win->xScrollSpeed = (x - win->dragPrevPosX) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        break;
    case MA_SELECTING:
        win->selectionRect.dx = x - win->selectionRect.x;
        win->selectionRect.dy = y - win->selectionRect.y;
        triggerRepaintDisplayNow(win);
        break;
    case MA_MAYBEDRAGGING:
        // have we already started a proper drag?
        if (abs(x - win->dragStartX) <= GetSystemMetrics(SM_CXDRAG) &&
            abs(y - win->dragStartY) <= GetSystemMetrics(SM_CYDRAG))
            break;
        win->mouseAction = MA_DRAGGING;
        win->linkOnLastButtonDown = NULL;
        // fall through
    case MA_DRAGGING:
        dragDx = win->dragPrevPosX - x;
        dragDy = win->dragPrevPosY - y;
        DBG_OUT(" drag move, x=%d, y=%d, dx=%d, dy=%d\n", x, y, dragDx, dragDy);
        WinMoveDocBy(win, dragDx, dragDy);
        win->dragPrevPosX = x;
        win->dragPrevPosY = y;
        break;
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
            DeleteOldSelectionInfo(win);
        } else {
            ConvertSelectionRectToSelectionOnPage (win);
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

    if (MA_SELECTING == win->mouseAction)
        OnSelectionStop(win, x, y);
    else
        OnDraggingStop(win, x, y);

    win->mouseAction = MA_IDLE;
}

static void OnMouseMiddleButtonDown(WindowInfo *win, int x, int y, int key)
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

static void OnMouseRightButtonClick(WindowInfo *win, int x, int y, int key)
{
    if ((win->fullScreen || PM_ENABLED == win->presentation) && win->dm)
        win->dm->goToPrevPage(0);
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
        switch (win->presentation) {
        case PM_BLACK_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushBlack);
            break;
        case PM_WHITE_SCREEN:
            FillRect(hdc, &ps.rcPaint, gBrushWhite);
            break;
        default:
            WinResizeIfNeeded(win);
            WindowInfo_Paint(win, hdc, &ps);
            WindowInfo_DoubleBuffer_Show(win, hdc);
        }
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
        WindowInfo_AbortFinding(win);
        delete win->dm;
        win->dm = NULL;
        if (win->loadedFilePath) {
            free(win->loadedFilePath);
            win->loadedFilePath = NULL;
        }
        if (win->hwndPdfProperties) {
            DestroyWindow(win->hwndPdfProperties);
            assert(NULL == win->hwndPdfProperties);
        }
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

static void OnMenuCustomZoom(WindowInfo *win)
{
    if (!win->dm)
        return;

    double zoom = win->dm->zoomVirtual();
    if (DIALOG_CANCEL_PRESSED == Dialog_CustomZoom(win->hwndFrame, &zoom))
        return;
    win->dm->zoomTo(zoom);
    MenuUpdateZoom(win);
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

    int printAreaWidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    int printAreaHeight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    double dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / 72.0, GetDeviceCaps(hdc, LOGPIXELSY) / 72.0);
    bool bPrintPortrait = printAreaWidth < printAreaHeight;
    if (devMode->dmFields & DM_ORIENTATION)
        bPrintPortrait = DMORIENT_PORTRAIT == devMode->dmOrientation;

    // print all the pages the user requested
    for (int i = 0; i < nPageRanges; i++) {
        if (-1 == pr->nToPage && 0 < pr->nFromPage) {
            // print with minimal margins as required by the printer
            printAreaWidth = GetDeviceCaps(hdc, HORZRES);
            printAreaHeight = GetDeviceCaps(hdc, VERTRES);

            assert(1 == nPageRanges && sel && !sel->next);
            DBG_OUT(" printing:  drawing bitmap for selection\n");
            StartPage(hdc);

            RectD * r = &sel->selectionPage;
            fz_rect clipRegion;
            clipRegion.x0 = r->x; clipRegion.x1 = r->x + r->dx;
            clipRegion.y0 = r->y; clipRegion.y1 = r->y + r->dy;

            int rotation = pdfEngine->pageRotation(pr->nFromPage) + dm->rotation();
            // Swap width and height for rotated documents
            SizeD sSize = (rotation % 180) == 0 ? SizeD(r->dx, r->dy) : SizeD(r->dy, r->dx);

            double zoom = min((double)printAreaWidth / sSize.dx(), (double)printAreaHeight / sSize.dy());
            RenderedBitmap *bmp = pdfEngine->renderBitmap(pr->nFromPage, 100.0 * zoom, dm->rotation(), &clipRegion, NULL, NULL);
            if (bmp) {
                bmp->stretchDIBits(hdc, (printAreaWidth - bmp->dx()) / 2,
                    (printAreaHeight - bmp->dy()) / 2, bmp->dx(), bmp->dy());
                delete bmp;
            }
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

            SizeD pSize = pdfEngine->pageSize(pageNo);
            int rotation = pdfEngine->pageRotation(pageNo);
            // Turn the document by 90° if it isn't in portrait mode
            if (pSize.dx() > pSize.dy()) {
                rotation += 90;
                pSize = SizeD(pSize.dy(), pSize.dx());
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90° in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                pSize = SizeD(pSize.dy(), pSize.dx());
            }

            // try to use correct zoom values (scale down to fit the physical page, though)
            double zoom = min(dpiFactor, min((double)printAreaWidth / pSize.dx(), (double)printAreaHeight / pSize.dy()));
            RenderedBitmap *bmp = pdfEngine->renderBitmap(pageNo, 100.0 * zoom, rotation, NULL, NULL, NULL);
            if (bmp) {
                bmp->stretchDIBits(hdc, (printAreaWidth - bmp->dx()) / 2 - leftMargin,
                    (printAreaHeight - bmp->dy()) / 2 - topMargin, bmp->dx(), bmp->dy());
                delete bmp;
            }
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

    WNDPROC nextWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWL_USERDATA);
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
            WNDPROC nextWndProc = (WNDPROC)SetWindowLongPtr(hApplyButton, GWL_WNDPROC, (LONG_PTR)DisableApplyBtnWndProc);
            SetWindowLongPtr(hApplyButton, GWL_USERDATA, (LONG_PTR)nextWndProc);
        }
        return S_FALSE;
    };
    STDMETHODIMP InitDone() { return E_NOTIMPL; };
    STDMETHODIMP SelectionChange() { return E_NOTIMPL; };
protected:
    LONG m_cRef;
    WNDPROC m_wndProc;
};

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

    if (gRestrictedUse) return;

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
    pd.lpCallback = new ApplyButtonDiablingCallback();

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

    // Prepare the file filters (slightly hacky because
    // translations can't contain the \0 character)
    TCHAR fileFilter[256] = {0};
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("PDF documents"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.pdf\1"));
    if (hasCopyPerm) {
        tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("Text documents"));
        tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.txt\1"));
    }
    tstr_cat_s(fileFilter, sizeof(fileFilter), _TR("All files"));
    tstr_cat_s(fileFilter, sizeof(fileFilter), _T("\1*.*\1"));
    tstr_trans_chars(fileFilter, _T("\1"), _T("\0"));

    // Remove the extension so that it can be re-added depending on the chosen filter
    tstr_copy(dstFileName, dimof(dstFileName), FilePath_GetBaseName(srcFileName));
    if (tstr_endswithi(dstFileName, _T(".pdf")))
        dstFileName[lstrlen(dstFileName) - 4] = 0;

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
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
    if (!tstr_endswithi(dstFileName, _T(".pdf")) && !(hasCopyPerm && tstr_endswithi(dstFileName, _T(".txt")))) {
        TCHAR *defaultExt = hasCopyPerm && 2 == ofn.nFilterIndex ? _T(".txt") : _T(".pdf");
        realDstFileName = tstr_cat_s(dstFileName, dimof(dstFileName), defaultExt);
    }
    // Extract all text when saving as a plain text file
    if (hasCopyPerm && tstr_endswithi(realDstFileName, _T(".txt"))) {
        TCHAR *text = win->dm->extractAllText();
        char *textUTF8 = tstr_to_utf8(text);
        char *textUTF8BOM = str_cat("\xEF\xBB\xBF", textUTF8);
        free(textUTF8);
        free(text);        
        write_to_file(realDstFileName, textUTF8BOM, str_len(textUTF8BOM));
        free(textUTF8BOM);
    }
    // Just copy the file when saving as PDF file
    else {
        BOOL ok = CopyFileEx(srcFileName, realDstFileName, NULL, NULL, NULL, 0);
        if (ok) {
            // Make sure that the copy isn't write-locked or hidden
            const DWORD attributesToDrop = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
            DWORD attributes = GetFileAttributes(realDstFileName);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & attributesToDrop))
                SetFileAttributes(realDstFileName, attributes & ~attributesToDrop);
        } else {
            TCHAR *msgBuf, *errorMsg;
            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPTSTR)&msgBuf, 0, NULL)) {
                errorMsg = tstr_printf(_T("%s\n\n%s"), _TR("Failed to save a file"), msgBuf);
                LocalFree(msgBuf);
            } else {
                errorMsg = tstr_dup(_TR("Failed to save a file"));
            }
            MessageBox(win->hwndFrame, errorMsg, _TR("Warning"), MB_OK | MB_ICONEXCLAMATION);
            free(errorMsg);
        }
    }
    if (realDstFileName != dstFileName)
        free(realDstFileName);
}

static void OnMenuOpen(WindowInfo *win)
{
    OPENFILENAME  ofn = {0};
    TCHAR         fileName[260];

    if (gRestrictedUse) return;

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
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->rotateBy(-90);
}

static void RotateRight(WindowInfo *win)
{
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
        if (DM_SINGLE_PAGE == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
            win->dm->goToPage(si.nPos + 1, 0);
        else if (DM_FACING == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
            win->dm->goToPage(si.nPos * 2 + 1, 0);
        else if (DM_BOOK_VIEW == win->dm->displayMode() && ZOOM_FIT_PAGE == win->dm->zoomVirtual())
            win->dm->goToPage(si.nPos * 2 + (si.nPos > 0 ? 0 : 1), 0);
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

static bool GetAcrobatPath(TCHAR * buffer=NULL, int bufSize=NULL)
{
    TCHAR path[MAX_PATH];

    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    bool foundAcrobat = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe"), NULL, path, dimof(path)) || ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe"), NULL, path, dimof(path));
    if (foundAcrobat && buffer)
        lstrcpyn(buffer, path, bufSize);

    return foundAcrobat && file_exists(path);
}

// The result value contains major and minor version in the high resp. the low WORD
static DWORD GetFileVersion(TCHAR *path)
{
    DWORD fileVersion = 0;
    DWORD handle;
    DWORD size = GetFileVersionInfoSize(path, &handle);
    LPVOID versionInfo = malloc(size);

    if (GetFileVersionInfo(path, handle, size, versionInfo)) {
        VS_FIXEDFILEINFO *fileInfo;
        UINT len;
        if (VerQueryValue(versionInfo, _T("\\"), (LPVOID *)&fileInfo, &len))
            fileVersion = fileInfo->dwFileVersionMS;
    }

    free(versionInfo);
    return fileVersion;
}

static bool CanViewWithAcrobat(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (win && (!win->loadedFilePath || !file_exists(win->loadedFilePath)))
        return false;
    return GetAcrobatPath() != NULL;
}

static void ViewWithAcrobat(WindowInfo *win)
{
    if (gRestrictedUse) return;

    if (!win || !win->loadedFilePath)
        return;

    TCHAR acrobatPath[MAX_PATH];
    if (!GetAcrobatPath(acrobatPath, dimof(acrobatPath)))
        return;

    TCHAR *params;
    // Command line format for version 6 and later:
    //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf
    // TODO: Also set zoom factor and scroll to current position?
    if (!win->dm)
        params = tstr_printf(_T("\"%s\""), win->loadedFilePath);
    else if (HIWORD(GetFileVersion(acrobatPath)) >= 6)
        params = tstr_printf(_T("/A \"page=%d\" \"%s\""), win->dm->currentPageNo(), win->dm->fileName());
    else
        params = tstr_printf(_T("\"%s\""), win->dm->fileName());
    exec_with_params(acrobatPath, params, FALSE);
    free(params);
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
static IDataObject* GetDataObjectForFile(LPCTSTR pszPath, HWND hwnd=NULL)
{
    IDataObject* pDataObject = NULL;
    IShellFolder *pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr))
        return NULL;

    LPWSTR lpWPath = tstr_to_wstr(pszPath);
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(NULL, NULL, lpWPath, NULL, &pidl, NULL);
    if (SUCCEEDED(hr)) {
        IShellFolder *pShellFolder;
        LPCITEMIDLIST pidlChild;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
        if (SUCCEEDED(hr)) {
            pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, NULL, (void **)&pDataObject);
            pShellFolder->Release();
        }
        CoTaskMemFree(pidl);
    }
    pDesktopFolder->Release();

    free(lpWPath);
    return pDataObject;
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE); 

static bool CanSendAsEmailAttachment(WindowInfo *win)
{
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (win && (!WindowInfo_PdfLoaded(win) || !file_exists(win->dm->fileName())))
        return false;

    IDropTarget *pDropTarget = NULL;
    if (FAILED(CoCreateInstance(CLSID_SendMail, NULL, CLSCTX_ALL, IID_IDropTarget, (void **)&pDropTarget)))
        return false;
    pDropTarget->Release();
    return true;
}

static bool SendAsEmailAttachment(WindowInfo *win)
{
    if (gRestrictedUse) return false;

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

static void OnMenuViewSinglePage(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_SINGLE_PAGE, true);
}

static void OnMenuViewFacing(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_FACING, true);
}

static void OnMenuViewBook(WindowInfo *win)
{
    assert(win);
    if (!win) return;
    SwitchToDisplayMode(win, DM_BOOK_VIEW, true);
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
    if (win->presentation)
        gGlobalPrefs.m_windowState = win->_windowStateBeforePresentation;
    else if (win->fullScreen)
        gGlobalPrefs.m_windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win->hwndFrame))
        gGlobalPrefs.m_windowState = WIN_STATE_NORMAL;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL != gGlobalPrefs.m_windowState || IsIconic(win->hwndFrame) || win->presentation)
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
    for (WindowInfo *win = gWindowList; win; win = win->next) {
        WindowInfo_RebuildMenu(win);
        // Setting the menu for a full screen window messes things up
        if (!win->fullScreen && PM_DISABLED == win->presentation)
            SetMenu(win->hwndFrame, win->hMenu);
        MenuUpdateStateForWindow(win);
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

    for (win = gWindowList; win; win = win->next) {
        if (gGlobalPrefs.m_showToolbar)
            ShowWindow(win->hwndReBar, SW_SHOW);
        else
            ShowWindow(win->hwndReBar, SW_HIDE);
        RECT rect;
        GetClientRect(win->hwndFrame, &rect);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect_dx(&rect),rect_dy(&rect)));
        MenuUpdateShowToolbarStateForWindow(win);
    }
}

static void OnMenuSettings(WindowInfo *win)
{
    if (gRestrictedUse) return;

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
    SwitchToDisplayMode(win, newMode, false);
}

static void OnMenuGoToNextPage(WindowInfo *win)
{
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToNextPage(0);
}

static void OnMenuGoToPrevPage(WindowInfo *win)
{
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToPrevPage(0);
}

static void OnMenuGoToLastPage(WindowInfo *win)
{
    if (!WindowInfo_PdfLoaded(win))
        return;
    win->dm->goToLastPage();
}

static void OnMenuGoToFirstPage(WindowInfo *win)
{
    if (!WindowInfo_PdfLoaded(win))
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
    if (!WindowInfo_PdfLoaded(win))
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
    if (!WindowInfo_PdfLoaded(win))
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.m_showToolbar && !win->fullScreen && PM_DISABLED == win->presentation) {
        win->FindStart();
        return;
    }

    const TCHAR * previousFind = win_get_text(win->hwndFindBox);
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

        Find(win, FIND_FORWARD);
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

static void WindowInfo_EnterFullscreen(WindowInfo *win, bool presentation)
{
    if ((presentation ? win->presentation : win->fullScreen) || !IsWindowVisible(win->hwndFrame)) 
        return;

    if (presentation) {
        if (win->fullScreen)
            win->_windowStateBeforePresentation = WIN_STATE_FULLSCREEN;
        else if (IsZoomed(win->hwndFrame))
            win->_windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        else
            win->_windowStateBeforePresentation = WIN_STATE_NORMAL;
        win->presentation = PM_ENABLED;
        win->_tocBeforePresentation = win->dm ? win->dm->_showToc : FALSE;
    }
    else {
        win->fullScreen = true;
        win->_tocBeforeFullScreen = win->dm ? win->dm->_showToc : FALSE;
    }

    // Remove TOC from full screen, add back later on exit fullscreen
    if (win->dm && win->dm->_showToc)
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
        w = rect_dx(&mi.rcMonitor);
        h = rect_dy(&mi.rcMonitor);
    }
    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    if (!presentation || !win->fullScreen)
        win->prevStyle = ws;
    ws &= ~(WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    GetWindowRect(win->hwndFrame, &win->frameRc);

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
    if (!win->fullScreen && PM_DISABLED == win->presentation) 
        return;

    bool wasPresentation = PM_DISABLED != win->presentation;
    if (wasPresentation) {
        win->dm->setPresentationMode(false);
        win->presentation = PM_DISABLED;
    }
    else
        win->fullScreen = false;

    if (win->dm && (wasPresentation ? win->_tocBeforePresentation : win->_tocBeforeFullScreen))
        win->ShowTocBox();

    if (win->fullScreen) {
        assert(wasPresentation);
        return;
    }

    if (gGlobalPrefs.m_showToolbar)
        ShowWindow(win->hwndReBar, SW_SHOW);
    SetMenu(win->hwndFrame, win->hMenu);
    SetWindowLong(win->hwndFrame, GWL_STYLE, win->prevStyle);
    SetWindowPos(win->hwndFrame, HWND_NOTOPMOST,
                 win->frameRc.left, win->frameRc.top,
                 rect_dx(&win->frameRc), rect_dy(&win->frameRc),
                 SWP_FRAMECHANGED|SWP_NOZORDER);
}

static void OnMenuViewPresentation(WindowInfo *win)
{
    assert(win);
    if (!win)
        return;

    if (win->presentation != PM_DISABLED)
        WindowInfo_ExitFullscreen(win);
    else
        WindowInfo_EnterFullscreen(win, true);
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

    if (win->presentation) {
        WindowInfo_ExitFullscreen(win);
        WindowInfo_EnterFullscreen(win);
    }
    else if (win->fullScreen)
        WindowInfo_ExitFullscreen(win);
    else
        WindowInfo_EnterFullscreen(win);
}

static void WindowInfo_ShowSearchResult(WindowInfo *win, PdfSearchResult *result)
{
    PdfPageInfo *pdfPage = win->dm->getPageInfo(result->page);
    RectI pageOnScreen = {
        pdfPage->screenX,
        pdfPage->screenY,
        pdfPage->bitmapDx,
        pdfPage->bitmapDy
    };

    DeleteOldSelectionInfo(win);
    for (int i = 0; i < result->len; i++) {
        RectI rect = {
            result->rects[i].left,
            result->rects[i].top,
            result->rects[i].right - result->rects[i].left,
            result->rects[i].bottom - result->rects[i].top
        };
        assert(result->rects[i].bottom >= result->rects[i].top);
        assert(result->rects[i].right >= result->rects[i].left);

        RectI intersect;
        if (RectI_Intersect(&rect, &pageOnScreen, &intersect)) {
            SelectionOnPage *selOnPage = (SelectionOnPage *)malloc(sizeof(SelectionOnPage));
            RectD_FromRectI(&selOnPage->selectionPage, &intersect);
            selOnPage->pageNo = result->page;
            win->dm->rectCvtScreenToUser(&selOnPage->pageNo, &selOnPage->selectionPage);
            selOnPage->next = win->selectionOnPage;
            win->selectionOnPage = selOnPage;
        }
    }

    win->showSelection = true;
    triggerRepaintDisplayNow(win);
}

// Show a message for 3000 millisecond at most
static DWORD WINAPI ShowMessageThread(LPVOID data)
{
    WindowInfo *win = (WindowInfo *)data;
    ShowWindowAsync(win->hwndFindStatus, SW_SHOWNA);
    WaitForSingleObject(win->stopFindStatusThreadEvent, 3000);
    ShowWindowAsync(win->hwndFindStatus, SW_HIDE);
    return 0;
}

// Display the message 'message' asynchronously
// If resize = true then the window width is adjusted to the length of the text
static void WindowInfo_ShowMessage_Asynch(WindowInfo *win, const TCHAR *message, bool resize)
{
    if (message)
        win_set_text(win->hwndFindStatus, message);
    if (resize) {
        // compute the length of the message
        RECT rc = {0,0,FIND_STATUS_WIDTH,0};
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
void WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, vector<RectI> &rects)
{
    win->fwdsearchmarkRects.clear();
    if (ret == PDFSYNCERR_SUCCESS && rects.size()>0 ) {
        // remember the position of the search result for drawing the rect later on
        const PdfPageInfo *pi = win->dm->getPageInfo(page);
        if (pi) {
            WindowInfo_HideMessage(win);

            RectI overallrc;
            RectI rc = rects[0];
            win->pdfsync->convert_coord_from_internal(&rc, (int)pi->pageDy, BottomLeft);

            overallrc = rc;
            for (UINT i = 0; i <rects.size(); i++)
            {
                rc = rects[i];
                win->pdfsync->convert_coord_from_internal(&rc, (int)pi->pageDy, BottomLeft);
                overallrc = RectI_Union(overallrc, rc);
                win->fwdsearchmarkRects.push_back(rc);
            }
            win->fwdsearchmarkPage = page;
            win->showForwardSearchMark = true;

            // Scroll to show the overall highlighted zone
            RECT rcRes;
            rcRes.left = overallrc.x;
            rcRes.top = overallrc.y;
            rcRes.right = overallrc.x + overallrc.dx;
            rcRes.bottom = overallrc.y + overallrc.dy;
            PdfSearchResult res = { page, 1, &rcRes };
            win->dm->goToPage(page, 0, true);
            win->dm->MapResultRectToScreen(&res);
            if (IsIconic(win->hwndFrame))
                ShowWindowAsync(win->hwndFrame, SW_RESTORE);
            return;
        }
    }

    TCHAR buf[MAX_PATH];    
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND )
        _sntprintf(buf, dimof(buf), _TR("No synchronization file found"));
    else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
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
        WindowInfo_ShowMessage_Asynch(win, NULL, false);
    else if (!win->dm->bFoundText)
        WindowInfo_ShowMessage_Asynch(win, _TR("No matches were found"), false);
    else {
        TCHAR buf[256];
        wsprintf(buf, _TR("Found text at page %d"), win->dm->currentPageNo());
        WindowInfo_ShowMessage_Asynch(win, buf, false);
    }    
}

static void OnMenuFindNext(WindowInfo *win)
{
    Find(win, FIND_FORWARD);
}

static void OnMenuFindPrev(WindowInfo *win)
{
    Find(win, FIND_BACKWARD);
}

static void OnMenuFindMatchCase(WindowInfo *win)
{
    DWORD state = SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win->dm->SetFindMatchCase((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindBox, TRUE);
}

static void AdvanceFocus(WindowInfo *win)
{
    // Tab order: Frame -> Page -> Find -> ToC -> Frame -> ...

    bool reversed = WasKeyDown(VK_SHIFT);
    bool hasToolbar = !win->fullScreen && !win->presentation && gGlobalPrefs.m_showToolbar;
    bool hasToC = win->dm && win->dm->_showToc;

    HWND current = GetFocus();
    HWND next = win->hwndFrame;

    if (current == win->hwndFrame) {
        if ((!hasToolbar || reversed) && hasToC)
            next = win->hwndTocBox;
        else if (!reversed && hasToolbar)
            next = win->hwndPageBox;
        else if (reversed && hasToolbar)
            next = win->hwndFindBox;
    } else if (current == win->hwndTocBox) {
        if (reversed && hasToolbar)
            next = win->hwndFindBox;
    } else if (current == win->hwndPageBox) {
        if (!reversed)
            next = win->hwndFindBox;
    } else if (current == win->hwndFindBox) {
        if (reversed)
            next = win->hwndPageBox;
        else if (hasToC)
            next = win->hwndTocBox;
    }

    SetFocus(next);
}

static bool OnKeydown(WindowInfo *win, int key, LPARAM lparam, bool inTextfield=false)
{
    if (!win || !win->dm)
        return false;
    
    //DBG_OUT("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    if (VK_PRIOR == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToPrevPage(0);
    } else if (VK_NEXT == key) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->goToNextPage(0);
    } else if (VK_UP == key) {
        if (win->dm->needVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        else
            win->dm->goToPrevPage(0);
    } else if (VK_DOWN == key) {
        if (win->dm->needVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        else
            win->dm->goToNextPage(0);
    } else if (inTextfield) {
        // The remaining keys have a different meaning
        return false;
    } else if (VK_SPACE == key) {
        return OnKeydown(win, WasKeyDown(VK_SHIFT) ? VK_PRIOR : VK_NEXT, 0, inTextfield);
    } else if (VK_LEFT == key) {
        if (win->dm->needHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, WasKeyDown(VK_SHIFT) ? SB_PAGELEFT : SB_LINELEFT, 0);
        else
            win->dm->goToPrevPage(0);
    } else if (VK_RIGHT == key) {
        if (win->dm->needHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, WasKeyDown(VK_SHIFT) ? SB_PAGERIGHT : SB_LINERIGHT, 0);
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
    triggerRepaintDisplayNow(win);
}

static void OnChar(WindowInfo *win, int key)
{
//    DBG_OUT("char=%d,%c\n", key, (char)key);

    if (!win) return;
    if (IsCharUpper((TCHAR)key))
        key = (TCHAR)CharLower((LPTSTR)(TCHAR)key);

    switch (key) {
    case VK_ESCAPE:
        if (win->findThread)
            WindowInfo_AbortFinding(win);
        else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
            ChangePresentationMode(win, PM_ENABLED);
        else if (win->presentation)
            OnMenuViewPresentation(win);
        else if (gGlobalPrefs.m_escToExit)
            CloseWindow(win, TRUE);
        else if (win->fullScreen)
            OnMenuViewFullscreen(win);
        else
            ClearSearch(win);
        return;
    case 'q':
        CloseWindow(win, TRUE);
        return;
    case 'r':
        WindowInfo_Refresh(win, false);
        return;
    }

    if (!win->dm)
        return;

    switch (key) {
    case VK_TAB: AdvanceFocus(win); break;
    case VK_BACK:
        {
            bool forward = WasKeyDown(VK_SHIFT);
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
        WindowInfo_ToggleZoom(win);
        break;
    case '+':
        win->dm->zoomBy(ZOOM_IN_FACTOR);
        break;
    case '-':
        win->dm->zoomBy(ZOOM_OUT_FACTOR);
        break;
    case '/':
        OnMenuFind(win);
        break;
    case 'c':
        {
            DisplayMode newMode = DM_CONTINUOUS;
            if (displayModeShowCover(win->dm->displayMode()))
                newMode = DM_CONTINUOUS_BOOK_VIEW;
            else if (displayModeFacing(win->dm->displayMode()))
                newMode = DM_CONTINUOUS_FACING;
            SwitchToDisplayMode(win, newMode, false);
        }
        break;
    case 'b':
        if (PM_BLACK_SCREEN == win->presentation)
            ChangePresentationMode(win, PM_ENABLED);
        else if (win->presentation)
            ChangePresentationMode(win, PM_BLACK_SCREEN);
        else {
            // experimental "e-book view": flip a single page
            bool forward = !WasKeyDown(VK_SHIFT);
            bool alreadyFacing = displayModeFacing(win->dm->displayMode());
            int currPage = win->dm->currentPageNo();

            DisplayMode newMode = DM_BOOK_VIEW;
            if (displayModeShowCover(win->dm->displayMode()))
                newMode = DM_FACING;
            if (displayModeContinuous(win->dm->displayMode()))
                newMode = DM_BOOK_VIEW == newMode ? DM_CONTINUOUS_BOOK_VIEW : DM_CONTINUOUS_FACING;
            SwitchToDisplayMode(win, newMode, false);

            if (!alreadyFacing)
                ; // don't do anything further
            else if (forward && currPage >= win->dm->currentPageNo() &&
                     (currPage > 1 || newMode == DM_BOOK_VIEW || newMode == DM_CONTINUOUS_BOOK_VIEW))
                win->dm->goToNextPage(0);
            else if (!forward && currPage <= win->dm->currentPageNo())
                win->dm->goToPrevPage(0);
        }
        break;
    case 'w':
        if (PM_WHITE_SCREEN == win->presentation)
            ChangePresentationMode(win, PM_ENABLED);
        else if (win->presentation)
            ChangePresentationMode(win, PM_WHITE_SCREEN);
        break;
    case 'i':
        // experimental "page info" tip: make figuring out current page and
        // total pages count a one-key action (unless they're already visible)
        if (!gGlobalPrefs.m_showToolbar || win->fullScreen || PM_ENABLED == win->presentation) {
            int current = win->dm->currentPageNo(), total = win->dm->pageCount();
            TCHAR *pageInfo = tstr_printf(_T("%s %d / %d"), _TR("Page:"), current, total);
            WindowInfo_ShowMessage_Asynch(win, pageInfo, true);
            free(pageInfo);
        }
        break;
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
    for (FileHistoryList* curr = gFileHistoryRoot; curr; curr = curr->next) {
        if (curr->menuId == menuId)
            return tstr_dup(curr->state.filePath);
    }
    return NULL;
}

static void OnMenuContributeTranslation()
{
    LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html"));
}

#define FRAMES_PER_SECS 60
#define ANIM_FREQ_IN_MS  1000 / FRAMES_PER_SECS

static void GoToTocLinkForTVItem(WindowInfo *win, HWND hTV, HTREEITEM hItem=NULL, bool allowExternal=true)
{
    if (!hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);
    if (win->dm && item.lParam && (allowExternal || PDF_LGOTO == ((pdf_link *)item.lParam)->kind))
        win->dm->goToTocLink((pdf_link *)item.lParam);
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
    TBBUTTONINFO buttonInfo;
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
    for (WindowInfo *win = gWindowList; win; win = win->next) {
        UpdateToolbarPageText(win, -1);
        UpdateToolbarFindText(win);
        UpdateToolbarButtonsToolTipsForWindow(win);
        MenuUpdateStateForWindow(win);
    }        
}

static WNDPROC DefWndProcFindBox = NULL;
static LRESULT CALLBACK WndProcFindBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_CHAR == message) {
        if (VK_ESCAPE == wParam)
        {
            if (win->findThread)
                WindowInfo_AbortFinding(win);
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

typedef struct FindThreadData {
    WindowInfo *win;
    PdfSearchDirection direction;
    BOOL wasModified;
    TCHAR text[256];
} FindThreadData;

static DWORD WINAPI FindThread(LPVOID data)
{
    FindThreadData *ftd = (FindThreadData *)data;
    WindowInfo *win = ftd->win;

    // Suspend page rendering while finding, since MuPDF isn't thread-safe
    LockCache();
    gSuspendRendering = true;
    UnlockCache();
    while (gCurPageRenderReq)
        Sleep(10);

    PdfSearchResult *rect;
    if (ftd->wasModified)
        rect = win->dm->Find(ftd->direction, ftd->text);
    else
        rect = win->dm->Find(ftd->direction);

    if (!win->findCanceled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (FIND_FORWARD == ftd->direction) ? 1 : win->dm->pageCount();
        if (!ftd->wasModified || win->dm->currentPageNo() != startPage)
            rect = win->dm->Find(ftd->direction, ftd->text, startPage);
    }
    gSuspendRendering = false;
    free(ftd);

    if (win->findCanceled)
        rect = NULL;
    LPARAM lParam = rect ? ftd->wasModified : win->findCanceled;
    PostMessage(win->hwndFrame, WM_APP_FIND_END, (WPARAM)rect, lParam);

    HANDLE hThread = win->findThread;
    win->findThread = NULL;
    CloseHandle(hThread);

    return 0;
}

static void Find(WindowInfo *win, PdfSearchDirection direction)
{
    WindowInfo_AbortFinding(win);

    FindThreadData *ftd = (FindThreadData *)malloc(sizeof(FindThreadData));
    ftd->win = win;
    ftd->direction = direction;
    GetWindowText(win->hwndFindBox, ftd->text, dimof(ftd->text));
    ftd->wasModified = Edit_GetModify(win->hwndFindBox);
    Edit_SetModify(win->hwndFindBox, FALSE);

    bool hasText = lstrlen(ftd->text) > 0;
    if (hasText)
        win->findThread = CreateThread(NULL, 0, FindThread, ftd, 0, 0);
    else
        free(ftd);
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
        HFONT oldfnt = SelectFont(hdc, gDefaultGuiFont);
        TCHAR text[256];

        GetClientRect(hwnd, &rect);
        GetWindowText(hwnd, text, 256);

        SetBkMode(hdc, TRANSPARENT);
        rect.left += 10;
        rect.top += 4;
        DrawText(hdc, text, lstrlen(text), &rect, DT_LEFT);
        
        int width = MulDiv(FIND_STATUS_WIDTH, win->dpi, USER_DEFAULT_SCREEN_DPI) - 20;
        rect.top += MulDiv(20, win->dpi, USER_DEFAULT_SCREEN_DPI);
        rect.bottom = rect.top + FIND_STATUS_PROGRESS_HEIGHT;
        rect.right = rect.left + width;
        PaintRectangle(hdc, &rect);
        
        int percent = win->findPercent;
        if (percent > 100)
            percent = 100;
        rect.top += 2;
        rect.left += 2;
        rect.right = rect.left + width * percent / 100 - 3;
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

#define TOOLBAR_MIN_ICON_SIZE 16
#define FIND_BOX_WIDTH 160
#define FIND_BOX_PADDING 3
static void UpdateToolbarFindText(WindowInfo *win)
{
    const TCHAR *text = _TR("Find:");
    win_set_text(win->hwndFindText, text);

    RECT findWndRect;
    GetWindowRect(win->hwndFindBg, &findWndRect);
    int findWndDx = rect_dx(&findWndRect);
    int findWndDy = rect_dy(&findWndRect);

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - findWndDy) / 2;

    SIZE size = TextSizeInHwnd(win->hwndFindText, text);
    size.cx += 6;

    MoveWindow(win->hwndFindText, pos_x, (findWndDy - size.cy + 1) / 2 + pos_y, size.cx, size.cy, true);
    MoveWindow(win->hwndFindBg, pos_x + size.cx, pos_y, findWndDx, findWndDy, false);
    MoveWindow(win->hwndFindBox, pos_x + size.cx + FIND_BOX_PADDING, (findWndDy - size.cy + 1) / 2 + pos_y,
        findWndDx - 1.5 * FIND_BOX_PADDING, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = size.cx + findWndDx + 12;
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

static void CreateFindBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND findBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, FIND_BOX_WIDTH * win->uiDPIFactor, TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 4,
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND find = CreateWindowEx(0, WC_EDIT, _T(""), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                            0, 1, FIND_BOX_WIDTH * win->uiDPIFactor - 2, TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 2,
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND status = CreateWindowEx(WS_EX_TOPMOST, FINDSTATUS_CLASS_NAME, _T(""), WS_CHILD|SS_CENTER,
                            0, 0, 0, 0, win->hwndCanvas, (HMENU)0, hInst, NULL);

    SetWindowFont(label, gDefaultGuiFont, true);
    SetWindowFont(find, gDefaultGuiFont, true);
    SetWindowFont(status, gDefaultGuiFont, true);

    if (!DefWndProcToolbar)
        DefWndProcToolbar = (WNDPROC)GetWindowLong(win->hwndToolbar, GWL_WNDPROC);
    SetWindowLong(win->hwndToolbar, GWL_WNDPROC, (LONG)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLong(find, GWL_WNDPROC);
    SetWindowLong(find, GWL_WNDPROC, (LONG)WndProcFindBox);

    win->hwndFindText = label;
    win->hwndFindBox = find;
    win->hwndFindBg = findBg;
    win->hwndFindStatus = status;

    UpdateToolbarFindText(win);
}

#define UWM_PAGE_SET_FOCUS (WM_APP + 1)
static WNDPROC DefWndProcPageBox = NULL;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    if (!win || !win->dm)
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (WM_CHAR == message) {
        if (VK_RETURN == wParam) {
            TCHAR *buf = win_get_text(win->hwndPageBox);
            int newPageNo;
            newPageNo = _ttoi(buf);
            if (win->dm->validPageNo(newPageNo)) {
                win->dm->goToPage(newPageNo, 0, true);
                SetFocus(win->hwndFrame);
            }
            free(buf);
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
    } else if (WM_SETFOCUS == message) {
        PostMessage(hwnd, UWM_PAGE_SET_FOCUS, 0, 0);
    } else if (UWM_PAGE_SET_FOCUS == message) {
        Edit_SetSel(hwnd, 0, -1);
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
    win_set_text(win->hwndPageText, text);
    SIZE size = TextSizeInHwnd(win->hwndPageText, text);
    size.cx += 6;

    RECT pageWndRect;
    GetWindowRect(win->hwndPageBg, &pageWndRect);
    int pageWndDx = rect_dx(&pageWndRect);
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
    MoveWindow(win->hwndPageBg, pos_x + size.cx, pos_y, pageWndDx, pageWndDy, false);
    MoveWindow(win->hwndPageBox, pos_x + size.cx + FIND_BOX_PADDING, (pageWndDy - size.cy + 1) / 2 + pos_y,
        pageWndDx - 1.5 * FIND_BOX_PADDING, size.cy, false);
    MoveWindow(win->hwndPageTotal, pos_x + size.cx + pageWndDx, (pageWndDy - size.cy + 1) / 2 + pos_y, size2.cx, size.cy, false);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = size.cx + pageWndDx + size2.cx + 12;
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
}

static void CreatePageBox(WindowInfo *win, HINSTANCE hInst)
{
    HWND pageBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, PAGE_BOX_WIDTH * win->uiDPIFactor, TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 4,
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND page = CreateWindowEx(0, WC_EDIT, _T("0"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                            0, 1, PAGE_BOX_WIDTH * win->uiDPIFactor - 2, TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor + 2,
                            win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    HWND total = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win->hwndToolbar, (HMENU)0, hInst, NULL);

    SetWindowFont(label, gDefaultGuiFont, true);
    SetWindowFont(page, gDefaultGuiFont, true);
    SetWindowFont(total, gDefaultGuiFont, true);

    if (!DefWndProcPageBox)
        DefWndProcPageBox = (WNDPROC)GetWindowLong(page, GWL_WNDPROC);
    SetWindowLong(page, GWL_WNDPROC, (LONG)WndProcPageBox);

    win->hwndPageText = label;
    win->hwndPageBox = page;
    win->hwndPageBg = pageBg;
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
    // stretch the toolbar bitmaps for higher DPI settings
    // TODO: get nicely interpolated versions of the toolbar icons for higher resolutions
    if (win->uiDPIFactor > 1 && bmp.bmHeight < TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor) {
        bmp.bmWidth *= win->uiDPIFactor;
        bmp.bmHeight *= win->uiDPIFactor;
        hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, bmp.bmWidth, bmp.bmHeight, LR_COPYDELETEORG);
    }
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
                if (tw <= SPLITTER_MIN_WIDTH) break;

                GetClientRect(win->hwndFrame, &r);
                int width = rect_dx(&r) - tw - SPLITTER_DX;
                int height = rect_dy(&r);

                if (gGlobalPrefs.m_showToolbar && !win->fullScreen && !win->presentation) {
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
    SendMessage(hwndFindBox, EM_SETSEL, 0, -1);
    SetFocus(hwndFindBox);
}

bool WindowInfo::FindUpdateStatus(int current, int total)
{
    PostMessage(hwndFrame, WM_APP_FIND_UPDATE, current, total);

    findPercent = current * 100 / total;

    return !findCanceled;
}

static WNDPROC DefWndProcTocBox = NULL;
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
    switch (message) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.m_escToExit)
                DestroyWindow(win->hwndFrame);
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
                        TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
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
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvinsert.itemex.state = entry->open ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)entry->link;
    // Replace unprintable whitespace with regular spaces
    tstr_trans_chars(entry->title, _T("\t\n\v\f\r"), _T("     "));
    tvinsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    if (!entry->pageNo)
        return TreeView_InsertItem(hwnd, &tvinsert);

    tvinsert.itemex.pszText = tstr_printf(_T("%s  %d"), entry->title, entry->pageNo);
    HTREEITEM hItem = TreeView_InsertItem(hwnd, &tvinsert);
    free(tvinsert.itemex.pszText);
    return hItem;
#else
    return TreeView_InsertItem(hwnd, &tvinsert);
#endif
}

static void PopulateTocTreeView(HWND hwnd, PdfTocItem *entry, HTREEITEM parent = NULL)
{
    for (; entry; entry = entry->next) {
        HTREEITEM node = AddTocItemToView(hwnd, entry, parent);
        PopulateTocTreeView(hwnd, entry->child, node);
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
    TCHAR *lpPageNo = item.pszText + lstrlen(item.pszText);
    for (lpPageNo--; lpPageNo > item.pszText && _istdigit(*lpPageNo); lpPageNo--);
    if (lpPageNo > item.pszText && ' ' == *lpPageNo && *(lpPageNo + 1) && ' ' == *--lpPageNo) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, lpPageNo, lstrlen(lpPageNo), &txtSize);
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

    PdfTocItem *toc = dm->getTocTree();
    if (toc) {
        PopulateTocTreeView(hwndTocBox, toc);
        delete toc;
    }
    tocLoaded = true;
}

void WindowInfo::ToggleTocBox()
{
    if (!dm)
        return;
    if (!dm->_showToc) {
        ShowTocBox();
        SetFocus(hwndTocBox);
    } else {
        HideTocBox();
    }
    MenuUpdateBookmarksStateForWindow(this);
}

void WindowInfo::ShowTocBox()
{
    if (!dm->hasTocTree()) {
        dm->_showToc = TRUE;
        return;
    }

    if (PM_BLACK_SCREEN == presentation || PM_WHITE_SCREEN == presentation)
        return;

    LoadTocTree();

    RECT rtoc, rframe;
    int cw, ch, cx, cy;

    GetClientRect(hwndFrame, &rframe);
    GetWindowRect(hwndTocBox, &rtoc);

    if (gGlobalPrefs.m_showToolbar && !fullScreen && !presentation)
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

    dm->_showToc = TRUE;
    WindowInfo_UpdateTocSelection(this, dm->currentPageNo());
}

void WindowInfo::HideTocBox()
{
    RECT r;
    GetClientRect(hwndFrame, &r);

    int cy = 0;
    int cw = rect_dx(&r), ch = rect_dy(&r);

    if (gGlobalPrefs.m_showToolbar && !fullScreen && !presentation)
        cy = gReBarDy + gReBarDyFrame;

    if (GetFocus() == hwndTocBox)
        SetFocus(hwndFrame);

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
    currPageNo = 0;
}

static void CustomizeToCInfoTip(WindowInfo *win, LPNMTVGETINFOTIP nmit)
{
    pdf_link *link = (pdf_link *)nmit->lParam;
    TCHAR *path = win->dm->getLinkPath(link);
    if (!path)
        return;

    RECT rcLine, rcLabel;
    HWND hTV = nmit->hdr.hwndFrom;
    // Display the item's full label, if it's overlong
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLine, FALSE);
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLabel, TRUE);
    if (rcLine.right + 2 < rcLabel.right) {
        TVITEM item;
        item.hItem = nmit->hItem;
        item.mask = TVIF_TEXT;
        item.pszText = nmit->pszText;
        item.cchTextMax = nmit->cchTextMax;
        TreeView_GetItem(hTV, &item);
        tstr_cat_s(nmit->pszText, nmit->cchTextMax, _T("\r\n"));
    }

    tstr_cat_s(nmit->pszText, nmit->cchTextMax, path);
    free(path);
}

static void CreateInfotipForPdfLink(WindowInfo *win, PdfLink *link, AboutLayoutInfoEl *aboutEl=NULL)
{
    if (!link && !aboutEl && !win->infotipVisible)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = win->hwndCanvas;
    ti.uFlags = TTF_SUBCLASS;

    if (link) {
        rect_set(&ti.rect, link->rectCanvas.x, link->rectCanvas.y, link->rectCanvas.dx, link->rectCanvas.dy);
        ti.lpszText = win->dm->getLinkPath(link->link);
        if (ti.lpszText) {
            SendMessage(win->hwndInfotip, win->infotipVisible ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0, (LPARAM)&ti);
            free(ti.lpszText);
            win->infotipVisible = true;
            return;
        }
    }
    if (aboutEl && aboutEl->url) {
        rect_set(&ti.rect, aboutEl->rightTxtPosX, aboutEl->rightTxtPosY, aboutEl->rightTxtDx, aboutEl->rightTxtDy);
        ti.lpszText = (TCHAR *)aboutEl->url;
        SendMessage(win->hwndInfotip, win->infotipVisible ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0, (LPARAM)&ti);
        win->infotipVisible = true;
        return;
    }

    SendMessage(win->hwndInfotip, TTM_DELTOOL, 0, (LPARAM)&ti);
    win->infotipVisible = false;
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

        case WM_MOUSEMOVE:
            if (win)
                OnMouseMove(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDBLCLK:
            if (win) {
#ifndef _TEX_ENHANCEMENT
                if (win->fullScreen || win->presentation)
                    OnMouseLeftButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
                else
#endif
                OnMouseLeftButtonDblClk(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
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
                OnMouseMiddleButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
            return 0;

        case WM_RBUTTONUP:
            if (win)
                OnMouseRightButtonClick(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_SETCURSOR:
            if (win && WS_ABOUT == win->state) {
                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    AboutLayoutInfoEl *aboutEl;
                    if (AboutGetLink(win, pt.x, pt.y, &aboutEl)) {
                        CreateInfotipForPdfLink(win, NULL, aboutEl);
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
            } else if (win && (MA_DRAGGING == win->mouseAction || MA_MAYBEDRAGGING == win->mouseAction)) {
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
                        CreateInfotipForPdfLink(win, link);
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
            }
            CreateInfotipForPdfLink(win, NULL);
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_TIMER:
            assert(win);
            if (win) {
                if (REPAINT_TIMER_ID == wParam) {
                    KillTimer(hwnd, REPAINT_TIMER_ID);
                    WindowInfo_RedrawAll(win);
                } else if (SMOOTHSCROLL_TIMER_ID == wParam) {
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
    HttpReqCtx *    ctx;

    win = WindowInfo_FindByHwnd(hwnd);

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
                    // for accelerators, lParam is 0 (FALSE), so
                    // make a value of 1 (TRUE) mean autorefresh
                    WindowInfo_Refresh(win, !!lParam);
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
                    OnMenuContributeTranslation();
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

                case IDM_SEND_BY_EMAIL:
                    SendAsEmailAttachment(win);
                    break;

                case IDM_PROPERTIES:
                    OnMenuProperties(win);
                    break;

                case IDM_MOVE_FRAME_FOCUS:
                    if (win->hwndFrame != GetFocus())
                        SetFocus(win->hwndFrame);
                    else if (win->dm && win->dm->_showToc)
                        SetFocus(win->hwndTocBox);
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
                        CopyPropertiesToClipboard(win);
                    else if (win->selectionOnPage)
                        CopySelectionToClipboard(win);
                    else
                        WindowInfo_ShowMessage_Asynch(win, _TR("Select content with Ctrl+left mouse button"), true);
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        case WM_CHAR:
            OnChar(win, wParam);
            break;

        case WM_KEYDOWN:
            OnKeydown(win, wParam, lParam);
            break;

        case WM_INITMENUPOPUP:
            if (GetMenuItemID((HMENU)wParam, 0) == menuDefZoom[0].m_id) {
                if (win)
                    MenuUpdateZoom(win);
            }
            else if (GetMenuItemID((HMENU)wParam, 0) == menuDefGoTo[0].m_id) {
                if (win && WS_SHOWING_PDF == win->state) {
                    EnableMenuItem((HMENU)wParam, IDM_GOTO_NAV_BACK,
                        MF_BYCOMMAND | (win->dm && win->dm->canNavigate(-1) ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem((HMENU)wParam, IDM_GOTO_NAV_FORWARD,
                        MF_BYCOMMAND | (win->dm && win->dm->canNavigate(1) ? MF_ENABLED : MF_GRAYED));
                }
            }
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

            // Note: not all mouse drivers correctly report the Ctrl key's state
            if ((LOWORD(wParam) & MK_CONTROL) || WasKeyDown(VK_CONTROL))
            {
                if ((short)HIWORD(wParam) < 0)
                    win->dm->zoomBy(ZOOM_OUT_FACTOR);
                else
                    win->dm->zoomBy(ZOOM_IN_FACTOR);
                return 0;
            }

            if (gDeltaPerLine == 0)
               break;

            if (!displayModeContinuous(win->dm->displayMode())) {
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
            ctx = (HttpReqCtx*)wParam;
            if (win)
                OnUrlDownloaded(win, ctx);
            else
                delete ctx;
            break;

        case WM_APP_FIND_UPDATE:
            if (!win->findStatusVisible)
                WindowInfo_ShowFindStatus(win);

            {
                TCHAR buf[256];
                wsprintf(buf, _TR("Searching %d of %d..."), (int)wParam, (int)lParam);
                win_set_text(win->hwndFindStatus, buf);
            }
            break;

        case WM_APP_FIND_END:
            if (wParam) {
                PdfSearchResult *rect = (PdfSearchResult *)wParam;
                win->dm->goToPage(rect->page, 0, !!lParam); // lParam == wasModified
                win->dm->MapResultRectToScreen(rect);
                WindowInfo_ShowSearchResult(win, rect);
                WindowInfo_HideFindStatus(win);
            } else {
                ClearSearch(win);
                WindowInfo_HideFindStatus(win, !!lParam);
            }
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

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) {
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.hIconSm        = 0;
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
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
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = WndProcCanvas;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = CANVAS_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcAbout;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = ABOUT_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcProperties;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = PROPERTIES_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcSpliter;
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = SPLITER_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;

    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcFindStatus;
    wcex.hCursor        = LoadCursor(NULL, IDC_APPSTARTING);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = FINDSTATUS_CLASS_NAME;
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
    gBrushBlack  = CreateSolidBrush(COL_BLACK);
    gBrushShadow = CreateSolidBrush(COL_WINDOW_SHADOW);

    NONCLIENTMETRICS ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirect(&ncm.lfMessageFont);
    
    gBitmapCloseToc = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_CLOSE_TOC));

    return TRUE;
}

static TStrList *TStrList_FromCmdLine(TCHAR *cmdLine, bool addExe=false)
{
    TCHAR *      exePath;
    TStrList *   strList = NULL;
    TCHAR *      txt;

    assert(cmdLine);

    if (!cmdLine)
        return NULL;

    if (addExe)
    {
        exePath = ExePathGet();
        if (!exePath)
            return NULL;
        if (!TStrList_InsertAndOwn(&strList, exePath)) {
            free((void*)exePath);
            return NULL;
        }
    }

    for (;;) {
        txt = tstr_parse_possibly_quoted(&cmdLine);
        if (!txt)
            break;
        if (!TStrList_InsertAndOwn(&strList, txt)) {
            free((void*)txt);
            break;
        }
    }
    TStrList_Reverse(&strList);
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
        bool suspended = gSuspendRendering;
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
        if (0 == gPageRenderRequestsCount || suspended) {
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
        if (bmp && gGlobalPrefs.m_invertColors)
            for (int i = 0; i < bmp->dx() * bmp->dy() * 3; i++)
                bmp->data()[i] = 255 - bmp->data()[i];
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
    assert(NULL == gPageRenderThreadHandle);

    gPageRenderSem = CreateSemaphore(NULL, 0, semMaxCount, NULL);
    gPageRenderClearQueue = CreateEvent(NULL, FALSE, FALSE, NULL);
    gPageRenderQueueCleared = CreateEvent(NULL, FALSE, FALSE, NULL);
    gPageRenderThreadHandle = CreateThread(NULL, 0, PageRenderThread, NULL, 0, 0);
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

    if (!win->dm->pdfEngine->hasPermission(PDF_PERM_PRINT)) {
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

#define is_arg(txt) tstr_ieq(_T(txt), currArg->str)

/* Parse 'txt' as hex color and return the result in 'destColor' */
static void ParseColor(int *destColor, const TCHAR* txt)
{
    if(!destColor)
        return;
    if (tstr_startswith(txt, _T("0x")))
        txt += 2;
    else if (tstr_startswith(txt, _T("#")))
        txt += 1;
    int r = hex_tstr_decode_byte(&txt);
    if (-1 == r)
        return;
    int g = hex_tstr_decode_byte(&txt);
    if (-1 == g)
        return;
    int b = hex_tstr_decode_byte(&txt);
    if (-1 == b)
        return;
    if (*txt)
        return;
    *destColor = RGB(r,g,b);
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

extern "C" void pdf_destroyfontlistMS(); // in pdf_fontfile.c

#ifdef DEBUG
// Code from http://www.halcyon.com/~ast/dload/guicon.htm
void RedirectIOToConsole(void)
{
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    int hConHandle;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = 500;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stderr, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *(FILE *)_fdopen(hConHandle, "r");
    setvbuf(stdin, NULL, _IONBF, 0);
}
#endif

#define PROCESS_EXECUTE_FLAGS 0x22

/*
 * enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4
 */
typedef HRESULT (WINAPI *_NtSetInformationProcess)(
   HANDLE  ProcessHandle,
   UINT    ProcessInformationClass,
   PVOID   ProcessInformation,
   ULONG   ProcessInformationLength
   );

static void EnableNx(void)
{
   HMODULE ntdll;
   DWORD dep_mode;
   _NtSetInformationProcess ntsip;

   ntdll = LoadLibrary(_T("ntdll.dll"));
   if (ntdll == NULL)
       return;

   ntsip = (_NtSetInformationProcess)GetProcAddress(ntdll, "NtSetInformationProcess");
   if (ntsip != NULL)
   {
       dep_mode = 13; /* ENABLE | DISABLE_ATL | PERMANENT */
       ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS,
           &dep_mode, sizeof(dep_mode));
   }
}

typedef BOOL (WINAPI *procSetProcessDPIAware)(VOID);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    TStrList *          argListRoot;
    TStrList *          fileNames = NULL;
    TCHAR *             benchPageNumStr = NULL;
    MSG                 msg = {0};
    HACCEL              hAccelTable;
    WindowInfo*         win;
    bool                exitOnPrint = false;
    bool                printToDefaultPrinter = false;
    bool                printDialog = false;
    bool                enterPresentation = false;
    TCHAR *             destName = NULL;
    int                 pageNumber = 0;
    TCHAR *             cmdLine;
    bool                firstDocLoaded = false;
#ifdef BUILD_RM_VERSION
    bool                deleteFilesOnClose = false; // Delete the files which were passed into the program by command line.
#endif

#ifdef _DEBUG
    // Memory leak detection
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    //_CrtSetBreakAlloc(421);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);

    EnableNx();
#ifdef _DEBUG
    // in release builds, DPI-awareness is enabled through the manifest
    HMODULE hUser32 = LoadLibrary(_T("user32.dll"));
    if (hUser32) {
        procSetProcessDPIAware SetProcessDPIAware;
        if ((SetProcessDPIAware = GetProcAddress(hUser32, "SetProcessDPIAware")))
            SetProcessDPIAware();
        FreeLibrary(hUser32);
    }
#endif

    u_DoAllTests();

    INITCOMMONCONTROLSEX cex;
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);

    CoInitialize(NULL);

    SerializableGlobalPrefs_Init();

    cmdLine  = GetCommandLine();
    argListRoot = TStrList_FromCmdLine(cmdLine);
    assert(argListRoot);
    if (!argListRoot)
        return 0;

#ifndef BUILD_RM_VERSION
    bool prefsLoaded = Prefs_Load();
#else
    bool prefsLoaded = false;
#endif

    if (!prefsLoaded) {
        // assume that this is because prefs file didn't exist i.e. this is
        // the first time Sumatra is launched.
        const char *lang = GuessLanguage();
        if (lang)
            CurrLangNameSet(lang);
    }

    /* parse argument list. If -bench was given, then we're in benchmarking mode. Otherwise
    we assume that all arguments are PDF file names.
    -bench can be followed by file or directory name. If file, it can additionally be followed by
    a number which we interpret as page number */
    bool reuse_instance = false;
    TCHAR *printerName = NULL;
    TCHAR *newWindowTitle = NULL;
    for (TStrList *currArg = argListRoot->next; currArg; currArg = currArg->next) {
        if (is_arg("-register-for-pdf")) {
            AssociateExeWithPdfExtension();
            return 0;
        }
        else if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            goto Exit;
        }
        else if (is_arg("-bench") && currArg->next) {
            currArg = currArg->next;
            gBenchFileName = tstr_dup(currArg->str);
            if (currArg->next) {
                currArg = currArg->next;
                benchPageNumStr = tstr_dup(currArg->str);
            }
            break;
        }
        else if (is_arg("-exit-on-print")) {
            exitOnPrint = true;
        }
        else if (is_arg("-print-to-default")) {
            printToDefaultPrinter = true;
        }
        else if (is_arg("-print-to") && currArg->next) {
            currArg = currArg->next;
            printerName = tstr_dup(currArg->str);
        }
        else if (is_arg("-print-dialog")) {
            printDialog = true;
        }
        else if (is_arg("-bgcolor") && currArg->next) {
            currArg = currArg->next;
            ParseColor(&gGlobalPrefs.m_bgColor, currArg->str);
        }
        else if (is_arg("-inverse-search") && currArg->next) {
            currArg = currArg->next;
            free(gGlobalPrefs.m_inverseSearchCmdLine);
            gGlobalPrefs.m_inverseSearchCmdLine = tstr_dup(currArg->str);
        }
        else if (is_arg("-fwdsearch-offset") && currArg->next) {
            currArg = currArg->next;
            gGlobalPrefs.m_fwdsearchOffset = _ttoi(currArg->str);
        }
        else if (is_arg("-fwdsearch-width") && currArg->next) {
            currArg = currArg->next;
            gGlobalPrefs.m_fwdsearchWidth = _ttoi(currArg->str);
        }
        else if (is_arg("-fwdsearch-color") && currArg->next) {
            currArg = currArg->next;
            ParseColor(&gGlobalPrefs.m_fwdsearchColor, currArg->str);
        }
        else if (is_arg("-esc-to-exit")) {
            gGlobalPrefs.m_escToExit = TRUE;
        }
        else if (is_arg("-reuse-instance")) {
            // find the window handle of a running instance of SumatraPDF
            // TODO: there should be a mutex here to reduce possibility of
            // race condition and having more than one copy launch because
            // FindWindow() in one process is called before a window is created
            // in another process
            reuse_instance = FindWindow(FRAME_CLASS_NAME, 0) != NULL;
        }
        else if (is_arg("-lang") && currArg->next) {
            currArg = currArg->next;
            char * s = tstr_to_multibyte(currArg->str, CP_ACP);
            CurrLangNameSet(s);
            free(s);
        }
        else if (is_arg("-nameddest") && currArg->next) {
            currArg = currArg->next;
            destName = tstr_dup(currArg->str);
        }
        else if (is_arg("-page") && currArg->next) {
            currArg = currArg->next;
            pageNumber = _ttoi(currArg->str);
        }
        else if (is_arg("-restrict")) {
            gRestrictedUse = true;
        }
        else if (is_arg("-title") && currArg->next) {
            currArg = currArg->next;
            newWindowTitle = tstr_dup(currArg->str); 
        }
        else if (is_arg("-invertcolors")) {
            gGlobalPrefs.m_invertColors = TRUE;
        }
        else if (is_arg("-presentation")) {
            enterPresentation = true;
        }
#ifdef BUILD_RM_VERSION
        else if (is_arg("-delete-these-on-close")) {
            deleteFilesOnClose = true;
        }
#endif
#ifdef DEBUG
        else if (is_arg("-console")) {
            RedirectIOToConsole();
        }
#endif
        else {
            // Remember this argument as a filename to open
            TStrList_InsertAndOwn(&fileNames, tstr_dup(currArg->str));
        }
    }
    TStrList_Destroy(&argListRoot);
    TStrList_Reverse(&fileNames);

    if (benchPageNumStr) {
        gBenchPageNum = _ttoi(benchPageNumStr);
        if (gBenchPageNum < 1)
            gBenchPageNum = INVALID_PAGE_NO;
        free(benchPageNumStr);
    }

    LoadString(hInstance, IDS_APP_TITLE, gWindowTitle, MAX_LOADSTRING);
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SUMATRAPDF));

    CreatePageRenderThread();
    if (NULL != gBenchFileName) {
        win = LoadPdf(gBenchFileName);
        if (win && WS_SHOWING_PDF == win->state)
            firstDocLoaded = true;
    } else {
        for (TStrList *currArg = fileNames; currArg; currArg = currArg->next) {
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
                    char * destName_utf8 = tstr_to_utf8(destName);
                    win->dm->goToNamedDest(destName_utf8);
                    free(destName_utf8);
                }
                else if (pageNumber > 0 && !firstDocLoaded) {
                    if (win->dm->validPageNo(pageNumber))
                        win->dm->goToPage(pageNumber, 0);
                }
                if (WS_SHOWING_PDF == win->state && enterPresentation && !firstDocLoaded)
                    WindowInfo_EnterFullscreen(win, true);
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
        }
    }

    if (((printerName || printDialog) && exitOnPrint)
          || reuse_instance)
        goto Exit;
 
    if (!firstDocLoaded) {
        bool enterFullscreen = (WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState);
        /* disable benchmark mode if we couldn't open file to benchmark */
        if (gBenchFileName)
            free(gBenchFileName);
        gBenchFileName = NULL;
        win = WindowInfo_CreateEmpty();
        if (!win)
            goto Exit;

        if (WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState ||
            WIN_STATE_MAXIMIZED == gGlobalPrefs.m_windowState)
            ShowWindow(win->hwndFrame, SW_MAXIMIZE);
        else
            ShowWindow(win->hwndFrame, SW_SHOW);
        UpdateWindow(win->hwndFrame);

        if (enterFullscreen)
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

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs.m_pdfAssociateShouldAssociate && win)
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
    if (gBenchFileName)
        free(gBenchFileName);

    FreePageRenderThread();

    WindowInfoList_DeleteAll();
    FileHistoryList_Free(&gFileHistoryRoot);
    DeleteObject(gBrushBg);
    DeleteObject(gBrushWhite);
    DeleteObject(gBrushBlack);
    DeleteObject(gBrushShadow);
    DeleteObject(gDefaultGuiFont);

    Translations_FreeData();
    CurrLangNameFree();
    WininetDeinit();
    SerializableGlobalPrefs_Deinit();

#ifdef BUILD_RM_VERSION
    if (deleteFilesOnClose)
    {
        // Delete the files which where passed to the command line.
        // This only really makes sense if we are in restricted use.
        for (TStrList *currArg = fileNames; currArg; currArg = currArg->next)
        {
            TCHAR fullpath[MAX_PATH];
            GetFullPathName(currArg->str, dimof(fullpath), fullpath, NULL);

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
        }
    }
#endif // BUILD_RM_VERSION
    TStrList_Destroy(&fileNames);

    pdf_destroyfontlistMS();

    CoUninitialize();

    //histDump();
    return msg.wParam;
}

