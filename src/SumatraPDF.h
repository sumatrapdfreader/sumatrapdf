/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRAPDF_H_
#define SUMATRAPDF_H_

/* TODO: those should be set from the makefile */
// Modify the following defines if you have to target a platform prior to the ones specified below.
// Their meaning: http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx
// and http://blogs.msdn.com/oldnewthing/archive/2007/04/11/2079137.aspx
// We set the features uniformly to Win 2000 or later.
#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT 
#define _WIN32_WINNT 0x0500
// the following is only defined for _WIN32_WINNT >= 0x0600
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0500
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define THREAD_BASED_FILEWATCH

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#ifdef _DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <windows.h>
#include <tchar.h>
#include "resource.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <direct.h>

#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base_util.h"
#include "file_util.h"
#include "geom_util.h"
#include "str_strsafe.h"
#include "strlist_util.h"
#include "win_util.h"
#include "tstr_util.h"
#include "Http.h"

#include "DisplayModel.h"
#include "FileWatch.h"
#include "PdfSync.h"
#include "translations.h"

#define KB 1024
#define MB (1024*KB)
#define GB (1024*MB)

#define APP_NAME_STR            _T("SumatraPDF")
#define CMD_ARG_SEND_CRASHDUMP  _T("/sendcrashdump")

#define COL_WHITE RGB(0xff,0xff,0xff)
#define COL_BLACK RGB(0,0,0)
#define COL_BLUE_LINK RGB(0,0x20,0xa0)
#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)
#define COL_WINDOW_SHADOW RGB(0x40, 0x40, 0x40)
#define COL_PAGE_FRAME RGB(0x88, 0x88, 0x88)

#define LEFT_TXT_FONT           _T("Arial")
#define LEFT_TXT_FONT_SIZE      12
#define RIGHT_TXT_FONT          _T("Arial Black")
#define RIGHT_TXT_FONT_SIZE     12

/* Current state of a window:
  - WS_ERROR_LOADING_PDF - showing an error message after failing to open a PDF
  - WS_SHOWING_PDF - showing a PDF file
  - WS_ABOUT - showing "about" screen */
enum WinState {
    WS_ERROR_LOADING_PDF = 1,
    WS_SHOWING_PDF,
    WS_ABOUT
};

/* When doing "about" animation, remembers the current animation state */
typedef struct {
    HWND        hwnd;
    int         frame;
    UINT_PTR    timerId;
} AnimState;

/* Describes actions which can be performed by mouse */
enum MouseAction {
    MA_IDLE = 0,
    MA_MAYBEDRAGGING,
    MA_DRAGGING,
    MA_SELECTING,
    MA_SCROLLING
};

/* Represents selected area on given page */
typedef struct SelectionOnPage {
    int              pageNo;
    RectD            selectionPage;     /* position of selection rectangle on page */
    RectI            selectionCanvas;   /* position of selection rectangle on canvas */
    SelectionOnPage* next;              /* pointer to next page with selected area
                                         * or NULL if such page not exists */
} SelectionOnPage;

typedef struct PdfPropertiesLayoutEl {
    /* A property is always in format:
    Name (left): Value (right) */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;

    /* data calculated by the layout */
    int             leftTxtPosX;
    int             leftTxtPosY;
    int             leftTxtDx;
    int             leftTxtDy;

    int             rightTxtPosX;
    int             rightTxtPosY;
    int             rightTxtDx;
    int             rightTxtDy;
} PdfPropertiesLayoutEl;

enum { MAX_PDF_PROPERTIES = 128 };

/* Describes information related to one window with (optional) pdf document
   on the screen */
class WindowInfo : public PdfSearchTracker
{
public:
    WindowInfo() {
        dm = NULL;
        dibInfo = NULL;
        next = NULL;
        linkOnLastButtonDown = NULL;
        url = NULL;
        selectionOnPage = NULL;
        tocLoaded = false;
        fullScreen = false;
        hwndFrame = NULL;
        hwndCanvas = NULL;
        hwndToolbar = NULL;
        hwndReBar = NULL;
        hwndFindText = NULL;
        hwndFindBox = NULL;
        hwndFindBg = NULL;
        hwndPageText = NULL;
        hwndPageBox = NULL;
        hwndPageBg = NULL;
        hwndPageTotal = NULL;
        hwndTocBox = NULL;
        hwndSpliter = NULL;
        hwndInfotip = NULL;
        hwndPdfProperties = NULL;

        infotipVisible = false;
        hMenu = NULL;
        hdc = NULL;
        dpi = USER_DEFAULT_SCREEN_DPI;
        uiDPIFactor = 1.0;
        findThread = NULL;
        findCanceled = false;
        findPercent = 0;
        findStatusVisible = false;
        showSelection = false;
        showForwardSearchMark = false;
        mouseAction = MA_IDLE;
        memzero(&animState, sizeof(animState));
        memzero(&selectionRect, sizeof(selectionRect));
        fwdsearchmarkRects.clear();
        needrefresh = false;
        pdfsync = NULL;
        findStatusThread = NULL;
        stopFindStatusThreadEvent = NULL;
        hdcToDraw = NULL;
        hdcDoubleBuffer = NULL;
        bmpDoubleBuffer = NULL;
        title = NULL;
        loadedFilePath = NULL;
        currPageNo = 0;
        pdfPropertiesCount = 0;
    }
    
    void GetCanvasSize() { 
        GetClientRect(hwndCanvas, &canvasRc);
    }

    int winDx() { return rect_dx(&canvasRc); }

    int winDy() { return rect_dy(&canvasRc); }

    SizeI winSize() { return SizeI(rect_dx(&canvasRc), rect_dy(&canvasRc)); }

    /* points to the next element in the list or the first element if
       this is the first element */
    WindowInfo *    next;
    WinState        state;
    bool            needrefresh; // true if the view of the PDF is not synchronized with the content of the file on disk
    TCHAR *         loadedFilePath;

    DisplayModel *  dm;
    HWND            hwndFrame;
    HWND            hwndCanvas;
    HWND            hwndToolbar;
    HWND            hwndReBar;
    HWND            hwndFindText;
    HWND            hwndFindBox;
    HWND            hwndFindBg;
    HWND            hwndFindStatus;
    HWND            hwndPageText;
    HWND            hwndPageBox;
    HWND            hwndPageBg;
    HWND            hwndPageTotal;
    HWND            hwndTocBox;
    HWND            hwndSpliter;
    HWND            hwndInfotip;
    HWND            hwndPdfProperties;

    bool            infotipVisible;
    HMENU           hMenu;

    HDC             hdc;
    BITMAPINFO *    dibInfo;
    int             dpi;
    float           uiDPIFactor;

    HANDLE          findThread;
    bool            findCanceled;
    int             findPercent;
    bool            findStatusVisible;    
    HANDLE          findStatusThread; // handle of the thread showing the status of the search result
    HANDLE          stopFindStatusThreadEvent; // event raised to tell the findstatus thread to stop

    /* bitmap and hdc for (optional) double-buffering */
    HDC             hdcToDraw;
    HDC             hdcDoubleBuffer;
    HBITMAP         bmpDoubleBuffer;

    PdfLink *       linkOnLastButtonDown;
    const TCHAR *   url;

    MouseAction     mouseAction;

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    int             dragPrevPosX, dragPrevPosY;

    /* when dragging, mouse x/y position when dragging was started */
    int             dragStartX, dragStartY;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int             xScrollSpeed, yScrollSpeed;

    AnimState       animState;

    /* when doing a forward search, the result location is highlighted with
     * a rectangular mark in the document. These variables indicate the position of the mark
     * and whether it is visible or not. */
    bool            showForwardSearchMark; // is the mark visible?
    vector<RectI>   fwdsearchmarkRects;    // location of the markers in user coordinates
    int             fwdsearchmarkPage;     // page 

    bool            showSelection;

    /* selection rectangle in screen coordinates
     * while selecting, it represents area which is being selected */
    RectI           selectionRect;

    /* after selection is done, the selected area is converted
     * to user coordinates for each page which has not empty intersection with it */
    SelectionOnPage *selectionOnPage;

    // file change watcher
    FileWatcher     watcher;
    
    // synchronizer based on .pdfsync file
    Synchronizer    *pdfsync;

    bool            tocLoaded;
    bool            fullScreen;

    long            prevStyle;
    RECT            frameRc;
    RECT            canvasRc;
    POINT           prevCanvasBR;

    TCHAR *         title;
    int             currPageNo;

    PdfPropertiesLayoutEl   pdfProperties[MAX_PDF_PROPERTIES];
    int                     pdfPropertiesCount;

    void ShowTocBox();
    void HideTocBox();
    void ClearTocBox();
    void LoadTocTree();
    void ToggleTocBox();

    void FindStart();
    virtual bool FindUpdateStatus(int count, int total);
    void FocusPageNoEdit();

};

WindowInfo* WindowInfo_FindByHwnd(HWND hwnd);
WindowInfo* WindowInfoList_Find(LPTSTR file);
WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true, TCHAR *windowTitle=NULL);
void WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, vector<RectI> &rects);
LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);

bool IsRunningInPortableMode(void);
bool IsExeAssociatedWithPdfExtension(void);
void AssociateExeWithPdfExtension();

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize);
void Win32_Font_Delete(HFONT font);
void LaunchBrowser(const TCHAR *url);

extern HCURSOR gCursorHand;
extern bool gRestrictedUse;
extern HINSTANCE ghinst;

// In SumatraAbout.cpp
#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")

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

void DrawAbout(HWND hwnd, HDC hdc, RECT *rect);
void OnMenuAbout();
LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
const TCHAR *AboutGetLink(WindowInfo *win, int x, int y, AboutLayoutInfoEl **el=NULL);
void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RECT * rect);

// In SumatraProperties.cpp
#define PROPERTIES_CLASS_NAME   _T("SUMATRA_PDF_PROPERTIES")

void FreePdfProperties(WindowInfo *win);
void OnMenuProperties(WindowInfo *win);
void CopyPropertiesToClipboard(WindowInfo *win);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
