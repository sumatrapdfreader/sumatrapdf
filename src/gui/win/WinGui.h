/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

//--- WindowBase

// Event handler conventions:
// - onXxx members are notifications: each is a single-assignment Func1 taking a
//   XxxEvent*. A handler that consumed the event sets `didHandle` (plus
//   `result` when an LRESULT must flow back to the window procedure);
//   unhandled events fall through to the default processing.
// - onGetXxx members are queries: the event struct carries an out-param the
//   handler fills in (e.g. VirtCtrl::onGetTooltip), and there is no didHandle.

TempStr WinMsgNameTemp(UINT);

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
enum WindowBorderStyle {
    kWindowBorderNone,
    kWindowBorderClient,
    kWindowBorderStatic
};

struct WindowBase;
struct ControlBase;
struct Edit;
struct HwndBase;
struct VirtRoot;
struct PlatformFont;

HwndBase* HwndBaseFromHwnd(HWND);
WindowBase* WindowBaseFromHwnd(HWND);
ControlBase* ControlFromHwnd(HWND);

struct ContextMenuEvent {
    ControlBase* w = nullptr;

    // mouse x,y position relative to the window
    Point mouseWindow;
    // global (screen) mouse x,y position
    Point mouseScreen;
};

using ContextMenuHandler = Func1<ContextMenuEvent*>;

struct CreateControlArgs {
    HWND parent = nullptr;
    WStr className;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos;
    HMENU ctrlId = nullptr;
    bool visible = true;
    PlatformFont* font = nullptr;
    Str text;
    bool isRtl = false;
};

struct CreateCustomArgs {
    HWND parent = nullptr;
    HWND owner = nullptr;
    WStr className;
    Str title;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos;
    // don't set both menu and cmdId
    HMENU menu = nullptr;
    int cmdId = 0; // command sent on click
    bool visible = true;
    PlatformFont* font = nullptr;
    HICON icon = nullptr;
    Color bgColor = kColorUnset;
    bool isRtl = false;
};

struct WmEvent {
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;
    uintptr_t userData = 0;
    // the WindowBase / ControlBase the message went to; consumers cast it
    void* self = nullptr;

    bool didHandle = true; // common case so set as default
};

// WM_KEYDOWN / WM_SYSKEYDOWN for WindowBase::onKeyDown (like VirtKeyEvent).
// hwnd is MSG::hwnd (often a child Edit/DropDown), not necessarily the WindowBase.
// Handlers set didHandle = true to consume the key.
struct KeyEvent {
    HWND hwnd = nullptr;
    int vkey = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool isSysKey = false; // true for WM_SYSKEYDOWN
    bool didHandle = false;
};

// The win32 plumbing WindowBase and ControlBase share: the HWND and its
// window-proc glue, subclassing, font, colors, an optional layout tree, and
// the single HWND -> object list used to route messages to both kinds
struct HwndBase {
    HWND hwnd = nullptr;
    PlatformFont* font = nullptr; // interned, not owned
    UINT_PTR subclassId = 0;

    Color bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    // the color bgBrush was created from, so BackgroundBrush(Color) can tell
    // when a theme change moved the resolved color under a cached brush
    Color bgBrushColor = kColorUnset;
    Color textColor = kColorUnset;

    HDC gfxDoubleBufferHdc = nullptr;
    HBITMAP gfxDoubleBufferBitmap = nullptr;
    HGDIOBJ gfxDoubleBufferPrevBitmap = nullptr;
    int gfxDoubleBufferDx = 0;
    int gfxDoubleBufferDy = 0;

    // the layout of our children, if we have one. It can hold HWND controls
    // (ControlBase) and virtual ones (VirtCtrl) side by side
    ILayout* layout = nullptr;
    // the virtual controls of `layout`, if it has any. Created on demand by
    // DoLayout(), owned here, but it doesn't own the controls - `layout` does
    VirtRoot* vroot = nullptr;

    uintptr_t userData = 0;

    HwndBase() = default;
    HwndBase(const HwndBase&) = delete;
    HwndBase& operator=(const HwndBase&) = delete;
    virtual ~HwndBase();

    virtual WindowBase* AsWindowBase();
    virtual ControlBase* AsControlBase();
    // full per-class message handling: the onWndProc hook, then WndProcDefault
    virtual LRESULT OnMessage(HWND, UINT, WPARAM, LPARAM) = 0;

    void Destroy();
    void Subclass();
    void UnSubclass();
    HWND Detach();

    void SetText(Str);
    TempStr GetTextTemp();
    void SetPos(Rect* r);
    void SetColors(Color textColor, Color bgColor);
    HBRUSH BackgroundBrush();
    HBRUSH BackgroundBrush(Color);

    PlatformFont* GetFont();
    HFONT GetHFont() const;
    void SetFont(PlatformFont*);

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    // lays `layout` out at the given size / the client size and refreshes the
    // virtual controls
    void DoLayout(Size);
    void DoLayout();

    // sends a control's own messages (colors, owner draw, ...) back to it
    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND CreateCustomHwnd(const CreateCustomArgs&, WStr defaultClassName);
};

// installed by the app (DarkMode_win.cpp): how WindowBase::ApplyDarkMode()
// re-applies OS dark mode to a window. gui/ doesn't name darkmodelib; null
// means there is nothing to apply
extern void (*gWindowBaseApplyDarkMode)(HWND);

// Base of the top-level windows (and the child windows that place themselves,
// like the notification toasts). Not an ILayout: a window isn't positioned by a
// parent's layout - it has a `layout` of its own children instead
struct WindowBase : HwndBase {
    struct CloseEvent {
        WmEvent* e = nullptr;
    };
    struct DestroyEvent {
        WmEvent* e = nullptr;
    };
    struct AttachEvent {
        WindowBase* w = nullptr;
    };
    struct FocusEvent {
        WindowBase* w = nullptr;
    };
    // WM_ACTIVATE: state is WA_INACTIVE / WA_ACTIVE / WA_CLICKACTIVE
    struct ActivateEvent {
        WindowBase* w = nullptr;
        UINT state = 0;
        bool minimized = false;
        HWND other = nullptr;
        bool didHandle = false;
    };
    // WM_DPICHANGED: suggested is the system-recommended window rect
    struct DpiChangedEvent {
        WindowBase* w = nullptr;
        UINT dpiX = 0;
        UINT dpiY = 0;
        RECT* suggested = nullptr;
        bool didHandle = false;
    };
    // WM_SETCURSOR: hitTest is LOWORD(lParam), mouseMsg is HIWORD(lParam)
    struct SetCursorEvent {
        WindowBase* w = nullptr;
        HWND hwndCursor = nullptr; // (HWND)wParam
        UINT hitTest = 0;
        UINT mouseMsg = 0;
        LRESULT result = FALSE;
        bool didHandle = false;
    };
    // WM_NCHITTEST: screenPos is from lParam; set result to HT* and didHandle
    struct NcHitTestEvent {
        WindowBase* w = nullptr;
        Point screenPos;
        LRESULT result = HTCLIENT;
        bool didHandle = false;
    };
    // WM_SHOWWINDOW: show is wParam; status is lParam (SW_*)
    struct ShowWindowEvent {
        WindowBase* w = nullptr;
        bool show = false;
        LPARAM status = 0;
        bool didHandle = false;
    };
    struct CommandEvent {
        WindowBase* w = nullptr;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        bool didHandle = false;
    };
    struct CreateEvent {
        WindowBase* w = nullptr;
        CREATESTRUCT* cs = nullptr;
        int result = 0;
    };
    struct DropFilesEvent {
        WindowBase* w = nullptr;
        HDROP dropInfo = nullptr;
    };
    struct GetMinMaxInfoEvent {
        WindowBase* w = nullptr;
        MINMAXINFO* mmi = nullptr;
    };
    struct MouseEvent {
        WindowBase* w = nullptr;
        UINT msg = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
        bool didHandle = false;
    };
    struct MoveEvent {
        WindowBase* w = nullptr;
        POINTS* pts = nullptr;
    };
    struct PaintEvent {
        WindowBase* w = nullptr;
        HDC hdc = nullptr;
        PAINTSTRUCT* ps = nullptr;
    };
    struct SizeEvent {
        WindowBase* w = nullptr;
        UINT msg = 0;
        UINT type = 0;
        Size size;
    };
    struct TaskbarCallbackEvent {
        WindowBase* w = nullptr;
        UINT msg = 0;
        LPARAM lparam = 0;
    };
    struct TimerEvent {
        WindowBase* w = nullptr;
        UINT_PTR timerId = 0;
    };
    struct WindowPosChangingEvent {
        WindowBase* w = nullptr;
        WINDOWPOS* windowPos = nullptr;
    };
    struct WndProcEvent {
        WindowBase* w = nullptr;
        HWND hwnd = nullptr;
        UINT msg = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
        bool didHandle = false; // true -> return result; else WndProcDefault
    };
    struct PreTranslateEvent {
        WindowBase* w = nullptr;
        MSG* msg = nullptr;
        bool didHandle = false;
    };
    struct NotifyEvent {
        WindowBase* w = nullptr;
        int controlId = 0;
        NMHDR* nmh = nullptr;
        LRESULT result = 0;
    };

    using CloseHandler = Func1<CloseEvent*>;
    using DestroyHandler = Func1<DestroyEvent*>;
    using AttachHandler = Func1<AttachEvent*>;
    using FocusHandler = Func1<FocusEvent*>;
    using ActivateHandler = Func1<ActivateEvent*>;
    using DpiChangedHandler = Func1<DpiChangedEvent*>;
    using SetCursorHandler = Func1<SetCursorEvent*>;
    using NcHitTestHandler = Func1<NcHitTestEvent*>;
    using ShowWindowHandler = Func1<ShowWindowEvent*>;
    using CommandHandler = Func1<CommandEvent*>;
    using CreateHandler = Func1<CreateEvent*>;
    using DropFilesHandler = Func1<DropFilesEvent*>;
    using GetMinMaxInfoHandler = Func1<GetMinMaxInfoEvent*>;
    using MouseHandler = Func1<MouseEvent*>;
    using MoveHandler = Func1<MoveEvent*>;
    using PaintHandler = Func1<PaintEvent*>;
    using SizeHandler = Func1<SizeEvent*>;
    using TaskbarCallbackHandler = Func1<TaskbarCallbackEvent*>;
    using TimerHandler = Func1<TimerEvent*>;
    using WindowPosChangingHandler = Func1<WindowPosChangingEvent*>;
    using WndProcHandler = Func1<WndProcEvent*>;
    using PreTranslateHandler = Func1<PreTranslateEvent*>;
    using KeyDownHandler = Func1<KeyEvent*>;
    using NotifyHandler = Func1<NotifyEvent*>;

    WindowBase();

    WindowBase* AsWindowBase() override;
    LRESULT OnMessage(HWND, UINT, WPARAM, LPARAM) override;

    // the system theme / colors changed, or the app's own theme did
    virtual void OnThemeChange();

    // kColWin* (GuiColors.h) resolved: this window's textColor / bgColor when
    // set, the gColsWin defaults otherwise
    Color GetColor(int idx) const;

    // the app pushed a new palette into GuiColors (its theme changed):
    // re-apply the gColsWin colors to this window and its native child
    // controls, re-apply dark mode, repaint. A window with more to refresh
    // (icons drawn in the text color, a font darkmode resets) overrides this
    // and calls the base
    virtual void UpdateTheme();
    // dark mode as UpdateTheme() re-applies it: gWindowBaseApplyDarkMode.
    // Overridden by the windows that need a different flavor (an erase-bg
    // subclass, title bar only)
    virtual void ApplyDarkMode();

    HWND CreateCustom(const CreateCustomArgs&);

    void SetVisibility(Visibility);
    Visibility GetVisibility();

    void Attach(HWND hwnd);

    // PreTranslateMessage: onPreTranslate, then key-down -> onKeyDown, then
    // closeOnEsc / closeOnCtrlW, then Enter (focused or default button), then
    // Tab default
    bool PreTranslateMessage(MSG& msg);

    void ScheduleDelete();

    void Close();
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;

    // dpi of the monitor this window is on
    int GetDpi() const;

    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    // Tab ring over `layout`, across HWND and virtual controls alike. A virtual
    // control holding the focus means this window holds the win32 focus and
    // `vroot->focused` says which one it is
    bool TabNavigate(bool backwards);
    bool ActivateOnEnter();
    bool MnemonicNavigate(char c);
    void SetFocusTo(ControlBase*);
    void SetFocusTo(VirtCtrl*);

    Kind kind = nullptr;

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    // reflow `layout` on WM_SIZE. On by default; set false when onSize does a
    // custom layout (FindBar sizes the HWND, annotations grow controls first,
    // SimpleBrowser / image edit split the client between a strip and a view)
    bool autoLayout = true;
    // if false, WM_ERASEBKGND returns TRUE without painting (WM_PAINT covers).
    // default false: custom windows usually double-buffer the full client
    bool shouldEraseBackground = false;
    // Close() on Esc (KEYDOWN and WM_CHAR, so it still works when an Edit has
    // focus). Off by default — specialized Esc (find bar, palette, in-place
    // editors) stays in the derived onKeyDown
    bool closeOnEsc = false;
    // Close() on Ctrl+W (no Alt). Off by default
    bool closeOnCtrlW = false;
    // Close() on F1 (no modifiers), so the key that opened the window also
    // dismisses it. Off by default
    bool closeOnF1 = false;
    // set by ScheduleDelete(); once set the window must only die via that path
    bool deleteScheduled = false;

    // runs right before a ScheduleDelete()-ed window is deleted, so the owner
    // can clear the pointer it keeps to this window
    Func0 onBeforeDelete;
    AttachHandler onAttach;
    FocusHandler onFocus;
    ActivateHandler onActivate;
    DpiChangedHandler onDpiChanged;
    SetCursorHandler onSetCursor;
    NcHitTestHandler onNcHitTest;
    ShowWindowHandler onShowWindow;
    CommandHandler onCommand;
    CreateHandler onCreate;
    DropFilesHandler onDropFiles;
    GetMinMaxInfoHandler onGetMinMaxInfo;
    MouseHandler onMouseEvent;
    MoveHandler onMove;
    PaintHandler onPaint;
    SizeHandler onSize;
    TaskbarCallbackHandler onTaskbarCallback;
    TimerHandler onTimer;
    WindowPosChangingHandler onWindowPosChanging;
    WndProcHandler onWndProc;
    PreTranslateHandler onPreTranslate;
    KeyDownHandler onKeyDown;
    NotifyHandler onNotify;
    CloseHandler onClose;
    DestroyHandler onDestroy;
};

bool PreTranslateMessage(MSG& msg);

// Base of the controls that a layout positions: the win32 controls (Static,
// Button, Edit, ...) and custom HWND hosts. Virtual-only controls (TabsCtrl,
// VirtListBox, ...) derive from VirtCtrl instead.
// It is an ILayout, and it has no window-only machinery (no WM_CLOSE handler,
// no min/max info, no drop files, no taskbar callback)
struct ControlBase : ILayout, HwndBase {
    struct DestroyEvent {
        WmEvent* e = nullptr;
    };
    struct AttachEvent {
        ControlBase* w = nullptr;
    };
    struct FocusEvent {
        ControlBase* w = nullptr;
    };
    struct CommandEvent {
        ControlBase* w = nullptr;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        bool didHandle = false;
    };
    struct CreateEvent {
        ControlBase* w = nullptr;
        CREATESTRUCT* cs = nullptr;
        int result = 0;
    };
    struct MouseEvent {
        ControlBase* w = nullptr;
        UINT msg = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
        bool didHandle = false;
    };
    struct PaintEvent {
        ControlBase* w = nullptr;
        HDC hdc = nullptr;
        PAINTSTRUCT* ps = nullptr;
    };
    struct SizeEvent {
        ControlBase* w = nullptr;
        UINT msg = 0;
        UINT type = 0;
        Size size;
    };
    struct TimerEvent {
        ControlBase* w = nullptr;
        UINT_PTR timerId = 0;
    };
    struct WndProcEvent {
        ControlBase* w = nullptr;
        HWND hwnd = nullptr;
        UINT msg = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
        bool didHandle = false;
    };
    struct NotifyEvent {
        ControlBase* w = nullptr;
        int controlId = 0;
        NMHDR* nmh = nullptr;
        LRESULT result = 0;
    };
    struct NotifyReflectEvent {
        ControlBase* w = nullptr;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
    };
    struct MessageReflectEvent {
        ControlBase* w = nullptr;
        UINT msg = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
        LRESULT result = 0;
    };

    using DestroyHandler = Func1<DestroyEvent*>;
    using AttachHandler = Func1<AttachEvent*>;
    using FocusHandler = Func1<FocusEvent*>;
    using CommandHandler = Func1<CommandEvent*>;
    using CreateHandler = Func1<CreateEvent*>;
    using MouseHandler = Func1<MouseEvent*>;
    using PaintHandler = Func1<PaintEvent*>;
    using SizeHandler = Func1<SizeEvent*>;
    using TimerHandler = Func1<TimerEvent*>;
    using WndProcHandler = Func1<WndProcEvent*>;
    using NotifyHandler = Func1<NotifyEvent*>;
    using NotifyReflectHandler = Func1<NotifyReflectEvent*>;
    using MessageReflectHandler = Func1<MessageReflectEvent*>;

    ControlBase();

    operator HWND() const { return hwnd; }

    ControlBase* AsControlBase() override;
    LRESULT OnMessage(HWND, UINT, WPARAM, LPARAM) override;

    HWND CreateControl(const CreateControlArgs&);
    HWND CreateCustom(const CreateCustomArgs&);

    // layout sizing (kept virtual: subclasses measure differently)
    virtual Size GetIdealSize();

    void SetVisibility(Visibility) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    ControlBase* AsControl() override;

    void SetInsetsPt(int uniform);
    void SetInsetsPt(int topBottom, int leftRight);
    void SetInsetsPt(int top, int right, int bottom, int left);

    void Attach(HWND hwnd);
    void AttachDlgItem(UINT id, HWND parent);

    // for reflection from parents (WindowBase / ControlBase WndProcDefault)
    bool DispatchCommand(WPARAM wparam, LPARAM lparam);
    LRESULT DispatchMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT DispatchNotifyReflect(WPARAM wparam, LPARAM lparam);

    void SetIsVisible(bool isVisible);
    bool IsVisible() const;

    virtual bool IsFocused() const;
    virtual void SetFocus();

    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    Insets insets{};
    Size childSize;

    // like HwndSlot::mapRtlX: SetBounds() x is an offset from the physical
    // left even when the parent is RTL (a control inside a tree that already
    // arranged itself right-to-left, like the toolbar's page box)
    bool mapRtlX = false;

    // if false, WM_ERASEBKGND returns TRUE without painting (WM_PAINT covers).
    // default true: native subclassed controls keep system erase
    bool shouldEraseBackground = true;

    AttachHandler onAttach;
    FocusHandler onFocus;
    CommandHandler onCommand;
    CreateHandler onCreate;
    MouseHandler onMouseEvent;
    PaintHandler onPaint;
    SizeHandler onSize;
    TimerHandler onTimer;
    WndProcHandler onWndProc;
    NotifyHandler onNotify;
    NotifyReflectHandler onNotifyReflect;
    MessageReflectHandler onMessageReflect;
    ContextMenuHandler onContextMenu;
    DestroyHandler onDestroy;
};

void SizeToIdealSize(ControlBase* c);

struct VirtRoot;
struct VirtCtrl;

//--- Button

// The only buttons left as HWND controls are the installer's and the
// uninstaller's: they have no theme to draw from, so Windows' own themed push
// button (with its UAC shield, mnemonics and dialog keyboard navigation) beats
// anything we would draw. Everything in the app itself uses VirtButton.
struct Button : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        PlatformFont* font = nullptr;
        Str text;
        bool isRtl = false;
    };

    Func0 onClick;

    bool isDefault = false;

    Button();

    HWND Create(const CreateArgs&);

    Size GetIdealSize() override;

    void OnCommand(ControlBase::CommandEvent* ev);
};

Button* CreateDefaultButton(HWND parent, Str s, bool isRtl);

//--- Tooltip

// a tooltip manages multiple areas within HWND
struct Tooltip : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        PlatformFont* font = nullptr;
        bool isRtl = false;
    };

    Tooltip();
    ~Tooltip() override;

    HWND Create(const CreateArgs&);
    Size GetIdealSize() override;

    int Add(Str s, const Rect& rc, bool multiline);
    void Update(int id, Str s, const Rect& rc, bool multiline);
    void Delete(int id = 0);

    int SetSingle(Str s, const Rect& rc, bool multiline);
    // Track-mode tip at an absolute screen position (e.g. keyboard nav), not the cursor.
    // If maxRightScreen > 0, shifts left so the bubble stays left of that x.
    int SetSingleAt(Str s, const Rect& rc, Point screenPos, bool multiline, int maxRightScreen = 0);

    int Count();

    TempStr GetTextTemp(int id = 0);

    void SetDelayTime(int type, int timeInMs);
    void SetMaxWidth(int dx);

    // window this tooltip is associated with
    HWND parent = nullptr;

    Vec<int> tooltipIds;
    Str lastText;
};

int TooltipGetCount(HWND hwnd);
void TooltipRemoveAll(HWND hwnd);

//--- Edit
using TextChangedHandler = Func0;

void PostDelayedEditSelectAll(HWND);
void PostDelayedEditCtrlBack(HWND);

// The base/Win.h edit helpers, taking the control rather than its HWND, so a
// caller that holds an Edit* can pass it straight in. A null control is the
// same no-op a null HWND is.
void EditSelectAll(Edit*);
void EditSelectText(Edit*, int start, int end);
void EditGetSelection(Edit*, int& start, int& end);
void EditSetCursorPos(Edit*, int pos);
void EditSetCursorPosAtEnd(Edit*);
int EditGetTextLen(Edit*);
void EditSetModified(Edit*, bool);
bool EditIsModified(Edit*);
void EditSetCueText(Edit*, Str);
void EditSetMargins(Edit*, int left, int right);
void EditSetNumbersOnly(Edit*, bool);
void EditSetPasswordVisible(Edit*, bool);
void EditSetFocus(Edit*);

struct Edit : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isMultiLine = false;
        bool withBorder = false;
        // 1px NC underline under the client area (no WS_EX_CLIENTEDGE)
        bool withBottomBorder = false;
        // 1px NC rectangle in the theme edge color (not WS_EX_CLIENTEDGE / Win11 accent).
        // withBorder uses this same frame and adds GetIdealSize padding.
        bool withFrame = false;
        bool numbersOnly = false;
        bool isPassword = false;
        bool alignRight = false;
        bool selectAllOnFocus = false;
        bool noTheme = false;
        Str cueText;
        Str text;
        int idealSizeLines = 1;
        // if > 0: ideal width is at least ~N average character widths
        int idealWidthChars = 0;
        // if > 0: ideal width is capped at ~N average character widths
        int maxWidthChars = 0;
        // if > 0: unscaled px of space between the text and all 4 client edges
        // (multi-line only; the edit control ignores EM_SETRECT otherwise)
        int textPadding = 0;
        int marginLeft = 0;
        int marginRight = 0;
        // center the text vertically when the control is taller than one line
        // (single-line only; no-op if the box is just one line tall)
        bool centerTextVert = true;
        PlatformFont* font = nullptr;
        bool isRtl = false;
    };

    struct CharEvent {
        int c = 0;
        bool didHandle = false;
    };
    using CharHandler = Func1<CharEvent*>;

    // EN_CHANGE only (do not use for kill-focus flush; see onKillFocus)
    TextChangedHandler onTextChanged;
    // EN_KILLFOCUS only (e.g. flush annotation Contents before blur)
    TextChangedHandler onKillFocus;
    // EN_SETFOCUS only
    TextChangedHandler onFocus;
    CharHandler onChar;

    // set before Create() (pixels); or use idealWidthChars / maxWidthChars
    int idealSizeLines = 1;
    int idealDx = 0;
    int maxDx = 0;
    int idealDy = 0;
    // DPI-scaled CreateArgs.textPadding
    int textPadding = 0;

    // remembers CreateArgs.withBorder (padded 1px frame). Not inferred from
    // window styles: we no longer use WS_EX_CLIENTEDGE (Win11 blue accent)
    bool createdWithBorder = false;
    bool createdWithBottomBorder = false;
    bool createdWithFrame = false;
    bool centerTextVert = false;
    // top strip reserved by WM_NCCALCSIZE for centerTextVert, in px
    int ncCenterTop = 0;
    bool selectAllOnFocus = false;
    bool delaySelectAll = false;
    // when set, shown instead of the edit's own I-beam (see SetCursorId)
    LPWSTR cursorId = nullptr;

    Edit();
    ~Edit() override;

    HWND Create(const CreateArgs&);
    void WndProc(ControlBase::WndProcEvent* ev);
    void OnMessageReflect(ControlBase::MessageReflectEvent* ev);
    void OnCommand(ControlBase::CommandEvent* ev);

    Size GetIdealSize() override;
    int LineDy();
    HBRUSH CtlColorBrush(HDC);

    void SetIdealWidthChars(int nChars);
    void SetMaxWidthChars(int nChars);
    void SetIdealWidthFromText(Str s, int extraPx = 0);

    int GetLeftTextMargin();

    void SetCursorId(LPWSTR);
    bool HasBorder();
    void ApplyTextPadding();
};

//--- CheckboxCtrl

struct Checkbox : ControlBase {
    enum class State {
        Unchecked = BST_UNCHECKED,
        Checked = BST_CHECKED,
        Indeterminate = BST_INDETERMINATE,
    };

    struct CreateArgs {
        HWND parent = nullptr;
        Str text;
        PlatformFont* font = nullptr;
        State initialState = State::Unchecked;
        bool isRtl = false;
        bool isRadio = false;
        bool isGroupStart = false;
    };

    using StateChangedHandler = Func0;

    StateChangedHandler onStateChanged;

    Checkbox();

    HWND Create(const CreateArgs&);

    void OnCommand(ControlBase::CommandEvent* ev);
    void OnMessageReflect(ControlBase::MessageReflectEvent* ev);

    Size GetIdealSize() override;

    void SetState(State);
    State GetState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

//--- Progress

struct Progress : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        int initialMax = 0;
        bool isRtl = false;
    };

    Progress();

    int idealDx = 0;
    int idealDy = 0;

    HWND Create(const CreateArgs&);

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    Size GetIdealSize() override;
};

//--- DropDown

struct DropDown;

// The base/Win.h combo box helpers, taking the control rather than its HWND, so
// a caller that holds a DropDown* can pass it straight in. A null control is
// the same no-op a null HWND is.
void CbSetCueBanner(DropDown*, Str);
int CbGetTextLen(DropDown*);
bool CbIsDropped(DropDown*);
int CbGetCurrentSelection(DropDown*);
void CbSetCurrentSelection(DropDown*, int);

HWND CbEditHwnd(DropDown*);
void CbEditSelectAll(DropDown*);
void CbEditSelectText(DropDown*, int start, int end);
void CbEditGetSelection(DropDown*, int& start, int& end);
void CbEditSetModified(DropDown*, bool);
bool CbEditIsModified(DropDown*);

struct DropDown : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        PlatformFont* font = nullptr;
        bool isRtl = false;
        bool isEditable = false;
        // draw a color swatch to the left of each item (annotation color lists)
        bool colorSwatches = false;
        // TODO: model or items
    };

    using SelectionChangedHandler = Func0;

    // TODO: use DropDownModel
    StrVec items;
    Vec<Color> itemColors; // parallel to items when colorSwatches
    bool colorSwatches = false;
    SelectionChangedHandler onSelectionChanged;
    TextChangedHandler onTextChanged;
    SelectionChangedHandler onCloseUp;
    // cap preferred width the same way Edit does (find bar / find window)
    int idealDx = 0;
    int maxDx = 0;
    LPWSTR cursorId = nullptr;
    // SetItems / SetText can send CBN_*; skip handlers while we rebuild the list
    bool suppressNotify = false;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDown::CreateArgs&);

    Size GetIdealSize() override;
    void OnCommand(ControlBase::CommandEvent* ev);
    void OnMessageReflect(ControlBase::MessageReflectEvent* ev);
    bool IsFocused() const override;
    void SetFocus() override;

    void SetItems(StrVec& newItems);
    void SetItemsKeepText(StrVec& newItems);
    void SetItemsSeqStrings(SeqStrings items);
    void SetCursorId(LPWSTR);
};

//--- Trackbar

struct Trackbar;

struct Trackbar : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isHorizontal = true;
        int rangeMin = 1;
        int rangeMax = 5;
        PlatformFont* font = nullptr;
        bool isRtl = false;
    };

    struct PositionChangingEvent {
        Trackbar* trackbar = nullptr;
        int pos = -1;
        NMTRBTHUMBPOSCHANGING* info = nullptr;
    };

    using PositionChangingHandler = Func1<PositionChangingEvent*>;

    Size idealSize;

    // for WM_NOTIFY with TRBN_THUMBPOSCHANGING
    PositionChangingHandler onPositionChanging;

    Trackbar();
    ~Trackbar() override = default;

    HWND Create(const CreateArgs&);

    void OnMessageReflect(ControlBase::MessageReflectEvent* ev);

    Size GetIdealSize() override;
    void SetRange(int min, int max);
    int GetRangeMin();
    int getRangeMax();

    void SetValue(int);
    int GetValue();
};

//--- TreeView

struct TreeView;

struct TreeView : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        PlatformFont* font = nullptr;
        DWORD exStyle = 0; // additional flags, will be OR with the rest
        bool fullRowSelect = false;
        bool isRtl = false;
    };

    struct GetTooltipEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        NMTVGETINFOTIPW* info = nullptr;
    };

    struct SelectionChangedEvent {
        TreeView* treeView = nullptr;
        TreeItem prevSelectedItem = 0;
        TreeItem selectedItem = 0;
        NMTREEVIEW* nmtv = nullptr;
        bool byKeyboard = false;
        bool byMouse = false;
    };

    struct CustomDrawEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        NMTVCUSTOMDRAW* nm = nullptr;

        LRESULT result = 0;
    };

    struct ClickEvent {
        TreeView* treeView = nullptr;
        TreeItem treeItem = 0;
        bool isDblClick = false;

        // mouse x,y position relative to the window
        Point mouseWindow;
        // global (screen) mouse x,y position
        Point mouseScreen;

        LRESULT result = 0;
    };

    struct KeyDownEvent {
        TreeView* treeView = nullptr;
        NMTVKEYDOWN* nmkd = nullptr;
        int keyCode = 0;
        u32 flags = 0;

        LRESULT result = 0;
    };

    using KeyDownHandler = Func1<KeyDownEvent*>;
    using ClickHandler = Func1<ClickEvent*>;
    using CustomDrawHandler = Func1<CustomDrawEvent*>;
    using GetTooltipHandler = Func1<TreeView::GetTooltipEvent*>;
    using SelectionChangedHandler = Func1<SelectionChangedEvent*>;

    TreeView();
    ~TreeView() override;

    HWND Create(const CreateArgs&);

    void SetColors(Color col, Color bgCol);

    void WndProc(ControlBase::WndProcEvent* ev);
    void OnNotifyReflect(ControlBase::NotifyReflectEvent* ev);

    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void SetToolTipsDelayTime(int type, int timeInMs);
    HWND GetToolTipsHwnd();

    bool IsExpanded(TreeItem ti);
    bool GetItemRect(TreeItem ti, bool justText, Rect& r);
    TreeItem GetSelection();
    bool SelectItem(TreeItem ti);
    void ExpandAll();
    void CollapseAll();
    void Clear();

    HTREEITEM GetHandleByTreeItem(TreeItem item);
    TempStr GetDefaultTooltipTemp(TreeItem ti);
    TreeItem GetItemAt(int x, int y);
    TreeItem GetTreeItemByHandle(HTREEITEM item);
    bool UpdateItem(TreeItem ti);
    void SetTreeModel(TreeModel* tm);
    void SetState(TreeItem item, bool enable);
    bool GetState(TreeItem item);
    TreeItemState GetItemState(TreeItem ti);

    bool fullRowSelect = false;
    Size idealSize;

    TreeModel* treeModel = nullptr; // not owned by us

    // for WM_NOTIFY with TVN_GETINFOTIP
    GetTooltipHandler onGetTooltip;

    // for WM_NOTIFY wiht NM_CUSTOMDRAW
    CustomDrawHandler onCustomDraw;

    // for WM_NOTIFY with TVN_SELCHANGED
    SelectionChangedHandler onSelectionChanged;

    // for WM_NOTIFY with NM_CLICK or NM_DBCLICK
    ClickHandler onClick;

    // for WM_NOITFY with TVN_KEYDOWN
    KeyDownHandler onKeyDown;

    // private
    TVITEMW item{};
};

TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, Point& pt);

// TabsCtrl is a VirtCtrl; see gui/win/TabsCtrl.h (include VirtCtrl first).
struct TabsCtrl;
struct TabInfo;

template <typename T>
void DeleteWnd(T** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog);
void RunModalWindow(HWND hwndDialog, HWND hwndParent);

HWND GetCurrentModelessDialog();
void SetCurrentModelessDialog(HWND);
