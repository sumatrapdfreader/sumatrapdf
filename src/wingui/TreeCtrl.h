/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void(NMTVGETINFOTIPW*)> GetTooltipCb;
typedef std::function<LRESULT(NMTREEVIEWW*, bool& didHandle)> TreeNotifyCb;

// function called for every item in the tree.
// returning false stops iteration
typedef std::function<bool(TVITEM*)> TreeItemVisitor;

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

class TreeCtrl {
  public:
    TreeCtrl(HWND parent, RECT* initialPosition);
    ~TreeCtrl();

    void Clear();
    TVITEMW* GetItem(HTREEITEM);
    TVITEMW* GetItem(TreeItem*);
    str::WStr GetTooltip(HTREEITEM);
    HTREEITEM GetRoot();
    HTREEITEM GetChild(HTREEITEM);
    HTREEITEM GetSiblingNext(HTREEITEM); // GetNextSibling is windows macro
    HTREEITEM GetSelection();
    bool SelectItem(HTREEITEM);
    HTREEITEM InsertItem(TVINSERTSTRUCTW*);

    void VisitNodes(const TreeItemVisitor& visitor);
    // TODO: create 2 functions for 2 different fItemRect values
    bool GetItemRect(HTREEITEM, bool fItemRect, RECT& r);
    bool IsExpanded(HTREEITEM);

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

    // creation parameters. must be set before CreateTreeCtrl() call
    HWND parent = nullptr;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                    TVS_SHOWSELALWAYS | TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;
    DWORD dwExStyle = 0;
    HMENU menu = nullptr;
    COLORREF backgroundColor = 0;
    COLORREF textColor = 0;
    bool withCheckboxes = false;
    // treeModel not owned by us
    TreeModel* treeModel = nullptr;

    // this data can be set directly
    MsgFilter preFilter = nullptr; // called at start of windows proc to allow intercepting messages

    // when set, allows the caller to set info tip by updating NMTVGETINFOTIP
    GetTooltipCb onGetTooltip = nullptr;

    // if set, called to process all WM_NOTIFY messages
    TreeNotifyCb onTreeNotify = nullptr;

    // if set, called to process WM_CONTEXTMENU
    ContextMenuCb onContextMenu = nullptr;

    // private
    HWND hwnd = nullptr;
    TVITEMW item = {0};
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;

    // TreeItem* -> HTREEITEM mapping so that we can
    // find HTREEITEM from TreeItem*
    std::vector<std::tuple<TreeItem*, HTREEITEM>> insertedItems;
};

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree);
