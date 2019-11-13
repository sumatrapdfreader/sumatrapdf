/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class TreeCtrl2;

typedef std::function<void(TreeCtrl2*, NMTVGETINFOTIPW*)> OnGetInfoTip2;
typedef std::function<LRESULT(TreeCtrl2*, NMTREEVIEWW*, bool&)> OnTree2Notify;

/* Creation sequence:
- auto ctrl = new TreeCtrl2()
- set creation parameters
- ctrl->Create()
*/

class TreeCtrl2 {
  public:
    TreeCtrl2(HWND parent, RECT* initialPosition);
    ~TreeCtrl2();

    void Clear();

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
    OnGetInfoTip2 onGetInfoTip;
    OnTree2Notify onTreeNotify;

    // private
    HWND hwnd = nullptr;
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
};

