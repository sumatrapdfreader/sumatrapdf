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
struct VirtText;
struct VirtRoot;
struct VirtSplitter;
struct HBox;
struct Splitter;
struct Tooltip;
struct TreeView;
struct SelectionToolbar;
struct ILayout;
struct Spacer;
struct HwndSlot;
struct VBox;
struct VirtCaptionButton;
struct DropDown;
struct Checkbox;
struct VirtButton;
struct TabsCtrl;
struct TocTree;
struct TocItem;
struct FindBarWnd;
struct FindWindowWnd;
struct ToolbarVirt;

// one link numbered by keyboard link following (CmdToggleKeyboardLinkFollowing).
// stored in page coordinates so the badges stay glued to their links while
// scrolling, between the debounced recomputes
struct KeyboardLinkTarget {
    int pageNo = 0;
    RectF rect;
};

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
    Rect rect;
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

// Edge / corner / interior of a rectangular selection for move/resize
// (mirrors crop handles in the save-crop-resize image dialog).
enum class SelectionDragEdge {
    None = 0,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Move,
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

    // long-press detection (issue #538). A finger resting on the glass streams
    // GID_PAN at the same spot -- that, not WM_CONTEXTMENU, is what a hold
    // looks like to an app that has gestures enabled. The gesture engine
    // reports a jump of tens of pixels when it first decides the contact is a
    // pan, so what counts is that the finger has come to rest, not that it
    // never moved: restPos/restTime are pushed forward on every move and the
    // press fires once they stop changing for long enough.
    POINTS pressRestPos{};
    DWORD pressRestTime = 0;
    bool longPressFired = false;
};

// Which end of a touch text selection a finger is dragging (issue #538).
// A long press over a word selects it and shows a handle under each end;
// dragging a handle extends the selection from the other end.
enum class TouchSelHandle {
    None = 0,
    Start,
    End,
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
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

    HWND hwndToolbar = nullptr;
    ToolbarVirt* toolbarVirt = nullptr;
    HWND hwndMenuReBar = nullptr;
    HWND hwndMenuToolbar = nullptr;
    // the search input of the active find UI (compact bar or floating window)
    Edit* findEdit = nullptr;
    // optional "10-25" page-range field of the active find UI (issue #5694)
    Edit* findPagesEdit = nullptr;
    FindBarWnd* findBar = nullptr;       // compact toolbar overlay
    FindWindowWnd* findWindow = nullptr; // floating window variant (SearchUIFloating)
    // owned by the toolbar layout
    Edit* pageEdit = nullptr;

    // state related to table of contents (PDF bookmarks etc.)
    HWND hwndTocBox = nullptr;
    UINT_PTR tocBoxSubclassId = 0;

    // the panel header's label; the ✕ next to it closes the panel
    VirtText* tocLabel = nullptr;
    // the virtual controls of the header, hosted in hwndTocBox
    VirtRoot* tocRoot = nullptr;
    Edit* tocFilterEdit = nullptr;
    TreeView* tocTreeView = nullptr;
    TocTree* tocFilteredTree = nullptr;
    // VBox(label, filter edit, tree); owns those three controls and lays them
    // out in hwndTocBox
    ILayout* tocLayout = nullptr;

    // whether the current tab's ToC has been loaded into the tree
    bool tocLoaded = false;
    // whether the ToC sidebar is currently visible
    // set to temporarily disable UpdateTocSelection
    bool tocKeepSelection = false;
    // Non-owning TOC items that match the current page (same page as best match
    // plus ancestors). Used when gShowAllMatchingTOC to multi-highlight; the
    // tree still has a single selection (the best match). Cleared with the TOC.
    Vec<TocItem*> tocMatchingItems;
    // width of the toc/favorites sidebar; the source of truth for layout
    // (the toc box window can be hidden and its rect stale, e.g. when only
    // favorites are showing). 0 = not laid out yet
    int sidebarDx = 0;

    // state related to favorites
    HWND hwndFavBox = nullptr;
    VirtText* favLabel = nullptr;
    VirtRoot* favRoot = nullptr;
    Edit* favFilterEdit = nullptr;
    TreeView* favTreeView = nullptr;
    // VBox(label, filter edit, tree); owns those controls and lays them out in hwndFavBox
    ILayout* favLayout = nullptr;
    Vec<FileState*> expandedFavorites;

    // AI chat sidebar (right side); a single set of controls shared by all
    // providers (Claude Code, Grok Build, OpenAI Codex), see AIChatPanel.cpp
    HWND hwndAiChatBox = nullptr;
    UINT_PTR aiChatBoxSubclassId = 0;
    VirtText* aiChatLabel = nullptr;
    // HBox(label, close button), the panel's header row
    HBox* aiChatHeader = nullptr;
    VirtRoot* aiChatRoot = nullptr;
    DropDown* aiChatSessionCombo = nullptr;
    DropDown* aiChatModelCombo = nullptr;
    DropDown* aiChatOptionCombo = nullptr; // effort / sandbox
    Checkbox* aiChatCheckbox = nullptr;    // skip permissions / always approve / skip sandbox
    VirtButton* aiChatStopBtn = nullptr;
    Edit* aiChatInput = nullptr;
    WebviewWnd* aiChatWebView = nullptr;
    bool aiChatWebViewReady = false;
    VirtSplitter* aiChatSplitter = nullptr;
    // VBox(label, session combo, webview slot, input row, options row);
    // owns those controls and lays them out in hwndAiChatBox
    ILayout* aiChatLayout = nullptr;
    // the webview is created lazily; this spacer reserves its area in the layout
    Spacer* aiChatWebViewSlot = nullptr;
    // provider (AIChatBackend value) the panel content is configured for; -1 = none
    int aiChatProvider = -1;

    // width of the AI chat sidebar
    int aiChatDx = 0;

    // vertical splitter for resizing left side panel
    // the splitters are virtual controls living in the frame's own tree
    // (frameRoot), not child windows
    VirtSplitter* sidebarSplitter = nullptr;

    // horizontal splitter for resizing favorites and bookmars parts
    VirtSplitter* favSplitter = nullptr;

    TabsCtrl* tabsCtrl = nullptr;
    bool tabsVisible = false;
    bool tabsInTitlebar = false;

    // per-monitor DPI of hwndFrame (from WM_DPICHANGED wParam). Used so UI
    // chrome can refresh at the destination scale even while GetDpiForWindow
    // still lags during a cross-monitor drag.
    int frameDpi = 0;
    // defer expensive chrome rebuild while the user is dragging/resizing;
    // finish on WM_EXITSIZEMOVE via a uitask
    bool deferDpiChromeRefresh = false;
    bool dpiChromeRefreshPending = false;
    // keeps the sequence of tab selection. This is needed for restoration
    // of the previous tab when the current one is closed. (Points into tabs.)
    Vec<WindowTab*>* tabSelectionHistory = nullptr;

    ButtonInfo captionBtn[CB_BTN_COUNT];
    bool isMenuOpen = false;
    Rect captionRect;

    Tooltip* infotip = nullptr;

    HMENU menu = nullptr;

    DoubleBuffer* buffer = nullptr;

    MouseAction mouseAction = MouseAction::None;
    bool dragRightClick = false; // if true, drag was initiated with right mouse click
    bool dragStartPending = false;
    bool textDragPending = false;  // true when mouse down on selected text, waiting for drag
    bool imageDragPending = false; // true when mouse down on image, waiting for drag
    IPageElement* imageDragElement = nullptr;
    int imageDragPageNo = -1; // page of imageDragElement for screen-rect / hotspot

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
    // a long press with a finger selected a word and put a drag handle under
    // each end of the selection; moving the mouse takes them away again and
    // leaves the selection alone (issue #538)
    bool touchSelHandles = false;
    // the handle a finger currently has hold of, if any
    TouchSelHandle touchSelDragging = TouchSelHandle::None;
    // whether the input sequence in progress came from a finger. Recorded at
    // button-down, where GetMessageExtraInfo() is reliable, because
    // WM_CONTEXTMENU (what a long press turns into) doesn't carry it
    bool lastInputWasTouch = false;
    // where and when the finger went down, to tell a long press from a tap
    Point touchDownPos;
    DWORD touchDownTime = 0;
    // the contact being timed, -1 when no finger is down; a second finger
    // means a gesture, not a press
    int touchPointerId = -1;
    // when a finger was last heard from, to tell a real mouse move from the
    // ones Windows synthesizes around a touch
    DWORD touchLastActivityTime = 0;
    // the hold already selected a word, so the rest of this contact adjusts it
    bool touchLongPressDone = false;
    // Windows raises its own context menu for a held finger after we've acted
    // on the hold; this swallows exactly that one
    bool touchSuppressContextMenu = false;
    // selection rectangle in screen coordinates (only needed while selecting)
    Rect selectionRect;
    // size of the current rectangular selection in document units
    SizeF selectionMeasure;
    // move/resize of an existing rectangular selection (Ctrl+drag region)
    SelectionDragEdge selectionDragEdge = SelectionDragEdge::None;
    // screen rect when the move/resize started (normalized)
    Rect selectionEditOrig;

    // virtual controls of the home page (header, view buttons, links, ...)
    struct VirtRoot* homeRoot = nullptr;
    // the frame's virtual controls: the three splitters. The frame paints
    // them and hands them its mouse input
    VirtRoot* frameRoot = nullptr;

    // chrome VBox: caption / tabs / menu / toolbar + the content row. Owns
    // the slots; HwndSlot::SetBounds moves each HWND (batched via winPos)
    VBox* chromeLayout = nullptr;
    // content row: sidebar | splitter | (canvas / full-window favorites) |
    // splitter | AI chat
    HBox* frameLayout = nullptr;
    HwndSlot* tocSlot = nullptr;
    HwndSlot* favSlot = nullptr;
    // same hwndFavBox as favSlot; shown instead of the canvas when the
    // Favorites tab is selected
    HwndSlot* fullFavSlot = nullptr;
    HwndSlot* canvasSlot = nullptr;
    HwndSlot* aiChatSlot = nullptr;
    HwndSlot* tabsSlot = nullptr;
    HwndSlot* menuSlot = nullptr;
    HwndSlot* toolbarTopSlot = nullptr;
    HwndSlot* toolbarBottomSlot = nullptr;
    // tabs-in-titlebar caption: VirtCtrl buttons + HwndSlots for tabs/menu
    VBox* captionLayout = nullptr;
    HBox* captionRow1 = nullptr;
    HBox* captionRow2 = nullptr;
    VirtCaptionButton* capBtn[CB_BTN_COUNT]{};
    HwndSlot* capMenuSlot = nullptr;
    HwndSlot* capTabsRow1 = nullptr;
    HwndSlot* capTabsRow2 = nullptr;
    Spacer* capGap = nullptr;
    Spacer* capDrag1 = nullptr;
    Spacer* capRow2Lead = nullptr;
    Spacer* capRow2Trail = nullptr;

    // home page thumbnail scrolling
    int homePageScrollY = 0;
    // keyboard-selected home page entry (index into the filtered list),
    // -1 when there's nothing to select. Enter opens it (issue #1136)
    int homePageSelIdx = 0;
    // grid column remembered when Up moves focus from the first thumbnail row
    // into the search box; Down restores it (clamped to the current column count)
    int homePageSearchReturnCol = 0;

    // home page search filter. The layout owns the edit and is what places it
    // inside the search box the home page draws
    Edit* homeSearch = nullptr;
    ILayout* homeSearchLayout = nullptr;
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
    // whether BeginFrameRedrawSuppression sent WM_SETREDRAW FALSE (it doesn't
    // for a hidden frame: WM_SETREDRAW TRUE would show the window)
    bool frameRedrawSuppressSent = false;

    long nonFullScreenWindowStyle = 0;
    Rect nonFullScreenFrameRect;

    Rect canvasRc; // size of the canvas (excluding any scroll bars)

    // deferred, coalesced UI update (see ScheduleUiUpdate): multiple
    // relayout/repaint requests before the uitask runs are handled in one
    // pass. `layout` is a snapshot of everything that affects frame layout;
    // RelayoutFrame skips when it's unchanged (force a relayout by resetting
    // it to {})
    struct UIState {
        struct Layout {
            Rect rc;
            int presentation = 0;
            bool tabsInTitlebar = false;
            bool isFullScreen = false;
            bool tabsVisible = false;
            bool isToolbarVisible = false;
            bool tocVisible = false;
            bool showFavorites = false;
            // full-window Favorites tab vs. sidebar panel: different geometry
            bool favoritesAsTab = false;
            bool showMenuBarRebar = false;
            bool aiChatVisible = false;
            int aiChatDx = 0;
            bool sidebarOnRight = false;
        };
        Layout layout; // last applied layout state
        // desired visibility of the sidebar / AI chat panels; applied
        // (HwndSetVisible) by RelayoutFrame
        bool tocVisible = false;
        bool favVisible = false;
        bool aiChatVisible = false;
        bool updatePending = false; // a FrameUpdateUi uitask is queued
        bool toolbarDirty = false;  // repaint the toolbar on the next update
        bool tabsDirty = false;     // repaint the tab bar on the next update
        bool sidebarDirty = false;  // repaint toc/favorites boxes on the next update
        // RelayoutFrame args for the pending update: updateToolbars is the OR
        // of all pending requests, sidebarDx is last-request-wins (-1 = keep
        // the current sidebar width)
        bool updateToolbars = false;
        int sidebarDx = -1;
    };
    UIState uiState;

    int currPageNo = 0; // cached value, needed to determine when to auto-update the ToC selection

    // User wants the page-info tip (I key / CmdTogglePageInfo). Survives tab
    // switches and visits to Home/About where the notification cannot show
    // (issue #4454); restored when a document tab is active again.
    bool pageInfoWanted = false;

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
    AtomicInt findCountEpoch = 0;
    Str findCountText;
    Str findPageRangeText; // last applied Pages box text (issue #5694)
    Str findCountRangeText;
    bool findCountMatchCase = false;
    bool findCountMatchWholeWord = false;
    bool findCountValid = false;
    // the scan stopped at kMaxFindCount matches; the real total is higher
    // (shown as "n / m+")
    bool findCountCapped = false;
    void* findCountEngine = nullptr; // engine the cache was built for (compared, never deref'd)
    // (page<<32 | startOffset) of each match, in scan order (the scan starts
    // at the page current at the time and wraps around)
    Vec<u64> findCountPositions;
    // a newer count request that arrived while a scan was running; the running
    // worker picks it up when it finishes (coalesces rapid typing to one scan)
    Str findCountPendingText;
    bool findCountPendingMatchCase = false;
    bool findCountPendingMatchWholeWord = false;
    // per-match positions (and optional snippets for the floating results list);
    // also used by PaintAllFindMatches to highlight every find hit (see SearchAndDDE.cpp)
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
    // keyboard link following: when on, visible links are numbered 1..9 and
    // pressing a digit follows that link (see LinkFollow.cpp)
    bool linkFollowActive = false;
    Vec<KeyboardLinkTarget> linkFollowTargets;

    // keyboard text selection: a caret you move with the arrow keys to select
    // text without the mouse (see SelectTextKeyboard.cpp)
    bool textSelectModeActive = false;
    // in visual mode plain movement extends the selection (like Shift+arrows)
    bool textSelectModeVisual = false;
    bool textSelectCaretVisible = true; // toggled by the blink timer
    int textSelectPage = 0;             // caret position, 0 if not set yet
    int textSelectGlyph = 0;
    int textSelectAnchorPage = 0; // where the selection started
    int textSelectAnchorGlyph = 0;

    IPageElement* linkOnLastButtonDown = nullptr;
    Str urlOnLastButtonDown;
    Annotation* annotationUnderCursor = nullptr;
    RefHoverState* refHover = nullptr;
    // highlight rectangle for element under cursor during context menu (in page coordinates)
    RectF contextMenuHighlightRect;
    int contextMenuHighlightPageNo = 0;
    Point contextMenuPt;
    bool contextMenuPtValid = false;
    HBRUSH brControlBgColor = nullptr;

    DocControllerCallback* cbHandler = nullptr;

    // Smooth mouse-wheel scrolling: exponential chase of scrollTargetY.
    // scrollAnimY is sub-pixel; only integer steps are applied to the view.
    int scrollTargetY = 0;
    double scrollAnimY = 0;
    LARGE_INTEGER scrollAnimLastTime{};
    bool scrollAnimActive = false;
    bool scrollAnimHiResTimer = false; // timeBeginPeriod(1) while animating

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

    // debugging aid; created on first use, see MainWindow::ShowFrameRateDur()
    FrameRateWnd* frameRateWnd = nullptr;

    ReadAloudPlaybackBar* readAloudPlaybackBar = nullptr;

    // small floating toolbar shown after a text selection in fixed-page
    // floating selection actions bar (controlled by the SelectionToolbar setting)
    SelectionToolbar* selectionToolbar = nullptr;
    // a debounced show of the selection toolbar is waiting on its timer
    bool selectionToolbarShowPending = false;

    // set at the beginning of CloseWindow() to prevent
    // processing commands while closing (e.g. reentrancy
    // via modal dialogs pumping messages)
    bool isBeingClosed = false;

    SumatraUIAutomationProvider* uiaProvider = nullptr;

    void UpdateCanvasSize();
    Size GetViewPortSize() const;
    void RedrawAll(bool update = false) const;
    void RedrawAllIncludingNonClient() const;

    // no-op unless gShowFrameRate is set
    void ShowFrameRateDur(double durMs);

    void ChangePresentationMode(PresentationMode mode);
    bool InPresentation() const;

    void Focus() const;

    void ToggleZoom() const;
    void MoveDocBy(int dx, int dy) const;

    void ShowToolTip(Str text, Rect& rc, bool multiline = false) const;
    void ShowToolTipAt(Str text, const Rect& rc, Point screenPos, bool multiline = false, int maxRightScreen = 0) const;
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
extern bool gShowFrameRate;
void HighlightTab(MainWindow*, WindowTab*);
HWND GetHwndForNotification();

void RelayoutCaption(MainWindow* win);
void OpenSystemMenu(MainWindow* win);

Str CleanRemoteDestName(Str destName);
