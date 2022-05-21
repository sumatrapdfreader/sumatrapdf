/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItemGetTooltipEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem treeItem = 0;
    NMTVGETINFOTIPW* info = nullptr;
};

using TreeItemGetTooltipHandler = std::function<void(TreeItemGetTooltipEvent*)>;

struct TreeSelectionChangedEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem prevSelectedItem = 0;
    TreeItem selectedItem = 0;
    NMTREEVIEW* nmtv = nullptr;
    bool byKeyboard = false;
    bool byMouse = false;
};

using TreeSelectionChangedHandler = std::function<void(TreeSelectionChangedEvent*)>;

struct TreeItemCustomDrawEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem treeItem = 0;
    NMTVCUSTOMDRAW* nm = nullptr;
};

using TreeItemCustomDrawHandler = std::function<void(TreeItemCustomDrawEvent*)>;

struct TreeClickEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem treeItem = 0;
    bool isDblClick = false;

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseGlobal{};
};

using TreeClickHandler = std::function<void(TreeClickEvent*)>;

struct TreeKeyDownEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    NMTVKEYDOWN* nmkd = nullptr;
    int keyCode = 0;
    u32 flags = 0;
};

using TreeKeyDownHandler = std::function<void(TreeKeyDownEvent*)>;

struct TreeCtrl : WindowBase {
    // creation parameters. must be set before Create() call

    // sets TVS_FULLROWSELECT style
    // https://docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles
    bool fullRowSelect = false;

    // treeModel not owned by us
    TreeModel* treeModel = nullptr;

    // for WM_NOTIFY with TVN_GETINFOTIP
    TreeItemGetTooltipHandler onGetTooltip = nullptr;

    // for WM_NOTIFY with TVN_SELCHANGED
    TreeSelectionChangedHandler onTreeSelectionChanged = nullptr;

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    TreeItemCustomDrawHandler onTreeItemCustomDraw = nullptr;

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    TreeClickHandler onTreeClick = nullptr;

    // for WM_NOITFY with TVN_KEYDOWN
    TreeKeyDownHandler onTreeKeyDown = nullptr;

    Size idealSize{};

    // private
    TVITEMW item{};

    TreeCtrl();
    ~TreeCtrl() override;

    Size GetIdealSize() override;
    void WndProc(WndEvent*) override;

    void Clear();

    void SetTreeModel(TreeModel*);

    str::WStr GetDefaultTooltip(TreeItem);
    TreeItem GetSelection();

    bool UpdateItem(TreeItem);
    bool SelectItem(TreeItem);
    bool GetItemRect(TreeItem, bool justText, RECT& r);
    TreeItem GetItemAt(int x, int y);
    bool IsExpanded(TreeItem);

    bool Create(HWND parent) override;

    void SetBackgroundColor(COLORREF);
    void SetTextColor(COLORREF);

    void ExpandAll();
    void CollapseAll();

    HTREEITEM GetHandleByTreeItem(TreeItem);
    TreeItem GetTreeItemByHandle(HTREEITEM);

    void SetCheckState(TreeItem, bool);
    bool GetCheckState(TreeItem);

    TreeItemState GetItemState(TreeItem);

    HWND GetToolTipsHwnd();
    void SetToolTipsDelayTime(int type, int timeInMs);
};

void FillTVITEM(TVITEMEXW* tvitem, TreeModel*, TreeItem ti);
TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt);

bool IsTreeKind(Kind);
