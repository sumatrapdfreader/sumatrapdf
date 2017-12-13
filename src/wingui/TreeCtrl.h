typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> MsgFilter;

// function called for every item in the tree.
// returning false stops iteration
typedef std::function<bool(TVITEM*)> TreeItemVisitor;

struct TreeCtrl {
    // creation parameters. must be set before CreateEditCtrl() call
    HWND parent = nullptr;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;
    HMENU menu = nullptr;
    COLORREF bgCol = 0;

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages

    // private
    HWND hwnd = nullptr;
    TVITEM item = {0};
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
};

/* Creation sequence:
- AllocTreeCtrl()
- set creation parameters
- CreateTreeCtrl()
*/

TreeCtrl* AllocTreeCtrl(HWND parent, RECT* initialPosition);
bool CreateTreeCtrl(TreeCtrl*, const WCHAR* title);

void ClearTreeCtrl(TreeCtrl*);
TVITEM* TreeCtrlGetItem(TreeCtrl*, HTREEITEM);
HTREEITEM TreeCtrlGetRoot(TreeCtrl*);
HTREEITEM TreeCtrlGetChild(TreeCtrl*, HTREEITEM);
HTREEITEM TreeCtrlGetNextSibling(TreeCtrl*, HTREEITEM);
bool TreeCtrlSelectItem(TreeCtrl*, HTREEITEM);
HTREEITEM TreeCtrlInsertItem(TreeCtrl*, TV_INSERTSTRUCT*);

void DeleteTreeCtrl(TreeCtrl*);

void SetFont(TreeCtrl*, HFONT);
void TreeCtrlVisitNodes(TreeCtrl* w, const TreeItemVisitor& visitor);

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree = false);
