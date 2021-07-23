/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItmGetTooltipEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    NMTVGETINFOTIPW* info{nullptr};
};

typedef std::function<void(TreeItmGetTooltipEvent*)> TreeItemGetTooltipHandler;

struct TreeSelectionChangedEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem prevSelectedItem{0};
    TreeItem selectedItem{0};
    NMTREEVIEW* nmtv{nullptr};
    bool byKeyboard{false};
    bool byMouse{false};
};

typedef std::function<void(TreeSelectionChangedEvent*)> TreeSelectionChangedHandler;

struct TreeItemExpandedEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    bool isExpanded{false};
};

typedef std::function<void(TreeItemExpandedEvent*)> TreeItemExpandedHandler;

struct TreeItemState {
    bool isSelected{false};
    bool isExpanded{false};
    bool isChecked{false};
    int nChildren{0};
};

struct TreeItemChangedEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    NMTVITEMCHANGE* nmic{nullptr};

    bool checkedChanged{false};
    bool expandedChanged{false};
    bool selectedChanged{false};
    // except for nChildren
    TreeItemState prevState{};
    TreeItemState newState{};
};

typedef std::function<void(TreeItemChangedEvent*)> TreeItemChangedHandler;

struct TreeItemCustomDrawEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    NMTVCUSTOMDRAW* nm{nullptr};
};

typedef std::function<void(TreeItemCustomDrawEvent*)> TreeItemCustomDrawHandler;

struct TreeClickEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    bool isDblClick{false};

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseGlobal{};
};

typedef std::function<void(TreeClickEvent*)> TreeClickHandler;

struct TreeKeyDownEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    NMTVKEYDOWN* nmkd{nullptr};
    int keyCode{0};
    u32 flags{0};
};

typedef std::function<void(TreeKeyDownEvent*)> TreeKeyDownHandler;

struct TreeGetDispInfoEvent : WndEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem treeItem{0};
    NMTVDISPINFOEXW* dispInfo{nullptr};
};

typedef std::function<void(TreeGetDispInfoEvent*)> TreeGetDispInfoHandler;

struct TreeItemDraggeddEvent {
    TreeCtrl* treeCtrl{nullptr};
    TreeItem draggedItem{0};
    TreeItem dragTargetItem{0};
    bool isStart{false};
};

typedef std::function<void(TreeItemDraggeddEvent*)> TreeItemDraggedHandler;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

struct TreeCtrl : WindowBase {
    // creation parameters. must be set before Create() call
    bool withCheckboxes{false};

    // sets TVS_FULLROWSELECT style
    // https://docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles
    bool fullRowSelect{false};

    // set before Create() to enable drag&drop
    bool supportDragDrop{false};

    // TODO: possibly not needed anymore
    bool isDragging{false};

    TreeItem draggedItem{0};
    TreeItem dragTargetItem{0};

    // treeModel not owned by us
    TreeModel* treeModel{nullptr};
    TreeItem treeModelItemNull{0}; // cached for the last treeModel

    // for all WM_NOTIFY messages
    WmNotifyHandler onNotify{nullptr};

    // for WM_NOTIFY with TVN_GETINFOTIP
    TreeItemGetTooltipHandler onGetTooltip{nullptr};

    // for WM_NOTIFY with TVN_SELCHANGED
    TreeSelectionChangedHandler onTreeSelectionChanged{nullptr};

    // for WM_NOTIFY with TVN_ITEMEXPANDED
    TreeItemExpandedHandler onTreeItemExpanded{nullptr};

    // for WM_NOTIFY with TVN_ITEMCHANGED
    TreeItemChangedHandler onTreeItemChanged{nullptr};

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    TreeItemCustomDrawHandler onTreeItemCustomDraw{nullptr};

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    TreeClickHandler onTreeClick{nullptr};

    // for WM_NOITFY with TVN_KEYDOWN
    TreeKeyDownHandler onTreeKeyDown{nullptr};

    // for WM_NOTIFY with TVN_GETDISPINFO
    TreeGetDispInfoHandler onTreeGetDispInfo{nullptr};

    // for TVN_BEGINDRAG / WM_MOUSEMOVE / WM_LBUTTONUP
    TreeItemDraggedHandler onTreeItemDragStartEnd{nullptr};

    Size idealSize{};

    // private
    TVITEMW item{};

    explicit TreeCtrl(HWND parent);
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

    bool Create() override;

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

void FillTVITEM(TVITEMEXW* tvitem, TreeModel*, TreeItem ti, bool withCheckboxes);
TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt);

bool IsTreeKind(Kind);
