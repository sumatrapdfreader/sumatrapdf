/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class DoubleBuffer;
class LinkHandler;
class Notifications;
class StressTest;
class SumatraUIAutomationProvider;
class FrameRateWnd;
struct LabelWithCloseWnd;
class SplitterWnd;
struct SplitterCtrl;
struct CaptionInfo;

class PageElement;
class PageDestination;
struct TocItem;
class Controller;
class ControllerCallback;
class ChmModel;
class DisplayModel;
class EbookController;
class TabInfo;

struct TreeCtrl;
struct TooltipCtrl;
struct DropDownCtrl;

/* Describes actions which can be performed by mouse */
// clang-format off
enum class MouseAction {
    Idle = 0,
    Dragging,
    DraggingRight,
    Selecting,
    Scrolling,
    SelectingText
};
// clang-format on

extern NotificationGroupId NG_CURSOR_POS_HELPER;
extern NotificationGroupId NG_RESPONSE_TO_ACTION;

enum NotificationOptions {
    NOS_DEFAULT = 0, // timeout after 3 seconds, no highlight
    NOS_PERSIST = (1 << 0),
    NOS_HIGHLIGHT = (1 << 1),
    NOS_WARNING = NOS_PERSIST | NOS_HIGHLIGHT,
};

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
    RectI rect{};
    const WCHAR* target = nullptr;
    const WCHAR* infotip = nullptr;

    StaticLinkInfo() = default;
    explicit StaticLinkInfo(RectI rect, const WCHAR* target, const WCHAR* infotip = nullptr)
        : rect(rect), target(target), infotip(infotip) {
    }
};

/* Describes information related to one window with (optional) a document
   on the screen */
class WindowInfo {
  public:
    explicit WindowInfo(HWND hwnd);
    ~WindowInfo();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    bool IsAboutWindow() const;
    bool IsDocLoaded() const;

    DisplayModel* AsFixed() const;
    ChmModel* AsChm() const;
    EbookController* AsEbook() const;

    // TODO: use currentTab->ctrl instead
    Controller* ctrl = nullptr; // owned by currentTab

    Vec<TabInfo*> tabs;
    TabInfo* currentTab = nullptr; // points into tabs

    HWND hwndFrame = nullptr;
    HWND hwndCanvas = nullptr;
    HWND hwndToolbar = nullptr;
    HWND hwndReBar = nullptr;
    HWND hwndFindText = nullptr;
    HWND hwndFindBox = nullptr;
    HWND hwndFindBg = nullptr;
    HWND hwndPageText = nullptr;
    HWND hwndPageBox = nullptr;
    HWND hwndPageBg = nullptr;
    HWND hwndPageTotal = nullptr;

    // state related to table of contents (PDF bookmarks etc.)
    HWND hwndTocBox = nullptr;
    DropDownCtrl* altBookmarks = nullptr;

    LabelWithCloseWnd* tocLabelWithClose = nullptr;
    TreeCtrl* tocTreeCtrl = nullptr;
    UINT_PTR tocBoxSubclassId = 0;

    // whether the current tab's ToC has been loaded into the tree
    bool tocLoaded = false;
    // whether the ToC sidebar is currently visible
    bool tocVisible = false;
    // set to temporarily disable UpdateTocSelection
    bool tocKeepSelection = false;

    // state related to favorites
    HWND hwndFavBox = nullptr;
    LabelWithCloseWnd* favLabelWithClose = nullptr;
    TreeCtrl* favTreeCtrl = nullptr;
    Vec<DisplayState*> expandedFavorites;

    // vertical splitter for resizing left side panel
    SplitterCtrl* sidebarSplitter = nullptr;

    // horizontal splitter for resizing favorites and bookmars parts
    SplitterCtrl* favSplitter = nullptr;

    HWND hwndTabBar = nullptr;
    bool tabsVisible = false;
    bool tabsInTitlebar = false;
    // keeps the sequence of tab selection. This is needed for restoration
    // of the previous tab when the current one is closed. (Points into tabs.)
    Vec<TabInfo*>* tabSelectionHistory = nullptr;

    HWND hwndCaption = nullptr;
    CaptionInfo* caption = nullptr;
    int extendedFrameHeight = 0;

    TooltipCtrl* infotip = nullptr;

    HMENU menu = nullptr;
    bool isMenuHidden = false; // not persisted at shutdown

    DoubleBuffer* buffer = nullptr;

    MouseAction mouseAction = MouseAction::Idle;
    bool dragStartPending = false;

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    PointI dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    PointI dragStart;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int xScrollSpeed = 0;
    int yScrollSpeed = 0;

    // true while selecting and when currentTab->selectionOnPage != nullptr
    bool showSelection = false;
    // selection rectangle in screen coordinates (only needed while selecting)
    RectI selectionRect;
    // size of the current rectangular selection in document units
    SizeD selectionMeasure;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo> staticLinks;

    bool isFullScreen = false;
    PresentationMode presentation = PM_DISABLED;
    int windowStateBeforePresentation = 0;

    long nonFullScreenWindowStyle = 0;
    RectI nonFullScreenFrameRect{};

    RectI canvasRc{};   // size of the canvas (excluding any scroll bars)
    int currPageNo = 0; // cached value, needed to determine when to auto-update the ToC selection

    int wheelAccumDelta = 0;
    UINT_PTR delayedRepaintTimer = 0;

    Notifications* notifications = nullptr; // only access from UI thread

    HANDLE printThread = nullptr;
    bool printCanceled = false;

    HANDLE findThread = nullptr;
    bool findCanceled = false;

    LinkHandler* linkHandler = nullptr;
    PageElement* linkOnLastButtonDown = nullptr;
    const WCHAR* url = nullptr;

    ControllerCallback* cbHandler = nullptr;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show;        // are the markers visible?
        Vec<RectI> rects; // location of the markers in user coordinates
        int page;
        int hideStep; // value used to gradually hide the markers
    } fwdSearchMark;

    StressTest* stressTest = nullptr;

    TouchState touchState;

    FrameRateWnd* frameRateWnd = nullptr;

    SumatraUIAutomationProvider* uia_provider = nullptr;

    void UpdateCanvasSize();
    SizeI GetViewPortSize();
    void RedrawAll(bool update = false);
    void RepaintAsync(UINT delay = 0);

    void ChangePresentationMode(PresentationMode mode);

    void Focus();

    void ToggleZoom();
    void MoveDocBy(int dx, int dy);

    void ShowInfoTip(const WCHAR* text, RectI& rc, bool multiline = false);
    void HideInfoTip();
    void ShowNotification(const WCHAR* message, int options = NOS_DEFAULT,
                          NotificationGroupId groupId = NG_RESPONSE_TO_ACTION);

    bool CreateUIAProvider();
};

class LinkHandler {
    WindowInfo* owner;

    void ScrollTo(PageDestination* dest);
    void LaunchFile(const WCHAR* path, PageDestination* link);
    PageDestination* FindTocItem(TocItem* item, const WCHAR* name, bool partially = false);

  public:
    explicit LinkHandler(WindowInfo* win) : owner(win) {
    }

    void GotoLink(PageDestination* link);
    void GotoNamedDest(const WCHAR* name);
};

// TODO: this belongs in SumatraPDF.h but introduces a dependency on SettingsStructs.h
void SwitchToDisplayMode(WindowInfo* win, DisplayMode displayMode, bool keepContinuous = false);
void UpdateTreeCtrlColors(WindowInfo* win);
