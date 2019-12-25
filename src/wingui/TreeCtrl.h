
/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TreeCtrl;

struct TreeItmGetTooltipArgs {
    TreeCtrl* w = nullptr;
    TreeItem* treeItem = nullptr;
    NMTVGETINFOTIPW* info = nullptr;
};

typedef std::function<void(TreeItmGetTooltipArgs*)> OnTreeItemGetTooltip;

struct TreeNotifyArgs {
    TreeCtrl* w = nullptr;
    NMTREEVIEWW* treeView = nullptr;

    WndProcArgs* procArgs = nullptr;
};

typedef std::function<void(TreeNotifyArgs*)> OnTreeNotify;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

struct TreeItemState {
    bool isSelected = false;
    bool isExpanded = false;
    bool isChecked = false;
    int nChildren = 0;
};

struct TreeCtrl : public WindowBase {
    TreeCtrl(HWND parent);
    ~TreeCtrl();

    void Clear();
    str::WStr GetTooltip(TreeItem*);
    TreeItem* GetSelection();

    bool SelectItem(TreeItem*);

    bool GetTreeItemRect(TreeItem*, bool justText, RECT& r);

    bool IsExpanded(TreeItem*);

    bool Create(const WCHAR* title);

    void SetTreeModel(TreeModel*);

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
    ContextMenuCb onContextMenu = nullptr;

    // private
    TVITEMW item = {0};

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    Vec<std::tuple<TreeItem*, HTREEITEM>> insertedItems;
};

ILayout* NewTreeLayout(TreeCtrl*);

bool IsTree(Kind);
bool IsTree(ILayout*);
