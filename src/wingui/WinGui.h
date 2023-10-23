/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

//--- Wnd

// global messages for wingui start at WM_APP + 0x300 to not
// collide with values defined for the app
const DWORD UWM_DELAYED_CTRL_BACK = WM_APP + 0x300 + 1;

UINT_PTR NextSubclassId();

TempStr WinMsgNameTemp(UINT);

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
enum WindowBorderStyle { kWindowBorderNone, kWindowBorderClient, kWindowBorderStatic };

struct Wnd;

struct ContextMenuEvent {
    Wnd* w = nullptr;

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseScreen{};
};

using ContextMenuHandler = std::function<void(ContextMenuEvent*)>;

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
    COLORREF bgColor = ColorUnset;
};

struct Wnd : public ILayout {
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
    virtual void OnClose();
    virtual int OnCreate(CREATESTRUCT*);
    virtual void OnDestroy();
    virtual void OnContextMenu(Point pt);
    virtual void OnDropFiles(HDROP drop_info);
    virtual void OnGetMinMaxInfo(MINMAXINFO* mmi);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnMove(POINTS* pts);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual bool OnEraseBkgnd(HDC dc);
    virtual void OnSize(UINT msg, UINT type, SIZE size);
    virtual void OnTaskbarCallback(UINT msg, LPARAM lparam);
    virtual void OnTimer(UINT_PTR event_id);
    virtual void OnWindowPosChanging(WINDOWPOS* window_pos);

    void Close();
    void SetPos(RECT* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(const WCHAR*);
    void SetText(const char*);
    TempStr GetTextTemp();

    HFONT GetFont();
    void SetFont(HFONT font);

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;
    void SetFocus() const;
    bool IsFocused() const;
    void SetRtl(bool) const;
    void SetBackgroundColor(COLORREF);

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    Kind kind = nullptr;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    COLORREF backgroundColor = ColorUnset;
    HBRUSH backgroundColorBrush = nullptr;

    ILayout* layout = nullptr;

    ContextMenuHandler onContextMenu;
};

bool PreTranslateMessage(MSG& msg);

//--- Static

using ClickedHandler = std::function<void()>;

struct StaticCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    const char* text = nullptr;
};

struct Static : Wnd {
    Static();

    ClickedHandler onClicked = nullptr;

    HWND Create(const StaticCreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

//--- Button

struct ButtonCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    const char* text = nullptr;
};

struct Button : Wnd {
    ClickedHandler onClicked = nullptr;
    bool isDefault = false;

    Button();

    HWND Create(const ButtonCreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

Button* CreateButton(HWND parent, const WCHAR* s, const ClickedHandler& onClicked);
Button* CreateDefaultButton(HWND parent, const WCHAR* s);
Button* CreateDefaultButton(HWND parent, const char* s);

//--- Tooltip

struct TooltipCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
};

// a tooltip manages multiple areas withing HWND
struct Tooltip : Wnd {
    Tooltip();
    HWND Create(const TooltipCreateArgs&);
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
using TextChangedHandler = std::function<void()>;

struct EditCreateArgs {
    HWND parent = nullptr;
    bool isMultiLine = false;
    bool withBorder = false;
    const char* cueText = nullptr;
    int idealSizeLines = 1;
    HFONT font = nullptr;
};

struct Edit : Wnd {
    TextChangedHandler onTextChanged = nullptr;

    // set before Create()
    int idealSizeLines = 1;
    int maxDx = 0;

    Edit();
    ~Edit() override;

    HWND Create(const EditCreateArgs&);
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
using ListBoxSelectionChangedHandler = std::function<void()>;
using ListBoxDoubleClickHandler = std::function<void()>;

struct ListBoxCreateArgs {
    HWND parent = nullptr;
    int idealSizeLines = 0;
    HFONT font = nullptr;
};

struct ListBox : Wnd {
    ListBoxModel* model = nullptr;
    ListBoxSelectionChangedHandler onSelectionChanged = nullptr;
    ListBoxDoubleClickHandler onDoubleClick = nullptr;

    Size idealSize = {};
    int idealSizeLines = 0;

    ListBox();
    virtual ~ListBox();

    HWND Create(const ListBoxCreateArgs&);

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

enum class CheckState {
    Unchecked = BST_UNCHECKED,
    Checked = BST_CHECKED,
    Indeterminate = BST_INDETERMINATE,
};

using CheckboxStateChangedHandler = std::function<void()>;

struct CheckboxCreateArgs {
    HWND parent = nullptr;
    const char* text = nullptr;
    CheckState initialState = CheckState::Unchecked;
};

struct Checkbox : Wnd {
    CheckboxStateChangedHandler onCheckStateChanged = nullptr;

    Checkbox();

    HWND Create(const CheckboxCreateArgs&);

    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetCheckState(CheckState);
    CheckState GetCheckState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

//--- Progress

struct ProgressCreateArgs {
    HWND parent = nullptr;
    int initialMax = 0;
};

struct Progress : Wnd {
    Progress();

    int idealDx = 0;
    int idealDy = 0;

    HWND Create(const ProgressCreateArgs&);

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    Size GetIdealSize() override;
};

//--- DropDown

using DropDownSelectionChangedHandler = std::function<void()>;

struct DropDownCreateArgs {
    HWND parent = nullptr;
    // TODO: model or items
};

struct DropDown : Wnd {
    // TODO: use DropDownModel
    StrVec items;
    DropDownSelectionChangedHandler onSelectionChanged = nullptr;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDownCreateArgs&);

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

struct TrackbarPosChangingEvent {
    Trackbar* trackbar = nullptr;
    int pos = -1;
    NMTRBTHUMBPOSCHANGING* info = nullptr;
};

using TrackbarPoschangingHandler = std::function<void(TrackbarPosChangingEvent*)>;

struct TrackbarCreateArgs {
    HWND parent = nullptr;
    bool isHorizontal = true;
    int rangeMin = 1;
    int rangeMax = 5;
};

struct Trackbar : Wnd {
    Size idealSize{};

    // for WM_NOTIFY with TRBN_THUMBPOSCHANGING
    TrackbarPoschangingHandler onPosChanging = nullptr;

    Trackbar();
    ~Trackbar() override = default;

    HWND Create(const TrackbarCreateArgs&);

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

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and setting resizeAllowed to false if it's not allowed to go there
struct SplitterMoveEvent {
    Splitter* w = nullptr;
    bool done = false; // TODO: rename to finishedDragging
    // user can set to false to forbid resizing here
    bool resizeAllowed = true;
};

using SplitterMoveHandler = std::function<void(SplitterMoveEvent*)>;

struct SplitterCreateArgs {
    HWND parent = nullptr;
    SplitterType type = SplitterType::Horiz;
    bool isLive = true;
    COLORREF backgroundColor = ColorUnset;
};

struct Splitter : public Wnd {
    SplitterType type = SplitterType::Horiz;
    bool isLive = true;
    SplitterMoveHandler onSplitterMove = nullptr;

    HBITMAP bmp = nullptr;
    HBRUSH brush = nullptr;

    Point prevResizeLinePos{};
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool parentClipsChildren = false;

    Splitter();
    ~Splitter() override;

    HWND Create(const SplitterCreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
};

//--- Webview2

char* GetWebView2VersionTemp();

// TODO: maybe hide those inside a private struct
typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;

using WebViewMsgCb = std::function<void(const char*)>;
// using dispatch_fn_t = std::function<void()>;

struct Webview2Wnd : Wnd {
    Webview2Wnd();
    ~Webview2Wnd() override;

    HWND Create(const CreateCustomArgs&);

    void Eval(const char* js);
    void SetHtml(const char* html);
    void Init(const char* js);
    void Navigate(const char* url);
    bool Embed(WebViewMsgCb cb);

    virtual void OnBrowserMessage(const char* msg);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void UpdateWebviewSize();

    // this is where the webview2 control stores data
    // must be set before we call create
    // TODO: make Webview2CreateCustomArgs
    // with dataDir
    char* dataDir = nullptr;
    // DWORD m_main_thread = GetCurrentThreadId();
    ICoreWebView2* webview = nullptr;
    ICoreWebView2Controller* controller = nullptr;
};

//--- TreeView

struct TreeView;

struct TreeViewCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    DWORD exStyle = 0; // additional flags, will be OR with the rest
    bool fullRowSelect = false;
};

struct TreeItemGetTooltipEvent {
    TreeView* treeView = nullptr;
    TreeItem treeItem = 0;
    NMTVGETINFOTIPW* info = nullptr;
};

using TreeItemGetTooltipHandler = std::function<void(TreeItemGetTooltipEvent*)>;

struct TreeItemCustomDrawEvent {
    TreeView* treeView = nullptr;
    TreeItem treeItem = 0;
    NMTVCUSTOMDRAW* nm = nullptr;
};

using TreeItemCustomDrawHandler = std::function<LRESULT(TreeItemCustomDrawEvent*)>;

struct TreeSelectionChangedEvent {
    TreeView* treeView = nullptr;
    TreeItem prevSelectedItem = 0;
    TreeItem selectedItem = 0;
    NMTREEVIEW* nmtv = nullptr;
    bool byKeyboard = false;
    bool byMouse = false;
};

using TreeSelectionChangedHandler = std::function<void(TreeSelectionChangedEvent*)>;

struct TreeClickEvent {
    TreeView* treeView = nullptr;
    TreeItem treeItem = 0;
    bool isDblClick = false;

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseScreen{};
};

using TreeClickHandler = std::function<LRESULT(TreeClickEvent*)>;

struct TreeKeyDownEvent {
    TreeView* treeView = nullptr;
    NMTVKEYDOWN* nmkd = nullptr;
    int keyCode = 0;
    u32 flags = 0;
};

using TreeKeyDownHandler = std::function<void(TreeKeyDownEvent*)>;

struct TreeView : Wnd {
    TreeView();
    ~TreeView() override;

    HWND Create(const TreeViewCreateArgs&);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotifyReflect(WPARAM, LPARAM) override;

    Size GetIdealSize() override;
    void SetToolTipsDelayTime(int type, int timeInMs);
    HWND GetToolTipsHwnd();

    bool IsExpanded(TreeItem ti);
    bool GetItemRect(TreeItem ti, bool justText, RECT& r);
    TreeItem GetSelection();
    bool SelectItem(TreeItem ti);
    void SetBackgroundColor(COLORREF bgCol);
    void SetTextColor(COLORREF col);
    void ExpandAll();
    void CollapseAll();
    void Clear();

    HTREEITEM GetHandleByTreeItem(TreeItem item);
    char* GetDefaultTooltipTemp(TreeItem ti);
    TreeItem GetItemAt(int x, int y);
    TreeItem GetTreeItemByHandle(HTREEITEM item);
    bool UpdateItem(TreeItem ti);
    void SetTreeModel(TreeModel* tm);
    void SetCheckState(TreeItem item, bool enable);
    bool GetCheckState(TreeItem item);
    TreeItemState GetItemState(TreeItem ti);

    bool fullRowSelect = false;
    Size idealSize{};

    TreeModel* treeModel = nullptr; // not owned by us

    // for WM_NOTIFY with TVN_GETINFOTIP
    TreeItemGetTooltipHandler onGetTooltip = nullptr;

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    TreeItemCustomDrawHandler onTreeItemCustomDraw = nullptr;

    // for WM_NOTIFY with TVN_SELCHANGED
    TreeSelectionChangedHandler onTreeSelectionChanged = nullptr;

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    TreeClickHandler onTreeClick = nullptr;

    // for WM_NOITFY with TVN_KEYDOWN
    TreeKeyDownHandler onTreeKeyDown = nullptr;

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

struct TabMouseState {
    int tabIdx = -1;
    bool overClose = false;
    TabInfo* tabInfo = nullptr;
};

struct TabClosedEvent {
    TabsCtrl* tabs = nullptr;
    int tabIdx = 0;
};

using TabClosedHandler = std::function<void(TabClosedEvent*)>;

struct TabsSelectionChangingEvent {
    TabsCtrl* tabs = nullptr;
    int tabIdx;
};

// return true to prevent changing tabs
using TabsSelectionChangingHandler = std::function<bool(TabsSelectionChangingEvent*)>;

struct TabsSelectionChangedEvent {
    TabsCtrl* tabs = nullptr;
    int tabIdx;
};

using TabsSelectionChangedHandler = std::function<void(TabsSelectionChangedEvent*)>;

struct TabMigrationEvent {
    TabsCtrl* tabs = nullptr;
    int tabIdx;
    Point releasePoint;
};

using TabMigrationHandler = std::function<void(TabMigrationEvent*)>;

struct TabDraggedEvent {
    TabsCtrl* tabs = nullptr;
    int tab1 = -1;
    int tab2 = -1;
};

using TabDraggedHandler = std::function<void(TabDraggedEvent*)>;

struct TabsCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    bool withToolTips = false;
    int ctrlID = 0;
    int tabDefaultDx = 300;
};

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

    TabClosedHandler onTabClosed = nullptr;
    TabsSelectionChangingHandler onSelectionChanging = nullptr;
    TabsSelectionChangedHandler onSelectionChanged = nullptr;
    TabMigrationHandler onTabMigration = nullptr;
    TabDraggedHandler onTabDragged = nullptr;

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

    HWND Create(TabsCreateArgs&);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotifyReflect(WPARAM, LPARAM) override;

    Size GetIdealSize() override;

    int InsertTab(int idx, TabInfo*);
    TabInfo* GetTab(int idx);

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

    HWND GetToolTipsHwnd();

    void Layout();
    void ScheduleRepaint();
    TabMouseState TabStateFromMousePosition(const Point& p);
    void Paint(HDC hdc, RECT& rc);
    HBITMAP RenderForDragging(int idx);
};

template <typename T>
T GetTabsUserData(TabsCtrl* tabs, int idx) {
    TabInfo* tabInfo = tabs->GetTab(idx);
    return (T)tabInfo->userData;
}

void DeleteWnd(Static**);
void DeleteWnd(Button**);
void DeleteWnd(Edit**);
void DeleteWnd(Checkbox**);
void DeleteWnd(Progress**);

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
