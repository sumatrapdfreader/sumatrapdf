typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> MsgFilter;

struct TreeCtrl {
    // creation parameters. must be set before CreateEditCtrl() call
    HWND parent;
    RECT initialPos;
    DWORD dwStyle;
    DWORD dwExStyle;

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages

    // private
    HWND hwnd;
};

/* Creation sequence:
- AllocTreeCtrl()
- set creation parameters
- CreateTreeCtrl()
*/

TreeCtrl* AllocTreeCtrl(HWND parent, RECT* initialPosition);
bool CreateTreeCtrl(TreeCtrl*);

void DeleteTreeCtrl(TreeCtrl*);

void SetFont(TreeCtrl*, HFONT);
