/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "SumatraConfig.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayMode.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Notifications.h"
#include "Menu.h"
#include "Toolbar.h"
#include "Theme.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Selection.h"
#include "SumatraPDF.h"
#include "ReadAloud.h"
#include "SumatraLog.h"

// Read-aloud, top to bottom:
//   1. text-to-speech backend: in shared/ReadAloud_win.cpp
//   2. mapping the spoken text back to page positions, for highlighting
//   3. the playback bar shown over the canvas
//   4. the session: what to read, chunking it, the menus that start it

// ---------------- 2. spoken text -> page positions (highlight) ----------------

void ReadAloudHighlightTimerStart(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    SetTimer(win->hwndCanvas, kReadAloudHighlightTimerID, kReadAloudHighlightDelayInMs, nullptr);
}

void ReadAloudHighlightTimerStop(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, kReadAloudHighlightTimerID);
}

static int gReadAloudPaintLogState = 0;

static void ReadAloudPaintLogOnce(int code, [[maybe_unused]] Str fmt) {
    if (gReadAloudPaintLogState == code) {
        return;
    }
    gReadAloudPaintLogState = code;
    dbgtts("%s\n", fmt);
}

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut) {
    if (!tab || !pageOut || !pageCountOut) {
        return false;
    }

    *pageOut = 0;
    *pageCountOut = 0;

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    *pageCountOut = dm->PageCount();

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        return false;
    }

    int absPos = -1;
    WindowTab* sourceTab = GetReadAloudSourceTab();
    if (sourceTab == tab && TtsIsSpeaking()) {
        int spokenPos = TtsGetSpokenPosUtf8();
        if (spokenPos >= 0) {
            absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
        }
    } else if (tab->readAloudResumePos >= 0) {
        absPos = tab->readAloudResumePos;
    } else if (tab->readAloudChunkEnd > 0) {
        absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    }

    if (absPos < 0 || absPos >= map->len) {
        return false;
    }

    int pageNo = dm->FindPageNoByLoc(map->locs[absPos].pageLoc);
    if (pageNo <= 0) {
        return false;
    }

    *pageOut = pageNo;
    return true;
}

static bool ReadAloudGetCurrentWordAbsRange(WindowTab* tab, int* startAbsOut, int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int spokenPos = TtsGetSpokenPosUtf8();
    if (spokenPos < 0) {
        return false;
    }

    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    Str chunkText = Str(tab->readAloudText.s + tab->readAloudChunkStart, chunkLen);
    int wordStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
    int wordEndAbs =
        tab->readAloudHighlightBase + tab->readAloudChunkStart + ReadAloudWordEndUtf8(chunkText, spokenPos);
    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        return false;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        return false;
    }

    *startAbsOut = wordStartAbs;
    *endAbsOut = wordEndAbs;
    return true;
}

static bool ReadAloudGetSentenceAbsRange(WindowTab* tab, int wordStartAbs, int wordEndAbs, int* startAbsOut,
                                         int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int rel = wordStartAbs - tab->readAloudHighlightBase;
    int sentRelStart = 0;
    int sentRelEnd = 0;
    if (!ReadAloudSentenceRange(tab->readAloudText, rel, &sentRelStart, &sentRelEnd)) {
        return false;
    }

    int chunkStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    int chunkEndAbs = chunkStartAbs + chunkLen;

    int startAbs = tab->readAloudHighlightBase + sentRelStart;
    int endAbs = tab->readAloudHighlightBase + sentRelEnd;
    startAbs = std::max(startAbs, chunkStartAbs);
    endAbs = std::min(endAbs, chunkEndAbs);
    startAbs = std::max(startAbs, 0);
    endAbs = std::min(endAbs, map->len);
    if (endAbs <= startAbs) {
        return false;
    }

    ReadAloudClampVisual(map, wordStartAbs, wordEndAbs, &startAbs, &endAbs);
    if (endAbs <= startAbs) {
        return false;
    }

    *startAbsOut = startAbs;
    *endAbsOut = endAbs;
    return true;
}

static bool ReadAloudGetCurrentWordScreenRect(MainWindow* win, Rect* rectOut) {
    if (!rectOut || !win) {
        return false;
    }

    *rectOut = Rect();

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        return false;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        return false;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    Rect unionRect;
    bool hasRect = false;
    for (int i = wordStartAbs; i < wordEndAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        // not laid out, e.g. its chapter is a placeholder after an EPUB restyle
        int pageNo = dm->FindPageNoByLoc(loc.pageLoc);
        if (pageNo < 1) {
            continue;
        }
        Rect sr = dm->CvtToScreen(pageNo, ToRectF(ReadAloudByteLocToRect(loc)));
        if (!hasRect) {
            unionRect = sr;
            hasRect = true;
        } else {
            unionRect = unionRect.Union(sr);
        }
    }

    if (!hasRect) {
        return false;
    }

    *rectOut = unionRect;
    return true;
}

static bool ReadAloudIsWordRectVisibleInViewport(MainWindow* win, const Rect& wordRect) {
    if (!win) {
        return false;
    }
    return !wordRect.Intersect(win->canvasRc).IsEmpty();
}

static bool ReadAloudIsWordRectFullyVisibleInViewport(MainWindow* win, const Rect& wordRect, int margin) {
    if (!win) {
        return false;
    }
    Rect canvas = win->canvasRc;
    if (wordRect.x < margin || wordRect.y < margin) {
        return false;
    }
    if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        return false;
    }
    if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        return false;
    }
    return true;
}

void ReadAloudOnUserViewChanged(MainWindow* win) {
    if (!win || win->readAloudScrollFromCode || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect) || !ReadAloudIsWordRectVisibleInViewport(win, wordRect)) {
        tab->readAloudAutoScroll = false;
        dbgtts("auto-scroll disabled (user scrolled away from highlight)\n");
    }
}

void ReadAloudUpdateAutoScroll(MainWindow* win) {
    if (!win || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }

    // In non-continuous modes (single page, facing, book view) the spoken word
    // can be on a page that isn't laid out, and scrolling cannot reach it, so
    // turn to that page first. The scrolling below still runs: zoomed in, the
    // word can be off screen on a page that is itself visible.
    if (!IsContinuous(dm->GetDisplayMode())) {
        int pageNo = 0;
        int pageCount = 0;
        if (ReadAloudGetProgressPage(tab, &pageNo, &pageCount) && !dm->PageVisible(pageNo)) {
            win->readAloudScrollFromCode = true;
            dm->GoToPage(pageNo, false);
            win->readAloudScrollFromCode = false;
        }
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect)) {
        return;
    }

    int margin = DpiScale(48);
    if (ReadAloudIsWordRectFullyVisibleInViewport(win, wordRect, margin)) {
        return;
    }

    Rect canvas = win->canvasRc;

    int dx = 0;
    int dy = 0;
    if (wordRect.y < margin) {
        dy = wordRect.y - margin;
    } else if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        dy = wordRect.y + wordRect.dy - (canvas.dy - margin);
    }
    if (wordRect.x < margin) {
        dx = wordRect.x - margin;
    } else if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        dx = wordRect.x + wordRect.dx - (canvas.dx - margin);
    }

    if (dx == 0 && dy == 0) {
        return;
    }

    int maxStep = std::max(canvas.dy / 4, DpiScale(120));
    dx = ClampI(dx, -maxStep, maxStep);
    dy = ClampI(dy, -maxStep, maxStep);

    win->readAloudScrollFromCode = true;
    win->MoveDocBy(dx, dy);
    win->readAloudScrollFromCode = false;
}

void PaintReadAloudHighlight(MainWindow* win, Gfx* gfx) {
    if (!TtsIsSpeaking()) {
        gReadAloudPaintLogState = 0;
        return;
    }
    if (!win) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        ReadAloudPaintLogOnce(1, StrL("PaintHighlight: no matching source tab"));
        return;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        ReadAloudPaintLogOnce(2, StrL("PaintHighlight: no highlight map"));
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        ReadAloudPaintLogOnce(3, StrL("PaintHighlight: tab is not a fixed-layout document"));
        return;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        if (gReadAloudPaintLogState != 4) {
            gReadAloudPaintLogState = 4;
            dbgtts("PaintHighlight: no spoken position (textLen=%d)\n", len(tab->readAloudText));
        }
        return;
    }

    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        ReadAloudPaintLogOnce(5, StrL("PaintHighlight: wordStartAbs out of range"));
        return;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        ReadAloudPaintLogOnce(6, StrL("PaintHighlight: empty word range"));
        return;
    }

    constexpr Color kSentenceCol = MkRgb(0x3b, 0x82, 0xf6);
    constexpr Color kWordCol = MkRgb(0xf5, 0x9e, 0x0b);
    int minThick = DpiScale(2);

    Vec<Rect> sentenceRects;
    int sentStartAbs = 0;
    int sentEndAbs = 0;
    if (ReadAloudGetSentenceAbsRange(tab, wordStartAbs, wordEndAbs, &sentStartAbs, &sentEndAbs)) {
        ReadAloudAppendUnderlines(dm, win->canvasRc, map, sentStartAbs, sentEndAbs, minThick, 10, sentenceRects);
    }
    if (len(sentenceRects) > 0) {
        gfx->FillRects(sentenceRects.els, len(sentenceRects), kSentenceCol);
    }

    Vec<Rect> wordRects;
    ReadAloudAppendUnderlines(dm, win->canvasRc, map, wordStartAbs, wordEndAbs, DpiScale(3), 7, wordRects);
    if (len(wordRects) == 0) {
        ReadAloudPaintLogOnce(7, StrL("PaintHighlight: no screen rects for current word"));
        return;
    }
    gfx->FillRects(wordRects.els, len(wordRects), kWordCol);
}

// ---------------- 3. playback bar ----------------

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
            return Tr("Selection");
        case WindowTab::ReadAloudScopeViewport:
            return Tr("Top of view");
        case WindowTab::ReadAloudScopeCursor:
            return Tr("From cursor");
        case WindowTab::ReadAloudScopeSmart:
        default:
            return Tr("Smart start");
    }
}

static TempStr ReadAloudPlaybackBarTextTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }

    Str docName = tab->GetTabTitle();
    if (len(docName) == 0) {
        docName = Tr("document");
    }

    int pageNo = 0;
    int pageCount = 0;
    bool hasPage = ReadAloudGetProgressPage(tab, &pageNo, &pageCount);
    Str scope = ReadAloudScopeLabel(tab);

    bool isPaused = CanContinueReadAloud(tab) && !TtsIsSpeaking();
    if (hasPage && pageCount > 0) {
        const char* pattern = isPaused ? Tr("Paused \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s
                                       : Tr("Reading \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s;
        return fmt(pattern, docName, pageNo, pageCount, scope);
    }
    const char* pattern = isPaused ? Tr("Paused \xC2\xB7 %s \xC2\xB7 %s").s : Tr("Reading \xC2\xB7 %s \xC2\xB7 %s").s;
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

static void RelayoutVisiblePlaybackBars() {
    for (MainWindow* win : gWindows) {
        ReadAloudPlaybackBar* bar = win->readAloudPlaybackBar;
        if (!bar || !bar->hwnd || !HwndIsVisible(bar->hwnd)) {
            continue;
        }
        bar->UpdateLayout(true);
        HwndInvalidate(bar->hwnd);
    }
}

static void OnSpeedSliderDrag(ReadAloudPlaybackBar* bar) {
    SyncSpeedLabel(bar);
    // "1x" vs "1.25x": the label's width changes, so relayout or it paints
    // over the status text until the window is resized
    bar->UpdateLayout(true);
}

static void OnSpeedSliderCommit(ReadAloudPlaybackBar* bar) {
    ReadAloudSetSpeedIdx(bar->speedSlider->value);
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

    btnStop = new VirtButton(Tr("Stop"), pf);
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
    btnPause->SetText(showResume ? Tr("Resume") : Tr("Pause"));
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

    // the spoken word's page, and its chapter location in the view's numbering
    WindowTab* srcTab = GetReadAloudSourceTab();
    int progressPage = 0;
    int progressPageCount = 0;
    Location progressLoc;
    if (srcTab && srcTab->AsFixed() && ReadAloudGetProgressPage(srcTab, &progressPage, &progressPageCount)) {
        PageInfo* pi = srcTab->AsFixed()->GetPageInfo(progressPage);
        progressLoc = pi ? pi->loc : kInvalidLocation;
    }
    out.Append(fmt("progress page=%d loc=%d:%d\n", progressPage, progressLoc.chapter, progressLoc.page));

    // pages the map covers: a complete map has one entry per page of its span
    ReadAloudHighlightMap* map = srcTab ? srcTab->readAloudHighlight : nullptr;
    Location mapStart;
    Location mapEnd;
    int mapPages = 0;
    for (int i = 0; map && i < map->len; i++) {
        Location l = map->locs[i].pageLoc;
        if (!l.IsValid() || l == mapEnd) {
            continue;
        }
        if (!mapStart.IsValid()) {
            mapStart = l;
        }
        mapEnd = l;
        mapPages++;
    }
    int mapSpan = 0;
    if (srcTab && srcTab->AsFixed() && mapPages > 0) {
        DisplayModel* srcDm = srcTab->AsFixed();
        mapSpan = srcDm->FindPageNoByLoc(mapEnd) - srcDm->FindPageNoByLoc(mapStart) + 1;
    }
    out.Append(fmt("map start=%d:%d end=%d:%d pages=%d span=%d\n", mapStart.chapter, mapStart.page, mapEnd.chapter,
                   mapEnd.page, mapPages, mapSpan));

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
    int labelIdealDx = bar->speedLabel ? bar->speedLabel->GetIdealSize().dx : 0;
    out.Append(fmt("speedLabelIdeal=%d\n", labelIdealDx));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", idx, ReadAloudSpeedCount(),
                   ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx))));
    Rect statusRc = bar->status ? bar->status->bounds : Rect{};
    out.Append(fmt("statusRect=%d,%d,%d,%d\n", statusRc.x, statusRc.y, statusRc.dx, statusRc.dy));
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

// ---------------- 4. read-aloud session ----------------

static WindowTab* gReadAloudSourceTab = nullptr;
static WindowTab* gReadAloudSessionTab = nullptr;

enum class SpeakChunkResult {
    Ok,
    Failed,
    TabGone
};

static HMENU gReadAloudAppSubmenu = nullptr;
static HMENU gReadAloudContextSubmenu = nullptr;

static void ReadAloudClearSourceTab();

void SetReadAloudAppSubmenu(HMENU menu) {
    gReadAloudAppSubmenu = menu;
}

HMENU GetReadAloudAppSubmenu() {
    return gReadAloudAppSubmenu;
}

bool IsReadAloudAppSubmenu(HMENU menu) {
    return menu && menu == gReadAloudAppSubmenu;
}

void SetReadAloudContextSubmenu(HMENU menu) {
    gReadAloudContextSubmenu = menu;
}

bool IsReadAloudContextSubmenu(HMENU menu) {
    return menu && menu == gReadAloudContextSubmenu;
}

HMENU GetReadAloudContextSubmenu() {
    return gReadAloudContextSubmenu;
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg);

static void ReadAloudSaveVoicePref(Str voiceId) {
    if (!gSettings) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->readAloudVoiceId, voiceId);
    ScheduleSaveSettings();
}

// WinRT speech synthesis is too slow for whole-document requests; speak in chunks.
static constexpr int kReadAloudMaxChunkLen = 1024;

static bool IsReadAloudSentencePunct(int c) {
    return c == '.' || c == '!' || c == '?' || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0x2026;
}

// Prefer a sentence end in the window so TTS does not drop intonation mid-clause
// (issue #6110). Skip "e.g. the" (lowercase after the period). Else last space.
static int ReadAloudFindChunkEnd(Str text, int start, int maxLen) {
    int textLen = text.len;
    if (start >= textLen) {
        return textLen;
    }

    int limit = start + maxLen;
    if (limit >= textLen) {
        return textLen;
    }

    int bestSent = start;
    int bestSpace = start;
    int i = start;
    while (i < limit) {
        int c = Utf8CodepointNext(text, i);
        if (c == ' ' || c == '\t') {
            bestSpace = i;
            continue;
        }
        if (!IsReadAloudSentencePunct(c)) {
            continue;
        }

        int after = i;
        while (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (!IsReadAloudCloser(d)) {
                break;
            }
            after = t;
        }
        while (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (d != ' ' && d != '\t') {
                break;
            }
            after = t;
        }
        if (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (d >= 'a' && d <= 'z') {
                continue;
            }
        }
        if (after > start && after <= limit) {
            bestSent = after;
        }
    }

    if (bestSent > start) {
        return bestSent;
    }
    if (bestSpace > start) {
        return bestSpace;
    }
    return limit;
}

static void ReadAloudQueueNext(WindowTab* tab) {
    if (!tab || tab->readAloudQueuedEnd > 0 || len(tab->readAloudText) == 0) {
        return;
    }
    if (tab->readAloudChunkEnd >= tab->readAloudText.len) {
        return;
    }

    int start = tab->readAloudChunkEnd;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return;
    }

    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, end - start));
    if (!TtsQueueUtf8(chunk) || !IsWindowTabValid(tab)) {
        return;
    }
    tab->readAloudQueuedEnd = end;
    dbgtts("queue-next %d..%d of %d\n", start, end, tab->readAloudText.len);
}

static void ReadAloudOnQueuedStarted(WindowTab* tab) {
    if (!tab || tab->readAloudQueuedEnd <= tab->readAloudChunkEnd) {
        tab->readAloudQueuedEnd = 0;
        return;
    }
    tab->readAloudChunkStart = tab->readAloudChunkEnd;
    tab->readAloudChunkEnd = tab->readAloudQueuedEnd;
    tab->readAloudQueuedEnd = 0;
    dbgtts("queued-now %d..%d of %d\n", tab->readAloudChunkStart, tab->readAloudChunkEnd, tab->readAloudText.len);
}

// Promote a prefetched chunk and start the next prefetch.
// a voice engine thread crashed and was ended: stop reading, forget that voice
static void ReadAloudOnEngineCrash() {
    if (!TtsTakeEngineCrash()) {
        return;
    }
    WindowTab* tab = GetReadAloudSourceTab();
    ReadAloudPlaybackStop();
    ReadAloudSaveVoicePref({});
    TtsSetVoiceById(StrL(""));
    if (tab && tab->win) {
        ReadAloudShowNotif(tab, Tr("Read aloud voice crashed and was turned off"));
    }
}

void ReadAloudAfterTtsEvents() {
    ReadAloudOnEngineCrash();
    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab) {
        return;
    }
    if (TtsDidStartQueued()) {
        ReadAloudOnQueuedStarted(tab);
    }
    if (TtsIsSpeaking()) {
        ReadAloudQueueNext(tab);
    }
}

static bool ReadAloudHasMoreChunks(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    return tab->readAloudChunkEnd < tab->readAloudText.len;
}

static void ReadAloudFinishSession(WindowTab* tab, MainWindow* win) {
    if (!tab) {
        return;
    }

    dbgtts("finish-session\n");
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarHide(tab->win);
    }
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    ReadAloudClearSourceTab();
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (win) {
        ToolbarUpdateStateForWindow(win, true);
    }
}

static SpeakChunkResult ReadAloudSpeakChunk(WindowTab* tab, Str errMsg) {
    if (!tab || len(tab->readAloudText) == 0) {
        return SpeakChunkResult::Failed;
    }

    int start = tab->readAloudChunkEnd;
    int textLen = tab->readAloudText.len;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return SpeakChunkResult::Failed;
    }

    int chunkLen = end - start;
    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, (int)((size_t)chunkLen)));
    // the backend call pumps messages (COM's modal loop), so the user can close
    // the tab's window inside it; ~WindowTab already reset the session then
    bool ok = TtsSpeakUtf8(chunk);
    if (!IsWindowTabValid(tab)) {
        logf("tts: SpeakChunk: tab closed during speak\n");
        if (ok) {
            TtsStop();
        }
        return SpeakChunkResult::TabGone;
    }
    if (!ok) {
        logf("tts: SpeakChunk: TtsSpeakUtf8 failed\n");
        dbgtts("chunk speak failed %d..%d of %d\n", start, end, textLen);
        ReadAloudShowNotif(tab, errMsg);
        return SpeakChunkResult::Failed;
    }
    dbgtts("chunk %d..%d of %d mapBase=%d\n", start, end, textLen, tab->readAloudHighlightBase);

    tab->readAloudChunkStart = start;
    tab->readAloudChunkEnd = end;
    tab->readAloudQueuedEnd = 0;
    ReadAloudQueueNext(tab);
    if (!IsWindowTabValid(tab)) {
        logf("tts: SpeakChunk: tab closed during queue\n");
        TtsStop();
        return SpeakChunkResult::TabGone;
    }
    ToolbarUpdateStateForWindow(tab->win, true);
    HwndInvalidate(tab->win->hwndCanvas);
    return SpeakChunkResult::Ok;
}

// Text cleanup for speech
static TempStr CleanReadAloudTextTemp(Str text) {
    if (len(text) == 0) {
        return {};
    }

    str::Builder out;
    int i = 0;
    bool lastWasSpace = false;

    while (i < text.len) {
        char c = text.s[i];

        // Remove likely soft hyphenation caused by PDF line wrapping:
        // "cap-\nturing" -> "capturing"
        //
        // Conservative rule: only join lowercase ASCII on both sides.
        // This avoids damaging many intentional hyphen cases.
        if (c == '-' && i + 1 < text.len && IsReadAloudLineBreak(text.s[i + 1])) {
            int after = i + 1;

            while (after < text.len && IsReadAloudLineBreak(text.s[after])) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(text.s[i - 1]);
            bool nextIsLower = after < text.len && IsReadAloudLowerAscii(text.s[after]);

            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        // Convert extracted visual line breaks into spaces.
        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;

            while (i < text.len && IsReadAloudLineBreak(text.s[i])) {
                if (text.s[i] == '\n') {
                    lineBreaks++;
                }
                i++;
            }

            while (i < text.len && IsReadAloudHorizontalSpace(text.s[i])) {
                i++;
            }

            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            // Keep a slightly stronger pause for paragraph breaks.
            if (lineBreaks >= 2) {
                out.AppendChar(' ');
            }

            continue;
        }

        // Collapse spaces and tabs.
        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            i++;
            continue;
        }

        out.AppendChar(c);
        lastWasSpace = false;
        i++;
    }

    if (len(out) == 0) {
        return {};
    }
    return ToStrTemp(out);
}

// Read-aloud lifetime and commands
static void ReadAloudSetSourceTab(WindowTab* tab) {
    gReadAloudSourceTab = tab;
}

static void ReadAloudClearSourceTab() {
    gReadAloudSourceTab = nullptr;
}

static void StopReadAloudIfSourceTab(WindowTab* tab) {
    if (!tab || gReadAloudSourceTab != tab) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
    }
    ReadAloudClearSourceTab();
}

// last-resort guard: whatever a close path forgets, a tab that is being
// destroyed can't be left pointed at by the read-aloud state
void ReadAloudForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (gReadAloudSourceTab == tab) {
        TtsStop();
        gReadAloudSourceTab = nullptr;
    }
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    for (MainWindow* win : gWindows) {
        ReadAloudPlaybackBarForgetTab(win, tab);
    }
}

void StopReadAloudIfSourceWindow(MainWindow* win) {
    if (!win || !gReadAloudSourceTab || gReadAloudSourceTab->win != win) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    ReadAloudClearSourceTab();
}

// reset "Continue reading" state, called when its document goes away
void ResetReadAloudStateForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    StopReadAloudIfSourceTab(tab);
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
    }
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (tab->win) {
        ReadAloudPlaybackBarHide(tab->win);
    }
}

// stop reading and remember where we stopped so that "Continue reading"
// can pick up from there
void ReadAloudStopRememberPos() {
    // drain pending word-boundary events for an accurate position
    TtsProcessEvents();
    WindowTab* tab = gReadAloudSourceTab;
    if (tab && TtsIsSpeaking()) {
        int pos = TtsGetSpokenPosUtf8();
        int absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + (pos >= 0 ? pos : 0);
        int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
        if (absPos >= 0 && absPos < maxPos) {
            tab->readAloudResumePos = absPos;
        }
    }
    TtsStop();
    ReadAloudClearSourceTab();
    if (tab && tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarUpdateSession(tab);
    }
}

void ReadAloudPlaybackPauseOrResume() {
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    dbgtts("pause-or-resume speaking=%d session=%d source=%d canContinue=%d\n", (int)TtsIsSpeaking(), tab ? 1 : 0,
           GetReadAloudSourceTab() ? 1 : 0, tab ? (int)CanContinueReadAloud(tab) : 0);
    if (!tab || !tab->win) {
        return;
    }

    if (TtsIsSpeaking() && GetReadAloudSourceTab() == tab) {
        dbgtts("pause now\n");
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(tab->win, true);
    } else if (CanContinueReadAloud(tab)) {
        dbgtts("resume now\n");
        ReadAloudContinueInTab(tab);
    }
}

// preset playback speeds offered in the Speed menu and cycled by the speed
// button on the playback bar
constexpr float kReadAloudSpeeds[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.25f, 2.5f, 2.75f, 3.0f, 3.25f, 3.5f};

// e.g. "1x", "0.75x", "1.5x"
TempStr ReadAloudSpeedLabelTemp(float speed) {
    int hundredths = (int)lroundf(speed * 100.0f);
    int whole = hundredths / 100;
    int frac = hundredths % 100;
    if (frac == 0) {
        return fmt("%dx", whole);
    }
    if (frac % 10 == 0) {
        return fmt("%d.%dx", whole, frac / 10);
    }
    return fmt("%d.%02dx", whole, frac);
}

int ReadAloudSpeedCount() {
    return dimofi(kReadAloudSpeeds);
}

float ReadAloudSpeedAt(int idx) {
    int n = dimofi(kReadAloudSpeeds);
    idx = ClampI(idx, 0, n - 1);
    return kReadAloudSpeeds[idx];
}

int ReadAloudClosestSpeedIdx() {
    float curr = TtsGetSpeed();
    int idx = 0;
    float bestDist = -1;
    for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
        float dist = kReadAloudSpeeds[i] - curr;
        if (dist < 0) {
            dist = -dist;
        }
        if (bestDist < 0 || dist < bestDist) {
            bestDist = dist;
            idx = i;
        }
    }
    return idx;
}

static void ReadAloudSetSpeed(float speed) {
    TtsSetSpeed(speed);
    gSettings->readAloudSpeed = TtsGetSpeed();
    dbgtts("SetSpeed: %s\n", ReadAloudSpeedLabelTemp(TtsGetSpeed()));
    ScheduleSaveSettings();
    RelayoutVisiblePlaybackBars();

    // the WinRT backend applies the new speed only to newly synthesized
    // chunks, so re-speak from the current position
    WindowTab* tab = GetReadAloudSourceTab();
    if (tab && TtsIsSpeaking()) {
        ReadAloudStopRememberPos();
        if (CanContinueReadAloud(tab)) {
            ReadAloudContinueInTab(tab);
        }
    }
}

// dir is +1 (next speed) or -1 (previous speed), wraps around
void ReadAloudSetSpeedIdx(int idx) {
    ReadAloudSetSpeed(ReadAloudSpeedAt(idx));
}

void ReadAloudPlaybackCycleSpeed(int dir) {
    int n = dimofi(kReadAloudSpeeds);
    int idx = (ReadAloudClosestSpeedIdx() + dir + n) % n;
    ReadAloudSetSpeed(kReadAloudSpeeds[idx]);
}

void ReadAloudPlaybackStop() {
    dbgtts("stop-click\n");
    // always halt TTS, even if the session tab is already gone (issue #6053)
    TtsStop();
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    if (!tab) {
        ReadAloudClearSourceTab();
        for (MainWindow* w : gWindows) {
            ReadAloudPlaybackBarHide(w);
        }
        return;
    }
    ReadAloudFinishSession(tab, tab->win);
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg) {
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.msg = msg;
    args.timeoutMs = 2000;
    ShowNotification(args);
}

// remembers cleaned text on the tab and starts speaking it in TTS-sized chunks
static void ReadAloudStartText(WindowTab* tab, Str cleaned, ReadAloudHighlightMap* newMap, int highlightBase,
                               Str errMsg) {
    if (len(cleaned) == 0) {
        logf("tts: StartText: empty cleaned text\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int cleanedLen = cleaned.len;
    int mapLen = newMap ? newMap->len : -1;
    dbgtts("StartText: cleanedLen=%d mapLen=%d highlightBase=%d\n", cleanedLen, mapLen, highlightBase);

    if (newMap) {
        if (!tab->readAloudHighlight) {
            tab->readAloudHighlight = new ReadAloudHighlightMap{};
        }
        ReadAloudHighlightFree(tab->readAloudHighlight);
        if (newMap->len > 0 && newMap->locs) {
            *tab->readAloudHighlight = *newMap;
            newMap->locs = nullptr;
            newMap->len = 0;
            newMap->cap = 0;
        } else {
            dbgtts("StartText: highlight map empty (len=%d locs=%p)\n", newMap->len, newMap->locs);
        }
    } else if (highlightBase == 0 && tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }

    str::ReplaceWithCopy(&tab->readAloudText, cleaned);
    tab->readAloudHighlightBase = highlightBase;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    gReadAloudSessionTab = tab;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    SpeakChunkResult res = ReadAloudSpeakChunk(tab, errMsg);
    if (res == SpeakChunkResult::TabGone) {
        return;
    }
    if (res == SpeakChunkResult::Failed) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
}

static void ReadAloudStartFromViewportTop(WindowTab* tab, Str errMsg) {
    dbgtts("StartFromViewportTop\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("tts: StartFromViewportTop: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetViewportStart(dm, &startPage, &startGlyph)) {
        logf("tts: StartFromViewportTop: GetViewportStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("tts: StartFromViewportTop: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudStartFromSelection(WindowTab* tab, Str errMsg) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm || dm->textSelection->result.len <= 0) {
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromTextSelection(dm->textSelection, &map, cleaned)) {
        bool isTextOnlySelection = false;
        TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelection);
        TempStr cleanedStr = CleanReadAloudTextTemp(text);
        ReadAloudStartText(tab, cleanedStr, nullptr, 0, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

void ReadAloudInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("tts: InTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: InTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    bool isTextOnlySelection = false;
    TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelection);

    if (len(text) > 0 && isTextOnlySelection) {
        dbgtts("InTab: using selection path (len=%d)\n", len(text));
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromSelection(tab, Tr("No text available to read aloud"));
    } else {
        dbgtts("InTab: using viewport-top path (hasSelection=%d isTextOnly=%d)\n", len(text) > 0, isTextOnlySelection);
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromViewportTop(tab, Tr("No text available to read aloud"));
    }
}

static void ReadAloudStartFromCursor(WindowTab* tab, Point screenPt, Str errMsg) {
    dbgtts("StartFromCursor\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("tts: StartFromCursor: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetCursorStart(dm, screenPt, &startPage, &startGlyph)) {
        logf("tts: StartFromCursor: GetCursorStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("tts: StartFromCursor: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt) {
    if (!tab || !tab->win) {
        logf("tts: FromCursorInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: FromCursorInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeCursor;
    ReadAloudStartFromCursor(tab, screenPt, Tr("No text available to read aloud"));
}

void ReadAloudFromViewportTopInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("tts: FromViewportTopInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: FromViewportTopInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeViewport;
    ReadAloudStartFromViewportTop(tab, Tr("No text available to read aloud"));
}

void ReadAloudSelectionInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: SelectionInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeSelection;
    ReadAloudStartFromSelection(tab, Tr("No text available to read aloud"));
}

// true if read aloud was paused and can be resumed in this tab
bool CanContinueReadAloud(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    int pos = tab->readAloudResumePos;
    int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
    return pos >= 0 && pos < maxPos;
}

void ReadAloudContinueInTab(WindowTab* tab) {
    if (!CanContinueReadAloud(tab) || !tab->win) {
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: ContinueInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    int resumeInText = tab->readAloudResumePos - tab->readAloudHighlightBase;
    tab->readAloudChunkEnd = resumeInText;
    tab->readAloudChunkStart = resumeInText;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    SpeakChunkResult res = ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"));
    if (res == SpeakChunkResult::TabGone) {
        return;
    }
    if (res == SpeakChunkResult::Failed) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
}

WindowTab* GetReadAloudSourceTab() {
    return gReadAloudSourceTab;
}

// Voice selection menu
static TempStr TtsLangIdToLocaleNameTemp(Str lang) {
    if (len(lang) == 0) {
        return str::DupTemp(StrL("unknown"));
    }

    // Windows.Media.SpeechSynthesis voices report a locale name like "en-US",
    // SAPI voices a hex language id like "409"
    if (str::ContainsChar(lang, '-')) {
        return str::DupTemp(lang);
    }

    char* langZ = CStrTemp(lang);
    char* end = nullptr;
    unsigned long langId = strtoul(langZ, &end, 16);
    if (end == langZ || langId == 0) {
        return str::DupTemp(lang);
    }

    WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = {};
    int n = LCIDToLocaleName((LCID)langId, localeName, dimof(localeName), 0);
    if (n <= 0) {
        return str::DupTemp(lang);
    }

    return ToUtf8Temp(localeName);
}

static void BuildReadAloudVoiceMenuItems(HMENU voiceMenu) {
    if (!voiceMenu) {
        return;
    }

    Str currentVoiceId = TtsGetVoiceId();

    UINT defaultFlags = MF_STRING;
    if (len(currentVoiceId) == 0) {
        defaultFlags |= MF_CHECKED;
    }

    AppendMenuW(voiceMenu, defaultFlags, CmdTtsVoiceDefault, L"System default");
    AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);

    Vec<TtsVoiceInfo> voices = TtsGetVoices();

    Str lastLang = {};

    UINT cmd = CmdTtsVoiceFirst;
    for (TtsVoiceInfo& voice : voices) {
        if (cmd > CmdTtsVoiceLast) {
            break;
        }

        Str lang = len(voice.lang) == 0 ? StrL("") : voice.lang;

        if (lastLang && !str::EqI(lastLang, lang)) {
            AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);
        }

        UINT flags = MF_STRING;
        if (str::Eq(voice.id, currentVoiceId)) {
            flags |= MF_CHECKED;
        }

        TempStr localeName = TtsLangIdToLocaleNameTemp(voice.lang);
        TempStr label = fmt("%s - %s", voice.name, localeName);
        AppendMenuW(voiceMenu, flags, cmd, CWStrTemp(label));

        lastLang = lang;
        cmd++;
    }

    TtsFreeVoices(voices);
    RemoveBadMenuSeparators(voiceMenu);
}

static void BuildReadAloudMenuItems(HMENU menu, MainWindow* win, bool includeCursorItem, bool canReadFromCursor) {
    WindowTab* currTab = win ? win->CurrentTab() : nullptr;
    bool isSpeaking = TtsIsSpeaking();
    bool canContinue = CanContinueReadAloud(currTab);
    bool hasSelection = currTab && win->showSelection && currTab->selectionOnPage && len(*currTab->selectionOnPage) > 0;

    if (isSpeaking) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuPauseReading, CWStrTemp(Tr("Pause Reading")));
    } else if (canContinue) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuContinueReading, CWStrTemp(Tr("Continue Reading")));
    }
    // always listed: the playback bar can be off-screen or lose z-order (issue #6053)
    UINT stopFlags = MF_STRING;
    if (!isSpeaking && !canContinue) {
        stopFlags |= MF_GRAYED;
    }
    AppendMenuW(menu, stopFlags, CmdTtsMenuStopReading, CWStrTemp(Tr("Stop Reading")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CmdTtsMenuReadCurrentPage, CWStrTemp(Tr("Start Reading From Top")));
    if (includeCursorItem) {
        AppendMenuW(menu, canReadFromCursor ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadFromCursor,
                    CWStrTemp(Tr("Start Reading From Cursor Position")));
    }
    AppendMenuW(menu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadSelection,
                CWStrTemp(Tr("Start Reading Selection")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    HMENU voiceMenu = CreatePopupMenu();
    if (voiceMenu) {
        BuildReadAloudVoiceMenuItems(voiceMenu);
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)voiceMenu, CWStrTemp(Tr("Voice")));
    }

    HMENU speedMenu = CreatePopupMenu();
    if (speedMenu) {
        int currIdx = ReadAloudClosestSpeedIdx();
        for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
            UINT flags = MF_STRING;
            if (i == currIdx) {
                flags |= MF_CHECKED;
            }
            TempStr label = ReadAloudSpeedLabelTemp(kReadAloudSpeeds[i]);
            AppendMenuW(speedMenu, flags, CmdTtsSpeedFirst + (UINT)i, CWStrTemp(label));
        }
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)speedMenu, CWStrTemp(Tr("Speed")));
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT showFlags = MF_STRING;
    if (gSettings->toolbarShowReadAloud) {
        showFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, showFlags, CmdToggleToolbarShowReadAloud, CWStrTemp(Tr("Show In Toolbar")));
}

void RebuildReadAloudMenu(MainWindow* win, HMENU menu, bool includeCursorItem, bool canReadFromCursor) {
    if (!menu || !win) {
        return;
    }
    MenuEmpty(menu);
    BuildReadAloudMenuItems(menu, win, includeCursorItem, canReadFromCursor);
    RemoveBadMenuSeparators(menu);
}

static void HandleReadAloudMenuSelection(MainWindow* win, UINT selected) {
    if (!win || selected == 0) {
        return;
    }

    WindowTab* currTab = win->CurrentTab();

    if (selected == CmdTtsMenuPauseReading) {
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(win, true);
    } else if (selected == CmdTtsMenuStopReading) {
        ReadAloudPlaybackStop();
    } else if (selected == CmdTtsMenuReadCurrentPage) {
        if (currTab) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromViewportTopInTab(currTab);
        }
    } else if (selected == CmdTtsMenuReadFromCursor) {
        if (currTab && win->contextMenuPtValid) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromCursorInTab(currTab, win->contextMenuPt);
        }
    } else if (selected == CmdTtsMenuContinueReading) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudContinueInTab(currTab);
    } else if (selected == CmdTtsMenuReadSelection) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudSelectionInTab(currTab);
    } else if (selected == CmdTtsVoiceDefault) {
        if (TtsSetVoiceById(StrL(""))) {
            ReadAloudSaveVoicePref(StrL(""));
        }
    } else if (selected >= CmdTtsVoiceFirst && selected <= CmdTtsVoiceLast) {
        Vec<TtsVoiceInfo> voices = TtsGetVoices();
        int voiceIndex = (int)(selected - CmdTtsVoiceFirst);
        if (voiceIndex >= 0 && voiceIndex < len(voices)) {
            if (TtsSetVoiceById(voices[voiceIndex].id)) {
                ReadAloudSaveVoicePref(voices[voiceIndex].id);
            }
        }
        TtsFreeVoices(voices);
    } else if (selected >= CmdTtsSpeedFirst && selected <= CmdTtsSpeedLast) {
        int speedIndex = (int)(selected - CmdTtsSpeedFirst);
        if (speedIndex >= 0 && speedIndex < dimofi(kReadAloudSpeeds)) {
            ReadAloudSetSpeed(kReadAloudSpeeds[speedIndex]);
        }
    }
}

bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId) {
    if (cmdId == CmdTtsVoiceDefault || (cmdId >= CmdTtsMenuReadCurrentPage && cmdId <= CmdTtsMenuStopReading) ||
        (cmdId >= CmdTtsVoiceFirst && cmdId <= CmdTtsVoiceLast) ||
        (cmdId >= CmdTtsSpeedFirst && cmdId <= CmdTtsSpeedLast)) {
        HandleReadAloudMenuSelection(win, (UINT)cmdId);
        return true;
    }
    return false;
}

// the menu shown by the dropdown arrow on the Read Aloud toolbar button
void ShowTtsVoiceMenu(MainWindow* win, Rect buttonScreen) {
    if (!win || buttonScreen.IsEmpty()) {
        return;
    }

    RECT rc = ToRECT(buttonScreen);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    BuildReadAloudMenuItems(menu, win, false, false);

    UINT selected = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD, rc.left, rc.bottom, 0, win->hwndFrame, nullptr);
    // the click that dismissed the menu is delivered to the toolbar afterwards;
    // this is what makes the toolbar ignore it instead of re-opening the menu
    ToolbarNoteDropdownClosed();

    DestroyMenu(menu);
    if (selected == 0) {
        return;
    }
    // the menu also carries real Cmd* ids (Show In Toolbar), which
    // HandleReadAloudMenuSelection knows nothing about - let the frame have them
    if (HandleReadAloudMenuCommand(win, (int)selected)) {
        return;
    }
    HwndSendCommand(win->hwndFrame, (int)selected);
}

// handles kWmTtsEvent posted by the tts backend
void ReadAloudOnTtsEvent(MainWindow* win) {
    TtsProcessEvents();
    ReadAloudAfterTtsEvents();

    WindowTab* tab = gReadAloudSourceTab;

    if (TtsIsSpeaking() && tab && tab->win) {
        HwndInvalidate(tab->win->hwndCanvas);
        // Tick, not UpdateSession: relayout rebuilds virt tops and
        // clears pressed, so Pause/Stop/Speed mouse-up is lost (issue #6106)
        dbgtts("event speaking pos=%d\n", TtsGetSpokenPosUtf8());
        ReadAloudPlaybackBarTick(tab->win);
    }

    // also gets here for word boundary events while still speaking;
    // only the end of speech needs handling
    if (TtsIsSpeaking() || !tab) {
        return;
    }
    dbgtts("event idle hasMore=%d chunkEnd=%d textLen=%d\n", (int)ReadAloudHasMoreChunks(tab), tab->readAloudChunkEnd,
           tab->readAloudText.len);
    if (ReadAloudHasMoreChunks(tab)) {
        SpeakChunkResult res = ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"));
        if (res != SpeakChunkResult::Failed) {
            return;
        }
    }
    ReadAloudFinishSession(tab, win);
}
