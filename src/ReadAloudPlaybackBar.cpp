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
#include "Commands.h"
#include "AudiobookCharacters.h"
#include "Theme.h"
#include "ReadAloudPlaybackBar.h"

using Gdiplus::Graphics;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;

// The transport buttons, in the order they appear.
enum RaBtn {
    kBtnRestart = 0,   // <<<  play from the beginning
    kBtnPrev,          // <<   previous sentence
    kBtnPause,         // ||   pause
    kBtnPlay,          // >    play / resume
    kBtnStop,          // []   stop (remembers the place)
    kBtnNext,          // >>   skip a sentence
    kBtnPage,          // >>>  skip a page
    kBtnSpeed,         // 1.0x (Windows TTS only - Chatterbox has no rate control)
    kBtnCount
};

struct ReadAloudPlaybackBar : Wnd {
    ReadAloudPlaybackBar() = default;
    ~ReadAloudPlaybackBar() override = default;

    HWND Create(HWND parentCanvas);
    void SetSession(WindowTab* tab);
    void UpdateLayout();
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    void OnPaint(HDC hdc, PAINTSTRUCT* ps) override;
    void OnClick(int btn);

    WindowTab* sessionTab = nullptr;
    Rect rBtn[kBtnCount];
    bool enabled[kBtnCount] = {};
    int btnsEndX = 0;           // where the buttons stop and the status text starts
    bool showResume = false;
    bool isAudiobook = false;   // Chatterbox engine, not Windows TTS
};

// Labels are drawn as text, so they must exist in the UI font. Segoe UI has
// these; the media glyphs (â®ï¸ etc) are emoji-font only and render as boxes.
static const char* kBtnLabels[kBtnCount] = {
    "|<<", "<<", "||", ">", "[]", ">>", ">>|",
    nullptr /* speed: label is computed */,
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

    if (AudiobookIsRunning()) {
        // the engine owns the position; page/scope here are Windows TTS state
        // and would be wrong for it
        return fmt(_TRA("Reading \xC2\xB7 %s \xC2\xB7 character voices").s, docName);
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
    args.font = GetAppBiggerFont(parentCanvas);
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

    isAudiobook = AudiobookIsRunning();
    showResume = !isAudiobook && CanContinueReadAloud(tab) && !TtsIsSpeaking();

    // Chatterbox reads a unit at a time so it can seek; it has no rate control.
    // Windows TTS is the opposite: it can change speed but not step by sentence.
    for (int i = 0; i < kBtnCount; i++) {
        enabled[i] = true;
    }
    enabled[kBtnRestart] = true;
    enabled[kBtnPrev] = isAudiobook;
    enabled[kBtnNext] = isAudiobook;
    enabled[kBtnPage] = isAudiobook;
    enabled[kBtnSpeed] = !isAudiobook;

    UpdateLayout();
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    HwndRepaintNow(hwnd);
}

void ReadAloudPlaybackBar::OnClick(int btn) {
    if (isAudiobook) {
        switch (btn) {
            case kBtnRestart:
                AudiobookSendCommand(StrL("/restart"));
                break;
            case kBtnPrev:
                AudiobookSendCommand(StrL("/prev"));
                break;
            case kBtnPause:
                AudiobookSendCommand(StrL("/pause"));
                break;
            case kBtnPlay:
                AudiobookSendCommand(StrL("/resume"));
                break;
            case kBtnStop:
                // stops and remembers the place; the frame clears the highlight
                HwndSendCommand(GetParent(GetParent(hwnd)), CmdStopReadAloud);
                return;
            case kBtnNext:
                AudiobookSendCommand(StrL("/next"));
                break;
            case kBtnPage:
                AudiobookSendCommand(StrL("/page"), StrL("{\"dir\":1}"));
                break;
        }
        HwndRepaintNow(hwnd);
        return;
    }

    // Windows TTS
    switch (btn) {
        case kBtnRestart:
            HwndSendCommand(GetParent(GetParent(hwnd)), CmdReadAloudFromTopPage);
            break;
        case kBtnPause:
            if (!showResume) {
                ReadAloudPlaybackPauseOrResume();
            }
            break;
        case kBtnPlay:
            if (showResume) {
                ReadAloudPlaybackPauseOrResume();
            }
            break;
        case kBtnStop:
            ReadAloudPlaybackStop();
            break;
        case kBtnSpeed:
            ReadAloudPlaybackCycleSpeed(+1);
            UpdateLayout();
            HwndRepaintNow(hwnd);
            break;
    }
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

    TempStr status = ReadAloudPlaybackBarTextTemp(sessionTab);
    TempStr speedLabel = SpeedLabelTemp();

    HDC hdc = GetDC(hwnd);
    Size szStatus = HdcMeasureText(hdc, status, DT_SINGLELINE | DT_NOPREFIX, font);
    int btnDx[kBtnCount] = {};
    int btnDy = 0;
    for (int i = 0; i < kBtnCount; i++) {
        if (!enabled[i]) {
            continue;
        }
        Str label = (i == kBtnSpeed) ? Str(speedLabel) : Str(kBtnLabels[i]);
        Size sz = HdcMeasureText(hdc, label, DT_SINGLELINE | DT_NOPREFIX, font);
        btnDx[i] = sz.dx + 2 * btnPadX;
        btnDy = std::max(btnDy, sz.dy);
    }
    ReleaseDC(hwnd, hdc);
    btnDy += padY;

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
    int cur = isRtl ? (barDx - padX) : padX;
    for (int i = 0; i < kBtnCount; i++) {
        if (!enabled[i]) {
            rBtn[i] = {};
            continue;
        }
        if (isRtl) {
            cur -= btnDx[i];
            rBtn[i] = {cur, rowY, btnDx[i], btnDy};
            cur -= btnGap;
        } else {
            rBtn[i] = {cur, rowY, btnDx[i], btnDy};
            cur += btnDx[i] + btnGap;
        }
    }
    btnsEndX = cur;

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
    Rect rTxt;
    int rowY = rBtn[kBtnStop].y;
    int rowDy = rBtn[kBtnStop].dy;
    if (IsUIRtl()) {
        rTxt = {padX, rowY, btnsEndX - padX, rowDy};
    } else {
        rTxt = {btnsEndX, rowY, rc.dx - btnsEndX - padX, rowDy};
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

    TempStr speedLabel = SpeedLabelTemp();
    for (int i = 0; i < kBtnCount; i++) {
        if (!enabled[i]) {
            continue;
        }
        Str label = (i == kBtnSpeed) ? Str(speedLabel) : Str(kBtnLabels[i]);
        drawBtn(rBtn[i], label);
    }

    buffer.Flush(hdcIn);
}

LRESULT ReadAloudPlaybackBar::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_SETCURSOR == msg) {
        Point pt = HwndGetCursorPos(hwndIn);
        for (int i = 0; i < kBtnCount; i++) {
            if (enabled[i] && ReadAloudPlaybackBarHitTest(rBtn[i], pt)) {
                SetCursorCached(IDC_HAND);
                return TRUE;
            }
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
        for (int i = 0; i < kBtnCount; i++) {
            if (enabled[i] && ReadAloudPlaybackBarHitTest(rBtn[i], pt)) {
                OnClick(i);
                return 0;
            }
        }
    }

    // right-click on the speed button cycles backwards; on skip-page it goes back
    if (WM_RBUTTONUP == msg) {
        Point pt = Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (enabled[kBtnSpeed] && ReadAloudPlaybackBarHitTest(rBtn[kBtnSpeed], pt)) {
            ReadAloudPlaybackCycleSpeed(-1);
            UpdateLayout();
            HwndRepaintNow(hwndIn);
            return 0;
        }
        if (enabled[kBtnPage] && ReadAloudPlaybackBarHitTest(rBtn[kBtnPage], pt)) {
            AudiobookSendCommand(StrL("/page"), StrL("{\"dir\":-1}"));
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
    // readAloudText is Windows TTS's session state and stays empty for the
    // Chatterbox engine, which keeps the text in its own process - so ask it
    // whether it's reading rather than infer from TTS state.
    bool audiobook = AudiobookIsRunning();
    if (!tab || !tab->win || (!audiobook && len(tab->readAloudText) == 0)) {
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
