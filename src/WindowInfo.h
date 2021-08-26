/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DoubleBuffer;
struct LinkHandler;
struct Notifications;
struct StressTest;
class SumatraUIAutomationProvider;
struct FrameRateWnd;
struct LabelWithCloseWnd;
struct SplitterCtrl;
struct CaptionInfo;
struct TabsCtrl2;

struct IPageElement;
struct PageDestination;
struct TocItem;
struct Controller;
struct ControllerCallback;
struct ChmModel;
struct DisplayModel;
struct TabInfo;

struct TreeCtrl;
struct TooltipCtrl;
struct DropDownCtrl;

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

extern Kind NG_CURSOR_POS_HELPER;
extern Kind NG_RESPONSE_TO_ACTION;

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
    bool panStarted{false};
    POINTS panPos{};
    int panScrollOrigX{0};
    double startArg{0};
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLinkInfo {
    Rect rect;
    WCHAR* target{nullptr};
    WCHAR* infotip{nullptr};

    explicit StaticLinkInfo(Rect rect, const WCHAR* target, const WCHAR* infotip = nullptr);
    StaticLinkInfo() = default;
    StaticLinkInfo(const StaticLinkInfo&);
    StaticLinkInfo& operator=(const StaticLinkInfo& other);
    ~StaticLinkInfo();
};

/* Describes information related to one window with (optional) a document
   on the screen */
struct WindowInfo {
    explicit WindowInfo(HWND hwnd);
    WindowInfo(const WindowInfo&) = delete;
    WindowInfo& operator=(const WindowInfo&) = delete;
    ~WindowInfo();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    [[nodiscard]] bool IsAboutWindow() const;
    [[nodiscard]] bool IsDocLoaded() const;

    [[nodiscard]] DisplayModel* AsFixed() const;
    [[nodiscard]] ChmModel* AsChm() const;

    // TODO: use currentTab->ctrl instead
    Controller* ctrl{nullptr}; // owned by currentTab

    Vec<TabInfo*> tabs;
    TabInfo* currentTab{nullptr}; // points into tabs

    HWND hwndFrame{nullptr};
    HWND hwndCanvas{nullptr};
    HWND hwndReBar{nullptr};
    HWND hwndToolbar{nullptr};
    HWND hwndFindText{nullptr};
    HWND hwndFindBox{nullptr};
    HWND hwndFindBg{nullptr};
    HWND hwndPageText{nullptr};
    HWND hwndPageBox{nullptr};
    HWND hwndPageBg{nullptr};
    HWND hwndPageTotal{nullptr};
    HWND hwndTbInfoText{nullptr};

    // state related to table of contents (PDF bookmarks etc.)
    HWND hwndTocBox{nullptr};

    LabelWithCloseWnd* tocLabelWithClose{nullptr};
    TreeCtrl* tocTreeCtrl{nullptr};
    UINT_PTR tocBoxSubclassId{0};

    // whether the current tab's ToC has been loaded into the tree
    bool tocLoaded{false};
    // whether the ToC sidebar is currently visible
    bool tocVisible{false};
    // set to temporarily disable UpdateTocSelection
    bool tocKeepSelection{false};

    // state related to favorites
    HWND hwndFavBox{nullptr};
    LabelWithCloseWnd* favLabelWithClose{nullptr};
    TreeCtrl* favTreeCtrl{nullptr};
    Vec<FileState*> expandedFavorites;

    // vertical splitter for resizing left side panel
    SplitterCtrl* sidebarSplitter{nullptr};

    // horizontal splitter for resizing favorites and bookmars parts
    SplitterCtrl* favSplitter{nullptr};

    TabsCtrl2* tabsCtrl{nullptr};
    bool tabsVisible{false};
    bool tabsInTitlebar{false};
    // keeps the sequence of tab selection. This is needed for restoration
    // of the previous tab when the current one is closed. (Points into tabs.)
    Vec<TabInfo*>* tabSelectionHistory{nullptr};

    HWND hwndCaption{nullptr};
    CaptionInfo* caption{nullptr};
    int extendedFrameHeight{0};

    TooltipCtrl* infotip{nullptr};

    HMENU menu{nullptr};
    bool isMenuHidden{false}; // not persisted at shutdown

    DoubleBuffer* buffer{nullptr};

    MouseAction mouseAction = MouseAction::Idle;
    bool dragRightClick{false}; // if true, drag was initiated with right mouse click
    bool dragStartPending{false};

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    Point dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    Point dragStart;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int xScrollSpeed{0};
    int yScrollSpeed{0};

    // true while selecting and when currentTab->selectionOnPage != nullptr
    bool showSelection{false};
    // selection rectangle in screen coordinates (only needed while selecting)
    Rect selectionRect;
    // size of the current rectangular selection in document units
    SizeF selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo*> staticLinks;

    bool isFullScreen{false};
    PresentationMode presentation{PM_DISABLED};
    int windowStateBeforePresentation{0};

    long nonFullScreenWindowStyle{0};
    Rect nonFullScreenFrameRect;

    Rect canvasRc;     // size of the canvas (excluding any scroll bars)
    int currPageNo{0}; // cached value, needed to determine when to auto-update the ToC selection

    int wheelAccumDelta{0};
    UINT_PTR delayedRepaintTimer{0};

    Notifications* notifications{nullptr}; // only access from UI thread

    HANDLE printThread{nullptr};
    bool printCanceled{false};

    HANDLE findThread{nullptr};
    bool findCanceled{false};

    ILinkHandler* linkHandler{nullptr};
    IPageElement* linkOnLastButtonDown{nullptr};
    const WCHAR* urlOnLastButtonDown{nullptr};
    Annotation* annotationOnLastButtonDown{nullptr};
    Size annotationBeingMovedSize;
    Point annotationBeingMovedOffset;
    HBITMAP bmpMovePattern{nullptr};
    HBRUSH brMovePattern{nullptr};

    ControllerCallback* cbHandler{nullptr};

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show{false}; // are the markers visible?
        Vec<Rect> rects;  // location of the markers in user coordinates
        int page{0};
        int hideStep{0}; // value used to gradually hide the markers
    } fwdSearchMark;

    StressTest* stressTest{nullptr};

    TouchState touchState;

    FrameRateWnd* frameRateWnd{nullptr};

    SumatraUIAutomationProvider* uiaProvider{nullptr};

    void UpdateCanvasSize();
    Size GetViewPortSize() const;
    void RedrawAll(bool update = false) const;
    void RedrawAllIncludingNonClient(bool update = false) const;

    void ChangePresentationMode(PresentationMode mode);

    void Focus() const;

    void ToggleZoom() const;
    void MoveDocBy(int dx, int dy) const;

    void ShowToolTip(const WCHAR* text, Rect& rc, bool multiline = false) const;
    void HideToolTip() const;
    NotificationWnd* ShowNotification(const WCHAR* msg, NotificationOptions opts = NotificationOptions::WithTimeout,
                                      Kind groupId = NG_RESPONSE_TO_ACTION);
    NotificationWnd* ShowNotification(std::string_view, NotificationOptions opts = NotificationOptions::WithTimeout,
                                      Kind groupId = NG_RESPONSE_TO_ACTION);

    bool CreateUIAProvider();
};

void UpdateTreeCtrlColors(WindowInfo*);
void RepaintAsync(WindowInfo*, int delay);
void ClearFindBox(WindowInfo*);
void CreateMovePatternLazy(WindowInfo*);
void ClearMouseState(WindowInfo*);
bool IsRightDragging(WindowInfo*);
WindowInfo* FindWindowInfoByTabInfo(TabInfo*);
WindowInfo* FindWindowInfoByHwnd(HWND);
bool WindowInfoStillValid(WindowInfo*);
WindowInfo* FindWindowInfoByController(Controller*);

extern Vec<WindowInfo*> gWindows;
