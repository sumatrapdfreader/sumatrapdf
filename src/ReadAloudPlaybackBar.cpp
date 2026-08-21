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

struct ReadAloudPlaybackBar : WindowBase {
    ReadAloudPlaybackBar() = default;
    ~ReadAloudPlaybackBar() override = default;

    HWND Create(HWND parentCanvas);
    void SetSession(WindowTab* tab);
    void BuildLayout();
    void SyncLabels();
    void SyncColors();
    void UpdateLayout();
    void OnPaint(WindowBase::PaintEvent* ev);

    WindowTab* sessionTab = nullptr;
    VirtButton* btnPause = nullptr;
    VirtButton* btnStop = nullptr;
    VirtButton* btnSpeed = nullptr;
    VirtText* status = nullptr;
    bool showResume = false;
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
            return _TRA("From top");
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

    if (hasPage && pageCount > 0) {
        return fmt(_TRA("Reading \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s, docName, pageNo, pageCount, scope);
    }
    return fmt(_TRA("Reading \xC2\xB7 %s \xC2\xB7 %s").s, docName, scope);
}

static TempStr SpeedLabelTemp() {
    return ReadAloudSpeedLabelTemp(TtsGetSpeed());
}

static void OnPauseClicked(ReadAloudPlaybackBar* bar, VirtMouseEvent*) {
    ReadAloudPlaybackPauseOrResume();
    bar->UpdateLayout();
    HwndRepaintNow(bar->hwnd);
}

static void OnStopClicked(ReadAloudPlaybackBar*, VirtMouseEvent*) {
    ReadAloudPlaybackStop();
}

// left-click steps up, right-click steps down
static void OnSpeedClicked(ReadAloudPlaybackBar* bar, VirtMouseEvent* ev) {
    ReadAloudPlaybackCycleSpeed(ev->button == 1 ? -1 : +1);
    bar->UpdateLayout();
    HwndRepaintNow(bar->hwnd);
}

HWND ReadAloudPlaybackBar::Create(HWND parentCanvas) {
    onPaint = MkMethod1<ReadAloudPlaybackBar, WindowBase::PaintEvent*, &ReadAloudPlaybackBar::OnPaint>(this);
    CreateCustomArgs args;
    args.parent = parentCanvas;
    args.style = WS_CHILD | SS_CENTER;
    args.exStyle = WS_EX_TOPMOST;
    args.font = GetAppBiggerFont();
    args.visible = false;
    args.isRtl = IsUIRtl();
    CreateCustom(args);
    if (hwnd) {
        BuildLayout();
    }
    return hwnd;
}

// [Pause] [Stop] [Speed] [status…]. The HWND is WS_EX_LAYOUTRTL, but we paint
// into a DoubleBuffer DC that is not mirrored, so HBox.rtl (not GDI's flip)
// is what reverses the row.
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
    btnPause->onClick = MkFunc1(OnPauseClicked, this);

    btnStop = new VirtButton(_TRA("Stop"), pf);
    btnStop->textPadding = btnPad;
    btnStop->flags &= ~vwfFocusable;
    btnStop->onClick = MkFunc1(OnStopClicked, this);

    btnSpeed = new VirtButton({}, pf);
    btnSpeed->textPadding = btnPad;
    btnSpeed->flags &= ~vwfFocusable;
    btnSpeed->onClick = MkFunc1(OnSpeedClicked, this);

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
    row->AddChild(btnSpeed);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(status, 1);
    layout = new Padding(row, Insets{padY, padX, padY, padX});
}

void ReadAloudPlaybackBar::SyncLabels() {
    showResume = sessionTab && CanContinueReadAloud(sessionTab) && !TtsIsSpeaking();
    btnPause->SetText(showResume ? _TRA("Resume") : _TRA("Pause"));
    btnSpeed->SetText(SpeedLabelTemp());
    status->SetText(ReadAloudPlaybackBarTextTemp(sessionTab));
}

void ReadAloudPlaybackBar::SyncColors() {
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colTxt = ThemeNotificationsTextColor();
    Color colBorder = kColGray;
    Color colBtnBg = AccentColor(colBg, 8, -8);
    Color colBtnHover = AccentColor(colBg, 16, -16);
    VirtButton* btns[] = {btnPause, btnStop, btnSpeed};
    for (VirtButton* b : btns) {
        b->SetColor(kColBtnBg, colBtnBg);
        b->SetColor(kColBtnBgHover, colBtnHover);
        b->SetColor(kColBtnBorder, colBorder);
        b->SetColor(kColBtnText, colTxt);
    }
    status->SetColor(kColText, colTxt);
}

void ReadAloudPlaybackBar::SetSession(WindowTab* tab) {
    sessionTab = tab;
    if (!tab || !hwnd) {
        return;
    }

    UpdateLayout();
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    HwndRepaintNow(hwnd);
}

void ReadAloudPlaybackBar::UpdateLayout() {
    if (!hwnd || !layout) {
        return;
    }

    SyncLabels();

    HWND parent = GetParent(hwnd);
    Rect canvas = HwndClientRect(parent);
    int margin = DpiScale(kBarMargin);
    int barDx = std::max(canvas.dx - (2 * margin), 0);
    Size natural = layout->Layout(ExpandInf());
    int barDy = natural.dy;

    int x = margin;
    int y = canvas.dy - barDy - margin;
    y = std::max(y, margin);

    uint flags = SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd, nullptr, x, y, barDx, barDy, flags);
    DoLayout({barDx, barDy});
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

static ReadAloudPlaybackBar* ReadAloudPlaybackBarEnsure(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return nullptr;
    }
    if (!win->readAloudPlaybackBar) {
        win->readAloudPlaybackBar = new ReadAloudPlaybackBar();
        win->readAloudPlaybackBar->Create(win->hwndCanvas);
    }
    return win->readAloudPlaybackBar;
}

void ReadAloudPlaybackBarDestroy(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar) {
        return;
    }
    delete win->readAloudPlaybackBar;
    win->readAloudPlaybackBar = nullptr;
}

void ReadAloudPlaybackBarHide(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    win->readAloudPlaybackBar->sessionTab = nullptr;
    ShowWindow(win->readAloudPlaybackBar->hwnd, SW_HIDE);
}

// the tab is going away; the bar has no reason to exist without it
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || bar->sessionTab != tab) {
        return;
    }
    bar->sessionTab = nullptr;
    if (bar->hwnd) {
        ShowWindow(bar->hwnd, SW_HIDE);
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
    HwndRepaintNow(win->readAloudPlaybackBar->hwnd);
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
