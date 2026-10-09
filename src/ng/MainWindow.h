/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Window;
struct InputState;
} // namespace gpui

struct WindowTab;
struct DocController;
struct DocControllerCallback;
struct ChmModel;
struct MarkdownModel;
struct DisplayModel;
struct MenuModel;
struct NotificationWnd;
struct ILinkHandler;
struct IPageElement;
struct SelectionToolbar;
struct Toolbar;
struct FindBar;
struct FindWindowWnd;
struct TocItem;
struct TocTree;
struct FileState;
struct SidebarUI;
struct TabsUI;
struct ShellUI;
struct DocCanvasUI;
struct HomePageUI;
struct WatchedFile;
enum class SidebarContent;

// one match of the current find, in (page, glyph) coordinates
struct FindMatch {
    int startPage = 0;
    int startGlyph = 0;
    int endPage = 0;
    int endGlyph = 0;
    Str snippet; // UTF-8, owned (freed when findMatches is rebuilt)
};

enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};

// Current action being performed with a mouse
enum class MouseAction {
    None = 0,
    Dragging,
    Selecting,
    Scrolling,
    SelectingText
};

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

// how many links a keyboard hint can be long (LinkFollow.cpp)
constexpr int kMaxKeyboardLinkHintLength = 3;

// one Vimium-style link hint: the link in page coordinates plus its letters
struct KeyboardLinkTarget {
    int pageNo = 0;
    RectF rect;
    char hint[kMaxKeyboardLinkHintLength]{};
    int hintLen = 0;
};

// ng: orig keeps the scroll range on the canvas HWND and reads it back with
// GetScrollInfo(). gpui has no window scrollbars, so UpdateScrollbars fills
// this in and the overlay scrollbars and OnVScroll / OnHScroll read it. The
// field names are win32's SCROLLINFO, so the ported code stays readable.
enum class ReadingBarDrag {
    None = 0,
    Move,
    ResizeTop,
    ResizeBottom,
};

struct ReadingAutoScrollBar;
struct ReadAloudPlaybackBar;
struct OverlayScrollbar;
struct Annotation;
struct AnnotEditToolbar;
struct AnnotFilterToolbar;
struct AnnotTextPopup;

// ng: orig's cursors that are bitmaps, which gpui's CursorKind cannot name.
// Windows only: the canvas asks gpui for a stand-in kind and the frame's
// subclass sets the real cursor (gui/NativeCursors_win.cpp).
enum class NativeCursor {
    None,
    // the hand of a drag-pan (orig's gCursorDrag)
    Drag,
    LaserPointer,
    TextAnnotationPlacement,
    InkAnnotationPlacement,
};
struct AnnotationHoverOverlay;
struct RefHoverState;

// One in-progress annotation placement. Only one kind is active at a time;
// `kind` says which command started it.
enum class AnnotPlacementKind {
    None = 0,
    Text,
    FreeText,
    Stamp,
    Caret,
    FileAttachment,
    Line,
    PolyLine,
    Shape,
    Ink,
    // not placed: each text selection made while it's on is highlighted
    Highlighter,
};

struct AnnotPlacement {
    AnnotPlacementKind kind = AnnotPlacementKind::None;
    int cmdId = 0;
    int pageNo = -1;
    Point pos;
    PointF start;
    Point end;
    RectF rect;
    Vec<PointF> points;
    Vec<int> strokeCounts;
    bool circle = false;
    bool mouseDown = false;
    bool didDrag = false;
    bool constrain = false;

    void Reset();
};

// Resize handle positions used when resizing annotations (orig Canvas.cpp)
enum class ResizeHandle {
    None = 0,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    LineStart,
    LineEnd,
    Vertex,
};

struct CanvasScrollInfo {
    int nMin = 0;
    int nMax = 0;
    int nPage = 0;
    int nPos = 0;
    bool visible = false;
};

struct StressTest;

/* Describes information related to one window with (optional) a document
   on the screen */
// ng: orig's MainWindow is ~500 fields of win32 UI state (toolbar, sidebar,
// find bar, annotations, touch, UIA, ...). This is the part the gpui shell
// needs now; the rest comes back with the step that draws it.
struct MainWindow {
    explicit MainWindow(gpui::Window* w);
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    ~MainWindow();

    bool IsCurrentTabAbout() const;
    bool IsDocLoaded() const;
    bool HasDocsLoaded() const;

    DisplayModel* AsFixed() const;
    ChmModel* AsChm() const;
    MarkdownModel* AsMarkdown() const;

    // owned by CurrentTab()
    DocController* ctrl = nullptr;
    DocControllerCallback* cbHandler = nullptr;
    ILinkHandler* linkHandler = nullptr;

    WindowTab* currentTabTemp = nullptr; // points into tabs
    WindowTab* CurrentTab() const;
    int TabCount() const;
    Vec<WindowTab*> Tabs() const;
    WindowTab* GetTab(int idx) const;
    int GetTabIdx(WindowTab*) const;

    // the gpui window this frame draws into
    gpui::Window* gpuiWin = nullptr;
    // ng: the gpui views of this window. Each one is an entity gpui's
    // listeners are bound to, so every window dispatches into its own state
    // (orig keeps the equivalent in MainWindow's child HWNDs)
    ShellUI* shell = nullptr;
    DocCanvasUI* docCanvas = nullptr;
    HomePageUI* homePage = nullptr;

    Vec<WindowTab*> tabs;
    // keeps the sequence of tab selection, for restoring the previous tab
    // when the current one is closed (points into tabs)
    Vec<WindowTab*>* tabSelectionHistory = nullptr;
    // ng: the gpui side of the tab strip (hover, drag, context menu); orig
    // keeps the same state in its TabsCtrl
    TabsUI* tabsUI = nullptr;

    // the menu bar, rebuilt when the document or the selection changes
    MenuModel* menu = nullptr;
    bool isMenuBarVisible = true;
    // ng: orig rebuilds the menu on the way out of fullscreen from
    // IsMenubarVisible(); the port's menu bar is a row of its own that F9
    // toggles, so what it was before is remembered instead
    bool menuBarVisibleBeforeFS = true;

    // --- toolbar (step 12a) ---------------------------------------------------

    Toolbar* toolbar = nullptr;
    bool isToolbarVisible = false;
    bool isToolbarOverlay = false;

    Rect canvasRc; // size of the canvas (excluding any scroll bars)
    // orig's tabsInTitlebar: the tab strip and the caption buttons are drawn
    // in a caption of the port's own (Windows; set when UseTabs is on)
    bool tabsInTitlebar = false;
    // the width the tab strip has in that caption; 0 = the frame's
    int tabsAvailDx = 0;
    // size of the whole window and whether it is maximized, as the shell saw
    // them on its last frame. gpui has no query for either after the window is
    // gone, and the settings are written after it closes
    Rect frameRc;
    bool isMaximized = false;

    // the viewport size the controller was last given, so a frame that didn't
    // resize the window doesn't relayout the document
    Size lastViewPortSize;
    // background chapter count starts once the canvas has a real size
    bool chapterLayoutStarted = false;

    // what the canvas last told the scrollbars; also what OnVScroll steps from
    CanvasScrollInfo scrollV;
    CanvasScrollInfo scrollH;
    // orig's OverlayScrollbar, minus its layered window (step 12b)
    OverlayScrollbar* overlayScrollV = nullptr;
    OverlayScrollbar* overlayScrollH = nullptr;

    MouseAction mouseAction = MouseAction::None;
    // a press that hasn't moved far enough to be a drag yet
    bool dragStartPending = false;
    // the drag was started with the right button: it pans, and its release
    // without a move opens the context menu
    bool dragRightClick = false;
    Point dragStart;
    Point dragPrevPos;
    // middle-button auto-scroll: pixels per 20 ms, with the sub-pixel leftover
    float xScrollSpeed = 0;
    float yScrollSpeed = 0;
    float xScrollAccum = 0;
    float yScrollAccum = 0;
    // mouse wheel notches not yet turned into a scroll or a page flip
    int wheelAccumDelta = 0;
    // Smooth mouse-wheel scrolling: exponential chase of scrollTargetY.
    // scrollAnimY is sub-pixel; only integer steps are applied to the view.
    int scrollTargetY = 0;
    double scrollAnimY = 0;
    bool scrollAnimActive = false;
    // a page was still rendering on the last paint, so paint again soon
    bool repaintPending = false;

    // --- reading bar / automatically scroll (step 12b) -----------------------

    ReadingBarDrag readingBarDrag = ReadingBarDrag::None;
    int readingBarDragOff = 0;
    bool readingBarHover = false;
    ReadingAutoScrollBar* readingAutoScrollBar = nullptr;

    void MoveDocBy(int dx, int dy) const;

    // --- read aloud (step 14c) ----------------------------------------------

    // the view is being moved by the read-aloud auto-scroll, not by the user
    mutable bool readAloudScrollFromCode = false;
    ReadAloudPlaybackBar* readAloudPlaybackBar = nullptr;
    // ng: orig's kReadAloudHighlightTimerID, counted down by the shell's tick
    bool readAloudTimerOn = false;
    int readAloudTimerLeftMs = 0;

    // --- links and selection (step 8a) --------------------------------------

    // the link the last left button press landed on; the click follows it if
    // the release lands on it too
    IPageElement* linkOnLastButtonDown = nullptr;
    // a press on already selected text: the drag-out gesture orig starts here
    // is not ported (no OLE drag and drop in gpui), but the click still clears
    bool textDragPending = false;
    // true when mouse down on image, waiting for drag (Windows: OLE drag-out)
    bool imageDragPending = false;
    IPageElement* imageDragElement = nullptr;
    int imageDragPageNo = -1; // page of imageDragElement for screen-rect / hotspot
    // the rubber band while a selection is being made, in canvas coordinates
    Rect selectionRect;
    // size of a rectangular selection in the document's units
    SizeF selectionMeasure;
    // moving / resizing an existing rectangular selection
    SelectionDragEdge selectionDragEdge = SelectionDragEdge::None;
    Rect selectionEditOrig;
    // a drag after a double click extends the selection a word at a time
    bool selectingByWord = false;
    // the time and place of the last double click, so a third one nearby is a
    // triple click (line selection)
    u64 lastWordSelectTime = 0;
    Point lastWordSelectPos;

    // the cursor the canvas wants, resolved on every mouse move
    int canvasCursor = 0;
    // the link tooltip: text and the link's rect in canvas coordinates
    Str linkTooltip;
    Rect linkTooltipRc;

    // Vimium-style keyboard link following (LinkFollow.cpp)
    bool linkFollowActive = false;
    Vec<KeyboardLinkTarget> linkFollowTargets;
    char linkFollowInput[kMaxKeyboardLinkHintLength]{};
    int linkFollowInputLen = 0;

    SelectionToolbar* selectionToolbar = nullptr;

    // --- find (step 8b) ------------------------------------------------------

    // ng: orig's find edit is a win32 DropDown with a history list; here it is
    // a gpui text field. `findBar` is the bar's own state (visible, status).
    gpui::InputState* findEdit = nullptr;
    FindBar* findBar = nullptr;
    // the floating variant (SearchUIFloating) and its "Limit to pages" box
    FindWindowWnd* findWindow = nullptr;
    gpui::InputState* findPagesEdit = nullptr;

    ThreadHandle printThread = nullptr;
    bool printCanceled = false;

    ThreadHandle findThread = nullptr;
    bool findCancelled = false;
    bool findMatchCase = false;
    bool findMatchWholeWord = false;
    // find-as-you-type is debounced: orig arms a WM_TIMER on hwndFrame, here
    // the shell's tick counts this down and fires the deferred search
    bool findDebouncePending = false;
    int findDebounceLeftMs = 0;
    // the current find session already recorded its starting view
    bool searchStartMarked = false;

    // find bar "n / m" match counter (see SearchAndDDE.cpp). The positions of
    // all matches for findCountText are cached so prev/next is instant; a
    // background thread rebuilds the cache when the term changes
    ThreadHandle findCountThread = nullptr;
    AtomicInt findCountEpoch = 0;
    Str findCountText;
    Str findPageRangeText; // last applied Pages box text (issue #5694)
    Str findCountRangeText;
    bool findCountMatchCase = false;
    bool findCountMatchWholeWord = false;
    bool findCountValid = false;
    bool findCountCapped = false;
    void* findCountEngine = nullptr; // engine the cache was built for (compared, never deref'd)
    Vec<u64> findCountPositions;
    Str findCountPendingText;
    bool findCountPendingMatchCase = false;
    bool findCountPendingMatchWholeWord = false;
    // also used by PaintAllFindMatches to highlight every find hit
    Vec<FindMatch> findMatches;
    bool findCountHasSnippets = false;

    // state of in-page find in a browser-hosted (chm / markdown) webview; the
    // controller can't do it yet (step 11), so these stay at their defaults
    int browserFindGen = 0;
    int browserFindPageCurrent = 0;
    int browserFindCurrent = -1;
    int browserFindTotal = -1;
    Str browserFindTerm;

    // --- keyboard text selection (SelectTextKeyboard.cpp, step 8b) -----------

    bool textSelectModeActive = false;
    bool textSelectModeVisual = false;
    bool textSelectCaretVisible = true; // toggled by the blink timer
    int textSelectBlinkLeftMs = 0;
    int textSelectPage = 0; // caret position, 0 if not set yet
    int textSelectGlyph = 0;
    int textSelectAnchorPage = 0; // where the selection started
    int textSelectAnchorGlyph = 0;

    // --- sidebar, table of contents, favorites (step 9a) ---------------------

    // what the sidebar shows; orig keeps the same three flags in win->uiState
    struct {
        bool tocVisible = false;
        bool favVisible = false;
        bool aiChatVisible = false;
    } uiState;
    SidebarContent sidebarBottomContent;
    // --- AI chat panel (step 14d) --------------------------------------------
    // provider (AIChatBackend value) the panel content is configured for;
    // -1 = none
    int aiChatProvider = -1;
    // width of the AI chat panel in dips; orig's win->aiChatDx
    int aiChatDx = 0;
    // width of the sidebar in dips; orig's win->sidebarDx
    int sidebarDx = 0;
    // frame pixels SidebarWindowSize = grow added for the sidebar (0: none)
    int sidebarGrewFrameDx = 0;
    // Windows: the frame's last non-maximized rectangle in screen pixels, for
    // settings saved after the frame is gone (AppShellNormalWindowRect)
    Rect normalWindowRc;
    bool tocLoaded = false;
    // set while a click on a bookmark navigates, so the page change does not
    // move the tree selection off the item the user picked
    bool tocKeepSelection = false;
    // the page the document is on, as the canvas last reported it
    int currPageNo = 0;

    // User wants the page-info tip (I key / CmdTogglePageInfo). Survives tab
    // switches and visits to Home/About where the notification cannot show
    // (issue #4454); restored when a document tab is active again.
    bool pageInfoWanted = false;
    // CmdTogglePageBoxes: outline PDF Media/Crop/Bleed/Trim/Art boxes
    bool showPageBoxes = false;

    // the tree built by the bookmark filter, shown instead of the document's
    TocTree* tocFilteredTree = nullptr;
    // every entry that should look "current" for the page (issue #4642)
    Vec<TocItem*> tocMatchingItems;
    // files whose favorites branch is expanded in the favorites tree
    Vec<FileState*> expandedFavorites;
    // ng: the gpui side of the two panes (trees, filters, scroll offsets)
    SidebarUI* sidebar = nullptr;

    // --- annotations (step 13a) ---------------------------------------------

    // orig's "Edit PDF" mode: the annotation button row under the toolbar
    bool pdfAnnotationsToolbarEnabled = false;
    // the property row under the selected annotation, and the annotation list
    AnnotEditToolbar* annotEditToolbar = nullptr;
    AnnotFilterToolbar* annotFilterToolbar = nullptr;
    // where the annotation list window was last (screen pixels), and whether
    // the user put it there; not saved to the settings file, as in orig
    Rect annotListFloatPos;
    bool annotListFloatPosUserSet = false;
    // read-only card with a clicked annotation's whole text
    AnnotTextPopup* annotTextPopup = nullptr;
    // what the canvas' cursor is when canvasCursor is the stand-in kind
    NativeCursor nativeCursor = NativeCursor::None;
    // Edit PDF: the card with the properties of the annotation under the cursor
    AnnotationHoverOverlay* annotationHoverOverlay = nullptr;
    // citation / reference hover popup (RefHover.cpp), created on first hover
    RefHoverState* refHover = nullptr;
    // the annotation the mouse is over (hover marker in Edit PDF mode)
    Annotation* annotationUnderCursor = nullptr;
    // moving an annotation: the one being dragged, its size and the offset of
    // its top-left corner from the pointer
    Annotation* annotationBeingDragged = nullptr;
    Size annotationBeingMovedSize;
    Point annotationBeingMovedOffset;
    // resizing an annotation
    bool annotationBeingResized = false;
    int resizeHandle = 0; // ResizeHandle
    RectF annotationOriginalRect;
    RectF annotationResizePreviewRect;
    // free text lays its text out again on every write, so the drag only moves
    // an outline and the annotation is written once, when it ends
    bool annotationResizeOutlineOnly = false;
    PointF annotationOriginalLineStart;
    PointF annotationOriginalLineEnd;
    PointF annotationLinePreviewStart;
    PointF annotationLinePreviewEnd;
    Vec<PointF> annotationVertexPreview;
    int annotationResizeVertexIndex = -1;
    float annotationResizeAspectRatio = 0;
    // where arrow keys moved an annotation; re-rendered once the keys pause
    WindowTab* annotationNudgeTab = nullptr;
    int annotationNudgePageNo = 0;
    int annotationNudgeLeftMs = 0;
    // page bitmap follows a rectangle resize once the pointer pauses
    int annotationResizeRerenderLeftMs = 0;
    // a press that only deselected an annotation; the click must do nothing
    bool pressOnlyDeselected = false;
    // an engine edit operation is open: everything until it closes is one
    // undo step
    bool pdfEditOperationActive = false;
    AnnotPlacement annotPlacement;

    // the page element the open context menu was opened on, highlighted while
    // it is up, and the canvas point every context-menu command works from
    RectF contextMenuHighlightRect;
    int contextMenuHighlightPageNo = 0;
    Point contextMenuPt;
    bool contextMenuPtValid = false;

    bool showSelection = false;
    bool isFullScreen = false;
    // -quicklook: a chrome-less always-on-top preview window (Explorer Space)
    bool isQuickLook = false;
    PresentationMode presentation = PM_DISABLED;
    int windowStateBeforePresentation = 0;
    // ng: orig arms kHideCursorTimerID on the canvas; the shell's tick counts
    // this down instead. -1 is "no timer armed"
    int presCursorHideLeftMs = -1;
    // set at the beginning of CloseWindow() to prevent processing commands
    // while closing
    bool isBeingClosed = false;
    bool isClosePending = false;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show = false; // are the markers visible?
        Vec<Rect> rects;   // location of the markers in user coordinates
        int page = 0;
        int hideStep = 0; // value used to gradually hide the markers
        // ng: orig arms kHideFwdSearchMarkTimerID; the tick counts this down
        int hideLeftMs = -1;
    } fwdSearchMark;

    StressTest* stressTest = nullptr;

    Size GetViewPortSize() const;
    void RedrawCanvas() const;
    void RedrawAll(bool update = false) const;
    void Focus() const;
    void ToggleZoom() const;
    bool InPresentation() const;
    void ChangePresentationMode(PresentationMode mode);
};

bool HasOpenedDocuments(MainWindow*);
MainWindow* FindMainWindowByTab(WindowTab*);
MainWindow* FindMainWindowByGpuiWindow(gpui::Window*);
#if OS_WIN
HWND MainWindowHwnd(MainWindow*);
MainWindow* FindMainWindowByHwnd(HWND);
#endif
WindowTab* FindTabByFilePath(Str path, MainWindow* limitWin = nullptr);
bool IsMainWindowValid(MainWindow*);
bool IsMainWindowValidAndNotClosing(MainWindow*);
bool IsWindowTabValid(WindowTab*);
extern Vec<MainWindow*> gWindows;
