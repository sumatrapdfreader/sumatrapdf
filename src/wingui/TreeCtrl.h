class TreeCtrl;

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> MsgFilter;
typedef std::function<void(TreeCtrl*, NMTVGETINFOTIP*)> OnGetInfoTip;
typedef std::function<LRESULT(TreeCtrl*, NMTREEVIEWW*, bool&)> OnTreeNotify;

// function called for every item in the tree.
// returning false stops iteration
typedef std::function<bool(TVITEM*)> TreeItemVisitor;

/* Creation sequence:
- auto ctrl = new TreeCtrl()
- set creation parameters
- ctrl->Create()
*/

class TreeCtrl {
  public:
    TreeCtrl(HWND parent, RECT* initialPosition);
    ~TreeCtrl();

    void Clear();
    TVITEM* GetItem(HTREEITEM);
    std::wstring GetInfoTip(HTREEITEM);
    HTREEITEM GetRoot();
    HTREEITEM GetChild(HTREEITEM);
    HTREEITEM GetSiblingNext(HTREEITEM); // GetNextSibling is windows macro
    HTREEITEM GetSelection();
    bool SelectItem(HTREEITEM);
    HTREEITEM InsertItem(TV_INSERTSTRUCT*);

    void VisitNodes(const TreeItemVisitor& visitor);
    // TODO: create 2 functions for 2 different fItemRect values
    bool GetItemRect(HTREEITEM, bool fItemRect, RECT& r);

    bool Create(const WCHAR* title);
    void SetFont(HFONT);

    // creation parameters. must be set before CreateTreeCtrl() call
    HWND parent = nullptr;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                    TVS_SHOWSELALWAYS | TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;
    DWORD dwExStyle = 0;
    HMENU menu = nullptr;
    COLORREF bgCol = 0;
    WCHAR infotipBuf[INFOTIPSIZE + 1]; // +1 just in case

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages
    // when set, allows the caller to set info tip by updating NMTVGETINFOTIP
    OnGetInfoTip onGetInfoTip;
    OnTreeNotify onTreeNotify;

    // private
    HWND hwnd = nullptr;
    TVITEM item = {0};
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
};

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree);
