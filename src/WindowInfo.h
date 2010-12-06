/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _WINDOWINFO_H_
#define _WINDOWINFO_H_

#include "DisplayModel.h"
#include "FileWatch.h"
#include "PdfSync.h"

/* Current state of a window:
  - WS_ERROR_LOADING_PDF - showing an error message after failing to open a PDF
  - WS_SHOWING_PDF - showing a PDF file
  - WS_ABOUT - showing "about" screen */
enum WinState {
    WS_ERROR_LOADING_PDF = 1,
    WS_SHOWING_PDF,
    WS_ABOUT
};

/* Describes actions which can be performed by mouse */
enum MouseAction {
    MA_IDLE = 0,
    MA_MAYBEDRAGGING,
    MA_DRAGGING,
    MA_SELECTING,
    MA_SCROLLING,
    MA_SELECTING_TEXT
};

enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};

/* Represents selected area on given page */
typedef struct SelectionOnPage {
    int              pageNo;
    RectD            selectionPage;     /* position of selection rectangle on page */
    RectI            selectionCanvas;   /* position of selection rectangle on canvas */
    SelectionOnPage* next;              /* pointer to next page with selected area
                                         * or NULL if such page not exists */
} SelectionOnPage;

/* Describes information related to one window with (optional) pdf document
   on the screen */
class WindowInfo : public PdfSearchTracker
{
public:
    WindowInfo(HWND hwnd) {
        state = WS_ABOUT;
        hwndFrame = hwnd;

        dm = NULL;
        next = NULL;
        linkOnLastButtonDown = NULL;
        url = NULL;
        selectionOnPage = NULL;
        tocLoaded = false;
        fullScreen = false;
        presentation = PM_DISABLED;
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
        hwndTocTree = NULL;
        hwndSpliter = NULL;
        hwndInfotip = NULL;
        hwndPdfProperties = NULL;

        infotipVisible = false;
        hMenu = NULL;
        hdc = NULL;
        findThread = NULL;
        findCanceled = false;
        findPercent = 0;
        findStatusVisible = false;
        showSelection = false;
        showForwardSearchMark = false;
        mouseAction = MA_IDLE;
        ZeroMemory(&selectionRect, sizeof(selectionRect));
        fwdsearchmarkRects.clear();
        fwdsearchmarkHideStep = 0;
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
        xScrollSpeed = 0;
        yScrollSpeed = 0;
        wheelAccumDelta = 0;
        delayedRepaintTimer = 0;
        resizingTocBox = false;
        pluginParent = NULL;

        HDC hdcFrame = GetDC(hwndFrame);
        dpi = GetDeviceCaps(hdcFrame, LOGPIXELSY);
        // round untypical resolutions up to the nearest quarter
        uiDPIFactor = ceil(dpi * 4.0 / USER_DEFAULT_SCREEN_DPI) / 4.0;
        ReleaseDC(hwndFrame, hdcFrame);
    }
    
    void GetCanvasSize() { 
        GetClientRect(hwndCanvas, &canvasRc);
    }

    int winDx() const { return canvasRc.right - canvasRc.left; }
    int winDy() const { return canvasRc.bottom - canvasRc.top; }
    SizeI winSize() const { return SizeI(winDx(), winDy()); }

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
    HWND            hwndTocTree;
    HWND            hwndSpliter;
    HWND            hwndInfotip;
    HWND            hwndPdfProperties;

    bool            infotipVisible;
    HMENU           hMenu;

    HDC             hdc;
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

    pdf_link *      linkOnLastButtonDown;
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

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    bool            showForwardSearchMark; // are the markers visible?
    vector<RectI>   fwdsearchmarkRects;    // location of the markers in user coordinates
    int             fwdsearchmarkPage;     // page 
    int             fwdsearchmarkHideStep; // value used to gradually hide the markers

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
    BOOL            _tocBeforeFullScreen;
    PresentationMode presentation;
    BOOL            _tocBeforePresentation;
    int             _windowStateBeforePresentation;

    long            prevStyle;
    RECT            frameRc;
    RECT            canvasRc;
    POINT           prevCanvasBR;

    TCHAR *         title;
    int             currPageNo;

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;
    bool            resizingTocBox;

    // (browser) parent, when displayed as a frame-less plugin
    HWND            pluginParent;

    void ShowTocBox();
    void HideTocBox();
    void ClearTocBox();
    void LoadTocTree();
    void ToggleTocBox();

    void FindStart();
    virtual bool FindUpdateStatus(int count, int total);
    void FocusPageNoEdit();

    static WindowInfo *FindByHwnd(HWND hwnd);
};

WindowInfo* WindowInfoList_Find(LPTSTR file);
WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true, TCHAR *windowTitle=NULL);
void WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, vector<RectI> &rects);

#endif
