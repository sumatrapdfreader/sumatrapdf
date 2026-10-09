/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui side of the shell. Only forward declarations of gpui types, so
// a file that does not include gpui.h can still talk to it.

namespace gpui {
struct App;
struct Window;
} // namespace gpui

struct MainWindow;

// creates the gpui window, its root view and the MainWindow that owns them
MainWindow* AppShellCreateWindow(gpui::App* app, int dipW, int dipH);
// the app the first window was created with, so a second one needs no plumbing
gpui::App* AppShellGetApp();
// ng: the theme is installed before the first window opens, so the app has to
// be known before AppShellCreateWindow()
void AppShellSetApp(gpui::App*);
// runs the event loop until the last window closes
int AppShellRun(gpui::App* app);
// height of the menu bar row above the canvas, in dips. Zero where the
// menu bar is the OS one (macOS) rather than a row of the window.
constexpr int kMenuBarDy = 28;
// true when the menu bar is the OS one, not a row of the window
bool AppShellNativeMenu();
// install the OS menu bar from the window's model. No-op when the bar is
// drawn into the window, and when this window is not the front one.
void AppShellSyncMenu(MainWindow* win);
// the model was rebuilt; the next sync of the front window reinstalls it
void AppShellMenuRebuilt(MainWindow* win);

void AppShellInvalidate(MainWindow* win);
// one line of gpui frame timings since the last call, for the -dbg-control
// performance snapshot: "frames=N meanMs=.. p95Ms=.. maxMs=.. over16ms=N"
TempStr AppShellFrameStatsTemp(MainWindow* win, bool reset);
// -dbg-control's TestInput / TestUiState: input a posted window message cannot
// carry (modifiers, hover, a held button) and what the shell made of it
TempStr AppShellTestInput(MainWindow* win, Str kind, int a, int b, int c, int d);
// the same into any gpui window (a tool window's)
TempStr AppShellTestInputGpui(gpui::Window* gw, Str kind, int a, int b, int c, int d);
TempStr AppShellUiStateTemp(MainWindow* win);
// orig's gSupressNextAltMenuTrigger: the Alt that is down was used for
// something (Alt + wheel), so its release must not enter the menu bar
void AppShellSuppressAltMenu(MainWindow* win);
void AppShellQuit();
// brings the window to the foreground (orig's MainWindow::Focus())
void AppShellActivateWindow(MainWindow* win);
bool AppShellIsFrameFocused(MainWindow* win);
void AppShellFocusFrame(MainWindow* win);
void AppShellClearEatChar(MainWindow* win);
// closes the gpui window of `win`; the MainWindow itself is deleted by the
// caller (CloseWindow)
void AppShellCloseWindow(MainWindow* win);
void AppShellDeleteWindow(MainWindow* win);
// gpui has no "window closed" callback, so the shell's tick notices a window
// the user closed with the title bar and runs CloseWindow for it
void AppShellReapClosedWindows();
// the open-file dialog; empty when cancelled
TempStr AppShellPromptForFileTemp(MainWindow* win);
// the same prompt with a title of its own (certificate, image, ...); filter is
// orig's lpstrFilter with \1 for \0, which only Windows' dialog can apply
TempStr AppShellPromptForPathTemp(MainWindow* win, Str title, Str filter, Str initialPath = {});
// false on a Linux without zenity / kdialog, which gpui's prompt runs
bool AppShellHasOsFilePicker();
// ng: where the platform's open prompt cannot answer at once (the browser's
// file input) or does not exist (see above), the pick comes back in onPicked
// later and this returns true; otherwise false, and the caller prompts.
// filter as AppShellPromptForPathTemp's
bool AppShellPickFileAsync(MainWindow* win, Str title, Str filter, const Func1<Str>& onPicked);
// File / Open style prompt taking several files where the platform can
bool AppShellPromptForFiles(MainWindow* win, Str filter, StrVec* pathsOut);
void AppShellSetTitle(MainWindow* win, Str title);
// claims the native handle for hotkeys and the other win32 services. OS file
// drops are gpui ExternalPaths drops on the shell root. No-op off Windows; on
// Windows it also registers the WM_DROPFILES fallback.
void AppShellEnableFileDrop(MainWindow* win);
enum class FileDropPhase {
    Hover,
    Drop,
    Leave
};
bool AppShellAcceptDrop(MainWindow* win, float x, float y, const Str* paths, int n, FileDropPhase phase);
// orig's SetTabsInTitlebar: whether the frame has the port's caption (tab
// strip, menu button, caption buttons) in place of the system's title bar
void SetTabsInTitlebar(MainWindow* win, bool inTitleBar);
// where the caption puts the menu bar and the tab strip, and how tall it is;
// false when the frame has no caption of the port's (TestLayout)
bool AppShellCaptionRects(MainWindow* win, Rect* menuOut, Rect* tabsOut, int* dyOut);
// the border the frame keeps around its content while it has the caption
int AppShellFrameBorder(MainWindow* win);
// gpui's client-side title bar for the frame, without touching the flag
// (fullscreen has no caption of either kind)
void AppShellSetClientTitleBar(MainWindow* win, bool on);
// the frame's style changed: have Windows recompute the non-client area
void AppShellFrameChanged(MainWindow* win);
// orig's OpenSystemMenu: the window menu, under the rect (window dips)
void AppShellOpenSystemMenu(MainWindow* win, Rect below);
// the app's small icon; null where there is none to ask for (off Windows)
struct Pixmap;
Pixmap* AppShellAppIconPixmap(MainWindow* win);
// Windows: the cursor of win->nativeCursor, when the pointer is on the canvas
// (gpui's cursor is the canvas' stand-in kind); see gui/NativeCursors_win.cpp
bool AppShellNativeCursorActive(MainWindow* win);
void AppShellApplyNativeCursor(MainWindow* win);
// DoDragDrop (gui/OleDragDrop_win.cpp) ate the button-up that ends the press
// gpui saw begin: tell gpui the button is up
void AppShellAfterNativeDrag(MainWindow* win);
void AppShellForgetNativeHwnd(MainWindow* win);
// ng: gpui has no fullscreen window state (see "gpui gaps"). On Windows this
// is orig's EnterFullScreen / ExitFullScreen on the native frame
// (src/gui/NativeWindow.cpp); elsewhere WindowSetFullScreen.
// `restoreMaximized` re-maximizes on the way out (orig: the window was
// maximized before presentation mode).
void AppShellSetFullScreen(MainWindow* win, bool fullScreen, bool restoreMaximized);
// screen rect of the window, for the -dbg-control layout snapshot. On wasm,
// the frame size at 0,0.
Rect AppShellWindowScreenRect(MainWindow* win);
// the canvas (MainWindow::canvasRc) in screen pixels; empty on wasm
Rect AppShellCanvasScreenRect(MainWindow* win);
// moves / sizes the frame to `r` (outer rectangle, screen pixels; empty: leave
// it) and maximizes it. False on wasm.
bool AppShellPlaceWindow(MainWindow* win, Rect r, bool maximize);
// Current: the canvas as the frame is now. DocumentTab: a document tab is
// about to exist, so a UseTabs strip counts before the tab does.
enum class CanvasPredict {
    Current,
    DocumentTab,
};
Rect AppShellPredictCanvasRc(MainWindow* win, CanvasPredict predict);
// work area of the monitor the frame is on. Primary work area when win is null.
Rect AppShellWorkArea(MainWindow* win);
// full bounds of that monitor, including the dock or taskbar
Rect AppShellMonitorRect(MainWindow* win);
// shift rect into the work area. fully: the whole rect must be inside
Rect AppShellShiftToWorkArea(Rect rect, MainWindow* win, bool fully);
// the frame's rectangle while it is not maximized. False when it is not known
// (minimized, fullscreen, no native handle, or not Windows)
bool AppShellNormalWindowRect(MainWindow* win, Rect* out);
// dpi of the monitor the frame is on; 96 where it is not known
int AppShellWindowDpi(MainWindow* win);
#if OS_DARWIN || OS_WASM
float AppShellRenderScale(gpui::Window* win);
#endif
#if OS_DARWIN
void AppShellDisableAutoTermination();
#endif
// presentation mode's auto-hidden cursor (orig hides it with SetCursor(null))
void AppShellShowCursor(MainWindow* win, bool show);
// Fullscreen.PreventSleep: keep the display awake (Windows only for now)
void AppShellPreventSleep(bool on);
#if OS_WIN
// the native handle of a window, or null; orig's MainWindow::hwndFrame
HWND AppShellNativeHwnd(MainWindow* win);
MainWindow* AppShellWindowFromHwnd(HWND hwnd);
// the top-level windows of another SumatraPDF process: `procId`, or with 0 any
// process running this exe (orig: FindWindowW(kFrameClassName))
void AppShellFindOtherInstanceWindows(DWORD procId, Vec<HWND>& out);
#endif
