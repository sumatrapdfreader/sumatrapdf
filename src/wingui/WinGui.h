/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

//--- WindowBase

// global messages for wingui start at WM_APP + 0x300 to not
// collide with values defined for the app
const DWORD UWM_DELAYED_CTRL_BACK = WM_APP + 0x300 + 1;

TempStr WinMsgNameTemp(UINT);

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
enum WindowBorderStyle {
    kWindowBorderNone,
    kWindowBorderClient,
    kWindowBorderStatic
};

struct WindowBase;
struct VirtRoot;

WindowBase* WindowBaseFromHwnd(HWND);
void MarkHWNDDestroyed(HWND);
bool HwndWasDestroyed(HWND);

struct ControlBase;

struct ContextMenuEvent {
    ControlBase* w = nullptr;

    // mouse x,y position relative to the window
    Point mouseWindow;
    // global (screen) mouse x,y position
    Point mouseScreen;
};

using ContextMenuHandler = Func1<ContextMenuEvent*>;

struct CreateControlArgs {
    HWND parent = nullptr;
    WStr className;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos;
    HMENU ctrlId = nullptr;
    bool visible = true;
    HFONT font = nullptr;
    Str text;
    bool isRtl = false;
};

struct CreateCustomArgs {
    HWND parent = nullptr;
    WStr className;
    Str title;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos;
    // don't set both menu and cmdId
    HMENU menu = nullptr;
    int cmdId = 0; // command sent on click
    bool visible = true;
    HFONT font = nullptr;
    HICON icon = nullptr;
    COLORREF bgColor = kColorUnset;
    bool isRtl = false;
};

struct WmEvent {
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;
    uintptr_t userData = 0;
    // the WindowBase / ControlBase the message went to; consumers cast it
    void* self = nullptr;

    bool didHandle = true; // common case so set as default
};

// WM_KEYDOWN / WM_SYSKEYDOWN for WindowBase::OnKeyDown (like VirtKeyEvent).
// hwnd is MSG::hwnd (often a child Edit/DropDown), not necessarily the WindowBase.
struct KeyEvent {
    HWND hwnd = nullptr;
    int vkey = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool isSysKey = false; // true for WM_SYSKEYDOWN
};

// Base of the top-level windows (and the child windows that place themselves,
// like the notification toasts). Not an ILayout: a window isn't positioned by a
// parent's layout - it has a `layout` of its own children instead
struct WindowBase {
    struct CloseEvent {
        WmEvent* e = nullptr;
    };
    struct DestroyEvent {
        WmEvent* e = nullptr;
    };

    using CloseHandler = Func1<CloseEvent*>;
    using DestroyHandler = Func1<DestroyEvent*>;

    WindowBase();
    WindowBase(HWND hwnd);
    virtual ~WindowBase();
    void Destroy();

    HWND CreateCustom(const CreateCustomArgs&);

    void SetVisibility(Visibility);
    Visibility GetVisibility();

    void Attach(HWND hwnd);
    HWND Detach();

    void Subclass();
    void UnSubclass();

    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    // default: WM_KEYDOWN/WM_SYSKEYDOWN -> OnKeyDown; override for non-key msgs
    virtual bool PreTranslateMessage(MSG& msg);
    // return true to consume; default handles Tab among mixed HWND/virtual layout
    virtual bool OnKeyDown(KeyEvent&);
    virtual LRESULT OnNotify(int controlId, NMHDR* nmh);

    virtual void OnAttach();
    virtual void OnFocus();
    virtual bool OnCommand(WPARAM wparam, LPARAM lparam);
    virtual int OnCreate(CREATESTRUCT*);
    virtual void OnDropFiles(HDROP drop_info);
    virtual void OnGetMinMaxInfo(MINMAXINFO* mmi);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnMove(POINTS* pts);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual void OnSize(UINT msg, UINT type, Size size);
    virtual void OnTaskbarCallback(UINT msg, LPARAM lparam);
    virtual void OnTimer(UINT_PTR timerId);
    virtual void OnWindowPosChanging(WINDOWPOS* window_pos);

    virtual void SetColors(COLORREF textColor, COLORREF bgColor);

    void Close();
    void SetPos(Rect* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(Str);
    TempStr GetTextTemp();

    HFONT GetFont();
    void SetFont(HFONT font);

    // dpi of the monitor this window is on
    int GetDpi() const;

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    // sends a control's own messages (colors, owner draw, ...) back to it
    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    HBRUSH BackgroundBrush();

    // lays `layout` out at `size` and refreshes the virtual controls
    void DoLayout(Size);
    void DoLayout();

    // Tab ring over `layout`, across HWND and virtual controls alike. A virtual
    // control holding the focus means this window holds the win32 focus and
    // `vroot->focused` says which one it is
    bool TabNavigate(bool backwards);
    void SetFocusTo(ControlBase*);
    void SetFocusTo(VirtWnd*);

    Kind kind = nullptr;
    uintptr_t userData = 0;

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    COLORREF bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    COLORREF textColor = kColorUnset;

    // the layout of our children, if we have one. It can hold HWND controls
    // (ControlBase) and virtual ones (VirtWnd) side by side
    ILayout* layout = nullptr;
    // the virtual controls of `layout`, if it has any. Created on demand by
    // DoLayout(), owned here, but it doesn't own the controls - `layout` does
    VirtRoot* vroot = nullptr;
    // relayout on WM_SIZE. Off by default: most windows do it themselves
    bool autoLayout = false;

    CloseHandler onClose;
    DestroyHandler onDestroy;
};

bool PreTranslateMessage(MSG& msg);

// Base of the controls that a layout positions: the win32 controls (Static,
// Button, Edit, ...) and the custom-drawn ones (Splitter, TabsCtrl).
// It is an ILayout, and it has no window-only machinery (no WM_CLOSE handler,
// no min/max info, no drop files, no taskbar callback)
struct ControlBase : ILayout {
    struct DestroyEvent {
        WmEvent* e = nullptr;
    };

    using DestroyHandler = Func1<DestroyEvent*>;

    ControlBase();
    ~ControlBase() override;
    void Destroy();

    HWND CreateControl(const CreateControlArgs&);
    HWND CreateCustom(const CreateCustomArgs&);

    virtual Size GetIdealSize();

    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    ControlBase* AsControl() override;

    void SetInsetsPt(int uniform);
    void SetInsetsPt(int topBottom, int leftRight);
    void SetInsetsPt(int top, int right, int bottom, int left);

    void Attach(HWND hwnd);
    void AttachDlgItem(UINT id, HWND parent);
    HWND Detach();

    void Subclass();
    void UnSubclass();

    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    virtual LRESULT OnNotify(int controlId, NMHDR* nmh);
    virtual LRESULT OnNotifyReflect(WPARAM, LPARAM);
    virtual LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);

    virtual void OnAttach();
    virtual void OnFocus();
    virtual bool OnCommand(WPARAM wparam, LPARAM lparam);
    virtual int OnCreate(CREATESTRUCT*);
    virtual void OnContextMenu(Point pt);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual void OnSize(UINT msg, UINT type, Size size);
    virtual void OnTimer(UINT_PTR timerId);

    virtual void SetColors(COLORREF textColor, COLORREF bgColor);

    void SetPos(Rect* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(Str);
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
    Size childSize;
    Rect lastBounds;

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    COLORREF bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    COLORREF textColor = kColorUnset;

    // a control can host a layout tree of its own, real and virtual mixed
    ILayout* layout = nullptr;
    VirtRoot* vroot = nullptr;

    void DoLayout(Size);

    ContextMenuHandler onContextMenu;
    DestroyHandler onDestroy;
};

ControlBase* ControlFromHwnd(HWND);
void SizeToIdealSize(ControlBase* c);

struct VirtRoot;
struct VirtWnd;

//--- Button

// The only buttons left as HWND controls are the installer's and the
// uninstaller's: they have no theme to draw from, so Windows' own themed push
// button (with its UAC shield, mnemonics and dialog keyboard navigation) beats
// anything we would draw. Everything in the app itself uses VirtButton.
struct Button : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        Str text;
        bool isRtl = false;
    };

    Func0 onClick;

    bool isDefault = false;

    Button();

    HWND Create(const CreateArgs&);

    Size GetIdealSize() override;

    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

Button* CreateDefaultButton(HWND parent, Str s, bool isRtl);

//--- Tooltip

// a tooltip manages multiple areas within HWND
struct Tooltip : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        bool isRtl = false;
    };

    Tooltip();
    HWND Create(const CreateArgs&);
    Size GetIdealSize() override;

    int Add(Str s, const Rect& rc, bool multiline);
    void Update(int id, Str s, const Rect& rc, bool multiline);
    void Delete(int id = 0);

    int SetSingle(Str s, const Rect& rc, bool multiline);
    // Track-mode tip at an absolute screen position (e.g. keyboard nav), not the cursor.
    // If maxRightScreen > 0, shifts left so the bubble stays left of that x.
    int SetSingleAt(Str s, const Rect& rc, Point screenPos, bool multiline, int maxRightScreen = 0);

    int Count();

    TempStr GetTextTemp(int id = 0);

    void SetDelayTime(int type, int timeInMs);
    void SetMaxWidth(int dx);

    // window this tooltip is associated with
    HWND parent = nullptr;

    Vec<int> tooltipIds;
};

struct TooltipInfo {
    Str s;
    Rect r;
    int id;
};

int TooltipGetCount(HWND hwnd);
void TooltipRemoveAll(HWND hwnd);
void TooltipAddTools(HWND hwnd, HWND owner, TooltipInfo* tools, int nTools);

//--- Edit
using TextChangedHandler = Func0;

COLORREF EditBottomBorderColor();

struct Edit : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isMultiLine = false;
        bool withBorder = false;
        // 1px NC underline under the client area (no WS_EX_CLIENTEDGE)
        bool withBottomBorder = false;
        Str cueText;
        Str text;
        int idealSizeLines = 1;
        // if > 0: ideal width is at least ~N average character widths
        int idealWidthChars = 0;
        // if > 0: ideal width is capped at ~N average character widths
        int maxWidthChars = 0;
        // if > 0: unscaled px of space between the text and all 4 client edges
        // (multi-line only; the edit control ignores EM_SETRECT otherwise)
        int textPadding = 0;
        HFONT font = nullptr;
        bool isRtl = false;
    };

    // EN_CHANGE only (do not use for kill-focus flush; see onKillFocus)
    TextChangedHandler onTextChanged;
    // EN_KILLFOCUS only (e.g. flush annotation Contents before blur)
    TextChangedHandler onKillFocus;
    // EN_SETFOCUS only
    TextChangedHandler onFocus;

    // set before Create() (pixels); or use idealWidthChars / maxWidthChars
    int idealSizeLines = 1;
    int idealDx = 0;
    int maxDx = 0;
    // DPI-scaled CreateArgs.textPadding
    int textPadding = 0;

    // remembers CreateArgs.withBorder: with themes darkmodelib strips
    // WS_EX_CLIENTEDGE / WS_BORDER and draws the border in a subclass, so
    // window styles can't be used to detect the border
    bool createdWithBorder = false;
    bool createdWithBottomBorder = false;

    Edit();
    ~Edit() override;

    HWND Create(const CreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetIdealWidthChars(int nChars);
    void SetMaxWidthChars(int nChars);

    int GetLeftTextMargin();

    void SetCue(Str);
    void SetMargins(int left, int right);
    void SetSelection(int start, int end);
    void SelectAll();
    void SetCursorPosition(int pos);
    void SetCursorPositionAtEnd();
    bool HasBorder();
    void ApplyTextPadding();
};

//--- CheckboxCtrl

struct Checkbox : ControlBase {
    enum class State {
        Unchecked = BST_UNCHECKED,
        Checked = BST_CHECKED,
        Indeterminate = BST_INDETERMINATE,
    };

    struct CreateArgs {
        HWND parent = nullptr;
        Str text;
        State initialState = State::Unchecked;
        bool isRtl = false;
    };

    using StateChangedHandler = Func0;

    StateChangedHandler onStateChanged;

    Checkbox();

    HWND Create(const CreateArgs&);

    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetState(State);
    State GetState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

//--- Progress

struct Progress : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        int initialMax = 0;
        bool isRtl = false;
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

struct DropDown : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        bool isRtl = false;
        bool isEditable = false;
        // TODO: model or items
    };

    using SelectionChangedHandler = Func0;

    // TODO: use DropDownModel
    StrVec items;
    SelectionChangedHandler onSelectionChanged;
    TextChangedHandler onTextChanged;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDown::CreateArgs&);

    Size GetIdealSize() override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    int GetCurrentSelection();
    void SetCurrentSelection(int n);
    void SetItems(StrVec& newItems);
    void SetItemsSeqStrings(SeqStrings items);
    void SetCueBanner(Str);
};

//--- Trackbar

struct Trackbar;

struct Trackbar : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isHorizontal = true;
        int rangeMin = 1;
        int rangeMax = 5;
        HFONT font = nullptr;
        bool isRtl = false;
    };

    struct PositionChangingEvent {
        Trackbar* trackbar = nullptr;
        int pos = -1;
        NMTRBTHUMBPOSCHANGING* info = nullptr;
    };

    using PositionChangingHandler = Func1<PositionChangingEvent*>;

    Size idealSize;

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

//--- TreeView

struct TreeView;

struct TreeView : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        DWORD exStyle = 0; // additional flags, will be OR with the rest
        bool fullRowSelect = false;
        bool isRtl = false;
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
        Point mouseWindow;
        // global (screen) mouse x,y position
        Point mouseScreen;

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
    bool GetItemRect(TreeItem ti, bool justText, Rect& r);
    TreeItem GetSelection();
    bool SelectItem(TreeItem ti);
    void ExpandAll();
    void CollapseAll();
    void Clear();

    HTREEITEM GetHandleByTreeItem(TreeItem item);
    TempStr GetDefaultTooltipTemp(TreeItem ti);
    TreeItem GetItemAt(int x, int y);
    TreeItem GetTreeItemByHandle(HTREEITEM item);
    bool UpdateItem(TreeItem ti);
    void SetTreeModel(TreeModel* tm);
    void SetState(TreeItem item, bool enable);
    bool GetState(TreeItem item);
    TreeItemState GetItemState(TreeItem ti);

    bool fullRowSelect = false;
    Size idealSize;

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

TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, Point& pt);

//--- TabsCtrl

using Gdiplus::PathData;

#define kTabBarDy 24
#define kTabMinDx 100

struct TabsCtrl;
struct TabInfo;
struct TabWnd;
struct VirtRoot;
struct VirtCloseButton;
struct VirtMouseEvent;

#define kTabDefaultBgCol ((COLORREF)(-1))

struct TabInfo {
    Str text;
    Str tooltip;
    bool isPinned = false;
    bool canClose = true; // TODO: same as !isPinned?
    bool isDirty = false;
    UINT_PTR userData = 0;
    COLORREF tabColor = (COLORREF)(0xfeffffff); // kColorUnset; use default tab color

    TabInfo() = default;
    ~TabInfo();
};

struct TabsCtrl : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        bool withToolTips = false;
        int ctrlID = 0;
        int tabDefaultDx = 300;
        bool isRtl = false;
    };

    struct MouseState {
        int tabIdx = -1;
        bool overClose = false;
        // if true, mouse is over right half of the tab rectangle
        // used to make drag&drop determine a better position for drop
        bool inRightHalf = false;
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

    int ctrlID = 0;
    bool withToolTips = false;
    bool inTitleBar = false;
    bool draggingTab = false;
    // dx of tab if there's more space available
    int tabDefaultDx = 300;

    Vec<TabInfo*> tabs;
    // the bar: one TabWnd per TabInfo, laid out by `bar` (which is `layout`)
    HBox* bar = nullptr;
    // in tab order (the box's children are reversed when the UI is RTL)
    Vec<TabWnd*> tabWnds;
    struct Tooltip* tooltip = nullptr;
    int selectedIdx = -1;

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

    Size GetIdealSize() override;

    int InsertTab(int idx, TabInfo*, bool update = true);
    TabInfo* GetTab(int idx);
    void SwapTabs(int idx1, int idx2);

    void SetTextAndTooltip(int idx, Str text, Str tooltip);
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

    HWND GetToolTipsHwnd();

    void LayoutTabs();
    void ScheduleRepaint();
    TabsCtrl::MouseState TabStateFromMousePosition(const Point& p);
    HBITMAP RenderForDragging(int idx);

    TabWnd* TabWndAt(int idx);
    void RebuildTabWnds();
    void UpdateHover(int tabUnderMouse);
    void OnTabMouseDown(TabWnd*, VirtMouseEvent&);
    void CloseTab(int idx);
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

template <typename T>
void DeleteWnd(T** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog);

HWND GetCurrentModelessDialog();
void SetCurrentModelessDialog(HWND);
