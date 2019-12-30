
/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItmGetTooltipArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVGETINFOTIPW* info = nullptr;
};

typedef std::function<void(TreeItmGetTooltipArgs*)> OnTreeItemGetTooltip;

struct TreeNotifyArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    NMTREEVIEWW* treeView = nullptr;
};

typedef std::function<void(TreeNotifyArgs*)> OnTreeNotify;

struct TreeContextMenuArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;

    // mouse x,y position relative to the window
    PointI mouseWindow{};
    // global (screen) mouse x,y position
    PointI mouseGlobal{};
};

typedef std::function<void(TreeContextMenuArgs*)> OnTreeContextMenu;

struct TreeSelectionChangedArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
};

typedef std::function<void(TreeSelectionChangedArgs*)> OnTreeSelectionChanged;

struct TreeItemExpandedArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
    bool isExpanded = false;
};

typedef std::function<void(TreeItemExpandedArgs*)> OnTreeItemExpanded;

struct TreeItemState {
    bool isSelected = false;
    bool isExpanded = false;
    bool isChecked = false;
    int nChildren = 0;
};

struct TreeItemChangedArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVITEMCHANGE* nmic = nullptr;

    // except for nChildren
    TreeItemState prevState{};
    TreeItemState newState{};
};

typedef std::function<void(TreeItemChangedArgs*)> OnTreeItemChanged;

struct TreeItemCustomDrawArgs {
    WndProcArgs* procArgs = nullptr;
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVCUSTOMDRAW* nm = nullptr;
};

typedef std::function<void(TreeItemCustomDrawArgs*)> OnTreeItemCustomDraw;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

struct TreeCtrl : public WindowBase {
    TreeCtrl(HWND parent);
    ~TreeCtrl();

    void Clear();

    void SetTreeModel(TreeModel*);

    str::WStr GetDefaultTooltip(TreeItem*);
    TreeItem* GetSelection();

    TreeItem* HitTest(int x, int y);

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

    void WndProc(WndProcArgs*) override;
    void WndProcParent(WndProcArgs*) override;

    // creation parameters. must be set before CreateTreeCtrl() call
    bool withCheckboxes = false;
    // treeModel not owned by us
    TreeModel* treeModel = nullptr;

    // allows the caller to set info tip by updating NMTVGETINFOTIP
    OnTreeItemGetTooltip onGetTooltip = nullptr;

    // called to process all WM_NOTIFY messages
    OnTreeNotify onTreeNotify = nullptr;

    // called to process WM_CONTEXTMENU
    OnTreeContextMenu onContextMenu = nullptr;

    OnTreeSelectionChanged onTreeSelectionChanged = nullptr;

    OnTreeItemExpanded onTreeItemExpanded = nullptr;

    OnTreeItemChanged onTreeItemChanged = nullptr;

    OnTreeItemCustomDraw onTreeItemCustomDraw = nullptr;

    // private
    TVITEMW item = {0};

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    Vec<std::tuple<TreeItem*, HTREEITEM>> insertedItems;
};

ILayout* NewTreeLayout(TreeCtrl*);

bool IsTree(Kind);
bool IsTree(ILayout*);
