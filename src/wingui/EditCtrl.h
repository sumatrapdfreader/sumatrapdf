class EditCtrl;

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> MsgFilter;
typedef std::function<void(EditCtrl*)> EditCtrlCb;

// pass to SetColor() function to indicate this color should not change
#define NO_CHANGE (COLORREF)(-2) // -1 is taken by NO_COLOR in windows headers

/* Creation sequence:
- auto ctrl = new EditCtrl()
- set creation parameters
- ctrl.Create()
*/
class EditCtrl {
  public:
    EditCtrl(HWND parent, RECT* initialPosition);
    ~EditCtrl();

    bool Create();

    void SetColors(COLORREF bgCol, COLORREF txtCol);
    void SetFont(HFONT);
    void SetText(const WCHAR*);
    bool SetCueText(const WCHAR*);
    WCHAR* GetTextW();
    char* GetText();
    SIZE GetIdealSize();
    void SetPos(RECT*);

    // creation parameters. must be set before CreateEditCtrl() call
    HWND parent = 0;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL;
    DWORD dwExStyle = 0;

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages
    EditCtrlCb onTextChanged;

    // set those via SetColors() to keep bgBrush in sync with bgCol
    HBRUSH bgBrush = nullptr;
    COLORREF bgCol = NO_COLOR;
    COLORREF txtCol = NO_COLOR;

    HWND hwnd = nullptr;

    // private by convention
    int ncDx = 0;
    int ncDy = 0;
    bool hasBorder = false;
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
};
