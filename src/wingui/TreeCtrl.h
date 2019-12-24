
/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void(NMTVGETINFOTIPW*)> GetTooltipCb;
typedef std::function<LRESULT(NMTREEVIEWW*, bool& didHandle)> TreeNotifyCb;

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
    TVITEMW* GetItem(TreeItem*);
    str::WStr GetTooltip(HTREEITEM);
    TreeItem* GetSelection();

    bool SelectItem(TreeItem*);
    HTREEITEM InsertItem(TVINSERTSTRUCTW*);

    // TODO: create 2 functions for 2 different fItemRect values
    bool GetItemRect(HTREEITEM, bool fItemRect, RECT& r);

    bool IsExpanded(TreeItem*);

    bool Create(const WCHAR* title);
    void SetFont(HFONT);
    HFONT GetFont();
    void SetTreeModel(TreeModel*);
    void SetBackgroundColor(COLORREF);
    void SetTextColor(COLORREF);

    void SuspendRedraw();
    void ResumeRedraw();

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

    // this data can be set directly
    // when set, allows the caller to set info tip by updating NMTVGETINFOTIP
    GetTooltipCb onGetTooltip = nullptr;

    // if set, called to process all WM_NOTIFY messages
    TreeNotifyCb onTreeNotify = nullptr;

    // if set, called to process WM_CONTEXTMENU
    ContextMenuCb onContextMenu = nullptr;

    // private
    TVITEMW item = {0};

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    std::vector<std::tuple<TreeItem*, HTREEITEM>> insertedItems;
};

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree);
ILayout* NewTreeLayout(TreeCtrl*);

bool IsTree(Kind);
bool IsTree(ILayout*);
