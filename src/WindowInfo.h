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
class MessageWndList;

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

enum NotificationGroup {
    NG_RESPONSE_TO_ACTION = 1,
    NG_FIND_PROGRESS,
    NG_PRINT_PROGRESS,
    NG_PAGE_INFO_HELPER,
    NG_STRESS_TEST_BENCHMARK,
    NG_STRESS_TEST_SUMMARY,
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

    // state related to PDF bookmarks (aka. table of contents)
    HWND            hwndTocBox;
    HWND            hwndTocTree;
    bool            tocLoaded;
    bool            tocVisible;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int>        tocState;
    DocToCItem *    tocRoot;

    // state related to favorites
    HWND            hwndFavBox;
    HWND            hwndFavTree;
    bool            favVisible;

    // vertical splitter for resizing left side panel
    HWND            hwndPanelSpliter;
    bool            panelBeingResized;

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
    bool            tocBeforeFullScreen;
    bool            tocBeforePresentation;
    int             windowStateBeforePresentation;

    long            prevStyle;
    RectI           frameRc; // window position before entering presentation/fullscreen mode
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

    RectI           canvasRc; // size of the canvas (excluding any scroll bars)
    int             currPageNo; // cached value, needed to determine when to auto-update the ToC selection

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;

    MessageWndList *messages;

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
    struct          {
                        bool show;          // are the markers visible?
                        Vec<RectI> rects;   // location of the markers in user coordinates
                        int page;
                        int hideStep;       // value used to gradually hide the markers
                    } fwdsearchmark;

    CallbackFunc *  stressTest;
    bool            suppressPwdUI;

    void UpdateToolbarState();

    void UpdateCanvasSize();
    SizeI GetViewPortSize();
    void RedrawAll(bool update=false);
    void RepaintAsync(UINT delay=0);
    void Reload(bool autorefresh=false);

    void ChangePresentationMode(PresentationMode mode) {
        presentation = mode;
        RedrawAll();
    }

    void ToggleZoom();
    void ZoomToSelection(float factor, bool relative);
    void SwitchToDisplayMode(DisplayMode displayMode, bool keepContinuous=false);
    void MoveDocBy(int dx, int dy);
    void AbortPrinting();
    void AbortFinding(bool hideMessage=false);

    void ShowTocBox();
    void HideTocBox();
    void ClearTocBox();
    void LoadTocTree();
    void ToggleTocBox();

    HTREEITEM TreeItemForPageNo(HTREEITEM hItem, int pageNo);
    void UpdateTocSelection(int currPageNo);
    void UpdateToCExpansionState(HTREEITEM hItem);
    void DisplayStateFromToC(DisplayState *ds);

    void CreateInfotip(const TCHAR *text, RectI& rc);
    void DeleteInfotip();

    void ShowNotification(const TCHAR *message, bool autoDismiss=true, bool highlight=false, NotificationGroup groupId=NG_RESPONSE_TO_ACTION);
    void ShowForwardSearchResult(const TCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI>& rects);

    // DisplayModelCallback implementation (incl. PasswordUI)
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey);
    virtual void Repaint() { RepaintAsync(); };
    virtual void PageNoChanged(int pageNo);
    virtual void UpdateScrollbars(SizeI canvas);
    virtual void RenderPage(int pageNo);
    virtual int  GetScreenDPI() { return dpi; }
    virtual void CleanUp(DisplayModel *dm);
};

/* Represents selected area on given page */
class SelectionOnPage {
public:
    SelectionOnPage(int pageNo=0, RectD *rect=NULL) :
        pageNo(pageNo), rect(rect ? *rect : RectD()) { }

    int     pageNo; // page this selection is on
    RectD   rect;   // position of selection rectangle on page (in page coordinates)

    // position of selection rectangle in the view port
    RectI   GetRect(DisplayModel *dm);

    static Vec<SelectionOnPage> *FromRectangle(DisplayModel *dm, RectI rect);
    static Vec<SelectionOnPage> *FromTextSelect(TextSel *textSel);
};

class LinkHandler {
    WindowInfo *owner;
    BaseEngine *engine() const;

    void ScrollTo(PageDestination *dest);
    PageDestination *FindToCItem(DocToCItem *item, const TCHAR *name, bool partially=false);

public:
    LinkHandler(WindowInfo& win) : owner(&win) { }

    void GotoLink(PageDestination *link);
    void GotoNamedDest(const TCHAR *name);
};

class LinkSaver : public LinkSaverUI {
    HWND hwnd;
    const TCHAR *fileName;

public:
    LinkSaver(HWND hwnd, const TCHAR *fileName) : hwnd(hwnd), fileName(fileName) { }

    virtual bool SaveEmbedded(unsigned char *data, int cbCount);
};

WindowInfo* FindWindowInfoByFile(TCHAR *file);
WindowInfo* FindWindowInfoByHwnd(HWND hwnd);
WindowInfo* FindWindowInfoBySyncFile(TCHAR *file);
WindowInfo* LoadDocument(const TCHAR *fileName, WindowInfo *win=NULL,
                         bool showWin=true, bool forceReuse=false, bool suppressPwdUI=false);

#endif
