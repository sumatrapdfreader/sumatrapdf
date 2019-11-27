/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindWindow;

struct Window {
    Kind kind;

    // creation parameters. must be set before Create() call
    HWND parent = nullptr;
    RECT initialPos = {};
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;

    // those tweak WNDCLASSEX for RegisterClass() class
    HICON hIcon = nullptr;
    HICON hIconSm = nullptr;
    LPCWSTR lpszMenuName = nullptr;

    // can be set any time
    MsgFilter preFilter = nullptr; // called at start of windows proc to allow intercepting commands
    WmCommandCb onCommand = nullptr;
    SizeCb onSize = nullptr;

    // public
    HWND hwnd = nullptr;

    explicit Window(HWND parent, RECT* initialPosition);
    virtual ~Window();

    bool Create(const WCHAR* title);
};

extern Kind kindWindowBase;

struct WindowBase {
    Kind kind;

    // data that can be set before calling Create()

    // either a custom class that we registered or
    // a win32 control class. Assumed static so not freed
    const WCHAR* winClass = nullptr;

    HWND parent = nullptr;
    RECT initialPos = {};
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;
    HFONT hfont = nullptr; // TODO: this should be abstract Font description
    int menuId = 0;

    COLORREF textColor = ColorUnset;
    COLORREF backgroundColor = ColorUnset;

    str::Str text;

    HWND hwnd = nullptr;

    WindowBase(HWND p);
    virtual ~WindowBase();
    virtual bool Create();
    virtual SIZE GetIdealSize() = 0;

    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    virtual LRESULT WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Subclass();
    void SubclassParent();

    void SetFont(HFONT f);
    void SetText(std::string_view);
    void SetPos(RECT* r);
    void SetBounds(const RECT& r);
    void SetTextColor(COLORREF);
    void SetBackgroundColor(COLORREF);
};

void HwndSetText(HWND hwnd, std::string_view s);
