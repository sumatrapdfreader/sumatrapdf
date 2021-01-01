
/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct WindowBase;
struct Window;

struct WndEvent {
    // args sent to WndProc
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;

    // indicate if we handled the message and the result (if handled)
    bool didHandle = false;
    LRESULT result = 0;

    // window that logically received the message
    // (we reflect messages sent to parent windows back to real window)
    WindowBase* w = nullptr;
};

void RegisterHandlerForMessage(HWND hwnd, UINT msg, void (*handler)(void* user, WndEvent*), void* user);
void UnregisterHandlerForMessage(HWND hwnd, UINT msg);
bool HandleRegisteredMessages(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res);

#define SetWndEvent(n) \
    {                  \
        n.w = w;       \
        n.hwnd = hwnd; \
        n.msg = msg;   \
        n.wp = wp;     \
        n.lp = lp;     \
    }

#define SetWndEventSimple(n) \
    {                        \
        n.hwnd = hwnd;       \
        n.msg = msg;         \
        n.wp = wp;           \
        n.lp = lp;           \
    }

struct CopyWndEvent {
    WndEvent* dst = nullptr;
    WndEvent* src = nullptr;

    CopyWndEvent() = delete;

    CopyWndEvent(CopyWndEvent&) = delete;
    CopyWndEvent(CopyWndEvent&&) = delete;
    CopyWndEvent& operator=(CopyWndEvent&) = delete;

    CopyWndEvent(WndEvent* dst, WndEvent* src);
    ~CopyWndEvent();
};

typedef std::function<void(WndEvent*)> MsgFilter;

struct SizeEvent : WndEvent {
    int dx = 0;
    int dy = 0;
};

typedef std::function<void(SizeEvent*)> SizeHandler;

struct ContextMenuEvent : WndEvent {
    // mouse x,y position relative to the window
    Point mouseWindow{};
    // global (screen) mouse x,y position
    Point mouseGlobal{};
};

typedef std::function<void(ContextMenuEvent*)> ContextMenuHandler;

struct WindowCloseEvent : WndEvent {
    bool cancel = false;
};

struct WmCommandEvent : WndEvent {
    int id = 0;
    int ev = 0;
};

typedef std::function<void(WmCommandEvent*)> WmCommandHandler;

struct WmNotifyEvent : WndEvent {
    NMTREEVIEWW* treeView = nullptr;
};

typedef std::function<void(WmNotifyEvent*)> WmNotifyHandler;

typedef std::function<void(WindowCloseEvent*)> CloseHandler;

struct WindowDestroyEvent : WndEvent {
    Window* window = nullptr;
};

typedef std::function<void(WindowDestroyEvent*)> DestroyHandler;

struct CharEvent : WndEvent {
    int keyCode = 0;
};

typedef std::function<void(CharEvent*)> CharHandler;

// TODO: extract data from LPARAM
struct KeyEvent : WndEvent {
    bool isDown = false;
    int keyVirtCode = 0;
};

typedef std::function<void(KeyEvent*)> KeyHandler;

struct MouseWheelEvent : WndEvent {
    bool isVertical = false;
    int delta = 0;
    u32 keys = 0;
    int x = 0;
    int y = 0;
};

typedef std::function<void(MouseWheelEvent*)> MouseWheelHandler;

// https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragacceptfiles
struct DropFilesEvent : WndEvent {
    HDROP hdrop = nullptr;
};

typedef std::function<void(DropFilesEvent*)> DropFilesHandler;

struct WindowBase;

struct WindowBase : public ILayout {
    Kind kind = nullptr;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility = Visibility::Visible;

    // either a custom class that we registered or
    // a win32 control class. Assumed static so not freed
    const WCHAR* winClass = nullptr;

    HWND parent = nullptr;
    Point initialPos{-1, -1};
    Size initialSize{0, 0};
    DWORD dwStyle{0};
    DWORD dwExStyle{0};
    HFONT hfont = nullptr; // TODO: this should be abstract Font description

    // those tweak WNDCLASSEX for RegisterClass() class
    HICON hIcon = nullptr;
    HICON hIconSm = nullptr;
    LPCWSTR lpszMenuName = nullptr;

    int ctrlID = 0;

    // called at start of windows proc to allow intercepting messages
    MsgFilter msgFilter;

    // allow handling WM_CONTEXTMENU. Must be set before Create()
    ContextMenuHandler onContextMenu = nullptr;
    // allow handling WM_SIZE
    SizeHandler onSize = nullptr;
    // for WM_COMMAND
    WmCommandHandler onWmCommand = nullptr;
    // for WM_NCDESTROY
    DestroyHandler onDestroy = nullptr;
    // for WM_CLOSE
    CloseHandler onClose = nullptr;
    // for WM_KEYDOWN / WM_KEYUP
    KeyHandler onKeyDownUp = nullptr;
    // for WM_CHAR
    CharHandler onChar = nullptr;
    // for WM_MOUSEWHEEL and WM_MOUSEHWHEEL
    MouseWheelHandler onMouseWheel = nullptr;
    // for WM_DROPFILES
    // when set after Create() must also call DragAcceptFiles(hwnd, TRUE);
    DropFilesHandler onDropFiles = nullptr;

    COLORREF textColor = ColorUnset;
    COLORREF backgroundColor = ColorUnset;
    HBRUSH backgroundColorBrush = nullptr;

    str::Str text;

    HWND hwnd = nullptr;
    UINT_PTR subclassId = 0;

    WindowBase() = default;
    WindowBase(HWND p);
    virtual ~WindowBase();

    virtual bool Create();
    virtual Size GetIdealSize();

    virtual void WndProc(WndEvent*);

    // ILayout
    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(const Constraints bc) override;
    void SetBounds(Rect) override;
    void SetInsetsPt(int top, int right = -1, int bottom = -1, int left = -1);

    void Destroy();
    void Subclass();
    void Unsubclass();

    void SetIsEnabled(bool);
    bool IsEnabled();

    void SetIsVisible(bool);
    bool IsVisible() const;

    void SuspendRedraw();
    void ResumeRedraw();

    void SetFocus();
    bool IsFocused();

    void SetFont(HFONT f);
    HFONT GetFont() const;

    void SetIcon(HICON);
    HICON GetIcon() const;

    void SetText(const WCHAR* s);
    void SetText(std::string_view);
    std::string_view GetText();

    void SetPos(RECT* r);
    void SetBounds(const RECT& r);
    void SetTextColor(COLORREF);
    void SetBackgroundColor(COLORREF);
    void SetColors(COLORREF bg, COLORREF txt);
    void SetRtl(bool);
};

void Handle_WM_CONTEXTMENU(WindowBase* w, WndEvent* ev);

// a top-level window. Must set winClass before
// calling Create()
struct Window : WindowBase {
    bool isDialog = false;

    Window();
    ~Window() override;

    bool Create() override;

    void SetTitle(std::string_view);

    void Close();
};

UINT_PTR NextSubclassId();
int RunMessageLoop(HACCEL accelTable, HWND hwndDialog);
void PositionCloseTo(WindowBase* w, HWND hwnd);
int GetNextCtrlID();
HWND GetCurrentModelessDialog();
void SetCurrentModelessDialog(HWND);
