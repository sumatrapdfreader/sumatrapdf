/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

//--- Wnd

// global messages for wingui start at WM_APP + 0x300 to not
// collide with values defined for the app
const DWORD UWM_DELAYED_CTRL_BACK = WM_APP + 0x300 + 1;

TempStr WinMsgNameTemp(UINT);

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
enum WindowBorderStyle { kWindowBorderNone, kWindowBorderClient, kWindowBorderStatic };

struct Wnd;

Wnd* WndListFindByHwnd(HWND);
void MarkHWNDDestroyed(HWND);

struct ContextMenuEvent {
    Wnd* w = nullptr;

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseScreen{};
};

using ContextMenuHandler = Func1<ContextMenuEvent*>;

struct CreateControlArgs {
    HWND parent = nullptr;
    const WCHAR* className = nullptr;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos = {};
    HMENU ctrlId = 0;
    bool visible = true;
    HFONT font = nullptr;
    const char* text = nullptr;
};

struct CreateCustomArgs {
    HWND parent = nullptr;
    const WCHAR* className = nullptr;
    const char* title = nullptr;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos = {};
    // don't set both menu and cmdId
    HMENU menu = nullptr;
    int cmdId = 0; // command sent on click
    bool visible = true;
    HFONT font = nullptr;
    HICON icon = nullptr;
    COLORREF bgColor = kColorUnset;
};

struct WmEvent {
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;
    uintptr_t userData = 0;
    Wnd* self = nullptr;

    bool didHandle = true; // common case so set as default
};

struct Wnd : ILayout {
    struct CloseEvent {
        WmEvent* e = nullptr;
    };
    struct DestroyEvent {
        WmEvent* e = nullptr;
    };

    using CloseHandler = Func1<CloseEvent*>;
    using DestroyHandler = Func1<DestroyEvent*>;

    Wnd();
    Wnd(HWND hwnd);
    virtual ~Wnd();
    void Destroy();

    HWND CreateCustom(const CreateCustomArgs&);
    HWND CreateControl(const CreateControlArgs&);

    virtual Size GetIdealSize();

    // ILayout
    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    void SetInsetsPt(int top, int right = -1, int bottom = -1, int left = -1);

    void Attach(HWND hwnd);
    void AttachDlgItem(UINT id, HWND parent);
    HWND Detach();

    void Subclass();
    void UnSubclass();

    void Cleanup();

    // over-ride those to hook into message processing
    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    virtual bool PreTranslateMessage(MSG& msg);
    virtual LRESULT OnNotify(int controlId, NMHDR* nmh);
    virtual LRESULT OnNotifyReflect(WPARAM, LPARAM);
    virtual LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);

    virtual void OnAttach();
    virtual void OnFocus();
    virtual bool OnCommand(WPARAM wparam, LPARAM lparam);
    virtual int OnCreate(CREATESTRUCT*);
    virtual void OnContextMenu(Point pt);
    virtual void OnDropFiles(HDROP drop_info);
    virtual void OnGetMinMaxInfo(MINMAXINFO* mmi);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnMove(POINTS* pts);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual void OnSize(UINT msg, UINT type, SIZE size);
    virtual void OnTaskbarCallback(UINT msg, LPARAM lparam);
    virtual void OnTimer(UINT_PTR event_id);
    virtual void OnWindowPosChanging(WINDOWPOS* window_pos);

    virtual void SetColors(COLORREF textColor, COLORREF bgColor);

    void Close();
    void SetPos(RECT* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(const char*);
    TempStr GetTextTemp();

    HFONT GetFont();
    void SetFont(HFONT font);

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    HBRUSH BackgroundBrush();

    Kind kind = nullptr;
    uintptr_t userData = 0;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    // used by all controls that inherit
    COLORREF bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    COLORREF textColor = kColorUnset;

    ILayout* layout = nullptr;

    ContextMenuHandler onContextMenu;

    CloseHandler onClose;
    DestroyHandler onDestroy;
};

bool PreTranslateMessage(MSG& msg);

//--- Static

struct Static : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        const char* text = nullptr;
    };

    Static();

    Func0 onClick;

    HWND Create(const CreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

//--- Button

struct Button : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        const char* text = nullptr;
    };

    Func0 onClick{};

    bool isDefault = false;

    Button();

    HWND Create(const CreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

Button* CreateButton(HWND parent, const char* s, const Func0& onClick);
Button* CreateDefaultButton(HWND parent, const char* s);

//--- Tooltip

// a tooltip manages multiple areas within HWND
struct Tooltip : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
    };

    Tooltip();
    HWND Create(const CreateArgs&);
    Size GetIdealSize() override;

    int Add(const char* s, const Rect& rc, bool multiline);
    void Update(int id, const char* s, const Rect& rc, bool multiline);
    void Delete(int id = 0);

    int SetSingle(const char* s, const Rect& rc, bool multiline);

    int Count();

    TempStr GetTextTemp(int id = 0);

    void SetDelayTime(int type, int timeInMs);
    void SetMaxWidth(int dx);

    // window this tooltip is associated with
    HWND parent = nullptr;

    Vec<int> tooltipIds;
};

//--- Edit
using TextChangedHandler = Func0;

struct Edit : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isMultiLine = false;
        bool withBorder = false;
        const char* cueText = nullptr;
        const char* text = nullptr;
        int idealSizeLines = 1;
        HFONT font = nullptr;
    };

    TextChangedHandler onTextChanged;

    // set before Create()
    int idealSizeLines = 1;
    int maxDx = 0;

    Edit();
    ~Edit() override;

    HWND Create(const CreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetSelection(int start, int end);
    void SetCursorPosition(int pos);
    void SetCursorPositionAtEnd();
    bool HasBorder();
};

//--- ListBox

struct ListBox : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        int idealSizeLines = 0;
        HFONT font = nullptr;
    };

    using SelectionChangedHandler = Func0;
    using DoubleClickHandler = Func0;

    ListBoxModel* model = nullptr;
    SelectionChangedHandler onSelectionChanged;
    DoubleClickHandler onDoubleClick;

    Size idealSize = {};
    int idealSizeLines = 0;

    ListBox();
    virtual ~ListBox();

    HWND Create(const CreateArgs&);

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    int GetItemHeight(int);

    Size GetIdealSize() override;

    int GetCount();
    int GetCurrentSelection();
    bool SetCurrentSelection(int);
    void SetModel(ListBoxModel*);
};

//--- CheckboxCtrl

struct Checkbox : Wnd {
    enum class State {
        Unchecked = BST_UNCHECKED,
        Checked = BST_CHECKED,
        Indeterminate = BST_INDETERMINATE,
    };

    struct CreateArgs {
        HWND parent = nullptr;
        const char* text = nullptr;
        State initialState = State::Unchecked;
    };

    using StateChangedHandler = Func0;

    StateChangedHandler onStateChanged;

    Checkbox();

    HWND Create(const CreateArgs&);

    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetState(State);
    State GetState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

//--- Progress

struct Progress : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        int initialMax = 0;
    };

    Progress();

    int idealDx = 0;
    int idealDy = 0;

    HWND Create(const CreateArgs&);

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    Size GetIdealSize() override;
};

//--- DropDown

struct DropDown : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        // TODO: model or items
    };

    using SelectionChangedHandler = Func0;

    // TODO: use DropDownModel
    StrVec items;
    SelectionChangedHandler onSelectionChanged;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDown::CreateArgs&);

    Size GetIdealSize() override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    int GetCurrentSelection();
    void SetCurrentSelection(int n);
    void SetItems(StrVec& newItems);
    void SetItemsSeqStrings(const char* items);
    void SetCueBanner(const char*);
};

//--- Trackbar

struct Trackbar;

struct Trackbar : Wnd {
    struct PositionChangingEvent {
        Trackbar* trackbar = nullptr;
        int pos = -1;
        NMTRBTHUMBPOSCHANGING* info = nullptr;
    };

    using PositionChangingHandler = Func1<PositionChangingEvent*>;

    struct CreateArgs {
        HWND parent = nullptr;
        bool isHorizontal = true;
        int rangeMin = 1;
        int rangeMax = 5;
        HFONT font = nullptr;
    };

    Size idealSize{};

    // for WM_NOTIFY with TRBN_THUMBPOSCHANGING
    PositionChangingHandler onPositionChanging;

    Trackbar();
    ~Trackbar() override = default;

    HWND Create(const CreateArgs&);

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;
    void SetRange(int min, int max);
    int GetRangeMin();
    int getRangeMax();

    void SetValue(int);
    int GetValue();
};

// -- Splitter

enum class SplitterType {
    Horiz,
    Vert,
};

struct Splitter;

struct Splitter : Wnd {
    // called when user drags the splitter ('finishedDragging' is false) and when drag is finished ('finishedDragging'
    // is true). the owner can constrain splitter by using current cursor position and setting resizeAllowed to false if
    // it's not allowed to go there
    struct MoveEvent {
        Splitter* w = nullptr;
        bool finishedDragging = false;
        // user can set to false to forbid resizing here
        bool resizeAllowed = true;
    };

    using MoveHandler = Func1<MoveEvent*>;

    struct CreateArgs {
        HWND parent = nullptr;
        SplitterType type = SplitterType::Horiz;
        bool isLive = true;
        COLORREF backgroundColor = kColorUnset;
    };

    SplitterType type = SplitterType::Horiz;
    bool isLive = true;
    MoveHandler onMove;

    HBITMAP bmp = nullptr;
    HBRUSH brush = nullptr;

    Point prevResizeLinePos{};
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool parentClipsChildren = false;

    Splitter();
    ~Splitter() override;

    HWND Create(const CreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
};

//--- TreeView

struct TreeView;

struct TreeView : Wnd {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        DWORD exStyle = 0; // additional flags, will be OR with the rest
        bool fullRowSelect = false;
    };

    struct GetTooltipEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        NMTVGETINFOTIPW* info = nullptr;
    };

    struct SelectionChangedEvent {
        TreeView* treeView = nullptr;
        TreeItem prevSelectedItem = 0;
        TreeItem selectedItem = 0;
        NMTREEVIEW* nmtv = nullptr;
        bool byKeyboard = false;
        bool byMouse = false;
    };

    struct CustomDrawEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        NMTVCUSTOMDRAW* nm = nullptr;

        LRESULT result = 0;
    };

    struct ClickEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        bool isDblClick = false;

        // mouse x,y position relative to the window
        Point mouseWindow{};
        // global (screen) mouse x,y position
        Point mouseScreen{};

        LRESULT result = 0;
    };

    struct KeyDownEvent {
        TreeView* treeView = nullptr;
        NMTVKEYDOWN* nmkd = nullptr;
        int keyCode = 0;
        u32 flags = 0;

        LRESULT result = 0;
    };

    using KeyDownHandler = Func1<KeyDownEvent*>;
    using ClickHandler = Func1<ClickEvent*>;
    using CustomDrawHandler = Func1<CustomDrawEvent*>;
    using GetTooltipHandler = Func1<TreeView::GetTooltipEvent*>;
    using SelectionChangedHandler = Func1<SelectionChangedEvent*>;

    TreeView();
    ~TreeView() override;

    HWND Create(const CreateArgs&);

    void SetColors(COLORREF col, COLORREF bgCol) override;

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotifyReflect(WPARAM, LPARAM) override;

    Size GetIdealSize() override;
    void SetToolTipsDelayTime(int type, int timeInMs);
    HWND GetToolTipsHwnd();

    bool IsExpanded(TreeItem ti);
    bool GetItemRect(TreeItem ti, bool justText, RECT& r);
    TreeItem GetSelection();
    bool SelectItem(TreeItem ti);
    void ExpandAll();
    void CollapseAll();
    void Clear();

    HTREEITEM GetHandleByTreeItem(TreeItem item);
    char* GetDefaultTooltipTemp(TreeItem ti);
    TreeItem GetItemAt(int x, int y);
    TreeItem GetTreeItemByHandle(HTREEITEM item);
    bool UpdateItem(TreeItem ti);
    void SetTreeModel(TreeModel* tm);
    void SetState(TreeItem item, bool enable);
    bool GetState(TreeItem item);
    TreeItemState GetItemState(TreeItem ti);

    bool fullRowSelect = false;
    Size idealSize{};

    TreeModel* treeModel = nullptr; // not owned by us

    // for WM_NOTIFY with TVN_GETINFOTIP
    GetTooltipHandler onGetTooltip;

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    CustomDrawHandler onCustomDraw;

    // for WM_NOTIFY with TVN_SELCHANGED
    SelectionChangedHandler onSelectionChanged;

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    ClickHandler onClick;

    // for WM_NOITFY with TVN_KEYDOWN
    KeyDownHandler onKeyDown;

    // private
    TVITEMW item{};
};

TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt);

//--- TabsCtrl

using Gdiplus::PathData;

#define kTabBarDy 24
#define kTabMinDx 100

struct TabsCtrl;
struct TabInfo;

#define kTabDefaultBgCol (COLORREF) - 1

struct TabInfo {
    char* text = nullptr;
    char* tooltip = nullptr;
    bool isPinned = false;
    bool canClose = true; // TODO: same as !isPinned?
    UINT_PTR userData = 0;

    TabInfo() = default;
    ~TabInfo();

    // for internal use
    Rect r;
    Rect rClose;
    Size titleSize;
    Point titlePos;
};

struct TabsCtrl : Wnd {
    struct MouseState {
        int tabIdx = -1;
        bool overClose = false;
        TabInfo* tabInfo = nullptr;
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
    };

    using SelectionChangingHandler = Func1<SelectionChangingEvent*>;
    using SelectionChangedHandler = Func1<SelectionChangedEvent*>;
    using ClosedHandler = Func1<ClosedEvent*>;
    using MigrationHandler = Func1<MigrationEvent*>;
    using DraggedHandler = Func1<DraggedEvent*>;

    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        bool withToolTips = false;
        int ctrlID = 0;
        int tabDefaultDx = 300;
    };

    int ctrlID = 0;
    bool withToolTips = false;
    bool inTitleBar = false;
    bool draggingTab = false;
    // dx of tab if there's more space available
    int tabDefaultDx = 300;

    Vec<TabInfo*> tabs;

    // tracking state of which tab is highlighted etc.
    int tabHighlighted = -1;
    int tabHighlightedClose = -1;
    int tabBeingClosed = -1;
    Point lastMousePos{-1, -1};
    // where we grabbed the tab with a leftclick, in tab coordinates
    Point grabLocation;

    // if >= 0 will paint this tab as selected vs. the real selected
    int tabForceShowSelected = -1;

    ClosedHandler onTabClosed;
    SelectionChangingHandler onSelectionChanging;
    SelectionChangedHandler onSelectionChanged;
    MigrationHandler onTabMigration;
    DraggedHandler onTabDragged;

    COLORREF currBgCol = 0;
    COLORREF tabBackgroundBg = 0;
    COLORREF tabBackgroundText = 0;
    COLORREF tabBackgroundCloseX = 0;
    COLORREF tabBackgroundCloseCircle = 0;
    COLORREF tabSelectedBg = 0;
    COLORREF tabSelectedText = 0;
    COLORREF tabSelectedCloseX = 0;
    COLORREF tabSelectedCloseCircle = 0;
    COLORREF tabHighlightedBg = 0;
    COLORREF tabHighlightedText = 0;
    COLORREF tabHighlightedCloseX = 0;
    COLORREF tabHighlightedCloseCircle = 0;
    COLORREF tabHoveredCloseX = 0;
    COLORREF tabHoveredCloseCircle = 0;
    COLORREF tabClickedCloseX = 0;
    COLORREF tabClickedCloseCircle = 0;

    Size tabSize{-1, -1};

    TabsCtrl();
    ~TabsCtrl() override;

    HWND Create(TabsCtrl::CreateArgs&);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotifyReflect(WPARAM, LPARAM) override;

    Size GetIdealSize() override;

    int InsertTab(int idx, TabInfo*);
    TabInfo* GetTab(int idx);
    void SwapTabs(int idx1, int idx2);

    void SetTextAndTooltip(int idx, const char* text, const char* tooltip);

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

    HWND GetToolTipsHwnd();

    void LayoutTabs();
    void ScheduleRepaint();
    TabsCtrl::MouseState TabStateFromMousePosition(const Point& p);
    void Paint(HDC hdc, RECT& rc);
    HBITMAP RenderForDragging(int idx);
};

template <typename T>
T GetTabsUserData(TabsCtrl* tabs, int idx) {
    TabInfo* tabInfo = tabs->GetTab(idx);
    return (T)tabInfo->userData;
}

template <typename T>
void DeleteWnd(T** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog);

// TODO: those are hacks
HWND GetCurrentModelessDialog();
void SetCurrentModelessDialog(HWND);

#define kColCloseX RGB(0xa0, 0xa0, 0xa0)
#define kColCloseXHover RGB(0xf9, 0xeb, 0xeb)   // white-ish
#define kColCloseXHoverBg RGB(0xC1, 0x35, 0x35) // red-ish

struct DrawCloseButtonArgs {
    HDC hdc = nullptr;
    Rect r;
    bool isHover = false;
    COLORREF colHoverBg = kColCloseXHoverBg;
    COLORREF colX = kColCloseX;
    COLORREF colXHover = kColCloseXHover;
};

void DrawCloseButton(const DrawCloseButtonArgs& args);
void DrawCloseButton2(const DrawCloseButtonArgs&);
