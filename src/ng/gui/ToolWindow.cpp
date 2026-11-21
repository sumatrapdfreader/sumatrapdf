/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "VirtKeys.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/DragDrop.h"
#include "SumatraDialogs.h"
#include "gui/ToolWindow.h"

#include "SumatraLog.h"

struct ToolRootView {
    ToolWindow* tw = nullptr;

    static gp::El* Render(ToolRootView* self, gp::Ctx* cx);
    static void OnKeyDown(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnKeyUp(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnCaptureKey(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnTick(ToolRootView* self, gp::Ctx* cx, const gp::TickEvent* ev);
    static void OnNoKey(ToolRootView*, gp::Ctx*, const gp::KeyEvent*) {}
    static void OnDropFiles(ToolRootView* self, gp::Ctx* cx, const gp::DropEvent* ev);
};

struct ToolWindow {
    ToolWindowDesc desc;
    MainWindow* owner = nullptr;
    gp::Window* gw = nullptr;
    gp::Entity<ToolRootView> view;
    // screen pixels; where it is, or where it will be made
    Rect outer;
    // ToolWindowClose() was called: onClosed is not wanted
    bool closing = false;
    // ToolWindowSetVisible(false)
    bool hidden = false;
    Str title; // owned; what the caption shows
    // the caption's theme last set: -1 none yet, 0 light, 1 dark
    int darkCaption = -1;
    // -dbg-control's handle for it, as MainWindow::testId; apart from theirs
    int testId = 0;
};

// open or about to be made; a closed one leaves at once and is deleted from
// the ui task queue
static Vec<ToolWindow*> gToolWindows;
// TestToolWindow off: draw in the frame too, to test the overlay fallback
static bool gToolWindowsOff = false;

bool ToolWindowsAvailable() {
#if OS_WASM
    return false;
#else
    return !gToolWindowsOff && !gPluginMode;
#endif
}

static bool IsLive(ToolWindow* tw) {
    return tw && VecContains(gToolWindows, tw);
}

// placing one can run code that opens or closes another
template <class Fn>
static void FollowOwned(MainWindow* win, Fn followOne) {
    Vec<ToolWindow*> wins = gToolWindows;
    for (ToolWindow* tw : wins) {
        if (tw->owner == win && IsLive(tw)) {
            followOne(tw);
        }
    }
}

ToolWindow* ToolWindowFind(Str name) {
    for (ToolWindow* tw : gToolWindows) {
        if (!tw->closing && str::Eq(Str(tw->desc.name), name)) {
            return tw;
        }
    }
    return nullptr;
}

bool ToolWindowIsLive(ToolWindow* tw) {
    return IsLive(tw) && !tw->closing;
}

ToolWindow* ToolWindowActiveOf(MainWindow* win) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->owner == win && !tw->closing && ToolWindowIsActive(tw)) {
            return tw;
        }
    }
    return nullptr;
}

ToolWindow* ToolWindowModalOf(MainWindow* win) {
    for (int i = len(gToolWindows) - 1; i >= 0; i--) {
        ToolWindow* tw = gToolWindows[i];
        if (tw->owner == win && !tw->closing && tw->gw && tw->desc.modal == ToolWinModal::Yes &&
            ToolWindowIsVisible(tw)) {
            return tw;
        }
    }
    return nullptr;
}

ToolWindowDesc ToolWindowModalDesc(const char* name, Str (*title)()) {
    ToolWindowDesc desc;
    desc.name = name;
    desc.title = title;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Fixed;
    desc.owner = ToolWinOwner::Owned;
    desc.modal = ToolWinModal::Yes;
    return desc;
}

ToolWindowDesc ToolWindowDockedDesc(const char* name) {
    ToolWindowDesc desc;
    desc.name = name;
    desc.frame = ToolWinFrame::None;
    desc.resize = ToolWinResize::Fixed;
    desc.owner = ToolWinOwner::Owned;
    desc.style = ToolWinStyle::Tool;
    desc.activate = ToolWinActivate::No;
    return desc;
}

bool ToolWindowDialogKey(gp::Ctx* cx, const gp::KeyEvent* ev, void (*close)()) {
    if (ev->vk == VK_ESCAPE) {
        close();
        return true;
    }
    if (ev->vk != VK_RETURN || ev->ctrl || ev->alt) {
        return false;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    if (editFocused && gp::InputIsMultiLine(cx->win->input)) {
        return false;
    }
    return DlgDefaultOnEnter(cx, editFocused);
}

MainWindow* ToolWindowOwner(ToolWindow* tw) {
    return IsLive(tw) ? tw->owner : nullptr;
}

gp::Window* ToolWindowGpui(ToolWindow* tw) {
    return IsLive(tw) ? tw->gw : nullptr;
}

void ToolWindowInvalidate(ToolWindow* tw) {
    if (IsLive(tw) && tw->gw) {
        gp::AppInvalidate(tw->gw);
    }
}

void ToolWindowsInvalidateFor(MainWindow* win) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->owner == win && tw->gw) {
            gp::AppInvalidate(tw->gw);
        }
    }
}

// the record of a window that is gone
static void FinishClosed(ToolWindow* tw) {
    if (!tw->closing && tw->desc.onClosed) {
        tw->desc.onClosed(IsMainWindowValid(tw->owner) ? tw->owner : nullptr);
    }
    if (tw->view.IsValid()) {
        gp::EntityDrop(AppShellGetApp(), tw->view.id);
    }
    str::Free(tw->title);
    delete tw;
}

static void Forget(ToolWindow* tw) {
    VecRemove(gToolWindows, tw);
    tw->gw = nullptr;
    uitask::Post(MkFunc0(FinishClosed, tw), "ToolWindowFinishClosed");
}

static gp::Window* OwnerGpui(MainWindow* owner) {
    return owner ? owner->gpuiWin : nullptr;
}

#if OS_WIN
bool ToolWindowOwnsHwnd(HWND hwnd) {
    return ToolWindowFromHwnd(hwnd) != nullptr;
}

ToolWindow* ToolWindowFromHwnd(HWND hwnd) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->gw && (HWND)gp::PlatWindowHandle(tw->gw) == hwnd) {
            return tw;
        }
    }
    return nullptr;
}

HWND ToolWindowHwnd(ToolWindow* tw) {
    return IsLive(tw) && tw->gw ? (HWND)gp::PlatWindowHandle(tw->gw) : nullptr;
}

MainWindow* ToolWindowOwnerFromHwnd(HWND hwnd) {
    ToolWindow* tw = ToolWindowFromHwnd(hwnd);
    return tw ? tw->owner : nullptr;
}
#endif

// orig's window styles as gpui's window options
static gp::WinOpts ToolOpts(const ToolWindowDesc& desc, MainWindow* owner) {
    gp::WinOpts opts;
    opts.frameless = desc.frame == ToolWinFrame::None;
    opts.utility = desc.style == ToolWinStyle::Tool;
    opts.resizable = desc.resize == ToolWinResize::Resizable;
    opts.minimizable = desc.frame == ToolWinFrame::Overlapped;
    opts.maximizable = desc.frame == ToolWinFrame::Overlapped;
    opts.activatable = desc.activate == ToolWinActivate::Yes;
    opts.focus = opts.activatable;
    if (desc.modal == ToolWinModal::Yes) {
        opts.kind = gp::WindowKind::Dialog;
    } else if (desc.owner == ToolWinOwner::Owned) {
        opts.kind = gp::WindowKind::Floating;
    }
    if (opts.kind != gp::WindowKind::Normal) {
        opts.parent = OwnerGpui(owner);
    }
    if (!desc.minClient.IsEmpty()) {
        opts.minSize = ToGpui(desc.minClient);
    }
    opts.maxSize = ToGpui(desc.maxClient);
    return opts;
}

static int OwnerDpi(MainWindow* owner) {
    int dpi = AppShellWindowDpi(owner);
    return dpi > 0 ? dpi : 96;
}

Size ToolWindowOuterSize(const ToolWindowDesc& desc, MainWindow* owner, Size clientDip) {
    int dpi = OwnerDpi(owner);
    Rect client{0, 0, MulDiv(clientDip.dx, dpi, 96), MulDiv(clientDip.dy, dpi, 96)};
    gp::ScreenBounds r = gp::WindowFrameForContent(OwnerGpui(owner), ToolOpts(desc, owner), ToGpuiScreen(client));
    return Size(r.w, r.h);
}

Rect ToolWindowCenteredOuter(MainWindow* owner, Size sz) {
    Rect frame = AppShellWindowScreenRect(owner);
    if (frame.IsEmpty()) {
        return {};
    }
    gp::ScreenBounds r = gp::ScreenBoundsCentered(gp::ScreenBounds{0, 0, sz.dx, sz.dy}, ToGpuiScreen(frame));
    gp::ScreenBounds work = gp::WindowDisplayVisibleBounds(OwnerGpui(owner));
    return FromGpui(gp::ScreenBoundsShiftInto(r, work, true));
}

Rect ToolWindowCenteredRect(const ToolWindowDesc& desc, MainWindow* owner, Size clientDip) {
    return ToolWindowCenteredOuter(owner, ToolWindowOuterSize(desc, owner, clientDip));
}

Rect ToolWindowRect(ToolWindow* tw) {
    if (!IsLive(tw)) {
        return {};
    }
    Rect r = tw->gw ? FromGpui(gp::WindowFrameBounds(tw->gw)) : Rect{};
    return r.IsEmpty() ? tw->outer : r;
}

void ToolWindowMove(ToolWindow* tw, Rect outer) {
    if (!IsLive(tw) || outer.IsEmpty()) {
        return;
    }
    tw->outer = outer;
    if (tw->gw) {
        gp::WindowSetFrameBounds(tw->gw, ToGpuiScreen(outer));
    }
}

// orig's SetForegroundWindow(hwnd)
void ToolWindowActivate(ToolWindow* tw) {
    if (IsLive(tw) && tw->gw) {
        gp::AppActivate(tw->gw);
    }
}

// the active window of the program: what gets the keys while it is the
// foreground one
bool ToolWindowIsActive(ToolWindow* tw) {
    return IsLive(tw) && tw->gw && gp::WindowIsActive(tw->gw);
}

bool ToolWindowIsVisible(ToolWindow* tw) {
    return IsLive(tw) && !tw->hidden;
}

// a hidden modal window gives its main window back (gpui's Dialog does that)
void ToolWindowSetVisible(ToolWindow* tw, bool visible) {
    if (!IsLive(tw) || tw->closing || tw->hidden == !visible) {
        return;
    }
    tw->hidden = !visible;
    if (!tw->gw) {
        // not made yet: CreateNow sees the flag
        return;
    }
    gp::WindowSetVisible(tw->gw, visible, tw->desc.activate == ToolWinActivate::Yes);
    if (visible) {
        ToolWindowInvalidate(tw);
    }
}

// a docked window goes where its content says, without being activated
static void FollowOne(ToolWindow* tw) {
    if (!tw->desc.place || tw->closing || !tw->gw || !IsMainWindowValid(tw->owner)) {
        return;
    }
    gp::Window* ownerGw = OwnerGpui(tw->owner);
    // the system hides an owned window with its minimized owner and shows it
    // again itself
    if (!ownerGw || gp::AppIsMinimized(ownerGw)) {
        return;
    }
    Rect r = tw->hidden ? Rect{} : tw->desc.place(tw->owner, tw);
    bool visible = gp::WindowIsVisible(tw->gw);
    if (r.IsEmpty()) {
        if (visible) {
            gp::WindowSetVisible(tw->gw, false, false);
        }
        return;
    }
    if (visible && r == FromGpui(gp::WindowFrameBounds(tw->gw))) {
        return;
    }
    tw->outer = r;
    gp::WindowSetFrameBounds(tw->gw, ToGpuiScreen(r));
    if (!visible) {
        gp::WindowSetVisible(tw->gw, true, false);
    }
}

void ToolWindowsFollow(MainWindow* win) {
    FollowOwned(win, FollowOne);
}

Rect ToolWindowDockedBarRect(MainWindow* owner, int marginDip, float barDyDip) {
    Rect canvas = AppShellCanvasScreenRect(owner);
    if (canvas.IsEmpty()) {
        return {};
    }
    int dpi = OwnerDpi(owner);
    int margin = MulDiv(marginDip, dpi, 96);
    int barDx = std::max(canvas.dx - (2 * margin), 0);
    int barDy = (int)(barDyDip * (float)dpi / 96.f + 0.5f);
    if (barDx <= 0 || barDy <= 0) {
        return {};
    }
    int x = canvas.x + margin;
    int y = canvas.y + canvas.dy - barDy - margin;
    if (y < canvas.y + margin) {
        y = canvas.y + margin;
    }
    return Rect{x, y, barDx, barDy};
}

// client area to client area
Point ToolWindowOffsetInOwner(ToolWindow* tw) {
    gp::Window* ownerGw = IsLive(tw) ? OwnerGpui(tw->owner) : nullptr;
    if (!ownerGw || !tw->gw) {
        return {};
    }
    gp::ScreenBounds mine = gp::WindowContentBounds(tw->gw);
    gp::ScreenBounds owner = gp::WindowContentBounds(ownerGw);
    if (mine.IsEmpty() || owner.IsEmpty()) {
        return {};
    }
    int dpi = OwnerDpi(tw->owner);
    return Point{MulDiv(mine.x - owner.x, 96, dpi), MulDiv(mine.y - owner.y, 96, dpi)};
}

// orig's DarkModeApplyToTitleBar: the caption follows the theme, not the system
static void SyncCaptionTheme(ToolWindow* tw) {
    int dark = IsLightColor(ThemeWindowBackgroundColor()) ? 0 : 1;
    if (!tw->gw || dark == tw->darkCaption) {
        return;
    }
    tw->darkCaption = dark;
    gp::WindowSetFrameAppearance(tw->gw, dark ? gp::FrameAppearance::Dark : gp::FrameAppearance::Light);
}

static void RunOnActivate(ToolWindow* tw, bool active) {
    if (IsLive(tw) && tw->desc.onActivate && IsMainWindowValidAndNotClosing(tw->owner)) {
        tw->desc.onActivate(tw->owner, active);
    }
}

static void RunOnActivated(ToolWindow* tw) {
    RunOnActivate(tw, true);
}

static void RunOnDeactivated(ToolWindow* tw) {
    RunOnActivate(tw, false);
}

// orig's WM_ACTIVATE. The listeners run their own code; not from inside the
// platform's notification
static void ToolOnActivation(void* data, gp::Window*, bool active) {
    auto* tw = (ToolWindow*)data;
    if (IsLive(tw) && tw->desc.onActivate) {
        uitask::Post(MkFunc0(active ? RunOnActivated : RunOnDeactivated, tw), "ToolWindowActivate");
    }
}

// orig's WM_WINDOWPOSCHANGED and WM_EXITSIZEMOVE
static void ToolOnBounds(void* data, gp::Window* gw, gp::WindowBoundsEvent ev) {
    auto* tw = (ToolWindow*)data;
    if (!IsLive(tw) || tw->gw != gw || !IsMainWindowValid(tw->owner)) {
        return;
    }
    if (ev == gp::WindowBoundsEvent::MoveResizeEnded) {
        if (tw->desc.onExitSizeMove) {
            tw->desc.onExitSizeMove(tw->owner, ToolWindowRect(tw));
        }
        return;
    }
    if (gp::AppIsMinimized(gw) || gp::AppIsMaximized(gw) || !gp::WindowIsVisible(gw)) {
        return;
    }
    Rect now = FromGpui(gp::WindowFrameBounds(gw));
    if (now.IsEmpty()) {
        return;
    }
    tw->outer = now;
    if (tw->desc.onMoved) {
        tw->desc.onMoved(tw->owner, now);
    }
}

// the close box, Alt+F4, AppQuit, the owner going: whichever way it went
static void OnGpuiWindowClosed(void*, gp::Window* gw) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->gw == gw) {
            Forget(tw);
            return;
        }
    }
}

static void CreateNow(ToolWindow* tw) {
    if (!IsLive(tw) || tw->gw) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(tw->owner)) {
        Forget(tw);
        return;
    }
    gp::App* app = AppShellGetApp();
    static bool observing = false;
    if (!observing) {
        observing = true;
        gp::AppOnWindowClosed(app, OnGpuiWindowClosed, nullptr);
    }
    tw->view = gp::EntityNew<ToolRootView>(app);
    auto* view = (ToolRootView*)gp::EntityGet(app, tw->view.id);
    view->tw = tw;
    str::ReplaceWithCopy(&tw->title, tw->desc.title ? tw->desc.title() : Str{});

    // made hidden, so the observers hear its first activation and placement
    gp::WinOpts opts = ToolOpts(tw->desc, tw->owner);
    opts.bounds = ToGpuiScreen(tw->outer);
    opts.show = false;
    gp::Window* gw = gp::WindowOpenView(app, ToGpui(tw->title), 320, 240, tw->view.id, opts);
    if (!gw) {
        logf("ToolWindow: could not create '%s'\n", Str(tw->desc.name));
        if (IsLive(tw)) {
            Forget(tw);
        }
        return;
    }
    tw->gw = gw;
#if OS_WIN
    // a command id sent at the tool window runs on its owner, like orig
    AppShellClaimToolHwnd((HWND)gp::PlatWindowHandle(gw));
#endif
    SyncCaptionTheme(tw);
    gp::WindowOnBoundsChanged(gw, ToolOnBounds, tw);
    gp::WindowOnActivationChanged(gw, ToolOnActivation, tw);
    if (tw->desc.onTick) {
        gp::WindowSetInterval(gw, tw->desc.tickMs, gp::ListenTo(tw->view, &ToolRootView::OnTick));
    }
    if (!tw->hidden) {
        gp::WindowSetVisible(gw, true, tw->desc.activate == ToolWinActivate::Yes);
    }
    logf("ToolWindow: '%s' at %d,%d %dx%d\n", Str(tw->desc.name), tw->outer.x, tw->outer.y, tw->outer.dx, tw->outer.dy);
    gp::AppInvalidate(gw);
}

static void DestroyNow(ToolWindow* tw) {
    if (!IsLive(tw)) {
        return;
    }
    MainWindow* owner = tw->owner;
    bool wasActive = tw->gw && gp::WindowIsActive(tw->gw);
    if (tw->gw) {
        // OnGpuiWindowClosed forgets it
        gp::AppQuit(tw->gw);
    }
    if (IsLive(tw)) {
        Forget(tw);
    }
    // Windows activates the owner of a closed active window; macOS could
    // leave no window with the keys (after the ⌃Tab switcher)
    gp::Window* ownerGw = OwnerGpui(owner);
    if (wasActive && ownerGw && IsMainWindowValidAndNotClosing(owner)) {
        gp::WindowActivateInApp(ownerGw);
    }
}

static void SyncTitle(ToolWindow* tw) {
    Str title = tw->desc.title ? tw->desc.title() : Str{};
    if (str::Eq(title, tw->title)) {
        return;
    }
    str::ReplaceWithCopy(&tw->title, title);
    if (tw->gw) {
        gp::AppSetTitle(tw->gw, ToGpui(tw->title));
    }
}

// gpui's size for the text of an input or a button
constexpr float kGpuiUiFontPx = 14;

float ToolWindowSetUiFontPx(gp::Ctx* cx, float px) {
    float scale = kGpuiUiFontPx / px;
    gp::WindowSetRemSize(cx->win, 16.f / scale);
    return scale;
}

void ToolWindowSetMinClient(ToolWindow* tw, Size minClientDip) {
    if (!IsLive(tw)) {
        return;
    }
    tw->desc.minClient = minClientDip;
    if (tw->gw) {
        gp::WindowSetMinSize(tw->gw, minClientDip.IsEmpty() ? gp::Size{} : ToGpui(minClientDip));
    }
}

void ToolWindowSetMaxClient(ToolWindow* tw, Size maxClientDip) {
    if (!IsLive(tw)) {
        return;
    }
    tw->desc.maxClient = maxClientDip;
    if (tw->gw) {
        gp::WindowSetMaxSize(tw->gw, ToGpui(maxClientDip));
    }
}

void ToolWindowSetClientSize(ToolWindow* tw, Size clientDip) {
    if (!IsLive(tw) || !IsMainWindowValid(tw->owner)) {
        return;
    }
    Size sz = ToolWindowOuterSize(tw->desc, tw->owner, clientDip);
    Rect r = ToolWindowRect(tw);
    ToolWindowMove(tw, Rect(r.x, r.y, sz.dx, sz.dy));
}

ToolWindow* ToolWindowOpen(const ToolWindowDesc& desc, MainWindow* owner, Rect outer) {
    if (!ToolWindowsAvailable() || !desc.build || !IsMainWindowValidAndNotClosing(owner)) {
        return nullptr;
    }
    static int nextTestId = 1000;
    auto* tw = new ToolWindow();
    tw->testId = nextTestId++;
    tw->desc = desc;
    tw->owner = owner;
    tw->outer = outer;
    VecAppend(gToolWindows, tw);
    uitask::Post(MkFunc0(CreateNow, tw), "ToolWindowCreate");
    return tw;
}

void ToolWindowClose(ToolWindow* tw) {
    if (!IsLive(tw) || tw->closing) {
        return;
    }
    tw->closing = true;
    uitask::Post(MkFunc0(DestroyNow, tw), "ToolWindowDestroy");
}

void ToolWindowSetOwner(ToolWindow* tw, MainWindow* owner) {
    if (!IsLive(tw) || tw->owner == owner) {
        return;
    }
    tw->owner = owner;
    if (tw->gw && OwnerGpui(owner)) {
        // gpui applies it to owned kinds only; a top-level one keeps none
        gp::WindowSetParent(tw->gw, OwnerGpui(owner));
    }
    ToolWindowInvalidate(tw);
}

static MainWindow* OtherMainWindow(MainWindow* win) {
    for (MainWindow* w : gWindows) {
        if (w != win && IsMainWindowValidAndNotClosing(w)) {
            return w;
        }
    }
    return nullptr;
}

// the main window is going: its tool windows go first, and say so. One that
// outlives its owner (onOwnerClosed) moves to another main window instead
void ToolWindowsCloseFor(MainWindow* win) {
    Vec<ToolWindow*> wins = gToolWindows;
    for (ToolWindow* tw : wins) {
        if (tw->owner != win) {
            continue;
        }
        MainWindow* other = tw->desc.onOwnerClosed && !tw->closing ? OtherMainWindow(win) : nullptr;
        if (!other) {
            // onClosed now, while the owner exists: FinishClosed is posted and
            // runs after the owner is deleted, and dialogs repaint their owner
            // when they close. closing stops FinishClosed calling it again
            void (*onClosed)(MainWindow*) = tw->closing ? nullptr : tw->desc.onClosed;
            DestroyNow(tw);
            if (onClosed) {
                tw->closing = true;
                onClosed(win);
            }
            continue;
        }
        ToolWindowSetOwner(tw, other);
        tw->desc.onOwnerClosed(other);
    }
}

gp::El* ToolRootView::Render(ToolRootView* self, gp::Ctx* cx) {
    // as the shell does: a tool window is one more frame of the same thread
    ResetTempArena();
    DlgAccelBeginFrame(cx->win);
    ToolWindow* tw = self->tw;
    gp::El* root = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(ToGpui(ThemeWindowControlBackgroundColor()));
    if (!IsLive(tw) || tw->closing || !IsMainWindowValid(tw->owner)) {
        return root;
    }
    SyncTitle(tw);
    SyncCaptionTheme(tw);
    root->OnKeyDown(gp::Listen(cx, &ToolRootView::OnKeyDown));
    root->CaptureKeyDown(gp::Listen(cx, &ToolRootView::OnCaptureKey));
    if (tw->desc.onKeyUp) {
        root->OnKeyUp(gp::Listen(cx, &ToolRootView::OnKeyUp));
    }
    if (tw->desc.onDropFiles) {
        root->OnDrop(GStrL("ExternalPaths"), gp::Listen(cx, &ToolRootView::OnDropFiles));
    }
    if (gp::El* el = tw->desc.build(tw->owner, cx)) {
        root->Child(el);
    }
    // ng: with nothing focused gpui calls a key listener only when a listener
    // is registered below it (DispatchKeyChain wants the anchor inside the
    // listener's subtree), so content without key listeners of its own (the
    // manual: a button row and a webview) would leave the root deaf
    root->Child(gp::Div(cx->a)->Absolute()->W(0)->H(0)->OnKeyDown(gp::Listen(cx, &ToolRootView::OnNoKey)));
    // the dropped list of an editable combo box in this window, over the rest
    if (gp::El* comboList = DialogComboListBuild(cx)) {
        root->Child(comboList);
    }
    // a message box raised while this window was the active one
    if (gp::El* msgBox = MsgBoxBuildInToolWindow(tw, cx)) {
        root->Child(msgBox);
    }
    return root;
}

// orig: the window is an OLE drop target of its own (MergeDropTarget)
void ToolRootView::OnDropFiles(ToolRootView* self, gp::Ctx*, const gp::DropEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !tw->desc.onDropFiles || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    constexpr int kMaxDropped = 64;
    Str paths[kMaxDropped];
    int n = SplitDroppedPaths(FromGpui(ev->externalPaths), paths, kMaxDropped);
    if (n > 0) {
        tw->desc.onDropFiles(tw->owner, paths, n, PointF{ev->x, ev->y});
    }
}

void ToolRootView::OnKeyDown(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    // as in the frame: Esc closes an open popup or dropped list, nothing else
    bool handled = ev->vk == VK_ESCAPE && DismissTrackedPopup(cx);
    if (!handled && tw->desc.onKey) {
        handled = tw->desc.onKey(tw->owner, cx, ev);
    }
    if (!handled) {
        // orig's IsDialogMessage: Alt + letter, and the bare letter while no
        // text field has the focus
        bool editFocused = cx->win->input && cx->win->input->focused;
        handled = DlgAccelOnKey(cx, ev->vk, ev->alt, ev->ctrl, editFocused);
    }
    if (!handled) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
}

// TestLayout for a tool window: what its last frame drew, in dips of its
// client area. gpui keeps the selectable texts, the accessibility nodes (a
// button with its label, a field with its value), the click targets and the
// scrolled boxes of a frame; those are what a test needs to find a row or a
// button
static TempStr GpuiLayoutTemp(gp::Window* gw, Str name, Rect r) {
    gp::WinSize ws = gp::WindowSize(gw);
    str::Builder out;
    out.Append(fmt("OK name=%s rect=%d,%d,%d,%d client=%d,%d rem=%.2f\n", name, r.x, r.y, r.dx, r.dy, (int)ws.dipW,
                   (int)ws.dipH, gp::WindowRemSize(gw)));
    const gp::PaintCtx& paint = gw->paint;
    for (const gp::TextHit& t : paint.texts) {
        const gp::Bounds& b = t.bounds;
        out.Append(fmt("text rect=%.0f,%.0f,%.0f,%.0f font=%.1f '%s'\n", b.x, b.y, b.w, b.h, t.font,
                       Str{(char*)t.text.s, (int)t.text.len}));
    }
    for (const gp::AccessibilityNode& n : gw->accessibility) {
        const gp::AccessibilityInfo& info = n.info;
        if (info.role == gp::AccessibilityRole::None && info.label.len == 0 && info.value.len == 0) {
            continue;
        }
        const gp::Bounds& b = n.bounds;
        out.Append(fmt("node rect=%.0f,%.0f,%.0f,%.0f role=%d label='%s' value='%s'", b.x, b.y, b.w, b.h,
                       (int)info.role, Str{(char*)info.label.s, (int)info.label.len},
                       Str{(char*)info.value.s, (int)info.value.len}));
        if (info.disabled) {
            out.Append(StrL(" disabled=1"));
        }
        if (info.toggled == gp::AccessibilityToggled::True) {
            out.Append(StrL(" checked=1"));
        }
        out.Append(StrL("\n"));
    }
    for (const gp::HitRect& h : paint.hits) {
        bool click = h.onClick.IsValid() || h.listener.IsValid() || h.clickAction != 0;
        if (!click && !h.input && !h.onMouseDown.IsValid()) {
            continue;
        }
        const gp::Bounds& b = h.bounds;
        out.Append(fmt("hit rect=%.0f,%.0f,%.0f,%.0f click=%d input=%d focusId=%d\n", b.x, b.y, b.w, b.h, click ? 1 : 0,
                       h.input ? 1 : 0, h.focusId));
    }
    for (const gp::ScrollRect& s : paint.scrolls) {
        const gp::Bounds& b = s.bounds;
        out.Append(fmt("scroll rect=%.0f,%.0f,%.0f,%.0f content=%.0f,%.0f offset=%.0f,%.0f\n", b.x, b.y, b.w, b.h,
                       s.contentW, s.contentH, s.scrollX, s.scrollY));
    }
    return ToStrTemp(out);
}

static TempStr ToolWindowLayoutTemp(ToolWindow* tw) {
    return GpuiLayoutTemp(tw->gw, Str(tw->desc.name), ToolWindowRect(tw));
}

void ToolRootView::OnTick(ToolRootView* self, gp::Ctx* cx, const gp::TickEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !tw->desc.onTick || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    tw->desc.onTick(tw->owner, cx, ev->ms);
}

void ToolRootView::OnKeyUp(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !tw->desc.onKeyUp || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    tw->desc.onKeyUp(tw->owner, cx, ev);
}

void ToolRootView::OnCaptureKey(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    // a message box in this window is modal to it
    bool handled = MsgBoxOnKeyInToolWindow(tw, ev->vk, ev->ctrl);
    if (!handled && tw->desc.onCaptureKey) {
        handled = tw->desc.onCaptureKey(tw->owner, cx, ev);
    }
    if (!handled) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
}

// TestWindows' lines for the tool windows that are made
TempStr ToolWindowsTestListTemp(int pid) {
    str::Builder out;
    for (ToolWindow* tw : gToolWindows) {
        if (!tw->gw || tw->closing) {
            continue;
        }
        Rect r = ToolWindowRect(tw);
        int owner = tw->owner ? tw->owner->testId : 0;
        out.Append(fmt("hwnd=%d class=GpuiSystemMonitor pid=%d visible=%d rect=%d,%d,%d,%d owner=%d name=%s title=%s\n",
                       tw->testId, pid, ToolWindowIsVisible(tw) ? 1 : 0, r.x, r.y, r.dx, r.dy, owner,
                       Str(tw->desc.name), tw->title));
    }
    return ToStrTemp(out);
}

TempStr ToolWindowTestTemp(Str action, Str name, Str kind, int a, int b, int c, int d) {
    if (str::Eq(action, StrL("on")) || str::Eq(action, StrL("off"))) {
        gToolWindowsOff = str::Eq(action, StrL("off"));
        return fmt("OK available=%d", ToolWindowsAvailable() ? 1 : 0);
    }
    if (str::Eq(action, StrL("list"))) {
        str::Builder out;
        out.Append(fmt("OK available=%d count=%d", ToolWindowsAvailable() ? 1 : 0, len(gToolWindows)));
        for (ToolWindow* tw : gToolWindows) {
            Rect r = ToolWindowRect(tw);
            out.Append(fmt("\n%s made=%d rect=%d,%d,%d,%d", Str(tw->desc.name), tw->gw ? 1 : 0, r.x, r.y, r.dx, r.dy));
#if !OS_WIN
            out.Append(fmt(" title='%s'", tw->title));
            // the ids TestWindows gives windows here, for tests that wait for a dialog
            if (tw->gw) {
                int owner = tw->owner ? tw->owner->testId : 0;
                gp::Window* mainGw = OwnerGpui(tw->owner);
                out.Append(fmt(" hwnd=%d owner=%d visible=%d active=%d", tw->testId, owner,
                               ToolWindowIsVisible(tw) ? 1 : 0, ToolWindowIsActive(tw) ? 1 : 0));
                out.Append(fmt(" mainEnabled=%d", mainGw && gp::WindowIsEnabled(mainGw) ? 1 : 0));
            }
#else
            if (tw->gw) {
                gp::Window* gw = tw->gw;
                gp::ScreenBounds cr = gp::WindowContentBounds(gw);
                bool owned = gw->opts.kind != gp::WindowKind::Normal && gw->opts.parent;
                void* owner = owned ? gp::PlatWindowHandle(gw->opts.parent) : nullptr;
                gp::Window* mainGw = OwnerGpui(tw->owner);
                out.Append(fmt(" hwnd=%d client=%d,%d owner=%d visible=%d active=%d title='%s'",
                               (int)(INT_PTR)gp::PlatWindowHandle(gw), cr.w, cr.h, (int)(INT_PTR)owner,
                               gp::WindowIsVisible(gw) ? 1 : 0, ToolWindowIsActive(tw) ? 1 : 0, tw->title));
                out.Append(fmt(" mainEnabled=%d", mainGw && gp::WindowIsEnabled(mainGw) ? 1 : 0));
            }
#endif
        }
        return ToStrTemp(out);
    }
    // the same dump of the first main window, for what is still in the frame
    if (str::Eq(action, StrL("layout")) && str::Eq(name, StrL("frame"))) {
        MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
        if (!win || !win->gpuiWin) {
            return StrL("NOTREADY");
        }
        return GpuiLayoutTemp(win->gpuiWin, name, AppShellWindowScreenRect(win));
    }
    ToolWindow* tw = ToolWindowFind(name);
    if (!tw) {
        return StrL("ERR no-such-window");
    }
    if (str::Eq(action, StrL("close"))) {
        // as the close box does: the owner of the content hears about it
        DestroyNow(tw);
        return StrL("OK");
    }
    if (!tw->gw) {
        return StrL("NOTREADY");
    }
    // a, b: the new top left corner in screen pixels
    if (str::Eq(action, StrL("move"))) {
        Rect r = ToolWindowRect(tw);
        ToolWindowMove(tw, Rect(a, b, r.dx, r.dy));
        return StrL("OK");
    }
    if (str::Eq(action, StrL("input"))) {
#if OS_DARWIN
        if (str::Eq(kind, StrL("mackey"))) {
            gp::WindowSendNativeKey(tw->gw, a);
            return StrL("OK");
        }
#endif
        TempStr res = AppShellTestInputGpui(tw->gw, kind, a, b, c, d);
        ToolWindowInvalidate(tw);
        return res;
    }
    if (str::Eq(action, StrL("layout"))) {
        return ToolWindowLayoutTemp(tw);
    }
    if (str::Eq(action, StrL("state"))) {
        gp::Window* gw = tw->gw;
        bool edit = gw->input && gw->input->focused;
        str::Builder out;
        gp::WinSize ws = gp::WindowSize(gw);
        out.Append(
            fmt("OK size=%d,%d focusId=%d edit=%d", (int)ws.dipW, (int)ws.dipH, gp::WindowFocusedId(gw), edit ? 1 : 0));
        if (edit) {
            gp::InputState* in = gw->input;
            gp::Str v = gp::InputValue(in);
            out.Append(fmt(" editText='%s' caret=%d sel=%d,%d", Str{(char*)v.s, (int)v.len}, gp::InputCursor(in),
                           (int)in->selectedRange.start, (int)in->selectedRange.end));
        }
        out.Append(fmt(" popup=%d active=%d", IsTrackedPopupOpenInApp(gw->app) ? 1 : 0, gw->active ? 1 : 0));
        return ToStrTemp(out);
    }
    return StrL("ERR action");
}
