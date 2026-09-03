/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Translations.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextToSpeech.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "ReadAloudHighlight.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "ReadAloudPlaybackBar.h"
#include "SumatraLog.h"

struct ReadAloudPlaybackBar : WindowBase {
    ReadAloudPlaybackBar() = default;
    ~ReadAloudPlaybackBar() override = default;

    HWND Create(HWND parentCanvas);
    void SetSession(WindowTab* tab);
    void BuildLayout();
    void SyncLabels();
    void SyncColors();
    void UpdateLayout(bool forceLayout = false);
    void OnPaint(WindowBase::PaintEvent* ev);

    WindowTab* sessionTab = nullptr;
    HWND hwndCanvas = nullptr;
    VirtButton* btnPause = nullptr;
    VirtButton* btnStop = nullptr;
    VirtSlider* speedSlider = nullptr;
    VirtText* speedLabel = nullptr;
    VirtText* status = nullptr;
    bool showResume = false;
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

static Str ReadAloudScopeLabel(WindowTab* tab) {
    if (!tab) {
        return {};
    }
    switch (tab->readAloudScope) {
        case WindowTab::ReadAloudScopeSelection:
            return _TRA("Selection");
        case WindowTab::ReadAloudScopeViewport:
            return _TRA("Top of view");
        case WindowTab::ReadAloudScopeCursor:
            return _TRA("From cursor");
        case WindowTab::ReadAloudScopeSmart:
        default:
            return _TRA("Smart start");
    }
}

static TempStr ReadAloudPlaybackBarTextTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }

    Str docName = tab->GetTabTitle();
    if (len(docName) == 0) {
        docName = _TRA("document");
    }

    int pageNo = 0;
    int pageCount = 0;
    bool hasPage = ReadAloudGetProgressPage(tab, &pageNo, &pageCount);
    Str scope = ReadAloudScopeLabel(tab);

    bool isPaused = CanContinueReadAloud(tab) && !TtsIsSpeaking();
    if (hasPage && pageCount > 0) {
        const char* pattern = isPaused ? _TRA("Paused \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s
                                       : _TRA("Reading \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s;
        return fmt(pattern, docName, pageNo, pageCount, scope);
    }
    const char* pattern =
        isPaused ? _TRA("Paused \xC2\xB7 %s \xC2\xB7 %s").s : _TRA("Reading \xC2\xB7 %s \xC2\xB7 %s").s;
    return fmt(pattern, docName, scope);
}

static void OnPauseClicked(ReadAloudPlaybackBar* bar, VirtMouseEvent*) {
    dbgtts("bar pause-click speaking=%d resume=%d\n", (int)TtsIsSpeaking(), (int)bar->showResume);
    ReadAloudPlaybackPauseOrResume();
    bar->UpdateLayout(true);
    HwndRepaintNow(bar->hwnd);
}

static void OnStopClicked(ReadAloudPlaybackBar*, VirtMouseEvent*) {
    dbgtts("bar stop-click\n");
    ReadAloudPlaybackStop();
}

static void SyncSpeedLabel(ReadAloudPlaybackBar* bar) {
    int idx = bar->speedSlider ? bar->speedSlider->value : ReadAloudClosestSpeedIdx();
    bar->speedLabel->SetText(ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx)));
}

static void OnSpeedSliderDrag(ReadAloudPlaybackBar* bar) {
    SyncSpeedLabel(bar);
    HwndInvalidate(bar->hwnd);
}

static void OnSpeedSliderCommit(ReadAloudPlaybackBar* bar) {
    ReadAloudSetSpeedIdx(bar->speedSlider->value);
    SyncSpeedLabel(bar);
    HwndInvalidate(bar->hwnd);
}

static void OnSpeedSliderTooltip(ReadAloudPlaybackBar* bar, VirtTooltipEvent* ev) {
    int idx = bar->speedSlider->ValueFromLocalX(ev->ptLocal.x);
    ev->tip = ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx));
}

static void OnBarWndProc(WindowBase::WndProcEvent* ev) {
    UINT msg = ev->msg;
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) {
        int x = GET_X_LPARAM(ev->lparam);
        int y = GET_Y_LPARAM(ev->lparam);
        dbgtts("bar-mouse msg=0x%x x=%d y=%d\n", (int)msg, x, y);
    }
}

HWND ReadAloudPlaybackBar::Create(HWND parentCanvas) {
    onPaint = MkMethod1<ReadAloudPlaybackBar, WindowBase::PaintEvent*, &ReadAloudPlaybackBar::OnPaint>(this);
    onWndProc = MkFunc1Void(OnBarWndProc);
    hwndCanvas = parentCanvas;
    CreateCustomArgs args;
    // Owned popup, not a canvas child: WebView2 fills the canvas and steals
    // clicks from sibling HWNDs (issue #6031). Same pattern as the overlay
    // scrollbar / find bar.
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

// [Pause] [Stop] [slider] [1.5x] [status…]. The HWND is WS_EX_LAYOUTRTL, but we
// paint into a DoubleBuffer DC that is not mirrored, so HBox.rtl (not GDI's
// flip) is what reverses the row.
void ReadAloudPlaybackBar::BuildLayout() {
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
    btnPause->onClick = MkFunc1(OnPauseClicked, this);

    btnStop = new VirtButton(_TRA("Stop"), pf);
    btnStop->textPadding = btnPad;
    btnStop->flags &= ~vwfFocusable;
    btnStop->flags |= vwfCapturesMouse;
    btnStop->onClick = MkFunc1(OnStopClicked, this);

    speedSlider = new VirtSlider();
    speedSlider->minVal = 0;
    speedSlider->maxVal = std::max(ReadAloudSpeedCount() - 1, 0);
    speedSlider->value = ReadAloudClosestSpeedIdx();
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

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->AddChild(btnPause);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(btnStop);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedSlider);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedLabel);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(status, 1);
    layout = new Padding(row, Insets{padY, padX, padY, padX});
}

void ReadAloudPlaybackBar::SyncLabels() {
    showResume = sessionTab && CanContinueReadAloud(sessionTab) && !TtsIsSpeaking();
    btnPause->SetText(showResume ? _TRA("Resume") : _TRA("Pause"));
    if (!speedSlider->IsAdjusting()) {
        speedSlider->SetValue(ReadAloudClosestSpeedIdx(), false);
        SyncSpeedLabel(this);
    }
    status->SetText(ReadAloudPlaybackBarTextTemp(sessionTab));
}

void ReadAloudPlaybackBar::SyncColors() {
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colTxt = ThemeNotificationsTextColor();
    Color colBorder = kColGray;
    Color colBtnBg = AccentColor(colBg, 8, -8);
    Color colBtnHover = AccentColor(colBg, 16, -16);
    VirtButton* btns[] = {btnPause, btnStop};
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
}

void ReadAloudPlaybackBar::SetSession(WindowTab* tab) {
    sessionTab = tab;
    if (!tab || !hwnd) {
        return;
    }

    UpdateLayout();
    if (!HwndIsVisible(hwnd)) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    HwndInvalidate(hwnd);
}

void ReadAloudPlaybackBar::UpdateLayout(bool forceLayout) {
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
        dbgtts("bar-move x=%d y=%d %dx%d\n", x, y, barDx, barDy);
        SetWindowPos(hwnd, HWND_TOP, x, y, barDx, barDy, SWP_NOACTIVATE);
    }
    if (!samePos || forceLayout) {
        dbgtts("bar-layout force=%d samePos=%d\n", (int)forceLayout, (int)samePos);
        DoLayout({barDx, barDy});
    }
}

void ReadAloudPlaybackBar::OnPaint(WindowBase::PaintEvent* ev) {
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

static void ReadAloudPlaybackBarOnWindowMoved(ReadAloudPlaybackBar* bar, MainWindow*) {
    if (!bar->hwnd || !HwndIsVisible(bar->hwnd)) {
        return;
    }
    bar->UpdateLayout();
}

static ReadAloudPlaybackBar* ReadAloudPlaybackBarEnsure(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return nullptr;
    }
    if (!win->readAloudPlaybackBar) {
        auto* bar = new ReadAloudPlaybackBar();
        bar->Create(win->hwndCanvas);
        bar->onWindowMoved = MkFunc1(ReadAloudPlaybackBarOnWindowMoved, bar);
        win->RegisterOnWindowMoved(&bar->onWindowMoved);
        win->readAloudPlaybackBar = bar;
    }
    return win->readAloudPlaybackBar;
}

void ReadAloudPlaybackBarDestroy(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar) {
        return;
    }
    win->UnregisterOnWindowMoved(&win->readAloudPlaybackBar->onWindowMoved);
    delete win->readAloudPlaybackBar;
    win->readAloudPlaybackBar = nullptr;
}

void ReadAloudPlaybackBarHide(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    win->readAloudPlaybackBar->sessionTab = nullptr;
    SetWindowPos(win->readAloudPlaybackBar->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

// the tab is going away; the bar has no reason to exist without it
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || bar->sessionTab != tab) {
        return;
    }
    bar->sessionTab = nullptr;
    if (bar->hwnd) {
        SetWindowPos(bar->hwnd, nullptr, 0, 0, 0, 0,
                     SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
}

void ReadAloudPlaybackBarRelayout(HWND hwndCanvas) {
    MainWindow* win = FindMainWindowByHwnd(hwndCanvas);
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    if (!HwndIsVisible(win->readAloudPlaybackBar->hwnd)) {
        return;
    }
    win->readAloudPlaybackBar->UpdateLayout();
}

// Highlight timer (~80ms): refresh Pause/page text without SetWindowPos.
// Relayouting every tick ate mouse-up (issue #6031).
void ReadAloudPlaybackBarTick(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || !bar->hwnd || !HwndIsVisible(bar->hwnd)) {
        return;
    }
    if (bar->speedSlider && bar->speedSlider->IsAdjusting()) {
        return;
    }
    bool wasResume = bar->showResume;
    TempStr statusBefore = str::DupTemp(bar->status ? bar->status->s : Str{});
    bar->SyncLabels();
    if (wasResume != bar->showResume) {
        bar->UpdateLayout(true);
        return;
    }
    SetWindowPos(bar->hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // skip a full paint when Pause/page text is unchanged: D2D BeginDraw on the
    // bar's memory DC throws a first-chance C++ EH inside d3d11 every time
    if (!str::Eq(statusBefore, bar->status ? bar->status->s : Str{})) {
        HwndInvalidate(bar->hwnd);
    }
}

TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    Vec<TtsVoiceInfo> voices = TtsGetVoices();
    int nVoices = len(voices);
    TtsFreeVoices(voices);
    out.Append(fmt("voices=%d speaking=%d\n", nVoices, (int)TtsIsSpeaking()));

    if (len(gWindows) == 0) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    MainWindow* win = gWindows[0];
    ReadAloudPlaybackBar* bar = win->readAloudPlaybackBar;
    if (!bar || !bar->hwnd || !HwndIsVisible(bar->hwnd) || !bar->btnPause || !bar->btnStop || !bar->speedSlider ||
        !bar->speedLabel) {
        out.Append(StrL("NOTREADY no-bar\n"));
        return finish(2);
    }

    Rect pause = bar->btnPause->bounds;
    Rect stop = bar->btnStop->bounds;
    Rect speed = bar->speedSlider->bounds;
    Rect speedLab = bar->speedLabel->bounds;
    int idx = bar->speedSlider->value;
    out.Append(fmt("OK visible=1 resume=%d hwnd=%d\n", (int)bar->showResume, (int)(uintptr_t)bar->hwnd));
    out.Append(fmt("pause=%d,%d,%d,%d\n", pause.x, pause.y, pause.dx, pause.dy));
    out.Append(fmt("stop=%d,%d,%d,%d\n", stop.x, stop.y, stop.dx, stop.dy));
    out.Append(fmt("speed=%d,%d,%d,%d\n", speed.x, speed.y, speed.dx, speed.dy));
    out.Append(fmt("speedLabel=%d,%d,%d,%d\n", speedLab.x, speedLab.y, speedLab.dx, speedLab.dy));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", idx, ReadAloudSpeedCount(),
                   ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx))));
    out.Append(fmt("status=%s\n", bar->status ? bar->status->s : Str{}));
    return finish(0);
}

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab) {
    if (!tab) {
        // no read-aloud source any more (callers pass GetReadAloudSourceTab()),
        // so no bar should be up. Hiding also drops the tab each bar points at,
        // which is about to be deleted on the tab-close path
        for (MainWindow* win : gWindows) {
            ReadAloudPlaybackBarHide(win);
        }
        return;
    }
    if (!tab->win || len(tab->readAloudText) == 0) {
        ReadAloudPlaybackBarHide(tab->win);
        return;
    }

    ReadAloudPlaybackBar* bar = ReadAloudPlaybackBarEnsure(tab->win);
    if (!bar) {
        return;
    }
    bar->SetSession(tab);

    // hide bars on other windows
    for (MainWindow* win : gWindows) {
        if (win != tab->win && win->readAloudPlaybackBar && HwndIsVisible(win->readAloudPlaybackBar->hwnd)) {
            ReadAloudPlaybackBarHide(win);
        }
    }
}
