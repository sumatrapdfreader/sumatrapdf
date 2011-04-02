/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef WindowInfo_h
#define WindowInfo_h

#include <shlobj.h>
#include "GeomUtil.h"
#include "DisplayState.h"
#include "PdfSearch.h"
#include "Vec.h"

class DisplayModel;
class FileWatcher;
class Synchronizer;

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

    bool IsAboutWindow() const { return !loadedFilePath; }
    bool PdfLoaded() const { return this->dm != NULL; }

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

    /* bitmap and hdc for (optional) double-buffering */
    HDC             hdcToDraw;
    HDC             hdcDoubleBuffer;
    HBITMAP         bmpDoubleBuffer;

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

    bool            showSelection;

    /* selection rectangle in screen coordinates
     * while selecting, it represents area which is being selected */
    RectI           selectionRect;

    /* after selection is done, the selected area is converted
     * to user coordinates for each page which has not empty intersection with it */
    SelectionOnPage *selectionOnPage;

    // file change watcher
    FileWatcher *   watcher;

    bool            fullScreen;
    bool            _tocBeforeFullScreen;
    PresentationMode presentation;
    bool            _tocBeforePresentation;
    int             _windowStateBeforePresentation;

    long            prevStyle;
    RectI           frameRc; // window position before entering presentation/fullscreen mode
    RectI           canvasRc;
    SizeI           prevCanvasSize;
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

    int             currPageNo;

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;

    // the following properties only apply to PDF documents

    HANDLE          findThread;
    bool            findCanceled;
    int             findPercent;
    bool            findStatusVisible;    
    HANDLE          findStatusThread; // handle of the thread showing the status of the search result
    HANDLE          stopFindStatusThreadEvent; // event raised to tell the findstatus thread to stop

    pdf_link *      linkOnLastButtonDown;
    const TCHAR *   url;

    bool            tocLoaded;
    bool            tocShow;
    // tocState is an array of ids for ToC items that have been expanded/collapsed
    // by the user (tocState[0] is the length of the list)
    int *           tocState;
    PdfTocItem *    tocRoot;
    bool            resizingTocBox;

    // synchronizer based on .pdfsync file
    Synchronizer *  pdfsync;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct          {
                        bool show;          // are the markers visible?
                        Vec<RectI> rects;   // location of the markers in user coordinates
                        int page;
                        int hideStep;       // value used to gradually hide the markers
                    } fwdsearchmark;

    void FocusPageNoEdit();
    void UpdateToolbarState();

    bool DoubleBuffer_New();
    void DoubleBuffer_Show(HDC hdc);
    void DoubleBuffer_Delete();
    void ResizeIfNeeded(bool resizeWindow=true);

    void UpdateCanvasSize();
    void RedrawAll(bool update=false);
    void RepaintAsync(UINT delay=0);
    void Reload(bool autorefresh=false);
    void ChangePresentationMode(PresentationMode mode);

    void ToggleZoom();
    void ZoomToSelection(float factor, bool relative);
    void SwitchToDisplayMode(DisplayMode displayMode, bool keepContinuous=false);
    void MoveDocBy(int dx, int dy);

    // the following methods only apply to PDF documents

    void Find(PdfSearchDirection direction=FIND_FORWARD);
    void FindStart();
    void AbortFinding();

    void ShowTocBox();
    void HideTocBox();
    void ClearTocBox();
    void LoadTocTree();
    void ToggleTocBox();

    HTREEITEM TreeItemForPageNo(HTREEITEM hItem, int pageNo);
    void UpdateTocSelection(int currPageNo);
    void UpdateToCExpansionState(HTREEITEM hItem);
    void DisplayStateFromToC(DisplayState *ds);

    void CreateInfotip(const TCHAR *text=NULL, RectI *rc=NULL);
    void DeleteInfotip();

    void ShowForwardSearchResult(const TCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI>& rects);

    virtual bool FindUpdateStatus(int count, int total);
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey);
};

WindowInfo* FindWindowInfoByFile(TCHAR *file);
WindowInfo* FindWindowInfoByHwnd(HWND hwnd);
WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win=NULL, bool showWin=true);

#endif
