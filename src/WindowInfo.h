/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef WindowInfo_h
#define WindowInfo_h

#include <shlobj.h>
#include "GeomUtil.h"
#include "DisplayModel.h"

class FileWatcher;
class Synchronizer;
class DoubleBuffer;
class SelectionOnPage;
class LinkHandler;
class Notifications;
#ifdef BUILD_RIBBON
class RibbonSupport;
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

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLinkInfo {
    StaticLinkInfo() : target(NULL), infotip(NULL) { }
    StaticLinkInfo(RectI rect, const TCHAR *target, const TCHAR *infotip=NULL) :
        rect(rect), target(target), infotip(infotip) { }

    RectI rect;
    const TCHAR *target;
    const TCHAR *infotip;
};

/* Describes information related to one window with (optional) a document
   on the screen */
class WindowInfo : public DisplayModelCallback
{
public:
    WindowInfo(HWND hwnd);
    ~WindowInfo();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    bool IsAboutWindow() const { return !loadedFilePath; }
    bool IsDocLoaded() const { return this->dm != NULL; }

    bool IsChm() const { return dm && dm->engineType == Engine_Chm; }
    bool IsPdf() const { return dm && dm->engineType == Engine_PDF; }

    TCHAR *         loadedFilePath;
    DisplayModel *  dm;

    HWND            hwndFrame;
    HWND            hwndCanvas;
    HWND            hwndToolbar;
    HWND            hwndReBar;
    HWND            hwndFindText;
    HWND            hwndFindBox;
    HWND            hwndFindBg;
    HWND            hwndPageText;
    HWND            hwndPageBox;
    HWND            hwndPageBg;
    HWND            hwndPageTotal;

    // state related to table of contents (PDF bookmarks etc.)
    HWND            hwndTocBox;
    HWND            hwndTocTree;
    bool            tocLoaded;
    bool            tocVisible;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int>        tocState;
    DocTocItem *    tocRoot;

    // state related to favorites
    HWND            hwndFavBox;
    HWND            hwndFavTree;
    StrVec          expandedFavorites;

    // vertical splitter for resizing left side panel
    HWND            hwndSidebarSplitter;

    // horizontal splitter for resizing favorites and bookmars parts
    HWND            hwndFavSplitter;

    HWND            hwndInfotip;
    HWND            hwndProperties;

    bool            infotipVisible;
    HMENU           menu;

    int             dpi;
    float           uiDPIFactor;

    DoubleBuffer *  buffer;

    MouseAction     mouseAction;
    bool            dragStartPending;

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    PointI          dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    PointI          dragStart;

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
    Vec<SelectionOnPage> *selectionOnPage;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo> staticLinks;

    // file change watcher
    FileWatcher *   watcher;

    bool            fullScreen;
    PresentationMode presentation;
    // were we showing toc before entering full screen or presentation mode
    bool            tocBeforeFullScreen;
    int             windowStateBeforePresentation;

    long            prevStyle;
    RectI           frameRc; // window position before entering presentation/fullscreen mode
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

    RectI           canvasRc; // size of the canvas (excluding any scroll bars)
    int             currPageNo; // cached value, needed to determine when to auto-update the ToC selection

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;

    Notifications * notifications;

    HANDLE          printThread;
    bool            printCanceled;

    HANDLE          findThread;
    bool            findCanceled;

    LinkHandler *   linkHandler;
    PageElement *   linkOnLastButtonDown;
    const TCHAR *   url;

    // synchronizer based on .pdfsync file
    Synchronizer *  pdfsync;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show;          // are the markers visible?
        Vec<RectI> rects;   // location of the markers in user coordinates
        int page;
        int hideStep;       // value used to gradually hide the markers
    } fwdSearchMark;

    CallbackFunc *  stressTest;
    bool            suppressPwdUI;

    // WM_GESTURE handling 
    bool    panStarted;
    POINTS  panPos;
    PointI  panScrollOrig;
    double  startArg;

#ifdef BUILD_RIBBON
    RibbonSupport *ribbonSupport;
#endif

    void  UpdateCanvasSize();
    SizeI GetViewPortSize();
    void  RedrawAll(bool update=false);
    void  RepaintAsync(UINT delay=0);

    void ChangePresentationMode(PresentationMode mode) {
        presentation = mode;
        RedrawAll();
    }

    void Focus() {
        if (IsIconic(hwndFrame))
            ShowWindow(hwndFrame, SW_RESTORE);
        SetFocus(hwndFrame);
    }

    void ToggleZoom();
    void MoveDocBy(int dx, int dy);

    void CreateInfotip(const TCHAR *text, RectI& rc, bool multiline=false);
    void DeleteInfotip();

    // DisplayModelCallback implementation (incl. PasswordUI and ChmNavigationCallback)
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey);
    virtual void PageNoChanged(int pageNo);
    virtual void LaunchBrowser(const TCHAR *url);
    virtual void FocusFrame(bool always);
    virtual void Repaint() { RepaintAsync(); };
    virtual void UpdateScrollbars(SizeI canvas);
    virtual void RenderPage(int pageNo);
    virtual void CleanUp(DisplayModel *dm);
};

class LinkHandler {
    WindowInfo *owner;
    BaseEngine *engine() const;

    void ScrollTo(PageDestination *dest);
    PageDestination *FindTocItem(DocTocItem *item, const TCHAR *name, bool partially=false);

public:
    LinkHandler(WindowInfo& win) : owner(&win) { }

    void GotoLink(PageDestination *link);
    void GotoNamedDest(const TCHAR *name);
};

class LinkSaver : public LinkSaverUI {
    WindowInfo *owner;
    const TCHAR *fileName;

public:
    LinkSaver(WindowInfo& win, const TCHAR *fileName) : owner(&win), fileName(fileName) { }

    virtual bool SaveEmbedded(unsigned char *data, int cbCount);
};

#endif
