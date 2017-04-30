
struct Win32Window;

typedef std::function<LRESULT(Win32Window* w, UINT msg, WPARAM wp, LPARAM lp, bool& discardMsg)> Win32MsgFilter;
typedef std::function<LRESULT(Win32Window* w, int id, int event, LPARAM lp, bool& discardMsg)> WmCommandCb;
typedef std::function<void(Win32Window* w, int dx, int dy, WPARAM resizeType)> OnSizeCb;

struct Win32Window {
    // creation parameters. must be set before CreateWin32Window() call
    HWND parent;
    RECT initialPos;
    DWORD dwStyle;
    DWORD dwExStyle;

    // those tweak WNDCLASSEX for RegisterClass() class
    HICON hIcon;
    HICON hIconSm;
    LPCWSTR lpszMenuName;

    // can be set any time
    Win32MsgFilter preFilter; // called at start of windows proc to allow intercepting commands
    WmCommandCb onCommand;
    OnSizeCb onSize;

    // public
    HWND hwnd;

    // arbitrary data associated with this window
    void* user;
};

Win32Window* AllocWin32Window(HWND parent, RECT* initialPosition);
void InitWin32Window(Win32Window*, HWND parent, RECT* initialPosition);
bool CreateWin32Window(Win32Window*, const WCHAR* title);
void DeleteWin32Window(Win32Window*);
