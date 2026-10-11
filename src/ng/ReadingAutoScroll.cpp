/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "FindBar.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "ReadingBar.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "ReadingAutoScroll.h"
#include "ReadingAutoScrollCommon.h"

#include "SumatraLog.h"

// Hands-free continuous pan (Acrobat / Foxit Automatically Scroll): a stored
// speed, not cursor offset. Middle-click auto-scroll is a separate mode.
// ng: orig's control bar is a layered WS_POPUP window it keeps aligned with
// the canvas; here it is an element the canvas places at its own bottom edge,
// so UpdateLayout / onWindowMoved / the lastX..lastDy cache are all gone.

struct ReadingAutoScrollView;

struct ReadingAutoScrollBar {
    gp::Entity<ReadingAutoScrollView> view;
    gp::SliderState speedSlider;
    // the slider reports a float; remember what we last pushed into it so a
    // speed change from a key doesn't fight a drag
    int sliderIdx = -1;
    // orig's popup over the bottom of the canvas, where the platform can have
    // one (gui/ToolWindow.h); null: an element of the canvas
    ToolWindow* tw = nullptr;
    // the row in that window, as of its last frame
    gp::Bounds rowBounds{};
};

void StopMiddleClickScroll(MainWindow* win) {
    if (!win || win->mouseAction != MouseAction::Scrolling) {
        return;
    }
    win->mouseAction = MouseAction::None;
    win->xScrollSpeed = 0;
    win->yScrollSpeed = 0;
    win->xScrollAccum = 0;
    win->yScrollAccum = 0;
    CanvasSetCursor(win, (int)gp::CursorKind::Arrow);
}

// ng: orig arms / kills a 10 ms WM_TIMER; the shell ticks unconditionally, so
// "armed" is just the per-tab state the tick reads. What is left of the two is
// resetting the time base and the sub-pixel accumulator.
void ArmReadingTimer(MainWindow*, WindowTab* tab) {
    if (!tab) {
        return;
    }
    tab->autoScroll.lastQpc = 0;
    tab->autoScroll.accum = 0;
}

void KillReadingTimer(MainWindow*) {}

// Seconds since the previous tick of this run, 0 on its first: the shell's
// tick hands us the elapsed time. lastQpc == 0 marks the first tick.
float ReadingTickSeconds(WindowTab* tab, int elapsedMs) {
    float dt = 0;
    if (tab->autoScroll.lastQpc != 0) {
        dt = (float)elapsedMs / 1000.0f;
    }
    tab->autoScroll.lastQpc = 1;
    return dt;
}

// --- the control bar --------------------------------------------------------

struct ReadingAutoScrollView {
    MainWindow* win = nullptr;

    static void OnCmd(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnSpeed(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
    static void OnSpeedMove(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnSpeedHover(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
};

// the bar's buttons; Focus is orig's, it toggles the reading bar
constexpr int kBarCmdPause = 1;
constexpr int kBarCmdStop = 2;
constexpr int kBarCmdReverse = 3;
constexpr int kBarCmdFocus = 4;

void ReadingAutoScrollView::OnCmd(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    switch ((int)cmdId) {
        case kBarCmdPause:
            ReadingAutoScrollPause(win);
            break;
        case kBarCmdStop:
            ReadingAutoScrollStop(win);
            break;
        case kBarCmdReverse:
            ReadingAutoScrollReverse(win);
            break;
        case kBarCmdFocus:
            ReadingBarToggle(win);
            break;
    }
    gp::Notify(cx);
}

void ReadingAutoScrollView::OnSpeed(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::SliderEvent*) {
    MainWindow* win = self->win;
    ReadingAutoScrollBar* bar = IsMainWindowValidAndNotClosing(win) ? win->readingAutoScrollBar : nullptr;
    if (!bar) {
        return;
    }
    int idx = limitValue((int)(bar->speedSlider.value.End() + 0.5f), 0, SpeedCount() - 1);
    bar->sliderIdx = idx;
    SetSpeed(kSpeeds[idx]);
    gp::Notify(cx);
}

// the bar's own window, when the event is in it
static ToolWindow* BarWindowOf(MainWindow* win, gp::Ctx* cx) {
    ReadingAutoScrollBar* bar = IsMainWindowValidAndNotClosing(win) ? win->readingAutoScrollBar : nullptr;
    if (!bar || !bar->tw || ToolWindowGpui(bar->tw) != cx->win) {
        return nullptr;
    }
    return bar->tw;
}

void ReadingAutoScrollView::OnSpeedMove(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    float pct = ev->el.w > 0 ? (ev->x - ev->el.x) / ev->el.w : 0;
    int idx = limitValue((int)(pct * (float)(SpeedCount() - 1) + 0.5f), 0, SpeedCount() - 1);
    TempStr tip = fmt("%d px/s", (int)lroundf(kSpeeds[idx]));
    ToolWindow* tw = BarWindowOf(self->win, cx);
    if (!tw) {
        HoverTooltipShow(cx, tip, ev->el);
        return;
    }
    // the bar's window is as high as the bar: the tooltip is the frame's
    Point off = ToolWindowOffsetInOwner(tw);
    gp::Bounds at = ev->el;
    at.x += (float)off.x;
    at.y += (float)off.y;
    HoverTooltipShowIn(cx->app, self->win->gpuiWin, tip, at);
}

void ReadingAutoScrollView::OnSpeedHover(ReadingAutoScrollView* self, gp::Ctx* cx, const gp::HoverEvent* ev) {
    if (ev->hovered) {
        return;
    }
    if (BarWindowOf(self->win, cx)) {
        HoverTooltipHideIn(self->win->gpuiWin);
        return;
    }
    HoverTooltipHide(cx);
}

ReadingAutoScrollBar* BarEnsure(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    if (!win->readingAutoScrollBar) {
        auto* bar = new ReadingAutoScrollBar();
        bar->speedSlider =
            gp::SliderStateNew(0, (float)(SpeedCount() - 1), gp::SliderSingle((float)ClosestSpeedIdx(CurrentSpeed())));
        win->readingAutoScrollBar = bar;
    }
    return win->readingAutoScrollBar;
}

// --- orig's popup: the bar in a window of its own ---------------------------

// the row's height until its first frame says: a button, the padding, the edge
constexpr float kBarGuessDy = 38;

// the bar shows while the tab it scrolls is the current one
static bool BarIsShown(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    ReadingAutoScrollBar* bar = win ? win->readingAutoScrollBar : nullptr;
    return tab && bar && SessionTab(win) == tab;
}

// orig's ReadingAutoScrollBar::UpdateLayout
static Rect BarPlace(MainWindow* win, ToolWindow*) {
    if (!BarIsShown(win)) {
        return {};
    }
    ReadingAutoScrollBar* bar = win->readingAutoScrollBar;
    float dy = bar->rowBounds.h > 0 ? bar->rowBounds.h : kBarGuessDy;
    return ToolWindowDockedBarRect(win, kBarMargin, dy);
}

static gp::El* BarRowBuild(MainWindow* win, gp::Ctx* cx, bool ownWindow);

static gp::El* BarToolBuild(MainWindow* win, gp::Ctx* cx) {
    return BarRowBuild(win, cx, true);
}

static void BarToolOnClosed(MainWindow* win) {
    ReadingAutoScrollBar* bar = win ? win->readingAutoScrollBar : nullptr;
    if (bar && bar->tw && !ToolWindowIsLive(bar->tw)) {
        bar->tw = nullptr;
    }
}

// the window is there while a tab of this main window scrolls
static void BarSyncWindow(MainWindow* win) {
    ReadingAutoScrollBar* bar = win ? win->readingAutoScrollBar : nullptr;
    if (!bar) {
        return;
    }
    bool want = SessionTab(win) != nullptr && ToolWindowsAvailable();
    if (!want) {
        if (bar->tw) {
            ToolWindowClose(bar->tw);
            bar->tw = nullptr;
        }
        return;
    }
    if (bar->tw && ToolWindowIsLive(bar->tw)) {
        ToolWindowInvalidate(bar->tw);
        return;
    }
    Rect r = BarPlace(win, nullptr);
    if (r.IsEmpty()) {
        return;
    }
    ToolWindowDesc desc = ToolWindowDockedDesc("autoscrollbar");
    desc.build = BarToolBuild;
    desc.place = BarPlace;
    desc.onClosed = BarToolOnClosed;
    bar->tw = ToolWindowOpen(desc, win, r);
}

void BarHide(MainWindow* win) {
    if (win && win->readingAutoScrollBar) {
        win->readingAutoScrollTab = nullptr;
    }
    BarSyncWindow(win);
    AppShellInvalidate(win);
}

void BarUpdate(MainWindow* win, bool) {
    BarSyncWindow(win);
    AppShellInvalidate(win);
}

void ReadingAutoScrollDestroy(MainWindow* win) {
    if (win && win->readingAutoScrollBar && win->readingAutoScrollBar->tw) {
        ToolWindowClose(win->readingAutoScrollBar->tw);
    }
    if (!win || !win->readingAutoScrollBar) {
        return;
    }
    delete win->readingAutoScrollBar;
    win->readingAutoScrollBar = nullptr;
    win->readingAutoScrollTab = nullptr;
}

static gp::El* BarButton(ReadingAutoScrollBar* bar, gp::Ctx* cx, Str id, Str label, int cmdId) {
    return gpc::Button::New(cx, GpuiDup(cx->a, id))
        ->WithSize(gp::UiSize::Small)
        ->Label(GpuiDup(cx->a, label))
        ->OnClick(gp::ListenTo(bar->view, &ReadingAutoScrollView::OnCmd, (intptr_t)cmdId))
        ->IntoEl();
}

// the bar in the canvas; in a window of its own it is not the frame's
gp::El* ReadingAutoScrollBarBuild(MainWindow* win, gp::Ctx* cx) {
    ReadingAutoScrollBar* bar = win ? win->readingAutoScrollBar : nullptr;
    if (!bar || bar->tw) {
        return nullptr;
    }
    return BarRowBuild(win, cx, false);
}

static gp::El* BarRowBuild(MainWindow* win, gp::Ctx* cx, bool ownWindow) {
    WindowTab* tab = ActiveTab(win);
    ReadingAutoScrollBar* bar = win ? win->readingAutoScrollBar : nullptr;
    if (!tab || !bar || SessionTab(win) != tab) {
        return nullptr;
    }
    if (!bar->view.IsValid()) {
        bar->view = gp::EntityNewState<ReadingAutoScrollView>(cx->app);
    }
    auto* view = (ReadingAutoScrollView*)gp::EntityGet(cx->app, bar->view.id);
    view->win = win;

    // a speed set from a key / command has to reach the slider, but not while
    // the user is dragging it (orig's VirtSlider::IsAdjusting)
    if (!bar->speedSlider.dragging) {
        int idx = ClosestSpeedIdx(CurrentSpeed());
        if (idx != bar->sliderIdx) {
            bar->sliderIdx = idx;
            gp::SliderSetValue(&bar->speedSlider, gp::SliderSingle((float)idx));
        }
    }

    bool paused = tab->autoScroll.paused || tab->autoScroll.atEnd;
    float margin = (float)DpiScale(kBarMargin);
    float gap = (float)DpiScale(kBtnGap);
    const gp::Theme& th = gp::ThemeNow(cx->app);

    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->Gap(gap)
                      ->PadX((float)DpiScale(kBarPadX))
                      ->PadY((float)DpiScale(kBarPadY))
                      ->Bg(ToGpui(ThemeNotificationsBackgroundColor()))
                      ->Border(1, ToGpui(kColGray));
    if (ownWindow) {
        // the window is the bar: orig's popup, as wide as the canvas less the
        // margins
        row->W(gp::kFill)->Shrink0()->BoundsOut(&bar->rowBounds);
    } else {
        row->Absolute()->Left(margin)->Right(margin)->Bottom(margin);
    }
    row->Child(BarButton(bar, cx, StrL("ras-pause"), paused ? Tr("Resume") : Tr("Pause"), kBarCmdPause));
    row->Child(BarButton(bar, cx, StrL("ras-stop"), Tr("Stop"), kBarCmdStop));
    row->Child(BarButton(bar, cx, StrL("ras-reverse"), Tr("Reverse"), kBarCmdReverse));
    row->Child(BarButton(bar, cx, StrL("ras-focus"), Tr("Focus"), kBarCmdFocus));
    // ng: the listener goes to the component; Slider::IntoEl() replaces the
    // one set on the state
    row->Child(gpc::Slider::New(cx, GStrL("ras-speed"), &bar->speedSlider)
                   ->OnChange(gp::ListenTo(bar->view, &ReadingAutoScrollView::OnSpeed))
                   ->W(160)
                   ->IntoEl()
                   ->OnMouseMove(gp::ListenTo(bar->view, &ReadingAutoScrollView::OnSpeedMove))
                   ->OnHover(gp::ListenTo(bar->view, &ReadingAutoScrollView::OnSpeedHover)));
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, SpeedLabelTemp(tab)))->Font(13)->Fg(th.foreground)->Shrink0());
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, StatusTextTemp(tab)))->Font(13)->Fg(th.foreground)->Shrink0());
    Str help =
        Tr("\xE2\x86\x91\xE2\x86\x93 speed \xC2\xB7 0-9 \xC2\xB7 \xE2\x88\x92 reverse \xC2\xB7 Space pause "
           "\xC2\xB7 Esc stop");
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, help))->Font(13)->Fg(ToGpui(ThemeWindowDarkerTextColor()))));
    return row;
}

TempStr ReadingAutoScrollBarStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    MainWindow* win = gWindows[0];
    WindowTab* tab = ActiveTab(win);
    DisplayModel* dm = tab ? tab->AsFixed() : win->AsFixed();
    int scrollY = dm ? dm->viewPort.y : -1;
    int page = dm ? dm->CurrentPageNo() : 0;
    int pages = dm ? dm->PageCount() : 0;
    ReadingAutoScrollBar* bar = win->readingAutoScrollBar;
    bool on = tab != nullptr;
    if (!on || !bar || SessionTab(win) != tab) {
        out.Append(fmt("NOTREADY no-bar on=%d scrollY=%d\n", (int)on, scrollY));
        return finish(2);
    }
    // ng draws the bar in the frame. hwnd is that frame on Windows, 0 elsewhere.
    int hwnd = 0;
#if OS_WIN
    hwnd = (int)(uintptr_t)MainWindowHwnd(win);
#endif
    out.Append(fmt("OK visible=1 paused=%d atEnd=%d dir=%d speed=%d scrollY=%d page=%d pages=%d hwnd=%d\n",
                   (int)tab->autoScroll.paused, (int)tab->autoScroll.atEnd, tab->autoScroll.dir,
                   (int)(CurrentSpeed() + 0.5f), scrollY, page, pages, hwnd));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", ClosestSpeedIdx(CurrentSpeed()), SpeedCount(),
                   SpeedLabelTemp(tab)));
    out.Append(fmt("status=%s\n", StatusTextTemp(tab)));
    return finish(0);
}
