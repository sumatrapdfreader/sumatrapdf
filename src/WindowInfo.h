/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _WINDOWINFO_H_
#define _WINDOWINFO_H_

#include <shlobj.h>
#include "geom_util.h"
#include "DisplayState.h"
#include "FileWatch.h"
#include "PdfSearch.h"
#include "Vec.h"

class DisplayModel;
class Synchronizer;

/* Current state of a window:
  - WS_ERROR_LOADING_PDF - showing an error message after failing to open a PDF
  - WS_SHOWING_PDF - showing a PDF file
  - WS_ABOUT - showing "about" screen */
enum WinState {
    WS_ERROR_LOADING_PDF = 1,
    WS_SHOWING_PDF,
    WS_ABOUT
};

#if 0
// TODO: WindowInfoType is meant to replace WinState
enum WindowInfoType {
    WitInvalid = 0,
    WitAbout,       // WindowInfoAbout
    WitError,       // WindowInfoError
    WitPdf,         // WindowInfoPdf
    WitComic        // WindowInfoComic
};
#endif

/* Describes actions which can be performed by mouse */
enum MouseAction {
    MA_IDLE = 0,
    MA_DRAGGING,
    MA_DRAGGING_RIGHT,
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
class WindowInfo : public PdfSearchTracker, public PasswordUI
{
public:
    WindowInfo(HWND hwnd);
    ~WindowInfo();

    void GetCanvasSize() { 
        GetClientRect(hwndCanvas, &canvasRc);
    }

    int winDx() const { return canvasRc.right - canvasRc.left; }
    int winDy() const { return canvasRc.bottom - canvasRc.top; }
    SizeI winSize() const { return SizeI(winDx(), winDy()); }
    bool IsAboutWindow() const { return WS_ABOUT == state; }

    WinState        state;
    bool            needrefresh; // true if the view of the PDF is not synchronized with the content of the file on disk
    TCHAR *         loadedFilePath;
    bool            threadStressRunning;
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
    HMENU           menu;

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
    bool            dragStartPending;

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
    Vec<RectI>      fwdsearchmarkRects;    // location of the markers in user coordinates
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
    Synchronizer *  pdfsync;

    bool            tocLoaded;
    bool            tocShow;
    // tocState is an array of ids for ToC items that have been expanded/collapsed
    // by the user (tocState[0] is the length of the list)
    int *           tocState;
    PdfTocItem *    tocRoot;

    bool            fullScreen;
    bool            _tocBeforeFullScreen;
    PresentationMode presentation;
    BOOL            _tocBeforePresentation;
    int             _windowStateBeforePresentation;

    long            prevStyle;
    RECT            frameRc;
    RECT            canvasRc;
    POINT           prevCanvasBR;
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

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
    void AbortFinding();
    void FocusPageNoEdit();

    bool DoubleBuffer_New();
    void DoubleBuffer_Show(HDC hdc);
    void DoubleBuffer_Delete();
    void RedrawAll(bool update=false);

    bool PdfLoaded() const { return this->dm != NULL; }
    HTREEITEM TreeItemForPageNo(HTREEITEM hItem, int pageNo);
    void UpdateTocSelection(int currPageNo);
    void UpdateToCExpansionState(HTREEITEM hItem);
    void DisplayStateFromToC(DisplayState *ds);
    void UpdateToolbarState();

    void ResizeIfNeeded(bool resizeWindow=true);
    void ToggleZoom();
    void ZoomToSelection(float factor, bool relative);
    void SwitchToDisplayMode(DisplayMode displayMode, bool keepContinuous=false);
    void MoveDocBy(int dx, int dy);

    virtual bool FindUpdateStatus(int count, int total);
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey);

};

WindowInfo* FindWindowInfoByFile(TCHAR *file);
WindowInfo* FindWindowInfoByHwnd(HWND hwnd);

WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true);
WindowInfo* LoadPdf(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true);
WindowInfo* LoadComicBook(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true);
void        WindowInfo_ShowForwardSearchResult(WindowInfo *win, LPCTSTR srcfilename, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects);

#endif
