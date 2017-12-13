struct EditCtrl;

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> MsgFilter;
typedef std::function<void(EditCtrl*)> EditCtrlCb;

// pass to SetColor() function to indicate this color should not change
#define NO_CHANGE (COLORREF)(-2) // -1 is taken by NO_COLOR in windows headers

struct EditCtrl {
    // creation parameters. must be set before CreateEditCtrl() call
    HWND parent = 0;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages
    EditCtrlCb onTextChanged;

    // set those via SetColors() to keep bgBrush in sync with bgCol
    HBRUSH bgBrush = nullptr;
    COLORREF bgCol = 0;
    COLORREF txtCol = 0;

    // private
    HWND hwnd = nullptr;
    int ncDx = 0;
    int ncDy = 0;
    bool hasBorder = false;
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
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
void SetPos(EditCtrl*, RECT*);
