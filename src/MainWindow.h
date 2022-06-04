/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DoubleBuffer;
struct LinkHandler;
struct StressTest;
class SumatraUIAutomationProvider;
struct FrameRateWnd;
struct LabelWithCloseWnd;
struct Splitter;
struct Tooltip;
struct TreeView;
struct CaptionInfo;
struct TabsCtrl;

struct IPageElement;
struct PageDestination;
struct TocItem;
struct DocController;
struct DocControllerCallback;
struct ChmModel;
struct DisplayModel;
struct TabInfo;

struct Annotation;
struct ILinkHandler;

/* Describes actions which can be performed by mouse */
// clang-format off
enum class MouseAction {
    Idle = 0,
    Dragging,
    Selecting,
    Scrolling,
    SelectingText
};
// clang-format on

// clang-format off
enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};
// clang-format on

// WM_GESTURE handling
struct TouchState {
    bool panStarted = false;
    POINTS panPos{};
    int panScrollOrigX = 0;
    double startArg = 0;
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLinkInfo {
    Rect rect;
    char* target = nullptr;
    char* infotip = nullptr;

    explicit StaticLinkInfo(Rect rect, const char* target, const char* infotip = nullptr);
    StaticLinkInfo() = default;
    StaticLinkInfo(const StaticLinkInfo&);
    StaticLinkInfo& operator=(const StaticLinkInfo& other);
    ~StaticLinkInfo();
};

/* Describes information related to one window with (optional) a document
   on the screen */
struct MainWindow {
    explicit MainWindow(HWND hwnd);
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    ~MainWindow();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    bool IsAboutWindow() const;
    bool IsDocLoaded() const;

    DisplayModel* AsFixed() const;
    ChmModel* AsChm() const;

    // TODO: use currentTab->ctrl instead
    DocController* ctrl = nullptr; // owned by currentTab

    Vec<TabInfo*> tabs;
    TabInfo* currentTab = nullptr; // points into tabs

    HWND hwndFrame = nullptr;
    HWND hwndCanvas = nullptr;
    HWND hwndReBar = nullptr;
    HWND hwndToolbar = nullptr;
    HWND hwndFindLabel = nullptr;
    HWND hwndFindEdit = nullptr;
    HWND hwndFindBg = nullptr;
    HWND hwndPageLabel = nullptr;
    HWND hwndPageEdit = nullptr;
    HWND hwndPageBg = nullptr;
    HWND hwndPageTotal = nullptr;
    HWND hwndTbInfoText = nullptr;

    // state related to table of contents (PDF bookmarks etc.)
    HWND hwndTocBox = nullptr;
    UINT_PTR tocBoxSubclassId = 0;

    LabelWithCloseWnd* tocLabelWithClose = nullptr;
    TreeView* tocTreeView = nullptr;

    // whether the current tab's ToC has been loaded into the tree
    bool tocLoaded = false;
    // whether the ToC sidebar is currently visible
    bool tocVisible = false;
    // set to temporarily disable UpdateTocSelection
    bool tocKeepSelection = false;

    // state related to favorites
    HWND hwndFavBox = nullptr;
    LabelWithCloseWnd* favLabelWithClose = nullptr;
    TreeView* favTreeView = nullptr;
    Vec<FileState*> expandedFavorites;

    // vertical splitter for resizing left side panel
    Splitter* sidebarSplitter = nullptr;

    // horizontal splitter for resizing favorites and bookmars parts
    Splitter* favSplitter = nullptr;

    TabsCtrl* tabsCtrl = nullptr;
    bool tabsVisible = false;
    bool tabsInTitlebar = false;
    // keeps the sequence of tab selection. This is needed for restoration
    // of the previous tab when the current one is closed. (Points into tabs.)
    Vec<TabInfo*>* tabSelectionHistory = nullptr;

    HWND hwndCaption = nullptr;
    CaptionInfo* caption = nullptr;
    int extendedFrameHeight = 0;

    Tooltip* infotip = nullptr;

    HMENU menu = nullptr;
    bool isMenuHidden = false; // not persisted at shutdown

    DoubleBuffer* buffer = nullptr;

    MouseAction mouseAction = MouseAction::Idle;
    bool dragRightClick = false; // if true, drag was initiated with right mouse click
    bool dragStartPending = false;

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    Point dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    Point dragStart;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int xScrollSpeed = 0;
    int yScrollSpeed = 0;

    // true while selecting and when currentTab->selectionOnPage != nullptr
    bool showSelection = false;
    // selection rectangle in screen coordinates (only needed while selecting)
    Rect selectionRect;
    // size of the current rectangular selection in document units
    SizeF selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo*> staticLinks;

    bool isFullScreen = false;
    PresentationMode presentation{PM_DISABLED};
    int windowStateBeforePresentation = 0;

    long nonFullScreenWindowStyle = 0;
    Rect nonFullScreenFrameRect;

    Rect canvasRc;      // size of the canvas (excluding any scroll bars)
    int currPageNo = 0; // cached value, needed to determine when to auto-update the ToC selection

    int wheelAccumDelta = 0;
    UINT_PTR delayedRepaintTimer = 0;

    HANDLE printThread = nullptr;
    bool printCanceled = false;

    HANDLE findThread = nullptr;
    bool findCanceled = false;

    ILinkHandler* linkHandler = nullptr;
    IPageElement* linkOnLastButtonDown = nullptr;
    AutoFreeStr urlOnLastButtonDown;
    Annotation* annotationOnLastButtonDown = nullptr;
    Size annotationBeingMovedSize;
    Point annotationBeingMovedOffset;
    HBITMAP bmpMovePattern = nullptr;
    HBRUSH brMovePattern = nullptr;

    DocControllerCallback* cbHandler = nullptr;

    // The target y offset for smooth scrolling.
    // We use a timer to gradually scroll there.
    int scrollTargetY = 0;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show = false; // are the markers visible?
        Vec<Rect> rects;   // location of the markers in user coordinates
        int page = 0;
        int hideStep = 0; // value used to gradually hide the markers
    } fwdSearchMark;

    StressTest* stressTest = nullptr;

    TouchState touchState;

    FrameRateWnd* frameRateWnd = nullptr;

    SumatraUIAutomationProvider* uiaProvider = nullptr;

    void UpdateCanvasSize();
    Size GetViewPortSize() const;
    void RedrawAll(bool update = false) const;
    void RedrawAllIncludingNonClient(bool update = false) const;

    void ChangePresentationMode(PresentationMode mode);

    void Focus() const;

    void ToggleZoom() const;
    void MoveDocBy(int dx, int dy) const;

    void ShowToolTip(const char* text, Rect& rc, bool multiline = false) const;
    void DeleteToolTip() const;

    bool CreateUIAProvider();
};

void UpdateTreeCtrlColors(MainWindow*);
void RepaintAsync(MainWindow*, int delay);
void ClearFindBox(MainWindow*);
void CreateMovePatternLazy(MainWindow*);
void ClearMouseState(MainWindow*);
bool IsRightDragging(MainWindow*);
MainWindow* FindMainWindowByTabInfo(TabInfo*);
MainWindow* FindMainWindowByHwnd(HWND);
bool MainWindowStillValid(MainWindow*);
MainWindow* FindMainWindowByController(DocController*);

extern Vec<MainWindow*> gWindows;
