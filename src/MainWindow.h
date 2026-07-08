/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DoubleBuffer;
struct Edit;
struct WebviewWnd;
struct LinkHandler;
struct StressTest;
class SumatraUIAutomationProvider;
struct FrameRateWnd;
struct ReadAloudPlaybackBar;
struct LabelWithCloseWnd;
struct Splitter;
struct Tooltip;
struct TreeView;
struct ILayout;
struct TabsCtrl;
struct TocTree;
struct FindBarWnd;
struct FindWindowWnd;

// one search match with a text snippet around it, for the floating results list
struct FindMatch {
    int startPage = 0;
    int startGlyph = 0;
    int endPage = 0;
    int endGlyph = 0;
    Str snippet; // UTF-8, owned (freed when findMatches is rebuilt)
};

// factor by how large the non-maximized caption should be in relation to the tabbar
#define kCaptionTabBarDyFactor 1.0f

// gap in pixels between top of caption and tabs; this area allows dragging the window
#define kCaptionTopPadding 8

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
struct MarkdownModel;
struct DisplayModel;
struct WindowTab;

struct Annotation;
struct ILinkHandler;
struct RefHoverState;

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
    Str target;
    Str tooltip;

    explicit StaticLink(Rect rect, Str target, Str infotip = nullptr);
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
    MarkdownModel* AsMarkdown() const;

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
    // hwndFindEdit is the search input; it lives inside the floating findBar
    // (Chrome-style), not in the toolbar
    HWND hwndFindEdit = nullptr;
    FindBarWnd* findBar = nullptr;       // compact toolbar overlay
    FindWindowWnd* findWindow = nullptr; // floating window variant (SearchUIFloating)
    HWND hwndPageLabel = nullptr;
    HWND hwndPageEdit = nullptr;
    HWND hwndPageBg = nullptr;
    HWND hwndPageTotal = nullptr;

    // state related to table of contents (PDF bookmarks etc.)
    HWND hwndTocBox = nullptr;
    UINT_PTR tocBoxSubclassId = 0;

    LabelWithCloseWnd* tocLabelWithClose = nullptr;
    Edit* tocFilterEdit = nullptr;
    TreeView* tocTreeView = nullptr;
    TocTree* tocFilteredTree = nullptr;
    // VBox(label, filter edit, tree); owns those three controls and lays them
    // out in hwndTocBox
    ILayout* tocLayout = nullptr;

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
    // VBox(label, tree); owns those two controls and lays them out in hwndFavBox
    ILayout* favLayout = nullptr;
    Vec<FileState*> expandedFavorites;

    // AI chat sidebars (right side; Claude Code and Grok Build are mutually exclusive)
    HWND hwndClaudeBox = nullptr;
    UINT_PTR claudeBoxSubclassId = 0;
    LabelWithCloseWnd* claudeLabelWithClose = nullptr;
    HWND hwndClaudeSessionCombo = nullptr;
    WebviewWnd* claudeWebView = nullptr;
    bool claudeWebViewReady = false;
    HWND hwndClaudeModelCombo = nullptr;
    HWND hwndClaudeEffortCombo = nullptr;
    HWND hwndClaudeSkipPermsCheck = nullptr;
    Edit* claudeInput = nullptr;
    HWND hwndClaudeStopBtn = nullptr;
    Splitter* claudeSplitter = nullptr;
    bool claudeVisible = false;

    HWND hwndGrokBox = nullptr;
    UINT_PTR grokBoxSubclassId = 0;
    LabelWithCloseWnd* grokLabelWithClose = nullptr;
    HWND hwndGrokSessionCombo = nullptr;
    WebviewWnd* grokWebView = nullptr;
    bool grokWebViewReady = false;
    HWND hwndGrokModelCombo = nullptr;
    HWND hwndGrokEffortCombo = nullptr;
    HWND hwndGrokAlwaysApproveCheck = nullptr;
    Edit* grokInput = nullptr;
    HWND hwndGrokStopBtn = nullptr;
    Splitter* grokSplitter = nullptr;
    bool grokVisible = false;

    HWND hwndCodexBox = nullptr;
    UINT_PTR codexBoxSubclassId = 0;
    LabelWithCloseWnd* codexLabelWithClose = nullptr;
    HWND hwndCodexSessionCombo = nullptr;
    WebviewWnd* codexWebView = nullptr;
    bool codexWebViewReady = false;
    HWND hwndCodexModelCombo = nullptr;
    HWND hwndCodexSandboxCombo = nullptr;
    HWND hwndCodexSkipSandboxCheck = nullptr;
    Edit* codexInput = nullptr;
    HWND hwndCodexStopBtn = nullptr;
    Splitter* codexSplitter = nullptr;
    bool codexVisible = false;

    // width of the active AI chat sidebar (shared by Claude Code, Grok Build, and OpenAI Codex)
    int aiChatDx = 0;

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
    bool textDragPending = false;  // true when mouse down on selected text, waiting for drag
    bool imageDragPending = false; // true when mouse down on image, waiting for drag
    IPageElement* imageDragElement = nullptr;

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

    /* when moving the document by middle-click auto-scroll, this keeps track of
       the speed (in pixels per 20ms) at which we should scroll, which depends on
       the distance of the mouse from the point where the user middle clicked.
       xScrollAccum/yScrollAccum carry the fractional pixels between timer ticks
       so the movement is smooth (issue #2693). */
    float xScrollSpeed = 0;
    float yScrollSpeed = 0;
    float xScrollAccum = 0;
    float yScrollAccum = 0;

    // true while selecting and when CurrentTab()->selectionOnPage != nullptr
    bool showSelection = false;
    // true while a text selection started by double-clicking a word is being
    // dragged, so the selection extends a word at a time instead of a glyph
    bool selectingByWord = false;
    // selection rectangle in screen coordinates (only needed while selecting)
    Rect selectionRect;
    // size of the current rectangular selection in document units
    SizeF selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLink*> staticLinks;

    // home page thumbnail scrolling
    int homePageScrollY = 0;

    // home page search filter
    HWND hwndHomeSearch = nullptr;
    // remembers the search query while the edit control is destroyed
    // (e.g. when a document tab is active)
    Str homeSearchQuery;

    bool isToolbarVisible = false;
    // overlay toolbar mode: the toolbar floats over the page (doesn't reserve
    // space) and is only revealed when the mouse is near the top
    bool isToolbarOverlay = false;
    bool toolbarOverlayShown = false;
    // a hide of the overlay toolbar is scheduled (after kDelayToolbarHide)
    bool toolbarOverlayHidePending = false;
    bool isFullScreen = false;
    PresentationMode presentation = PM_DISABLED;
    int windowStateBeforePresentation = 0;
    bool suppressFrameRedraw = false;

    long nonFullScreenWindowStyle = 0;
    Rect nonFullScreenFrameRect;

    Rect canvasRc; // size of the canvas (excluding any scroll bars)

    // state snapshot used to skip redundant RelayoutFrame calls
    struct LayoutState {
        Rect rc;
        int presentation = 0;
        bool tabsInTitlebar = false;
        bool isFullScreen = false;
        bool tabsVisible = false;
        bool isToolbarVisible = false;
        bool tocVisible = false;
        bool showFavorites = false;
        bool showMenuBarRebar = false;
        bool claudeVisible = false;
        bool grokVisible = false;
        bool codexVisible = false;
        int aiChatDx = 0;
    };
    LayoutState lastLayoutState;

    int currPageNo = 0; // cached value, needed to determine when to auto-update the ToC selection

    // overlay scrollbars (used when scrollbars mode is "smart" or "overlay")
    struct OverlayScrollbar* overlayScrollV = nullptr;
    struct OverlayScrollbar* overlayScrollH = nullptr;

    int wheelAccumDelta = 0;
    UINT_PTR delayedRepaintTimer = 0;

    ThreadHandle printThread = nullptr;
    bool printCanceled = false;

    ThreadHandle findThread = nullptr;
    bool findCancelled = false;
    bool findMatchCase = false;
    bool findMatchWholeWord = false;
    // find-as-you-type is debounced: a WM_TIMER on hwndFrame fires the actual
    // search a short while after the last keystroke (see SearchAndDDE.cpp).
    // true while that timer is armed and hasn't fired yet.
    bool findDebouncePending = false;

    // find bar "n / m" match counter (see SearchAndDDE.cpp). The positions of all
    // matches for findCountText are cached so prev/next is instant; a background
    // thread (re)builds the cache when the search term or match-case changes.
    ThreadHandle findCountThread = nullptr;
    LONG findCountEpoch = 0;
    Str findCountText;
    bool findCountMatchCase = false;
    bool findCountMatchWholeWord = false;
    bool findCountValid = false;
    void* findCountEngine = nullptr; // engine the cache was built for (compared, never deref'd)
    Vec<u64> findCountPositions;     // sorted (page<<32 | startOffset) of each match
    // a newer count request that arrived while a scan was running; the running
    // worker picks it up when it finishes (coalesces rapid typing to one scan)
    Str findCountPendingText;
    bool findCountPendingMatchCase = false;
    bool findCountPendingMatchWholeWord = false;
    // per-match positions (and optional snippets for the floating results list);
    // also built when gShowAllMatches paints all highlights (see SearchAndDDE.cpp)
    Vec<FindMatch> findMatches;
    bool findCountHasSnippets = false;

    // state of in-page find in a browser-hosted (chm / markdown) webview (see
    // SearchAndDDE.cpp BrowserFind* functions); findMatches then holds (page,
    // in-page match index, snippet) built from the webview's all-pages sweep
    int browserFindGen = 0;         // generation; JS echoes it so stale async results are dropped
    int browserFindPageCurrent = 0; // 1-based current match on the current page (0: none)
    int browserFindCurrent = -1;    // index into findMatches of the current match (-1: none)
    int browserFindTotal = -1;      // total matches across all pages (-1: sweep not done)
    Str browserFindTerm;            // owned; the term the current md find ran with

    ILinkHandler* linkHandler = nullptr;
    IPageElement* linkOnLastButtonDown = nullptr;
    Str urlOnLastButtonDown;
    Annotation* annotationUnderCursor = nullptr;
    RefHoverState* refHover = nullptr;
    // highlight rectangle for element under cursor during context menu (in page coordinates)
    RectF contextMenuHighlightRect{};
    int contextMenuHighlightPageNo = 0;
    Point contextMenuPt{};
    bool contextMenuPtValid = false;
    HBRUSH brControlBgColor = nullptr;

    DocControllerCallback* cbHandler = nullptr;

    // The target y offset for smooth scrolling.
    // We use a timer to gradually scroll there.
    int scrollTargetY = 0;

    // suppress Read Aloud user-scroll detection during programmatic follow scrolling
    mutable bool readAloudScrollFromCode = false;

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

    ReadAloudPlaybackBar* readAloudPlaybackBar = nullptr;

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

    void ShowToolTip(Str text, Rect& rc, bool multiline = false) const;
    void DeleteToolTip() const;

    bool CreateUIAProvider();
};

bool HasOpenedDocuments(MainWindow*);
void UpdateControlsColors(MainWindow*);
void ScheduleRepaint(MainWindow*, int delay);
void CreateMovePatternLazy(MainWindow*);
void ClearMouseState(MainWindow*);
bool IsRightDragging(MainWindow*);
MainWindow* FindMainWindowByTab(WindowTab*);
MainWindow* FindMainWindowByHwnd(HWND);
bool IsMainWindowValid(MainWindow*);
bool IsWindowTabValid(WindowTab*);
extern Vec<MainWindow*> gWindows;
void HighlightTab(MainWindow*, WindowTab*);
HWND GetHwndForNotification();

void RelayoutCaption(MainWindow* win);
void OpenSystemMenu(MainWindow* win);

// strips mupdf's "nameddest=" prefix from a remote link's destination name
// so it can be passed to GetNamedDest (issue #5642)
Str CleanRemoteDestName(Str destName);
