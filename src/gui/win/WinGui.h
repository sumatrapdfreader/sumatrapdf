/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

//--- WindowBase

TempStr WinMsgNameTemp(UINT);

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
enum WindowBorderStyle {
    kWindowBorderNone,
    kWindowBorderClient,
    kWindowBorderStatic
};

struct WindowBase;
struct VirtRoot;

WindowBase* WindowBaseFromHwnd(HWND);
void MarkHWNDDestroyed(HWND);
bool HwndWasDestroyed(HWND);

struct ControlBase;

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
    HFONT font = nullptr;
    Str text;
    bool isRtl = false;
};

struct CreateCustomArgs {
    HWND parent = nullptr;
    WStr className;
    Str title;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos;
    // don't set both menu and cmdId
    HMENU menu = nullptr;
    int cmdId = 0; // command sent on click
    bool visible = true;
    HFONT font = nullptr;
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

// Base of the top-level windows (and the child windows that place themselves,
// like the notification toasts). Not an ILayout: a window isn't positioned by a
// parent's layout - it has a `layout` of its own children instead
struct WindowBase {
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
        Point screenPos{};
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
        LRESULT result = -1; // -1 = not handled
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
        Size size{};
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
    WindowBase(HWND hwnd);
    virtual ~WindowBase();
    void Destroy();

    HWND CreateCustom(const CreateCustomArgs&);

    void SetVisibility(Visibility);
    Visibility GetVisibility();

    void Attach(HWND hwnd);
    HWND Detach();

    void Subclass();
    void UnSubclass();

    // PreTranslateMessage: onPreTranslate, then key-down -> onKeyDown, then Tab default
    bool PreTranslateMessage(MSG& msg);

    void SetColors(Color textColor, Color bgColor);

    void Close();
    void SetPos(Rect* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(Str);
    TempStr GetTextTemp();

    HFONT GetFont();
    void SetFont(HFONT font);

    // dpi of the monitor this window is on
    int GetDpi() const;

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    // sends a control's own messages (colors, owner draw, ...) back to it
    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    HBRUSH BackgroundBrush();

    // lays `layout` out at `size` and refreshes the virtual controls
    void DoLayout(Size);
    void DoLayout();

    // Tab ring over `layout`, across HWND and virtual controls alike. A virtual
    // control holding the focus means this window holds the win32 focus and
    // `vroot->focused` says which one it is
    bool TabNavigate(bool backwards);
    void SetFocusTo(ControlBase*);
    void SetFocusTo(VirtCtrl*);

    Kind kind = nullptr;
    uintptr_t userData = 0;

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    Color bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    Color textColor = kColorUnset;

    // the layout of our children, if we have one. It can hold HWND controls
    // (ControlBase) and virtual ones (VirtCtrl) side by side
    ILayout* layout = nullptr;
    // the virtual controls of `layout`, if it has any. Created on demand by
    // DoLayout(), owned here, but it doesn't own the controls - `layout` does
    VirtRoot* vroot = nullptr;
    // relayout on WM_SIZE. Off by default: most windows do it themselves
    bool autoLayout = false;
    // if false, WM_ERASEBKGND returns TRUE without painting (WM_PAINT covers).
    // default false: custom windows usually double-buffer the full client
    bool shouldEraseBackground = false;

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
struct ControlBase : ILayout {
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
        LRESULT result = -1; // -1 = not handled
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
        Size size{};
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
    ~ControlBase() override;
    void Destroy();

    HWND CreateControl(const CreateControlArgs&);
    HWND CreateCustom(const CreateCustomArgs&);

    // layout sizing (kept virtual: subclasses measure differently)
    virtual Size GetIdealSize();

    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
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
    HWND Detach();

    void Subclass();
    void UnSubclass();

    // for reflection from parents (WindowBase / ControlBase WndProcDefault)
    bool DispatchCommand(WPARAM wparam, LPARAM lparam);
    LRESULT DispatchMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT DispatchNotifyReflect(WPARAM wparam, LPARAM lparam);

    void SetColors(Color textColor, Color bgColor);

    void SetPos(Rect* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(Str);
    TempStr GetTextTemp();

    HFONT GetFont();
    void SetFont(HFONT font);

    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;
    bool IsFocused() const;
    void SetFocus();

    void SuspendRedraw() const;
    void ResumeRedraw() const;

    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);

    HBRUSH BackgroundBrush();

    Kind kind = nullptr;
    uintptr_t userData = 0;

    Insets insets{};
    Size childSize;
    Rect lastBounds;

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    HWND hwnd = nullptr;
    HFONT font = nullptr; // we don't own it
    UINT_PTR subclassId = 0;

    Color bgColor = kColorUnset;
    HBRUSH bgBrush = nullptr;
    Color textColor = kColorUnset;

    // a control can host a layout tree of its own, real and virtual mixed
    ILayout* layout = nullptr;
    VirtRoot* vroot = nullptr;

    void DoLayout(Size);

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

ControlBase* ControlFromHwnd(HWND);
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
        HFONT font = nullptr;
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
        HFONT font = nullptr;
        bool isRtl = false;
    };

    Tooltip();
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
};

struct TooltipInfo {
    Str s;
    Rect r;
    int id;
};

int TooltipGetCount(HWND hwnd);
void TooltipRemoveAll(HWND hwnd);
void TooltipAddTools(HWND hwnd, HWND owner, TooltipInfo* tools, int nTools);

//--- Edit
using TextChangedHandler = Func0;

Color EditBottomBorderColor();
void PostDelayedEditSelectAll(HWND);
void PostDelayedEditCtrlBack(HWND);

struct Edit : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isMultiLine = false;
        bool withBorder = false;
        // 1px NC underline under the client area (no WS_EX_CLIENTEDGE)
        bool withBottomBorder = false;
        // 1px NC rectangle (not the themed WS_EX_CLIENTEDGE / Win11 accent)
        bool withFrame = false;
        bool numbersOnly = false;
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
        HFONT font = nullptr;
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

    // remembers CreateArgs.withBorder: with themes darkmodelib strips
    // WS_EX_CLIENTEDGE / WS_BORDER and draws the border in a subclass, so
    // window styles can't be used to detect the border
    bool createdWithBorder = false;
    bool createdWithBottomBorder = false;
    bool createdWithFrame = false;
    bool selectAllOnFocus = false;
    bool delaySelectAll = false;

    Edit();
    ~Edit() override;

    HWND Create(const CreateArgs&);
    void WndProc(ControlBase::WndProcEvent* ev);
    void OnMessageReflect(ControlBase::MessageReflectEvent* ev);
    void OnCommand(ControlBase::CommandEvent* ev);

    Size GetIdealSize() override;

    void SetIdealWidthChars(int nChars);
    void SetMaxWidthChars(int nChars);
    void SetIdealWidthFromText(Str s, int extraPx = 0);
    void SetNumbersOnly(bool);

    int GetLeftTextMargin();

    void SetCue(Str);
    void SetMargins(int left, int right);
    void SetSelection(int start, int end);
    void GetSelection(int& start, int& end) const;
    void SelectAll();
    void SetCursorPosition(int pos);
    void SetCursorPositionAtEnd();
    int GetTextLen() const;
    void SetModified(bool);
    bool IsModified() const;
    void SetClassCursor(LPWSTR);
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
        State initialState = State::Unchecked;
        bool isRtl = false;
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

struct DropDown : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        HFONT font = nullptr;
        bool isRtl = false;
        bool isEditable = false;
        // TODO: model or items
    };

    using SelectionChangedHandler = Func0;

    // TODO: use DropDownModel
    StrVec items;
    SelectionChangedHandler onSelectionChanged;
    TextChangedHandler onTextChanged;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDown::CreateArgs&);

    Size GetIdealSize() override;
    void OnCommand(ControlBase::CommandEvent* ev);

    int GetCurrentSelection();
    void SetCurrentSelection(int n);
    void SetItems(StrVec& newItems);
    void SetItemsSeqStrings(SeqStrings items);
    void SetCueBanner(Str);
};

//--- Trackbar

struct Trackbar;

struct Trackbar : ControlBase {
    struct CreateArgs {
        HWND parent = nullptr;
        bool isHorizontal = true;
        int rangeMin = 1;
        int rangeMax = 5;
        HFONT font = nullptr;
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
        HFONT font = nullptr;
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

HWND GetCurrentModelessDialog();
void SetCurrentModelessDialog(HWND);
