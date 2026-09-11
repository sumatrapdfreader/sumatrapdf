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

    HWND hwndCanvas = nullptr;
    VirtButton* btnPause = nullptr;
    VirtButton* btnStop = nullptr;
    VirtButton* btnReverse = nullptr;
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

static TempStr SpeedLabelTemp(MainWindow* win) {
    const char* arrow = (!win || win->readingAutoScrollDir >= 0) ? "\xE2\x86\x93" : "\xE2\x86\x91";
    return fmt("%s %d px/s", Str(arrow), (int)(CurrentSpeed() + 0.5f));
}

static TempStr StatusTextTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    if (win->readingAutoScrollAtEnd) {
        return str::DupTemp(Tr("End of document"));
    }
    if (win->readingAutoScrollPaused) {
        return str::DupTemp(Tr("Paused"));
    }
    return str::DupTemp(Tr("Scrolling"));
}

static DisplayModel* ScrollModel(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return nullptr;
    }
    return win->AsFixed();
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

static void ArmReadingTimer(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    win->readingAutoScrollLastQpc = TimeGet().QuadPart;
    win->readingAutoScrollAccum = 0;
    SetTimer(win->hwndCanvas, kReadingAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
}

static ReadingAutoScrollBar* BarEnsure(MainWindow* win);
static void BarHide(MainWindow* win);
static void BarUpdate(MainWindow* win, bool forceLayout = false);

bool ReadingAutoScrollIsOn(MainWindow* win) {
    return win && win->readingAutoScrollOn;
}

void ReadingAutoScrollStop(MainWindow* win) {
    if (!win || !win->readingAutoScrollOn) {
        return;
    }
    win->readingAutoScrollOn = false;
    win->readingAutoScrollPaused = false;
    win->readingAutoScrollAtEnd = false;
    win->readingAutoScrollAccum = 0;
    KillReadingTimer(win);
    BarHide(win);
}

static void ReadingAutoScrollStart(MainWindow* win) {
    if (!win || !ScrollModel(win)) {
        return;
    }
    StopMiddleClickScroll(win);
    win->readingAutoScrollOn = true;
    win->readingAutoScrollPaused = false;
    win->readingAutoScrollAtEnd = AtScrollLimit(ScrollModel(win), win->readingAutoScrollDir);
    win->readingAutoScrollAccum = 0;
    if (!win->readingAutoScrollAtEnd) {
        ArmReadingTimer(win);
    } else {
        win->readingAutoScrollPaused = true;
        KillReadingTimer(win);
    }
    BarEnsure(win);
    BarUpdate(win, true);
}

void ReadingAutoScrollToggle(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->readingAutoScrollOn) {
        ReadingAutoScrollStop(win);
        return;
    }
    ReadingAutoScrollStart(win);
}

void ReadingAutoScrollPause(MainWindow* win) {
    if (!win || !win->readingAutoScrollOn) {
        return;
    }
    if (win->readingAutoScrollAtEnd && win->readingAutoScrollPaused) {
        return;
    }
    win->readingAutoScrollPaused = !win->readingAutoScrollPaused;
    if (win->readingAutoScrollPaused) {
        KillReadingTimer(win);
        win->readingAutoScrollAccum = 0;
    } else {
        win->readingAutoScrollAtEnd = AtScrollLimit(ScrollModel(win), win->readingAutoScrollDir);
        if (win->readingAutoScrollAtEnd) {
            win->readingAutoScrollPaused = true;
        } else {
            ArmReadingTimer(win);
        }
    }
    BarUpdate(win, true);
}

void ReadingAutoScrollFaster(MainWindow* win) {
    if (!win || !win->readingAutoScrollOn) {
        return;
    }
    StepSpeed(1);
    BarUpdate(win);
}

void ReadingAutoScrollSlower(MainWindow* win) {
    if (!win || !win->readingAutoScrollOn) {
        return;
    }
    StepSpeed(-1);
    BarUpdate(win);
}

void ReadingAutoScrollReverse(MainWindow* win) {
    if (!win || !win->readingAutoScrollOn) {
        return;
    }
    win->readingAutoScrollDir = win->readingAutoScrollDir >= 0 ? -1 : 1;
    win->readingAutoScrollAtEnd = AtScrollLimit(ScrollModel(win), win->readingAutoScrollDir);
    if (win->readingAutoScrollAtEnd) {
        win->readingAutoScrollPaused = true;
        KillReadingTimer(win);
    } else if (!win->readingAutoScrollPaused) {
        ArmReadingTimer(win);
    }
    BarUpdate(win, true);
}

static void ApplyArrowSpeed(MainWindow* win, int keyDir) {
    // Acrobat: the arrow that matches the pan direction speeds up; the other slows.
    if (keyDir == win->readingAutoScrollDir) {
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
    if (!win || !win->readingAutoScrollOn) {
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
        ApplyArrowSpeed(win, 1);
        return true;
    }
    if (key == VK_UP) {
        ApplyArrowSpeed(win, -1);
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
    if (!win || !win->readingAutoScrollOn || win->readingAutoScrollPaused) {
        KillReadingTimer(win);
        return;
    }
    DisplayModel* dm = ScrollModel(win);
    if (!dm) {
        ReadingAutoScrollStop(win);
        return;
    }
    int dir = win->readingAutoScrollDir >= 0 ? 1 : -1;
    if (AtScrollLimit(dm, dir)) {
        win->readingAutoScrollAtEnd = true;
        win->readingAutoScrollPaused = true;
        KillReadingTimer(win);
        BarUpdate(win, true);
        return;
    }

    TimeStamp now = TimeGet();
    float dt = 0;
    if (win->readingAutoScrollLastQpc != 0) {
        TimeStamp start{};
        start.QuadPart = win->readingAutoScrollLastQpc;
        dt = (float)(TimeSinceInMs(start) / 1000.0);
    }
    win->readingAutoScrollLastQpc = now.QuadPart;
    if (dt <= 0) {
        return;
    }
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    win->readingAutoScrollAccum += CurrentSpeed() * (float)dir * dt;
    int dy = (int)win->readingAutoScrollAccum;
    win->readingAutoScrollAccum -= (float)dy;
    if (dy == 0) {
        return;
    }
    dm->ScrollYBy(dy, true);
    if (AtScrollLimit(dm, dir)) {
        win->readingAutoScrollAtEnd = true;
        win->readingAutoScrollPaused = true;
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

static void OnReverseClickedBar(ReadingAutoScrollBar* bar, VirtMouseEvent*) {
    ReadingAutoScrollReverse(WinFromBar(bar));
}

static void SyncSpeedLabel(ReadingAutoScrollBar* bar) {
    MainWindow* win = WinFromBar(bar);
    bar->speedLabel->SetText(SpeedLabelTemp(win));
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
    MainWindow* win = WinFromBar(this);
    bool paused = win && (win->readingAutoScrollPaused || win->readingAutoScrollAtEnd);
    btnPause->SetText(paused ? Tr("Resume") : Tr("Pause"));
    if (speedSlider && !speedSlider->IsAdjusting()) {
        speedSlider->SetValue(ClosestSpeedIdx(CurrentSpeed()), false);
    }
    SyncSpeedLabel(this);
    status->SetText(StatusTextTemp(win));
}

void ReadingAutoScrollBar::SyncColors() {
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colTxt = ThemeNotificationsTextColor();
    Color colMuted = ThemeWindowDarkerTextColor();
    Color colBorder = kColGray;
    Color colBtnBg = AccentColor(colBg, 8, -8);
    Color colBtnHover = AccentColor(colBg, 16, -16);
    VirtButton* btns[] = {btnPause, btnStop, btnReverse};
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
    if (!win || !win->readingAutoScrollOn) {
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
    win->readingAutoScrollOn = false;
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
    DisplayModel* dm = win->AsFixed();
    int scrollY = dm ? dm->viewPort.y : -1;
    int page = dm ? dm->CurrentPageNo() : 0;
    int pages = dm ? dm->PageCount() : 0;
    ReadingAutoScrollBar* bar = win->readingAutoScrollBar;
    if (!win->readingAutoScrollOn || !bar || !bar->hwnd || !HwndIsVisible(bar->hwnd) || !bar->btnPause ||
        !bar->btnStop || !bar->speedSlider) {
        out.Append(fmt("NOTREADY no-bar on=%d scrollY=%d\n", (int)win->readingAutoScrollOn, scrollY));
        return finish(2);
    }

    Rect pause = bar->btnPause->bounds;
    Rect stop = bar->btnStop->bounds;
    Rect reverse = bar->btnReverse->bounds;
    Rect speed = bar->speedSlider->bounds;
    out.Append(fmt("OK visible=1 paused=%d atEnd=%d dir=%d speed=%d scrollY=%d page=%d pages=%d hwnd=%d\n",
                   (int)win->readingAutoScrollPaused, (int)win->readingAutoScrollAtEnd, win->readingAutoScrollDir,
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
