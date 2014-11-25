/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class DoubleBuffer;
class LinkHandler;
class Notifications;
class SelectionOnPage;
class StressTest;
class SumatraUIAutomationProvider;

struct FrameRateWnd;
struct LabelWithCloseWnd;
struct TabData;
struct SplitterWnd;
class CaptionInfo;

/* Describes actions which can be performed by mouse */
enum MouseAction {
    MA_IDLE = 0,
    MA_DRAGGING,
    MA_DRAGGING_RIGHT,
    MA_SELECTING,
    MA_SCROLLING,
    MA_SELECTING_TEXT
};

enum NotificationGroup {
    NG_RESPONSE_TO_ACTION = 1,
    NG_FIND_PROGRESS,
    NG_PERSISTENT_WARNING,
    NG_PAGE_INFO_HELPER,
    NG_CURSOR_POS_HELPER,
    NG_STRESS_TEST_BENCHMARK,
    NG_STRESS_TEST_SUMMARY,
};

enum NotificationOptions {
    NOS_DEFAULT = 0, // timeout after 3 seconds, no highlight
    NOS_PERSIST = (1 << 0),
    NOS_HIGHLIGHT = (1 << 1),
    NOS_WARNING = NOS_PERSIST | NOS_HIGHLIGHT,
};

enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};

// WM_GESTURE handling
struct TouchState {
    bool    panStarted;
    POINTS  panPos;
    int     panScrollOrigX;
    double  startArg;
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLinkInfo {
    StaticLinkInfo() : target(NULL), infotip(NULL) { }
    StaticLinkInfo(RectI rect, const WCHAR *target, const WCHAR *infotip=NULL) :
        rect(rect), target(target), infotip(infotip) { }

    RectI rect;
    const WCHAR *target;
    const WCHAR *infotip;
};

/* Describes information related to one window with (optional) a document
   on the screen */
class WindowInfo
{
public:
    explicit WindowInfo(HWND hwnd);
    ~WindowInfo();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    bool IsAboutWindow() const { return !loadedFilePath; }
    bool IsDocLoaded() const { return this->ctrl != NULL; }

    DisplayModel *AsFixed() const { return ctrl ? ctrl->AsFixed() : NULL; }
    ChmModel *AsChm() const { return ctrl ? ctrl->AsChm() : NULL; }
    EbookController *AsEbook() const { return ctrl ? ctrl->AsEbook() : NULL; }

    EngineType GetEngineType() const;

    WCHAR *         loadedFilePath;
    Controller *    ctrl;

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
    LabelWithCloseWnd *tocLabelWithClose;
    HWND            hwndTocTree;
    bool            tocLoaded;
    bool            tocVisible;
    // set to temporarily disable UpdateTocSelection
    bool            tocKeepSelection;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int>        tocState;
    DocTocItem *    tocRoot;

    // state related to favorites
    HWND            hwndFavBox;
    LabelWithCloseWnd *favLabelWithClose;
    HWND            hwndFavTree;
    Vec<DisplayState *> expandedFavorites;

    // vertical splitter for resizing left side panel
    SplitterWnd *   sidebarSplitter;

    // horizontal splitter for resizing favorites and bookmars parts
    SplitterWnd *   favSplitter;

    HWND            hwndTabBar;
    bool            tabsVisible;
    bool            tabsInTitlebar;
    // keeps the sequence of tab selection. This is needed for restoration 
    // of the previous tab when the current one is closed.
    Vec<TabData *> *tabSelectionHistory;

    HWND            hwndCaption;
    CaptionInfo *   caption;
    int             extendedFrameHeight;

    HWND            hwndInfotip;

    bool            infotipVisible;
    HMENU           menu;
    bool            isMenuHidden; // not persisted at shutdown

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

    // size of the current rectangular selection in document units
    SizeD           selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo> staticLinks;

    bool            isFullScreen;
    PresentationMode presentation;
    // were we showing toc before entering full screen or presentation mode
    bool            tocBeforeFullScreen;
    int             windowStateBeforePresentation;

    long            nonFullScreenWindowStyle;
    RectI           nonFullScreenFrameRect;
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

    RectI           canvasRc; // size of the canvas (excluding any scroll bars)
    int             currPageNo; // cached value, needed to determine when to auto-update the ToC selection

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;

    Notifications * notifications; // only access from UI thread

    HANDLE          printThread;
    bool            printCanceled;

    HANDLE          findThread;
    bool            findCanceled;

    LinkHandler *   linkHandler;
    PageElement *   linkOnLastButtonDown;
    const WCHAR *   url;

    ControllerCallback *cbHandler;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show;          // are the markers visible?
        Vec<RectI> rects;   // location of the markers in user coordinates
        int page;
        int hideStep;       // value used to gradually hide the markers
    } fwdSearchMark;

    StressTest *    stressTest;

    TouchState      touchState;

    FrameRateWnd *  frameRateWnd;

    SumatraUIAutomationProvider * uia_provider;

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
        SetForegroundWindow(hwndFrame);
        SetFocus(hwndFrame);
    }

    void ToggleZoom();
    void MoveDocBy(int dx, int dy);

    void CreateInfotip(const WCHAR *text, RectI& rc, bool multiline=false);
    void DeleteInfotip();
    void ShowNotification(const WCHAR *message, int options=NOS_DEFAULT, NotificationGroup groupId=NG_RESPONSE_TO_ACTION);

    bool CreateUIAProvider();
};

class LinkHandler {
    WindowInfo *owner;

    void ScrollTo(PageDestination *dest);
    void LaunchFile(const WCHAR *path, PageDestination *link);
    PageDestination *FindTocItem(DocTocItem *item, const WCHAR *name, bool partially=false);

public:
    explicit LinkHandler(WindowInfo& win) : owner(&win) { }

    void GotoLink(PageDestination *link);
    void GotoNamedDest(const WCHAR *name);
};

class LinkSaver : public LinkSaverUI {
    WindowInfo *owner;
    const WCHAR *fileName;

public:
    LinkSaver(WindowInfo& win, const WCHAR *fileName) : owner(&win), fileName(fileName) { }

    virtual bool SaveEmbedded(const unsigned char *data, size_t cbCount);
};
