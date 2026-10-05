/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig draws the overlay scrollbar into a layered WS_POPUP window it keeps
// aligned with the canvas and drives with SCROLLINFO / WM_VSCROLL. ng: the bar
// is an element the owner puts over its content; it keeps orig's look (thin /
// thick widths, colors, alphas, arrows, minimum thumb), its mouse handling
// (arrows and track paging with auto-repeat, Shift+click, thumb drag) and its
// "smart" state machine (reveal on scroll for 5 s, on mouse move for 3 s,
// thicken while the cursor is over the edge band). The "windows" mode, where
// orig has the window's own WS_VSCROLL / WS_HSCROLL bars, uses it too.

namespace gpui {
struct Ctx;
struct El;
struct Window;
struct Listener;
} // namespace gpui

struct MainWindow;
enum class ScrollMsg;

// gOverlayScrollbarSuppressThick: while something else owns the pointer (a
// splitter drag), proximity must not pop the bar out
extern bool gOverlayScrollbarSuppressThick;

struct OverlayScrollbar;

// orig's SendScrollMsg: WM_VSCROLL / WM_HSCROLL to the owner. For
// ScrollMsg::ThumbTrack the position is sb->nTrackPos.
using OverlayScrollbarScrollFn = void (*)(MainWindow* win, OverlayScrollbar* sb, ScrollMsg msg);

struct OverlayScrollbar {
    enum class Type {
        Vert,
        Horz
    };
    enum class Mode {
        Smart,
        Thick,
        // ng: the "windows" scrollbar mode. Always thick and opaque, takes
        // room beside the content instead of covering it
        Windows
    };
    enum class State {
        Hidden,
        SmartInvisible,
        SmartThin,
        SmartThick,
        AlwaysThick
    };

    Type type = Type::Vert;
    Mode mode = Mode::Smart;
    State state = State::Hidden;

    int thinWidth = 0;
    int thickWidth = 0;

    // how long to show the thin bar after a scroll, and after the mouse stops
    int showAfterScrollMs = 5000;
    int hideAfterMouseStopMs = 3000;
    // what is left of the auto-hide timer, in ms; 0 = not armed
    int hideLeftMs = 0;

    // scroll info (same semantics as SCROLLINFO)
    int nMin = 0;
    int nMax = 0;
    int nPage = 0;
    int nPos = 0;
    int nTrackPos = 0;

    // drag state
    bool isDragging = false;
    int dragStartY = 0;
    int dragStartPos = 0;
    bool mouseOverThumb = false;

    // auto-repeat for arrow / track clicks: ScrollMsg::None when not repeating
    ScrollMsg repeatScrollCode{};
    bool repeatIsInitial = false;
    // ng: orig's kTimerRepeatScroll, counted down by the owner's tick
    int repeatLeftMs = 0;
    // ng: where in the track the press that pages was (client coords)
    int repeatClickPos = 0;

    // ng: orig's hwndOwner and what SendScrollMsg reaches
    MainWindow* win = nullptr;
    OverlayScrollbarScrollFn onScroll = nullptr;
    // ng: orig's HwndClientRect(sb->hwnd), in pixels, and dips per pixel
    int clientDx = 0;
    int clientDy = 0;
    float k = 1;
    // ng: the "windows" mode's right-click menu and where it was opened
    int popupIdx = -1;
    unsigned int popupGen = 0;
    int menuPos = 0;
};

OverlayScrollbar* OverlayScrollbarCreate(MainWindow* win, OverlayScrollbar::Type type, OverlayScrollbar::Mode mode,
                                         OverlayScrollbarScrollFn onScroll);
void OverlayScrollbarDestroy(OverlayScrollbar* sb);
void OverlayScrollbarSetMode(OverlayScrollbar* sb, OverlayScrollbar::Mode mode);
// same as SetScrollInfo with SIF_ALL; doesn't reveal the bar
void OverlayScrollbarSetInfo(OverlayScrollbar* sb, int nMin, int nMax, int nPage, int nPos);
// the cursor moved over the owner: `overBand` is orig's GetScrollbarScreenRect
// test (the cursor is within the thick width of this edge)
void OverlayScrollbarOnMouse(OverlayScrollbar* sb, bool overCanvas, bool overBand, bool mouseMoved);
// the document scrolled without the mouse moving (wheel, keys): reveal the bar
void OverlayScrollbarNotifyScroll(OverlayScrollbar* sb);
// auto-hide and auto-repeat; false `ownerActive` hides a smart bar (orig's poll)
// returns true when the bar has to be drawn again
bool OverlayScrollbarTick(OverlayScrollbar* sb, int elapsedMs, bool ownerActive);
void OverlayScrollbarHide(OverlayScrollbar* sb);
bool IsOverlayScrollbarVisible(OverlayScrollbar* sb);
bool IsOverlayScrollbarThick(OverlayScrollbar* sb);
int OverlayScrollbarWidth(OverlayScrollbar* sb);
// the bar as an element of `len` pixels along its axis (`k` dips per pixel)
// and OverlayScrollbarWidth() across; the caller positions it. `onWheel` gets
// the wheel over the bar (orig forwards it to the owner).
gpui::El* OverlayScrollbarBuild(gpui::Ctx* cx, OverlayScrollbar* sb, Str id, int len, float k,
                                const gpui::Listener* onWheel);
// a thumb drag and the auto-repeat end wherever the button comes up
void OverlayScrollbarHookWindow(gpui::Window* gw);
// what the whole window does on a mouse move / a scroll / a tick
void OverlayScrollbarsOnMouse(MainWindow* win, int x, int y, bool onCanvas);
void OverlayScrollbarsNotifyScroll(MainWindow* win);
void OverlayScrollbarsTick(MainWindow* win, int elapsedMs);
void OverlayScrollbarsSyncMode(MainWindow* win);
void OverlayScrollbarsDelete(MainWindow* win);
