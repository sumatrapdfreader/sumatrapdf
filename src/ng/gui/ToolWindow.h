/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig shows a number of things in top-level windows of their own (Browse
// Files In Folder, the find window, Document Properties, ...). gpui opens a
// window at a size and nothing else (see "gpui gaps": no owner, no style, no
// position). On Windows, macOS and Linux the native frame is configured, and
// such a window is a second gpui window described by a ToolWindowDesc. On
// wasm, and in plugin mode, ToolWindowsAvailable() is false and the caller
// draws the same content inside the frame.

namespace gpui {
struct Ctx;
struct El;
struct Window;
struct KeyEvent;
} // namespace gpui

struct MainWindow;
struct ToolWindow;

// orig's window styles, as far as they differ between the tool windows
enum class ToolWinFrame {
    Caption,    // WS_CAPTION | WS_SYSMENU
    Overlapped, // the same with the minimize and maximize boxes
    None,       // WS_POPUP: no caption; moved by its body where the content says so
};

enum class ToolWinResize {
    Fixed,
    Resizable, // WS_THICKFRAME
};

enum class ToolWinOwner {
    Owned,    // stays above the main window, no taskbar button, minimized with it
    TopLevel, // a window of its own (Alt-Tab switches to it)
};

enum class ToolWinStyle {
    Normal,
    Tool, // WS_EX_TOOLWINDOW: the small caption
};

enum class ToolWinActivate {
    Yes,
    No, // WS_EX_NOACTIVATE: showing or clicking it leaves the main window active
};

// orig's modal dialogs and the windows that EnableWindow(owner, FALSE)
enum class ToolWinModal {
    No,
    Yes, // the main window takes no input while this one is up
};

struct ToolWindowDesc {
    // how the automation channel names it (TestToolWindow)
    const char* name = nullptr;
    // read on every frame, so a language change reaches the caption
    Str (*title)() = nullptr;
    ToolWinFrame frame = ToolWinFrame::Caption;
    ToolWinResize resize = ToolWinResize::Fixed;
    ToolWinOwner owner = ToolWinOwner::Owned;
    ToolWinStyle style = ToolWinStyle::Normal;
    ToolWinActivate activate = ToolWinActivate::Yes;
    ToolWinModal modal = ToolWinModal::No;
    // smallest client size (dips) a resizable one can be dragged to
    Size minClient;
    // files can be dropped on it: an OLE drop target on its HWND (Windows)
    bool dropFiles = false;
    // the content; fills the window
    gpui::El* (*build)(MainWindow* owner, gpui::Ctx* cx) = nullptr;
    // a key the content's own listeners did not take; true when handled
    bool (*onKey)(MainWindow* owner, gpui::Ctx* cx, const gpui::KeyEvent* ev) = nullptr;
    // a key before the focused element (a text field) sees it; true to take it
    bool (*onCaptureKey)(MainWindow* owner, gpui::Ctx* cx, const gpui::KeyEvent* ev) = nullptr;
    // a key going up (Windows sends no key-down for PrtSc)
    void (*onKeyUp)(MainWindow* owner, gpui::Ctx* cx, const gpui::KeyEvent* ev) = nullptr;
    // set for a window that outlives its main window, as orig's unowned
    // windows do: when that one closes and another main window is left, the
    // tool window stays, belongs to `newOwner` from then on and is told so
    void (*onOwnerClosed)(MainWindow* newOwner) = nullptr;
    // the window went away without ToolWindowClose(): the close box, Alt+F4,
    // the owner closing. The ToolWindow is gone when this runs
    void (*onClosed)(MainWindow* owner) = nullptr;
    // moved or sized; `outer` is the window rectangle in screen pixels
    void (*onMoved)(MainWindow* owner, Rect outer) = nullptr;
    // called about every tickMs from the window's own timer, outside a frame
    // (what the shell's tick is for the frame: a webview is made from it)
    void (*onTick)(MainWindow* owner, gpui::Ctx* cx, int ms) = nullptr;
    int tickMs = 50;
    // the user finished moving or sizing it (orig's WM_EXITSIZEMOVE)
    void (*onExitSizeMove)(MainWindow* owner, Rect outer) = nullptr;
    // orig's WM_ACTIVATE
    void (*onActivate)(MainWindow* owner, bool active) = nullptr;
    // a window without a caption is moved by its body (orig's WM_NCHITTEST ->
    // HTCAPTION): true for the points that are the content's (a button). The
    // point is in the window's client area, in dips
    bool (*isClientPoint)(MainWindow* owner, Point pt) = nullptr;
    // for a window docked to the frame, as orig's popups that are put back in
    // place on the frame's WM_MOVE / WM_SIZE (RegisterOnWindowMoved): where it
    // belongs now, in screen pixels; empty: nowhere, it is hidden. Asked when
    // the frame moved or was sized and on the shell's tick. `tw` is the
    // window asked about
    Rect (*place)(MainWindow* owner, ToolWindow* tw) = nullptr;
};

// false where a second window cannot be owned, styled and placed (off
// Windows, wasm, plugin mode): the caller keeps its content in the frame
bool ToolWindowsAvailable();

// `outer` is the window rectangle in screen pixels (see ToolWindowOuterSize).
// The window itself is made from the ui task queue, not inside the caller.
// null when ToolWindowsAvailable() is false.
ToolWindow* ToolWindowOpen(const ToolWindowDesc& desc, MainWindow* owner, Rect outer);
// from the ui task queue as well; onClosed is not called. `tw` must not be
// used after this
void ToolWindowClose(ToolWindow* tw);
ToolWindow* ToolWindowFind(Str name);
// false once the window is gone (closed, or its owner was)
bool ToolWindowIsLive(ToolWindow* tw);
// the tool window of `win` that is the active window, if one is
ToolWindow* ToolWindowActiveOf(MainWindow* win);
// the modal window that keeps `win` disabled, if one is up (the topmost)
ToolWindow* ToolWindowModalOf(MainWindow* win);
// orig's modal dialog (RunModalWindow): a captioned window of a fixed size,
// owned by the main window, which takes no input while it is up
ToolWindowDesc ToolWindowModalDesc(const char* name, Str (*title)());
// what orig's WindowBase does with a dialog's keys once its controls passed
// on them: Esc is `close` (closeOnEsc), Enter the default button
// (DlgSetDefault). For a modal window's onKey
bool ToolWindowDialogKey(gpui::Ctx* cx, const gpui::KeyEvent* ev, void (*close)());
MainWindow* ToolWindowOwner(ToolWindow* tw);
void ToolWindowSetOwner(ToolWindow* tw, MainWindow* owner);
// null until the window was made
gpui::Window* ToolWindowGpui(ToolWindow* tw);
void ToolWindowInvalidate(ToolWindow* tw);
// orig's SetForegroundWindow(hwnd)
void ToolWindowActivate(ToolWindow* tw);
bool ToolWindowIsActive(ToolWindow* tw);
// the window rectangle for a client size in dips, on the owner's monitor
Size ToolWindowOuterSize(const ToolWindowDesc& desc, MainWindow* owner, Size clientDip);
// orig's HwndCenterDialog: the window rectangle for a client size in dips,
// centered on the owner's window and kept inside its work area
Rect ToolWindowCenteredRect(const ToolWindowDesc& desc, MainWindow* owner, Size clientDip);
// the same for a window size in screen pixels
Rect ToolWindowCenteredOuter(MainWindow* owner, Size outer);
// what desc.minClient was at the start; for content whose minimum changes
void ToolWindowSetMinClient(ToolWindow* tw, Size minClientDip);
// screen pixels; empty until the window was made
Rect ToolWindowRect(ToolWindow* tw);
// sizes the client area (dips) where the window is, as orig's
// HwndResizeClientSize
void ToolWindowSetClientSize(ToolWindow* tw, Size clientDip);
void ToolWindowMove(ToolWindow* tw, Rect outer);
// starts the system's move loop, for a window dragged by its body
void ToolWindowDragMove(ToolWindow* tw);
// orig's SetIsVisible() on a dialog that stays alive while it is out of the
// way: a hidden modal window gives its main window back (enabled, active),
// shown again it takes it away. The window keeps its place
void ToolWindowSetVisible(ToolWindow* tw, bool visible);
bool ToolWindowIsVisible(ToolWindow* tw);
// puts the docked windows (ToolWindowDesc::place) of `win` where they belong
void ToolWindowsFollow(MainWindow* win);
// orig's bars over the canvas (ReadAloudPlaybackBar, ReadingAutoScrollBar): an
// owned popup without a caption that is never activated (WS_POPUP,
// WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)
ToolWindowDesc ToolWindowDockedDesc(const char* name);
// where such a bar goes (orig's UpdateLayout): `marginDip` inside the canvas
// at its sides and bottom, `barDyDip` high; kept under the canvas's top when
// the canvas is lower than the bar. Screen pixels; empty without a canvas
Rect ToolWindowDockedBarRect(MainWindow* owner, int marginDip, float barDyDip);
// where the client area of `tw` starts in its main window's client area, in
// dips: a tooltip for an element of a small window is drawn in the main
// one, which has room for it
Point ToolWindowOffsetInOwner(ToolWindow* tw);

// ng: gpui's inputs and buttons draw their text at 14 px whatever the element
// says, and a font size is in rems. Sets the window's rem so that they come
// out at `px` (orig's app font) and returns the factor the window's other
// font sizes have to be multiplied by. For the content of a tool window
float ToolWindowSetUiFontPx(gpui::Ctx* cx, float px);

// the shell's side: AppShellInvalidate() repaints the tool windows of `win`
// too, and they are closed before it is
void ToolWindowsInvalidateFor(MainWindow* win);
void ToolWindowsCloseFor(MainWindow* win);
#if OS_WIN
// NativeWindow.cpp claims gpui windows by class name; these are not frames
bool ToolWindowOwnsHwnd(HWND hwnd);
HWND ToolWindowHwnd(ToolWindow* tw);
// the main window of the tool window with this handle
MainWindow* ToolWindowOwnerFromHwnd(HWND hwnd);
#endif

// -dbg-control's TestToolWindow: list | on | off | close <name> |
// input <name> <kind> a b c d (see AppShellTestInput) | state <name> |
// layout <name> (also TestLayout <name>): the texts, click targets and
// scrolled boxes of the window's last frame; `layout frame`: the same of the
// first main window
TempStr ToolWindowTestTemp(Str action, Str name, Str kind, int a, int b, int c, int d);
