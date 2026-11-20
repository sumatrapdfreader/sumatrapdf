/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TabsCtrl is a VirtCtrl hosted in its own child HWND (for SetWindowPos, drag
// capture, and title-bar HTTRANSPARENT). Each tab is a TabCtrl child with a
// VirtCloseButton. Include after gui/VirtCtrl.h (and its PlatformFont/Gfx
// prerequisites).

constexpr int kTabMinDx = 100;

struct TabsCtrl;
struct TabInfo;
struct TabCtrl;
struct TabsPlusBtn;
struct TabGroupHeader;
struct VirtRoot;
struct VirtCloseButton;
struct VirtMouseEvent;

constexpr Color kTabDefaultBgCol = (Color)-1;

struct TabInfo {
    Str text;
    Str tooltip;
    Str pageText;
    bool isPinned = false;
    bool canClose = true; // TODO: same as !isPinned?
    bool isDirty = false;
    // document failed to load: the title paints in red
    bool isError = false;
    // as wide as its label plus margins, with no room for a close button (Home)
    bool fitToLabel = false;
    UINT_PTR userData = 0;
    Color tabColor = (Color)0xfeffffff; // kColorUnset; use default tab color
    // id of its TabGroupInfo in TabsCtrl::groups; 0: none
    int groupId = 0;

    TabInfo() = default;
    ~TabInfo();
};

// how a group of neighbouring tabs looks: a colored chip with `label` before
// its tabs, and only the chip when collapsed. The app owns the groups' logic
struct TabGroupInfo {
    int id = 0;
    Str label;
    Color color = (Color)0;
    bool collapsed = false;

    TabGroupInfo() = default;
    ~TabGroupInfo();
};

// Tab bar as a virtual control: owns TabCtrl children, paints and hit-tests
// them via VirtRoot on a host HWND that the frame positions.
struct TabsCtrl : VirtCtrl {
    struct CreateArgs {
        HWND parent = nullptr;
        PlatformFont* font = nullptr;
        bool withToolTips = false;
        int ctrlID = 0;
        int tabDefaultDx = 300;
        bool equalWidth = true;
        bool isRtl = false;
    };

    struct MouseState {
        int tabIdx = -1;
        bool overClose = false;
        // if true, mouse is over right half of the tab rectangle
        // used to make drag&drop determine a better position for drop
        bool inRightHalf = false;
        TabInfo* tabInfo = nullptr;
        // != 0 when the mouse is over the header chip of that group
        int headerGroupId = 0;
    };

    struct SelectionChangingEvent {
        TabsCtrl* tabs = nullptr;
        int tabIdx = -1;
        // set to true to prevent changing tabs
        bool preventChanging = false;
    };

    struct SelectionChangedEvent {
        TabsCtrl* tabs = nullptr;
        int tabIdx;
    };

    struct ClosedEvent {
        TabsCtrl* tabs = nullptr;
        int tabIdx = -1;
    };

    struct MigrationEvent {
        TabsCtrl* tabs = nullptr;
        int tabIdx;
        Point releasePoint;
    };

    struct DraggedEvent {
        TabsCtrl* tabs = nullptr;
        int tab1 = -1;
        int tab2 = -1;
        // the group of the tab or chip it was dropped on; 0: none
        int groupId = 0;
    };

    struct GroupClickEvent {
        TabsCtrl* tabs = nullptr;
        int groupId = 0;
    };

    using SelectionChangingHandler = Func1<SelectionChangingEvent*>;
    using SelectionChangedHandler = Func1<SelectionChangedEvent*>;
    using ClosedHandler = Func1<ClosedEvent*>;
    using MigrationHandler = Func1<MigrationEvent*>;
    using DraggedHandler = Func1<DraggedEvent*>;
    using GroupClickHandler = Func1<GroupClickEvent*>;

    // host HWND (child of the frame); GetHwnd() also returns this once attached
    HWND hwnd = nullptr;
    PlatformFont* font = nullptr; // interned, not owned

    int ctrlID = 0;
    bool withToolTips = false;
    bool inTitleBar = false;
    bool draggingTab = false;
    // dx of tab if there's more space available
    int tabDefaultDx = 300;
    // false: a tab is as wide as its name, up to tabDefaultDx
    bool equalWidth = true;

    Vec<TabInfo*> tabs;
    // VirtRoot over this control on `hwnd`; does not own us
    VirtRoot* vroot = nullptr;
    // in tab order (children are reversed when the UI is RTL)
    Vec<TabCtrl*> tabCtrls;
    // the "+" after the tabs, always there
    TabsPlusBtn* plusBtn = nullptr;
    int selectedIdx = -1;

    // groups' chips are laid out before the first tab of the group
    Vec<TabGroupInfo*> groups;
    Vec<TabGroupHeader*> headerCtrls;

    // tracking state of which tab is highlighted etc.
    int tabHighlighted = -1;
    int tabHighlightedClose = -1;
    int tabBeingClosed = -1;
    Point lastMousePos{-1, -1};
    // where we grabbed the tab with a leftclick, in tab coordinates
    Point grabLocation;

    // if >= 0 will paint this tab as selected vs. the real selected
    int tabForceShowSelected = -1;

    // Chrome-like close: keep tab widths frozen briefly after closing via close button
    bool tabWidthFrozen = false;
    int frozenTabDx = 0;

    ClosedHandler onTabClosed;
    SelectionChangingHandler onSelectionChanging;
    SelectionChangedHandler onSelectionChanged;
    MigrationHandler onTabMigration;
    // set: the app moves the dropped tab; unset: the tab strip does
    DraggedHandler onTabDragged;
    // a group's chip was clicked
    GroupClickHandler onGroupClick;
    // must not change the tabs directly: that deletes the button being clicked
    Func0 onPlusClick;

    // colors: kColTab* (the unselected and hovered shades are derived from
    // kColTabBg; a tab with a TabInfo::tabColor of its own overrides it)

    Size tabSize{-1, -1};

    TabsCtrl();
    ~TabsCtrl() override;

    HWND Create(TabsCtrl::CreateArgs&);
    void Destroy();

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;

    // show/hide host HWND + VirtCtrl visibility
    void SetIsVisible(bool);
    bool IsVisible() const;

    PlatformFont* GetFont() const;
    void SetFont(PlatformFont*);

    int InsertTab(int idx, TabInfo*, bool update = true);
    TabInfo* GetTab(int idx);
    int TabDx(int idx);
    void SwapTabs(int idx1, int idx2);

    void SetTextAndTooltip(int idx, Str text, Str tooltip);
    void SetPageText(int idx, Str);
    void SetTabDirty(int idx, bool isDirty);

    int TabCount();

    UINT_PTR RemoveTab(int idx);

    template <typename T>
    T RemoveTab(int idx) {
        UINT_PTR res = RemoveTab(idx);
        return (T)res;
    }
    void RemoveAllTabs();

    int GetSelected();
    int SetSelected(int idx);
    bool IsValidIdx(int idx);

    void SetHighlighted(int idx);

    TabGroupInfo* FindGroup(int groupId);
    TabGroupInfo* GroupOfTab(int idx);
    bool IsTabHidden(int idx);
    int GroupFirstIdx(int groupId);
    int GroupLastIdx(int groupId);
    void ClearGroups();

    void LayoutTabs();
    void ScheduleRepaint();
    TabsCtrl::MouseState TabStateFromMousePosition(const Point& p);
    HBITMAP RenderForDragging(int idx);

    TabCtrl* TabCtrlAt(int idx);
    void RebuildTabCtrls();
    void UpdateHover(int tabUnderMouse);
    void OnTabMouseDown(TabCtrl*, VirtMouseEvent&);
    void CloseTab(int idx);

    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);
};

template <typename T>
T GetTabsUserData(TabsCtrl* tabs, int idx) {
    TabInfo* tabInfo = tabs->GetTab(idx);
    if (!tabInfo) {
        // GetTab returns nullptr for an out-of-range/sentinel idx (e.g. -1 when
        // nothing is selected); don't dereference it
        return (T)0;
    }
    return (T)tabInfo->userData;
}
