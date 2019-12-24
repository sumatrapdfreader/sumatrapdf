
/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WindowBase;
struct Window;

struct WndProcArgs {
    WindowBase* w = nullptr;
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wparam = 0;
    LPARAM lparam = 0;

    bool didHandle = false;
    LRESULT result = 0;
};

#define SetWndProcArgs(n) \
    {                     \
        n.w = w;          \
        n.hwnd = hwnd;    \
        n.msg = msg;      \
        n.wparam = wp;    \
        n.lparam = lp;    \
    }

typedef std::function<void(WndProcArgs*)> MsgFilter;

struct SizeArgs {
    WindowBase* w = nullptr;
    HWND hwnd = nullptr;
    int dx = 0;
    int dy = 0;

    WPARAM wparam = 0; // resize type
    LPARAM lparam = 0;

    bool didHandle = false;
};

typedef std::function<void(SizeArgs*)> OnSize;

struct WindowCloseArgs {
    Window* window = nullptr;
    bool cancel = false;
};

typedef std::function<void(WindowCloseArgs*)> OnClose;

struct WindowDestroyedArgs {
    Window* window = nullptr;
};

typedef std::function<void(WindowDestroyedArgs*)> OnDestroyed;

extern Kind kindWindowBase;

struct WindowBase {
    Kind kind = nullptr;

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

    // called at start of windows proc to allow intercepting messages
    MsgFilter msgFilter;

    COLORREF textColor = ColorUnset;
    COLORREF backgroundColor = ColorUnset;
    HBRUSH backgroundColorBrush = nullptr;

    str::Str text;

    HWND hwnd = nullptr;
    UINT_PTR subclassId = 0;
    UINT_PTR subclassParentId = 0;

    WindowBase() = default;
    WindowBase(HWND p);
    virtual ~WindowBase();

    virtual bool Create();
    virtual SIZE GetIdealSize();

    virtual void WndProc(WndProcArgs*);
    virtual void WndProcParent(WndProcArgs*);

    void Destroy();
    void Subclass();
    void SubclassParent();
    void Unsubclass();

    void SetIsEnabled(bool);
    bool IsEnabled();

    void SetIsVisible(bool);
    bool IsVisible();

    void SuspendRedraw();
    void ResumeRedraw();

    void SetFocus();
    void SetFont(HFONT f);
    void SetText(const WCHAR* s);
    void SetText(std::string_view);
    void SetPos(RECT* r);
    void SetBounds(const RECT& r);
    void SetTextColor(COLORREF);
    void SetBackgroundColor(COLORREF);
    void SetColors(COLORREF bg, COLORREF txt);
    void SetRtl(bool);
};

extern Kind kindWindow;

// a top-level window. Must set winClass before
// calling Create()
struct Window : public WindowBase {
    // those tweak WNDCLASSEX for RegisterClass() class
    HICON hIcon = nullptr;
    HICON hIconSm = nullptr;
    LPCWSTR lpszMenuName = nullptr;

    OnWmCommand onWmCommand = nullptr;
    OnSize onSize = nullptr;
    OnClose onClose = nullptr;
    OnDestroyed onDestroyed = nullptr;

    Window();
    ~Window() override;

    bool Create() override;

    void SetTitle(std::string_view);
};

struct WindowBaseLayout : public ILayout {
    WindowBase* wb = nullptr;

    WindowBaseLayout(WindowBase*, Kind);
    ~WindowBaseLayout() override;

    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

void HwndSetText(HWND hwnd, std::string_view s);
UINT_PTR NextSubclassId();
int RunMessageLoop(HACCEL accelTable);
