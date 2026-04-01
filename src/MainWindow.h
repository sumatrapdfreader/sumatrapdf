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
struct TabsCtrl;

// factor by how large the non-maximized caption should be in relation to the tabbar
#define kCaptionTabBarDyFactor 1.25f

// gap in pixels between top of caption and tabs; this area allows dragging the window
#define kCaptionTopPadding 14

enum CaptionButtons {
    CB_BTN_FIRST = 0,
    CB_MINIMIZE = CB_BTN_FIRST,
    CB_MAXIMIZE,
    CB_RESTORE,
    CB_CLOSE,
    CB_MENU,
    CB_SYSTEM_MENU,
    CB_BTN_COUNT
};

struct ButtonInfo {
    int id = -1; // CaptionButtons value
    Rect rect{};
    bool highlighted = false;
    bool pressed = false;
    bool inactive = false;
    bool visible = true;
    ButtonInfo() = default;
};

struct IPageElement;
struct PageDestination;
struct TocItem;
struct DocController;
struct DocControllerCallback;
struct ChmModel;
struct DisplayModel;
struct WindowTab;

struct Annotation;
struct ILinkHandler;

// Current action being performed with a mouse
enum class MouseAction {
    None = 0,
    Dragging,
    Selecting,
    Scrolling,
    SelectingText
};

enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};

// WM_GESTURE handling
struct TouchState {
    bool panStarted = false;
    POINTS panPos{};
    int panScrollOrigX = 0;
    float zoomIntermediate = 0;
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLink {
    Rect rect;
    char* target = nullptr;
    char* tooltip = nullptr;

    explicit StaticLink(Rect rect, const char* target, const char* infotip = nullptr);
    StaticLink() = default;
    ~StaticLink();
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
    bool IsCurrentTabAbout() const;
    bool IsDocLoaded() const;
    bool HasDocsLoaded() const;

    DisplayModel* AsFixed() const;
    ChmModel* AsChm() const;

    // TODO: use CurrentTab()->ctrl instead
    DocController* ctrl = nullptr; // owned by CurrentTab()

    WindowTab* currentTabTemp = nullptr; // points into tabs
    WindowTab* CurrentTab() const;
    int TabCount() const;
    Vec<WindowTab*> Tabs() const;
    WindowTab* GetTab(int idx) const;
    int GetTabIdx(WindowTab*) const;

    HWND hwndFrame = nullptr;
    HWND hwndCanvas = nullptr;

    HWND hwndReBar = nullptr;
    HWND hwndToolbar = nullptr;
    HWND hwndMenuReBar = nullptr;
    HWND hwndMenuToolbar = nullptr;
    HWND hwndFindLabel = nullptr;
    HWND hwndFindEdit = nullptr;
    HWND hwndFindBg = nullptr;
    HWND hwndPageLabel = nullptr;
    HWND hwndPageEdit = nullptr;
    HWND hwndPageBg = nullptr;
    HWND hwndPageTotal = nullptr;

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
    Vec<WindowTab*>* tabSelectionHistory = nullptr;

    ButtonInfo captionBtn[CB_BTN_COUNT];
    bool isMenuOpen = false;
    Rect captionRect{};

    Tooltip* infotip = nullptr;

    HMENU menu = nullptr;

    DoubleBuffer* buffer = nullptr;

    MouseAction mouseAction = MouseAction::None;
    bool dragRightClick = false; // if true, drag was initiated with right mouse click
    bool dragStartPending = false;
    bool textDragPending = false; // true when mouse down on selected text, waiting for drag

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    Point dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    Point dragStart;

    Size annotationBeingMovedSize;
    Point annotationBeingMovedOffset;
    HBITMAP bmpMovePattern = nullptr;
    HBRUSH brMovePattern = nullptr;
    Annotation* annotationBeingDragged = nullptr;

    // Vars for resizing annotations
    int resizeHandle = 0; // ResizeHandle enum casted to int
    bool annotationBeingResized = false;
    RectF annotationOriginalRect;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int xScrollSpeed = 0;
    int yScrollSpeed = 0;

    // true while selecting and when CurrentTab()->selectionOnPage != nullptr
    bool showSelection = false;
    // selection rectangle in screen coordinates (only needed while selecting)
    Rect selectionRect;
    // size of the current rectangular selection in document units
    SizeF selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLink*> staticLinks;

    // home page thumbnail scrolling
    int homePageScrollY = 0;

    bool isFullScreen = false;
    PresentationMode presentation = PM_DISABLED;
    int windowStateBeforePresentation = 0;
    bool suppressFrameRedraw = false;

    long nonFullScreenWindowStyle = 0;
    Rect nonFullScreenFrameRect;

    Rect canvasRc;      // size of the canvas (excluding any scroll bars)
    int currPageNo = 0; // cached value, needed to determine when to auto-update the ToC selection

    // overlay scrollbars (used when useOverlayScrollbar is true)
    struct OverlayScrollbar* overlayScrollV = nullptr;
    struct OverlayScrollbar* overlayScrollH = nullptr;

    int wheelAccumDelta = 0;
    UINT_PTR delayedRepaintTimer = 0;

    HANDLE printThread = nullptr;
    bool printCanceled = false;

    HANDLE findThread = nullptr;
    bool findCancelled = false;
    bool findMatchCase = false;

    ILinkHandler* linkHandler = nullptr;
    IPageElement* linkOnLastButtonDown = nullptr;
    AutoFreeStr urlOnLastButtonDown;
    Annotation* annotationUnderCursor = nullptr;
    HBRUSH brControlBgColor = nullptr;

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

    // set at the beginning of CloseWindow() to prevent
    // processing commands while closing (e.g. reentrancy
    // via modal dialogs pumping messages)
    bool isBeingClosed = false;

    SumatraUIAutomationProvider* uiaProvider = nullptr;

    void UpdateCanvasSize();
    Size GetViewPortSize() const;
    void RedrawAll(bool update = false) const;
    void RedrawAllIncludingNonClient() const;

    void ChangePresentationMode(PresentationMode mode);
    bool InPresentation() const;

    void Focus() const;

    void ToggleZoom() const;
    void MoveDocBy(int dx, int dy) const;

    void ShowToolTip(const char* text, Rect& rc, bool multiline = false) const;
    void DeleteToolTip() const;

    bool CreateUIAProvider();
};

void UpdateControlsColors(MainWindow*);
void ScheduleRepaint(MainWindow*, int delay);
void ClearFindBox(MainWindow*);
void CreateMovePatternLazy(MainWindow*);
void ClearMouseState(MainWindow*);
bool IsRightDragging(MainWindow*);
MainWindow* FindMainWindowByTab(WindowTab*);
MainWindow* FindMainWindowByHwnd(HWND);
bool IsMainWindowValid(MainWindow*);
MainWindow* FindMainWindowByController(DocController*);
extern Vec<MainWindow*> gWindows;
void HighlightTab(MainWindow*, WindowTab*);
HWND GetHwndForNotification();

void RelayoutCaption(MainWindow* win);
void OpenSystemMenu(MainWindow* win);
