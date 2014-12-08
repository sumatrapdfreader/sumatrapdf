
struct EditCtrl;

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)>
MsgFilter;
typedef std::function<void(EditCtrl*)> EditCtrlCb;

// pass to SetColor() function to indicate this color should not change
#define NO_CHANGE (COLORREF)(-2) // -1 is taken by NO_COLOR in windows headers

struct EditCtrl {
    // creation parameters. must be set before CreateEditWnd() call
    HWND parent;
    RECT initialPos;
    DWORD dwStyle;
    DWORD dwExStyle;

    // private
    int ncDx;
    int ncDy;
    bool hasBorder;

    // public
    HWND hwnd;

    // this data should be set via corresponding API call
    HBRUSH bgBrush;
    COLORREF bgCol;
    COLORREF txtCol;

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to
    EditCtrlCb onTextChanged;
};

/* Creation sequence:
- AllocEditCtrl()
- set creation parameters
- CreateEditCtrl()
*/

EditCtrl* AllocEditCtrl(HWND parent, RECT* initialPosition);
bool CreateEditCtrl(EditCtrl*);

void DeleteEditCtrl(EditCtrl*);
void SetColors(EditCtrl*, COLORREF bgCol, COLORREF txtCol);
void SetFont(EditCtrl*, HFONT);
void SetText(EditCtrl*, const WCHAR*);
bool SetCueText(EditCtrl*, const WCHAR*);
WCHAR* GetTextW(EditCtrl*);
char* GetText(EditCtrl*);
SIZE GetIdealSize(EditCtrl*);
