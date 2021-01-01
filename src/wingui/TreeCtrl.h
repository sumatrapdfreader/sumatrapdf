/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItmGetTooltipEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVGETINFOTIPW* info = nullptr;
};

typedef std::function<void(TreeItmGetTooltipEvent*)> TreeItemGetTooltipHandler;

struct TreeSelectionChangedEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* prevSelectedItem = nullptr;
    TreeItem* selectedItem = nullptr;
    NMTREEVIEW* nmtv = nullptr;
    bool byKeyboard = false;
    bool byMouse = false;
};

typedef std::function<void(TreeSelectionChangedEvent*)> TreeSelectionChangedHandler;

struct TreeItemExpandedEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    bool isExpanded = false;
};

typedef std::function<void(TreeItemExpandedEvent*)> TreeItemExpandedHandler;

struct TreeItemState {
    bool isSelected = false;
    bool isExpanded = false;
    bool isChecked = false;
    int nChildren = 0;
};

struct TreeItemChangedEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVITEMCHANGE* nmic = nullptr;

    bool checkedChanged = false;
    bool expandedChanged = false;
    bool selectedChanged = false;
    // except for nChildren
    TreeItemState prevState{};
    TreeItemState newState{};
};

typedef std::function<void(TreeItemChangedEvent*)> TreeItemChangedHandler;

struct TreeItemCustomDrawEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVCUSTOMDRAW* nm = nullptr;
};

typedef std::function<void(TreeItemCustomDrawEvent*)> TreeItemCustomDrawHandler;

struct TreeClickEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    bool isDblClick = false;

    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseGlobal{};
};

typedef std::function<void(TreeClickEvent*)> TreeClickHandler;

struct TreeKeyDownEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    NMTVKEYDOWN* nmkd = nullptr;
    int keyCode = 0;
    u32 flags = 0;
};

typedef std::function<void(TreeKeyDownEvent*)> TreeKeyDownHandler;

struct TreeGetDispInfoEvent : WndEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVDISPINFOEXW* dispInfo = nullptr;
};

typedef std::function<void(TreeGetDispInfoEvent*)> TreeGetDispInfoHandler;

struct TreeItemDraggeddEvent {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* draggedItem = nullptr;
    TreeItem* dragTargetItem = nullptr;
    bool isStart = false;
};

typedef std::function<void(TreeItemDraggeddEvent*)> TreeItemDraggedHandler;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

struct TreeCtrl : WindowBase {
    // creation parameters. must be set before CreateTreeCtrl() call
    bool withCheckboxes{false};

    // set before Create() to enable drag&drop
    bool supportDragDrop{false};

    // TODO: possibly not needed anymore
    bool isDragging{false};

    TreeItem* draggedItem{nullptr};
    TreeItem* dragTargetItem{nullptr};

    // treeModel not owned by us
    TreeModel* treeModel{nullptr};

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

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    Vec<std::tuple<TreeItem*, HTREEITEM>> insertedItems;

    TreeCtrl(HWND parent);
    ~TreeCtrl();

    Size GetIdealSize() override;
    void WndProc(WndEvent*) override;

    void Clear();

    void SetTreeModel(TreeModel*);

    str::WStr GetDefaultTooltip(TreeItem*);
    TreeItem* GetSelection();

    bool UpdateItem(TreeItem*);
    bool SelectItem(TreeItem*);
    bool GetItemRect(TreeItem*, bool justText, RECT& r);
    TreeItem* GetItemAt(int x, int y);
    bool IsExpanded(TreeItem*);

    bool Create() override;

    void SetBackgroundColor(COLORREF);
    void SetTextColor(COLORREF);

    void ExpandAll();
    void CollapseAll();

    HTREEITEM GetHandleByTreeItem(TreeItem*);
    TreeItem* GetTreeItemByHandle(HTREEITEM);

    void SetCheckState(TreeItem*, bool);
    bool GetCheckState(TreeItem*);

    TreeItemState GetItemState(TreeItem*);

    HWND GetToolTipsHwnd();
    void SetToolTipsDelayTime(int type, int timeInMs);
};

void FillTVITEM(TVITEMEXW* tvitem, TreeItem* ti, bool withCheckboxes);
TreeItem* GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt);

bool IsTreeKind(Kind);
