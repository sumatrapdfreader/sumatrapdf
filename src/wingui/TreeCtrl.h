

/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItmGetTooltipArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVGETINFOTIPW* info = nullptr;
};

typedef std::function<void(TreeItmGetTooltipArgs*)> TreeItemGetTooltipHandler;

struct TreeNotifyArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    NMTREEVIEWW* treeView = nullptr;
};

typedef std::function<void(TreeNotifyArgs*)> TreeNotifyHandler;

struct TreeSelectionChangedArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* prevSelectedItem = nullptr;
    TreeItem* selectedItem = nullptr;
    NMTREEVIEW* nmtv = nullptr;
    bool byKeyboard = false;
    bool byMouse = false;
};

typedef std::function<void(TreeSelectionChangedArgs*)> TreeSelectionChangedHandler;

struct TreeItemExpandedArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    bool isExpanded = false;
};

typedef std::function<void(TreeItemExpandedArgs*)> TreeItemExpandedHandler;

struct TreeItemState {
    bool isSelected = false;
    bool isExpanded = false;
    bool isChecked = false;
    int nChildren = 0;
};

struct TreeItemChangedArgs : WndProcArgs {
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

typedef std::function<void(TreeItemChangedArgs*)> TreeItemChangedHandler;

struct TreeItemCustomDrawArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVCUSTOMDRAW* nm = nullptr;
};

typedef std::function<void(TreeItemCustomDrawArgs*)> TreeItemCustomDrawHandler;

struct TreeClickArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    bool isDblClick = false;

    // mouse x,y position relative to the window
    PointI mouseWindow{};
    // global (screen) mouse x,y position
    PointI mouseGlobal{};
};

typedef std::function<void(TreeClickArgs*)> TreeClickHandler;

struct TreeKeyDownArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    NMTVKEYDOWN* nmkd = nullptr;
    int keyCode = 0;
    u32 flags = 0;
};

typedef std::function<void(TreeKeyDownArgs*)> TreeKeyDownHandler;

struct TreeGetDispInfoArgs : WndProcArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVDISPINFOEXW* dispInfo = nullptr;
};

typedef std::function<void(TreeGetDispInfoArgs*)> TreeGetDispInfoHandler;

struct TreeItemDraggeddArgs {
    TreeCtrl* treeCtrl = nullptr;
    TreeItem* draggedItem = nullptr;
    TreeItem* dragTargetItem = nullptr;
};

typedef std::function<void(TreeItemDraggeddArgs*)> TreeItemDraggedHandler;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

struct TreeCtrl : public WindowBase {
    // creation parameters. must be set before CreateTreeCtrl() call
    bool withCheckboxes = false;

    // set before Create() to enable drag&drop
    bool supportDragDrop = false;

    bool isDragging = false;

    TreeItem* draggedItem = nullptr;
    TreeItem* dragTargetItem = nullptr;

    // treeModel not owned by us
    TreeModel* treeModel = nullptr;

    // for all WM_NOTIFY messages
    TreeNotifyHandler onTreeNotify = nullptr;

    // for WM_NOTIFY with TVN_GETINFOTIP
    TreeItemGetTooltipHandler onGetTooltip = nullptr;

    // for WM_NOTIFY with TVN_SELCHANGED
    TreeSelectionChangedHandler onTreeSelectionChanged = nullptr;

    // for WM_NOTIFY with TVN_ITEMEXPANDED
    TreeItemExpandedHandler onTreeItemExpanded = nullptr;

    // for WM_NOTIFY with TVN_ITEMCHANGED
    TreeItemChangedHandler onTreeItemChanged = nullptr;

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    TreeItemCustomDrawHandler onTreeItemCustomDraw = nullptr;

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    TreeClickHandler onTreeClick = nullptr;

    // for WM_NOITFY with TVN_KEYDOWN
    TreeKeyDownHandler onTreeKeyDown = nullptr;

    // for WM_NOTIFY with TVN_GETDISPINFO
    TreeGetDispInfoHandler onTreeGetDispInfo = nullptr;

    // for TVN_BEGINDRAG / WM_MOUSEMOVE / WM_
    TreeItemDraggedHandler onTreeItemDragged = nullptr;

    Size idealSize{};

    // private
    TVITEMW item = {0};

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    Vec<std::tuple<TreeItem*, HTREEITEM>> insertedItems;

    TreeCtrl(HWND parent);
    ~TreeCtrl();

    SIZE GetIdealSize() override;
    void WndProc(WndProcArgs*) override;
    void WndProcParent(WndProcArgs*) override;

    void Clear();

    void SetTreeModel(TreeModel*);

    str::WStr GetDefaultTooltip(TreeItem*);
    TreeItem* GetSelection();

    TreeItem* HitTest(int x, int y);

    bool UpdateItem(TreeItem*);

    bool SelectItem(TreeItem*);

    bool GetItemRect(TreeItem*, bool justText, RECT& r);

    bool IsExpanded(TreeItem*);

    bool Create(const WCHAR* title);

    void SetBackgroundColor(COLORREF);
    void SetTextColor(COLORREF);

    void ExpandAll();
    void CollapseAll();

    HTREEITEM GetHandleByTreeItem(TreeItem*);
    TreeItem* GetTreeItemByHandle(HTREEITEM);

    void SetCheckState(TreeItem*, bool);
    bool GetCheckState(TreeItem*);

    TreeItemState GetItemState(TreeItem*);

    void DragBegin(NMTREEVIEWW*);
    void DragMove(int x, int y);
    void DragEnd();
};

WindowBaseLayout* NewTreeLayout(TreeCtrl*);

bool IsTree(Kind);
bool IsTree(ILayout*);

void FillTVITEM(TVITEMEXW* tvitem, TreeItem* ti, bool withCheckboxes);
