/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

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

using Gdiplus::Graphics;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;

struct ReadAloudPlaybackBar : Wnd {
    ReadAloudPlaybackBar() = default;
    ~ReadAloudPlaybackBar() override = default;

    HWND Create(HWND parentCanvas);
    void SetSession(WindowTab* tab);
    void UpdateLayout();
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    void OnPaint(HDC hdc, PAINTSTRUCT* ps) override;

    WindowTab* sessionTab = nullptr;
    Rect rPause;
    Rect rStop;
    Rect rSpeed;
    bool showResume = false;
};

constexpr int kBarMargin = 8;
constexpr int kBarPadX = 12;
constexpr int kBarPadY = 6;
constexpr int kBtnGap = 8;
constexpr int kBtnPadX = 10;

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

static bool ReadAloudPlaybackBarHitTest(const Rect& r, Point pt) {
    return !r.IsEmpty() && r.Contains(pt);
}

static TempStr SpeedLabelTemp() {
    return ReadAloudSpeedLabelTemp(TtsGetSpeed());
}

HWND ReadAloudPlaybackBar::Create(HWND parentCanvas) {
    CreateCustomArgs args;
    args.parent = parentCanvas;
    args.style = WS_CHILD | SS_CENTER;
    args.exStyle = WS_EX_TOPMOST;
    args.font = GetAppBiggerFont();
    args.visible = false;
    args.isRtl = IsUIRtl();
    CreateCustom(args);
    return hwnd;
}

void ReadAloudPlaybackBar::SetSession(WindowTab* tab) {
    sessionTab = tab;
    if (!tab || !hwnd) {
        return;
    }

    showResume = CanContinueReadAloud(tab) && !TtsIsSpeaking();

    UpdateLayout();
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    HwndRepaintNow(hwnd);
}

void ReadAloudPlaybackBar::UpdateLayout() {
    if (!hwnd) {
        return;
    }

    HWND parent = GetParent(hwnd);
    Rect canvas = ClientRect(parent);
    int margin = DpiScale(hwnd, kBarMargin);
    int padX = DpiScale(hwnd, kBarPadX);
    int padY = DpiScale(hwnd, kBarPadY);
    int btnGap = DpiScale(hwnd, kBtnGap);
    int btnPadX = DpiScale(hwnd, kBtnPadX);

    Str pauseLabel = showResume ? _TRA("Resume") : _TRA("Pause");
    Str stopLabel = _TRA("Stop");
    TempStr speedLabel = SpeedLabelTemp();
    TempStr status = ReadAloudPlaybackBarTextTemp(sessionTab);

    HDC hdc = GetDC(hwnd);
    Size szStatus = HdcMeasureText(hdc, status, DT_SINGLELINE | DT_NOPREFIX, font);
    Size szPause = HdcMeasureText(hdc, pauseLabel, DT_SINGLELINE | DT_NOPREFIX, font);
    Size szStop = HdcMeasureText(hdc, stopLabel, DT_SINGLELINE | DT_NOPREFIX, font);
    Size szSpeed = HdcMeasureText(hdc, speedLabel, DT_SINGLELINE | DT_NOPREFIX, font);
    ReleaseDC(hwnd, hdc);

    int btnDy = std::max(std::max(szPause.dy, szStop.dy), szSpeed.dy) + padY;
    int btnPauseDx = szPause.dx + 2 * btnPadX;
    int btnStopDx = szStop.dx + 2 * btnPadX;
    int btnSpeedDx = szSpeed.dx + 2 * btnPadX;
    int barDy = std::max(szStatus.dy, btnDy) + 2 * padY;
    int barDx = canvas.dx - 2 * margin;
    if (barDx < 0) {
        barDx = 0;
    }

    int x = margin;
    int y = canvas.dy - barDy - margin;
    if (y < margin) {
        y = margin;
    }

    int rowY = padY;
    if (barDy > 2 * padY + btnDy) {
        rowY = padY + (barDy - 2 * padY - btnDy) / 2;
    }

    bool isRtl = IsUIRtl();
    if (isRtl) {
        rPause = {barDx - padX - btnPauseDx, rowY, btnPauseDx, btnDy};
        rStop = {rPause.x - btnGap - btnStopDx, rowY, btnStopDx, btnDy};
        rSpeed = {rStop.x - btnGap - btnSpeedDx, rowY, btnSpeedDx, btnDy};
    } else {
        rPause = {padX, rowY, btnPauseDx, btnDy};
        rStop = {rPause.x + btnPauseDx + btnGap, rowY, btnStopDx, btnDy};
        rSpeed = {rStop.x + btnStopDx + btnGap, rowY, btnSpeedDx, btnDy};
    }

    uint flags = SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd, nullptr, x, y, barDx, barDy, flags);
}

void ReadAloudPlaybackBar::OnPaint(HDC hdcIn, PAINTSTRUCT* ps) {
    Rect rc = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rc);
    HDC hdc = buffer.GetDC();

    ScopedSelectObject fontPrev(hdc, font);

    COLORREF colBg = ThemeNotificationsBackgroundColor();
    COLORREF colBorder = MkGray(0xdd);
    COLORREF colTxt = ThemeNotificationsTextColor();
    COLORREF colBtnBg = AccentColor(colBg, 8, -8);
    COLORREF colBtnHover = AccentColor(colBg, 16, -16);

    Graphics graphics(hdc);
    SolidBrush br(GdiRgbFromCOLORREF(colBg));
    graphics.FillRectangle(&br, 0, 0, rc.dx, rc.dy);

    Pen pen(GdiRgbFromCOLORREF(colBorder));
    pen.SetWidth(1);
    graphics.DrawRectangle(&pen, 0, 0, rc.dx - 1, rc.dy - 1);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colTxt);

    TempStr status = ReadAloudPlaybackBarTextTemp(sessionTab);
    int padX = DpiScale(hwnd, kBarPadX);
    int btnGap = DpiScale(hwnd, kBtnGap);
    Rect rTxt;
    if (IsUIRtl()) {
        rTxt = {padX, rPause.y, rSpeed.x - padX - btnGap, rPause.dy};
    } else {
        int textX = rSpeed.x + rSpeed.dx + btnGap;
        rTxt = {textX, rPause.y, rc.dx - textX - padX, rPause.dy};
    }
    RECT rTmp = ToRECT(rTxt);
    uint txtFmt = DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | DT_END_ELLIPSIS;
    if (IsUIRtl()) {
        txtFmt |= DT_RIGHT | DT_RTLREADING;
    } else {
        txtFmt |= DT_LEFT;
    }
    HdcDrawText(hdc, status, &rTmp, txtFmt);

    Point curPos = HwndGetCursorPos(hwnd);
    auto drawBtn = [&](const Rect& r, Str label) {
        if (r.IsEmpty()) {
            return;
        }
        COLORREF bg = ReadAloudPlaybackBarHitTest(r, curPos) ? colBtnHover : colBtnBg;
        HBRUSH brBtn = CreateSolidBrush(bg);
        RECT rr = ToRECT(r);
        FillRect(hdc, &rr, brBtn);
        DeleteObject(brBtn);
        graphics.DrawRectangle(&pen, r.x, r.y, r.dx - 1, r.dy - 1);
        SetTextColor(hdc, colTxt);
        DrawCenteredText(hdc, r, label);
    };

    Str pauseLabel = showResume ? _TRA("Resume") : _TRA("Pause");
    drawBtn(rPause, pauseLabel);
    drawBtn(rStop, _TRA("Stop"));
    drawBtn(rSpeed, SpeedLabelTemp());

    buffer.Flush(hdcIn);
}

LRESULT ReadAloudPlaybackBar::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_SETCURSOR == msg) {
        Point pt = HwndGetCursorPos(hwndIn);
        if (ReadAloudPlaybackBarHitTest(rPause, pt) || ReadAloudPlaybackBarHitTest(rStop, pt) ||
            ReadAloudPlaybackBarHitTest(rSpeed, pt)) {
            SetCursorCached(IDC_HAND);
            return TRUE;
        }
    }

    if (WM_ERASEBKGND == msg) {
        return TRUE;
    }

    if (WM_MOUSEMOVE == msg) {
        HwndScheduleRepaint(hwndIn);
        TrackMouseLeave(hwndIn);
        return 0;
    }

    if (WM_MOUSELEAVE == msg) {
        HwndScheduleRepaint(hwndIn);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        Point pt = Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (ReadAloudPlaybackBarHitTest(rPause, pt)) {
            ReadAloudPlaybackPauseOrResume();
            return 0;
        }
        if (ReadAloudPlaybackBarHitTest(rStop, pt)) {
            ReadAloudPlaybackStop();
            return 0;
        }
        if (ReadAloudPlaybackBarHitTest(rSpeed, pt)) {
            ReadAloudPlaybackCycleSpeed(+1);
            UpdateLayout();
            HwndRepaintNow(hwndIn);
            return 0;
        }
    }

    // right-click on the speed button cycles backwards
    if (WM_RBUTTONUP == msg) {
        Point pt = Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (ReadAloudPlaybackBarHitTest(rSpeed, pt)) {
            ReadAloudPlaybackCycleSpeed(-1);
            UpdateLayout();
            HwndRepaintNow(hwndIn);
            return 0;
        }
    }

    return WndProcDefault(hwndIn, msg, wp, lp);
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

void ReadAloudPlaybackBarRelayout(HWND hwndCanvas) {
    MainWindow* win = FindMainWindowByHwnd(hwndCanvas);
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    if (!IsWindowVisible(win->readAloudPlaybackBar->hwnd)) {
        return;
    }
    win->readAloudPlaybackBar->UpdateLayout();
    HwndRepaintNow(win->readAloudPlaybackBar->hwnd);
}

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab) {
    if (!tab || !tab->win || len(tab->readAloudText) == 0) {
        if (tab && tab->win) {
            ReadAloudPlaybackBarHide(tab->win);
        }
        return;
    }

    ReadAloudPlaybackBar* bar = ReadAloudPlaybackBarEnsure(tab->win);
    if (!bar) {
        return;
    }
    bar->SetSession(tab);

    // hide bars on other windows
    for (MainWindow* win : gWindows) {
        if (win != tab->win && win->readAloudPlaybackBar && IsWindowVisible(win->readAloudPlaybackBar->hwnd)) {
            ReadAloudPlaybackBarHide(win);
        }
    }
}