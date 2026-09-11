/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

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
#include "Canvas.h"
#include "SumatraPDF.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "SumatraLog.h"

// Hands-free continuous pan (Acrobat / Foxit Automatically Scroll): a stored
// speed, not cursor offset. Middle-click auto-scroll is a separate mode.

struct ReadingAutoScrollBar : WindowBase {
    ReadingAutoScrollBar() = default;
    ~ReadingAutoScrollBar() override = default;

    HWND Create(HWND parentCanvas);
    void BuildLayout();
    void SyncLabels();
    void SyncColors();
    void UpdateLayout(bool forceLayout = false);
    void OnPaint(WindowBase::PaintEvent* ev);

    WindowTab* sessionTab = nullptr;
    HWND hwndCanvas = nullptr;
    VirtButton* btnPause = nullptr;
    VirtButton* btnStop = nullptr;
    VirtButton* btnReverse = nullptr;
    VirtButton* btnFocus = nullptr;
    VirtSlider* speedSlider = nullptr;
    VirtText* speedLabel = nullptr;
    VirtText* status = nullptr;
    VirtText* help = nullptr;
    int lastX = 0;
    int lastY = 0;
    int lastDx = 0;
    int lastDy = 0;
    Func1List<MainWindow*> onWindowMoved;
};

constexpr int kBarMargin = 8;
constexpr int kBarPadX = 12;
constexpr int kBarPadY = 6;
constexpr int kBtnGap = 8;
constexpr int kBtnPadX = 10;
constexpr int kBtnPadY = 3;

// Arrow keys step these. Finer than Acrobat's 0-9 so speed is tunable.
static const float kSpeeds[] = {8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 80, 96, 120, 160, 200, 260, 320};

// Acrobat maps 0 (slowest) .. 9 (fastest) onto these.
static const float kDigitSpeeds[] = {8, 12, 16, 24, 36, 48, 72, 108, 160, 240};

static int SpeedCount() {
    return dimofi(kSpeeds);
}

static float ClampSpeed(float s) {
    if (s < kSpeeds[0]) {
        return kSpeeds[0];
    }
    if (s > kSpeeds[SpeedCount() - 1]) {
        return kSpeeds[SpeedCount() - 1];
    }
    return s;
}

static float CurrentSpeed() {
    if (!gSettings) {
        return 40;
    }
    return ClampSpeed(gSettings->readingAutoScrollSpeed);
}

static int ClosestSpeedIdx(float s) {
    int best = 0;
    float bestD = fabsf(kSpeeds[0] - s);
    for (int i = 1; i < SpeedCount(); i++) {
        float d = fabsf(kSpeeds[i] - s);
        if (d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

static void SetSpeed(float s) {
    if (!gSettings) {
        return;
    }
    s = ClampSpeed(s);
    if (gSettings->readingAutoScrollSpeed == s) {
        return;
    }
    gSettings->readingAutoScrollSpeed = s;
    ScheduleSaveSettings();
}

static void StepSpeed(int dir) {
    int idx = ClosestSpeedIdx(CurrentSpeed()) + dir;
    idx = limitValue(idx, 0, SpeedCount() - 1);
    SetSpeed(kSpeeds[idx]);
}

static TempStr SpeedLabelTemp(WindowTab* tab) {
    const char* arrow = (!tab || tab->autoScroll.dir >= 0) ? "\xE2\x86\x93" : "\xE2\x86\x91";
    return fmt("%s %d px/s", Str(arrow), (int)(CurrentSpeed() + 0.5f));
}

static TempStr StatusTextTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }
    if (tab->autoScroll.atEnd) {
        return str::DupTemp(Tr("End of document"));
    }
    if (tab->autoScroll.paused) {
        return str::DupTemp(Tr("Paused"));
    }
    return str::DupTemp(Tr("Scrolling"));
}

static DisplayModel* ScrollModel(WindowTab* tab) {
    if (!tab) {
        return nullptr;
    }
    return tab->AsFixed();
}

static WindowTab* CurrentDocTab(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || tab->IsNonDocumentTab()) {
        return nullptr;
    }
    return tab;
}

static WindowTab* ActiveTab(MainWindow* win) {
    WindowTab* tab = CurrentDocTab(win);
    if (!tab || !tab->autoScroll.on) {
        return nullptr;
    }
    return tab;
}

static WindowTab* SessionTab(MainWindow* win) {
    if (!win || !win->readingAutoScrollBar) {
        return nullptr;
    }
    return win->readingAutoScrollBar->sessionTab;
}

static bool AtScrollLimit(DisplayModel* dm, int dir) {
    if (!dm) {
        return true;
    }
    if (dir > 0) {
        int maxY = std::max(0, dm->canvasSize.dy - dm->viewPort.dy);
        if (dm->viewPort.y < maxY) {
            return false;
        }
        if (IsContinuous(dm->GetDisplayMode())) {
            return true;
        }
        return dm->CurrentPageNo() >= dm->PageCount();
    }
    if (dm->viewPort.y > 0) {
        return false;
    }
    if (IsContinuous(dm->GetDisplayMode())) {
        return true;
    }
    return dm->CurrentPageNo() <= 1;
}

static void StopMiddleClickScroll(MainWindow* win) {
    if (!win || win->mouseAction != MouseAction::Scrolling) {
        return;
    }
    win->mouseAction = MouseAction::None;
    win->xScrollSpeed = 0;
    win->yScrollSpeed = 0;
    win->xScrollAccum = 0;
    win->yScrollAccum = 0;
    if (win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kAutoScrollTimerID);
        SetCursorCached(IDC_ARROW);
    }
}

static void KillReadingTimer(MainWindow* win) {
    if (win && win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kReadingAutoScrollTimerID);
    }
}

static void ArmReadingTimer(MainWindow* win, WindowTab* tab) {
    if (!win || !win->hwndCanvas || !tab) {
        return;
    }
    tab->autoScroll.lastQpc = TimeGet().QuadPart;
    tab->autoScroll.accum = 0;
    SetTimer(win->hwndCanvas, kReadingAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
}

static ReadingAutoScrollBar* BarEnsure(MainWindow* win);
static void BarHide(MainWindow* win);
static void BarUpdate(MainWindow* win, bool forceLayout = false);

static void ClearTabScroll(WindowTab* tab) {
    if (tab) {
        tab->autoScroll = {};
    }
}

bool ReadingAutoScrollIsOn(MainWindow* win) {
    return ActiveTab(win) != nullptr;
}

void ReadingAutoScrollHideBar(MainWindow* win) {
    KillReadingTimer(win);
    BarHide(win);
}

void ReadingAutoScrollStop(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    if (!tab) {
        ReadingAutoScrollHideBar(win);
        return;
    }
    ClearTabScroll(tab);
    ReadingAutoScrollHideBar(win);
}

void ReadingAutoScrollForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    bool wasSession = win && SessionTab(win) == tab;
    ClearTabScroll(tab);
    if (wasSession) {
        ReadingAutoScrollHideBar(win);
    }
}

static void ReadingAutoScrollStart(MainWindow* win) {
    WindowTab* tab = CurrentDocTab(win);
    if (!tab || !ScrollModel(tab)) {
        return;
    }
    StopMiddleClickScroll(win);
    tab->autoScroll.on = true;
    tab->autoScroll.paused = false;
    tab->autoScroll.atEnd = AtScrollLimit(ScrollModel(tab), tab->autoScroll.dir);
    tab->autoScroll.accum = 0;
    if (!tab->autoScroll.atEnd) {
        ArmReadingTimer(win, tab);
    } else {
        tab->autoScroll.paused = true;
        KillReadingTimer(win);
    }
    ReadingAutoScrollBar* bar = BarEnsure(win);
    if (bar) {
        bar->sessionTab = tab;
    }
    BarUpdate(win, true);
}

void ReadingAutoScrollSyncToTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    MainWindow* win = tab->win;
    if (tab->IsNonDocumentTab() || !tab->autoScroll.on || !ScrollModel(tab)) {
        ReadingAutoScrollHideBar(win);
        return;
    }
    if (!tab->autoScroll.paused && !tab->autoScroll.atEnd) {
        ArmReadingTimer(win, tab);
    } else {
        KillReadingTimer(win);
    }
    ReadingAutoScrollBar* bar = BarEnsure(win);
    if (bar) {
        bar->sessionTab = tab;
    }
    BarUpdate(win, true);
}

void ReadingAutoScrollToggle(MainWindow* win) {
    if (!win) {
        return;
    }
    if (ActiveTab(win)) {
        ReadingAutoScrollStop(win);
        return;
    }
    ReadingAutoScrollStart(win);
}

void ReadingAutoScrollPause(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    if (!tab) {
        return;
    }
    if (tab->autoScroll.atEnd && tab->autoScroll.paused) {
        return;
    }
    tab->autoScroll.paused = !tab->autoScroll.paused;
    if (tab->autoScroll.paused) {
        KillReadingTimer(win);
        tab->autoScroll.accum = 0;
    } else {
        tab->autoScroll.atEnd = AtScrollLimit(ScrollModel(tab), tab->autoScroll.dir);
        if (tab->autoScroll.atEnd) {
            tab->autoScroll.paused = true;
        } else {
            ArmReadingTimer(win, tab);
        }
    }
    BarUpdate(win, true);
}

void ReadingAutoScrollFaster(MainWindow* win) {
    if (!ActiveTab(win)) {
        return;
    }
    StepSpeed(1);
    BarUpdate(win);
}

void ReadingAutoScrollSlower(MainWindow* win) {
    if (!ActiveTab(win)) {
        return;
    }
    StepSpeed(-1);
    BarUpdate(win);
}

void ReadingAutoScrollReverse(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    if (!tab) {
        return;
    }
    tab->autoScroll.dir = tab->autoScroll.dir >= 0 ? -1 : 1;
    tab->autoScroll.atEnd = AtScrollLimit(ScrollModel(tab), tab->autoScroll.dir);
    if (tab->autoScroll.atEnd) {
        tab->autoScroll.paused = true;
        KillReadingTimer(win);
    } else if (!tab->autoScroll.paused) {
        ArmReadingTimer(win, tab);
    }
    BarUpdate(win, true);
}

static void ApplyArrowSpeed(MainWindow* win, WindowTab* tab, int keyDir) {
    // Acrobat: the arrow that matches the pan direction speeds up; the other slows.
    if (keyDir == tab->autoScroll.dir) {
        StepSpeed(1);
    } else {
        StepSpeed(-1);
    }
    BarUpdate(win);
}

static void ApplyDigitSpeed(MainWindow* win, int digit) {
    if (digit < 0 || digit > 9) {
        return;
    }
    SetSpeed(kDigitSpeeds[digit]);
    BarUpdate(win);
}

bool ReadingAutoScrollOnKey(MainWindow* win, WPARAM key) {
    WindowTab* tab = ActiveTab(win);
    if (!tab) {
        return false;
    }
    if (IsCtrlPressed() || IsShiftPressed() || IsAltPressed()) {
        return false;
    }
    if (IsFindUIVisible(win)) {
        return false;
    }

    if (key == VK_ESCAPE) {
        ReadingAutoScrollStop(win);
        return true;
    }
    if (key == VK_SPACE) {
        ReadingAutoScrollPause(win);
        return true;
    }
    if (key == VK_DOWN) {
        ApplyArrowSpeed(win, tab, 1);
        return true;
    }
    if (key == VK_UP) {
        ApplyArrowSpeed(win, tab, -1);
        return true;
    }
    if (key == VK_LEFT) {
        if (win->ctrl) {
            win->ctrl->GoToPrevPage();
        }
        return true;
    }
    if (key == VK_RIGHT) {
        if (win->ctrl) {
            win->ctrl->GoToNextPage();
        }
        return true;
    }
    if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        ReadingAutoScrollReverse(win);
        return true;
    }
    if (key >= '0' && key <= '9') {
        ApplyDigitSpeed(win, (int)(key - '0'));
        return true;
    }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        ApplyDigitSpeed(win, (int)(key - VK_NUMPAD0));
        return true;
    }
    return false;
}

void ReadingAutoScrollTick(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    if (!tab || tab != SessionTab(win) || tab->autoScroll.paused) {
        KillReadingTimer(win);
        return;
    }
    DisplayModel* dm = ScrollModel(tab);
    if (!dm) {
        ReadingAutoScrollStop(win);
        return;
    }
    int dir = tab->autoScroll.dir >= 0 ? 1 : -1;
    if (AtScrollLimit(dm, dir)) {
        tab->autoScroll.atEnd = true;
        tab->autoScroll.paused = true;
        KillReadingTimer(win);
        BarUpdate(win, true);
        return;
    }

    TimeStamp now = TimeGet();
    float dt = 0;
    if (tab->autoScroll.lastQpc != 0) {
        TimeStamp start{};
        start.QuadPart = tab->autoScroll.lastQpc;
        dt = (float)(TimeSinceInMs(start) / 1000.0);
    }
    tab->autoScroll.lastQpc = now.QuadPart;
    if (dt <= 0) {
        return;
    }
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    tab->autoScroll.accum += CurrentSpeed() * (float)dir * dt;
    int dy = (int)tab->autoScroll.accum;
    tab->autoScroll.accum -= (float)dy;
    if (dy == 0) {
        return;
    }
    dm->ScrollYBy(dy, true);
    if (AtScrollLimit(dm, dir)) {
        tab->autoScroll.atEnd = true;
        tab->autoScroll.paused = true;
        KillReadingTimer(win);
        BarUpdate(win, true);
    }
}

static MainWindow* WinFromBar(ReadingAutoScrollBar* bar) {
    if (!bar || !bar->hwndCanvas) {
        return nullptr;
    }
    return FindMainWindowByHwnd(bar->hwndCanvas);
}

static void OnPauseClickedBar(ReadingAutoScrollBar* bar, VirtMouseEvent*) {
    ReadingAutoScrollPause(WinFromBar(bar));
}

static void OnStopClickedBar(ReadingAutoScrollBar* bar, VirtMouseEvent*) {
    ReadingAutoScrollStop(WinFromBar(bar));
}

static void OnFocusClickedBar(ReadingAutoScrollBar* bar, VirtMouseEvent*) {
    ReadingBarToggle(WinFromBar(bar));
}

static void OnReverseClickedBar(ReadingAutoScrollBar* bar, VirtMouseEvent*) {
    ReadingAutoScrollReverse(WinFromBar(bar));
}

static void SyncSpeedLabel(ReadingAutoScrollBar* bar) {
    bar->speedLabel->SetText(SpeedLabelTemp(bar->sessionTab));
}

static void OnSpeedSliderDrag(ReadingAutoScrollBar* bar) {
    int idx = limitValue(bar->speedSlider->value, 0, SpeedCount() - 1);
    SetSpeed(kSpeeds[idx]);
    SyncSpeedLabel(bar);
    HwndInvalidate(bar->hwnd);
}

static void OnSpeedSliderCommit(ReadingAutoScrollBar* bar) {
    OnSpeedSliderDrag(bar);
}

static void OnSpeedSliderTooltip(ReadingAutoScrollBar* bar, VirtTooltipEvent* ev) {
    int idx = bar->speedSlider->ValueFromLocalX(ev->ptLocal.x);
    idx = limitValue(idx, 0, SpeedCount() - 1);
    ev->tip = fmt("%d px/s", (int)(kSpeeds[idx] + 0.5f));
}

HWND ReadingAutoScrollBar::Create(HWND parentCanvas) {
    onPaint = MkMethod1<ReadingAutoScrollBar, WindowBase::PaintEvent*, &ReadingAutoScrollBar::OnPaint>(this);
    hwndCanvas = parentCanvas;
    CreateCustomArgs args;
    args.owner = GetAncestor(parentCanvas, GA_ROOT);
    args.style = WS_POPUP;
    args.exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    args.font = GetAppBiggerFont();
    args.visible = false;
    args.isRtl = IsUIRtl();
    CreateCustom(args);
    if (hwnd) {
        BuildLayout();
    }
    return hwnd;
}

void ReadingAutoScrollBar::BuildLayout() {
    PlatformFont* pf = font;
    int gap = DpiScale(kBtnGap);
    int padX = DpiScale(kBarPadX);
    int padY = DpiScale(kBarPadY);
    int btnPadX = DpiScale(kBtnPadX);
    int btnPadY = DpiScale(kBtnPadY);
    Insets btnPad{btnPadY, btnPadX, btnPadY, btnPadX};

    btnPause = new VirtButton({}, pf);
    btnPause->textPadding = btnPad;
    btnPause->flags &= ~vwfFocusable;
    btnPause->flags |= vwfCapturesMouse;
    btnPause->onClick = MkFunc1(OnPauseClickedBar, this);

    btnStop = new VirtButton(Tr("Stop"), pf);
    btnStop->textPadding = btnPad;
    btnStop->flags &= ~vwfFocusable;
    btnStop->flags |= vwfCapturesMouse;
    btnStop->onClick = MkFunc1(OnStopClickedBar, this);

    btnReverse = new VirtButton(Tr("Reverse"), pf);
    btnReverse->textPadding = btnPad;
    btnReverse->flags &= ~vwfFocusable;
    btnReverse->flags |= vwfCapturesMouse;
    btnReverse->onClick = MkFunc1(OnReverseClickedBar, this);

    btnFocus = new VirtButton(Tr("Focus"), pf);
    btnFocus->textPadding = btnPad;
    btnFocus->flags &= ~vwfFocusable;
    btnFocus->flags |= vwfCapturesMouse;
    btnFocus->onClick = MkFunc1(OnFocusClickedBar, this);

    speedSlider = new VirtSlider();
    speedSlider->minVal = 0;
    speedSlider->maxVal = SpeedCount() - 1;
    speedSlider->value = ClosestSpeedIdx(CurrentSpeed());
    speedSlider->onValueChanged = MkFunc0(OnSpeedSliderDrag, this);
    speedSlider->onValueCommitted = MkFunc0(OnSpeedSliderCommit, this);
    speedSlider->onGetTooltip = MkFunc1(OnSpeedSliderTooltip, this);

    speedLabel = NewVirtText({
        .font = pf,
        .isRtl = IsUIRtl(),
    });

    status = NewVirtText({
        .font = pf,
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });

    help = NewVirtText({
        .s = Tr("\xE2\x86\x91\xE2\x86\x93 speed \xC2\xB7 0-9 \xC2\xB7 \xE2\x88\x92 reverse \xC2\xB7 Space pause "
                "\xC2\xB7 Esc stop"),
        .font = pf,
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->AddChild(btnPause);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(btnStop);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(btnReverse);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(btnFocus);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedSlider);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedLabel);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(status);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(help, 1);
    layout = new Padding(row, Insets{padY, padX, padY, padX});
}

void ReadingAutoScrollBar::SyncLabels() {
    WindowTab* tab = sessionTab;
    bool paused = tab && (tab->autoScroll.paused || tab->autoScroll.atEnd);
    btnPause->SetText(paused ? Tr("Resume") : Tr("Pause"));
    if (speedSlider && !speedSlider->IsAdjusting()) {
        speedSlider->SetValue(ClosestSpeedIdx(CurrentSpeed()), false);
    }
    SyncSpeedLabel(this);
    status->SetText(StatusTextTemp(tab));
}

void ReadingAutoScrollBar::SyncColors() {
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colTxt = ThemeNotificationsTextColor();
    Color colMuted = ThemeWindowDarkerTextColor();
    Color colBorder = kColGray;
    Color colBtnBg = AccentColor(colBg, 8, -8);
    Color colBtnHover = AccentColor(colBg, 16, -16);
    VirtButton* btns[] = {btnPause, btnStop, btnReverse, btnFocus};
    for (VirtButton* b : btns) {
        b->SetColor(kColBtnBg, colBtnBg);
        b->SetColor(kColBtnBgHover, colBtnHover);
        b->SetColor(kColBtnBorder, colBorder);
        b->SetColor(kColBtnText, colTxt);
    }
    Color thumb = colTxt;
    speedSlider->SetColor(kColSliderTrack, AccentColor(colBg, 28, -28));
    speedSlider->SetColor(kColSliderFill, thumb);
    speedSlider->SetColor(kColSliderThumb, thumb);
    speedSlider->SetColor(kColSliderThumbHover, AccentColor(thumb, 18, -18));
    speedLabel->SetColor(kColText, colTxt);
    status->SetColor(kColText, colTxt);
    help->SetColor(kColText, colMuted);
}

void ReadingAutoScrollBar::UpdateLayout(bool forceLayout) {
    if (!hwnd || !layout || !hwndCanvas) {
        return;
    }

    SyncLabels();

    Rect canvas = HwndMapLtrClientRectToScreen(hwndCanvas, HwndClientRect(hwndCanvas));
    int margin = DpiScale(kBarMargin);
    int barDx = std::max(canvas.dx - (2 * margin), 0);
    int barDy = lastDy;
    if (forceLayout || barDy <= 0 || barDx != lastDx) {
        barDy = layout->Layout(ExpandInf()).dy;
    }

    int x = canvas.x + margin;
    int y = canvas.y + canvas.dy - barDy - margin;
    if (y < canvas.y + margin) {
        y = canvas.y + margin;
    }

    bool samePos = (x == lastX && y == lastY && barDx == lastDx && barDy == lastDy);
    lastX = x;
    lastY = y;
    lastDx = barDx;
    lastDy = barDy;
    if (!samePos) {
        SetWindowPos(hwnd, HWND_TOP, x, y, barDx, barDy, SWP_NOACTIVATE);
    }
    if (!samePos || forceLayout) {
        DoLayout({barDx, barDy});
    }
}

void ReadingAutoScrollBar::OnPaint(WindowBase::PaintEvent* ev) {
    Rect rc = HwndClientRect(hwnd);
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colBorder = kColGray;
    SyncColors();
    Gfx* gfx = GfxCreateWithDoubleBuffer(this, ev->hdc);
    gfx->FillRect(rc, colBg);
    if (vroot) {
        vroot->Paint(gfx, rc);
    }
    gfx->DrawRect(rc, colBorder);
    delete gfx;
}

static void BarOnWindowMoved(ReadingAutoScrollBar* bar, MainWindow*) {
    if (!bar->hwnd || !HwndIsVisible(bar->hwnd)) {
        return;
    }
    bar->UpdateLayout();
}

static ReadingAutoScrollBar* BarEnsure(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return nullptr;
    }
    if (!win->readingAutoScrollBar) {
        auto* bar = new ReadingAutoScrollBar();
        bar->Create(win->hwndCanvas);
        bar->onWindowMoved = MkFunc1(BarOnWindowMoved, bar);
        win->RegisterOnWindowMoved(&bar->onWindowMoved);
        win->readingAutoScrollBar = bar;
    }
    ReadingAutoScrollBar* bar = win->readingAutoScrollBar;
    if (bar && bar->hwnd && !HwndIsVisible(bar->hwnd)) {
        SetWindowPos(bar->hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    return bar;
}

static void BarHide(MainWindow* win) {
    if (!win || !win->readingAutoScrollBar || !win->readingAutoScrollBar->hwnd) {
        return;
    }
    win->readingAutoScrollBar->sessionTab = nullptr;
    SetWindowPos(win->readingAutoScrollBar->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

static void BarUpdate(MainWindow* win, bool forceLayout) {
    if (!win || !win->readingAutoScrollBar || !win->readingAutoScrollBar->hwnd) {
        return;
    }
    if (!HwndIsVisible(win->readingAutoScrollBar->hwnd)) {
        return;
    }
    win->readingAutoScrollBar->UpdateLayout(forceLayout);
    HwndInvalidate(win->readingAutoScrollBar->hwnd);
}

void ReadingAutoScrollRelayout(HWND hwndCanvas) {
    MainWindow* win = FindMainWindowByHwnd(hwndCanvas);
    if (!win || !ActiveTab(win)) {
        return;
    }
    BarUpdate(win);
}

void ReadingAutoScrollDestroy(MainWindow* win) {
    if (!win || !win->readingAutoScrollBar) {
        return;
    }
    KillReadingTimer(win);
    win->UnregisterOnWindowMoved(&win->readingAutoScrollBar->onWindowMoved);
    delete win->readingAutoScrollBar;
    win->readingAutoScrollBar = nullptr;
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
    if (!on || !bar || !bar->hwnd || !HwndIsVisible(bar->hwnd) || !bar->btnPause || !bar->btnStop ||
        !bar->speedSlider) {
        out.Append(fmt("NOTREADY no-bar on=%d scrollY=%d\n", (int)on, scrollY));
        return finish(2);
    }

    Rect pause = bar->btnPause->bounds;
    Rect stop = bar->btnStop->bounds;
    Rect reverse = bar->btnReverse->bounds;
    Rect speed = bar->speedSlider->bounds;
    out.Append(fmt("OK visible=1 paused=%d atEnd=%d dir=%d speed=%d scrollY=%d page=%d pages=%d hwnd=%d\n",
                   (int)tab->autoScroll.paused, (int)tab->autoScroll.atEnd, tab->autoScroll.dir,
                   (int)(CurrentSpeed() + 0.5f), scrollY, page, pages, (int)(uintptr_t)bar->hwnd));
    out.Append(fmt("pause=%d,%d,%d,%d\n", pause.x, pause.y, pause.dx, pause.dy));
    out.Append(fmt("stop=%d,%d,%d,%d\n", stop.x, stop.y, stop.dx, stop.dy));
    out.Append(fmt("reverse=%d,%d,%d,%d\n", reverse.x, reverse.y, reverse.dx, reverse.dy));
    out.Append(fmt("speed=%d,%d,%d,%d\n", speed.x, speed.y, speed.dx, speed.dy));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", bar->speedSlider->value, SpeedCount(),
                   bar->speedLabel ? bar->speedLabel->s : Str{}));
    out.Append(fmt("status=%s\n", bar->status ? bar->status->s : Str{}));
    return finish(0);
}
