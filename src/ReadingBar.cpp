/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Gfx.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "FindBar.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"

// Horizontal reading guide: a viewport band (Skim / #5771 / #3389). Highlight
// fills it; Invert dims the rest. Per-tab on/yFrac; color, invert, height in
// gSettings->readingBar.

constexpr int kDefaultHeight96 = 48;
constexpr int kMinHeight96 = 16;
constexpr int kEdgeHit96 = 4;
constexpr int kCloseSize96 = 14;
constexpr int kClosePad96 = 4;
constexpr u8 kDefaultAlpha = 0x66;
constexpr u8 kMaskAlpha = 0x99;
constexpr float kDefaultYFrac = 0.40f;

enum class ReadingBarHit {
    None = 0,
    Band,
    ResizeTop,
    ResizeBottom,
    Close,
};

static WindowTab* DocTab(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || tab->IsNonDocumentTab()) {
        return nullptr;
    }
    return tab;
}

static WindowTab* ActiveBarTab(MainWindow* win) {
    WindowTab* tab = DocTab(win);
    if (!tab || !tab->readingBar.on || !tab->AsFixed()) {
        return nullptr;
    }
    return tab;
}

static int HeightPx() {
    int h = (gSettings && gSettings->readingBar.height > 0) ? gSettings->readingBar.height : kDefaultHeight96;
    if (h < kMinHeight96) {
        h = kMinHeight96;
    }
    return DpiScale(h);
}

static void SetHeightPx(int px, bool save) {
    if (!gSettings) {
        return;
    }
    int dpi = DpiGet();
    int unscaled = (dpi > 0) ? MulDiv(px, 96, dpi) : px;
    if (unscaled < kMinHeight96) {
        unscaled = kMinHeight96;
    }
    if (unscaled > 400) {
        unscaled = 400;
    }
    if (gSettings->readingBar.height == unscaled) {
        if (save) {
            ScheduleSaveSettings();
        }
        return;
    }
    gSettings->readingBar.height = unscaled;
    if (save) {
        ScheduleSaveSettings();
    }
}

static Rect CanvasRect(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return {};
    }
    return HwndClientRect(win->hwndCanvas);
}

static Rect BandRect(MainWindow* win) {
    WindowTab* tab = ActiveBarTab(win);
    if (!tab) {
        return {};
    }
    Rect canvas = CanvasRect(win);
    if (canvas.dy <= 0 || canvas.dx <= 0) {
        return {};
    }
    int h = HeightPx();
    int maxH = canvas.dy * 4 / 5;
    if (maxH < DpiScale(kMinHeight96)) {
        maxH = canvas.dy;
    }
    if (h > maxH) {
        h = maxH;
    }
    if (h < 1) {
        return {};
    }
    float frac = tab->readingBar.yFrac;
    if (frac < 0) {
        frac = 0;
    }
    if (frac > 1) {
        frac = 1;
    }
    int y = (int)(frac * (float)canvas.dy + 0.5f);
    if (y < 0) {
        y = 0;
    }
    if (y + h > canvas.dy) {
        y = canvas.dy - h;
    }
    if (y < 0) {
        y = 0;
        h = canvas.dy;
    }
    return {0, y, canvas.dx, h};
}

static void SetBandY(WindowTab* tab, int y, int canvasDy) {
    if (!tab || canvasDy <= 0) {
        return;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > canvasDy) {
        y = canvasDy;
    }
    tab->readingBar.yFrac = (float)y / (float)canvasDy;
}

static Rect CloseRect(const Rect& band) {
    int sz = DpiScale(kCloseSize96);
    int pad = DpiScale(kClosePad96);
    if (band.dx < sz + (2 * pad) || band.dy < sz + (2 * pad)) {
        return {};
    }
    return {band.x + band.dx - sz - pad, band.y + pad, sz, sz};
}

static ReadingBarHit HitTest(MainWindow* win, Point pt) {
    Rect band = BandRect(win);
    if (band.IsEmpty() || !band.Contains(pt)) {
        return ReadingBarHit::None;
    }
    Rect close = CloseRect(band);
    if (!close.IsEmpty() && close.Contains(pt)) {
        return ReadingBarHit::Close;
    }
    int edge = DpiScale(kEdgeHit96);
    if (edge > band.dy / 3) {
        edge = band.dy / 3;
    }
    if (edge < 1) {
        edge = 1;
    }
    if (pt.y < band.y + edge) {
        return ReadingBarHit::ResizeTop;
    }
    if (pt.y >= band.Bottom() - edge) {
        return ReadingBarHit::ResizeBottom;
    }
    return ReadingBarHit::Band;
}

static void InvalidateCanvas(MainWindow* win) {
    if (win && win->hwndCanvas) {
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
}

static Color BandFill(u8& alphaOut) {
    Color col = MkRgb(0xff, 0xe0, 0x82);
    alphaOut = kDefaultAlpha;
    if (!gSettings) {
        return col;
    }
    ParseColor(gSettings->readingBar.background);
    if (gSettings->readingBar.background.parsedOk) {
        col = gSettings->readingBar.background.col;
        u8 r, g, b, a;
        UnpackColor(col, r, g, b, a);
        if (a > 0) {
            alphaOut = a;
        }
        col = MkRgb(r, g, b);
    }
    return col;
}

static bool InvertOn() {
    return gSettings && gSettings->readingBar.invert;
}

void ReadingBarPaint(MainWindow* win, Gfx* gfx) {
    if (!win || !gfx) {
        return;
    }
    Rect band = BandRect(win);
    if (band.IsEmpty()) {
        return;
    }
    Rect canvas = CanvasRect(win);
    if (InvertOn()) {
        Rect above{0, 0, canvas.dx, band.y};
        Rect below{0, band.Bottom(), canvas.dx, canvas.dy - band.Bottom()};
        if (!above.IsEmpty()) {
            gfx->FillRects(&above, 1, kColBlack, kMaskAlpha);
        }
        if (!below.IsEmpty()) {
            gfx->FillRects(&below, 1, kColBlack, kMaskAlpha);
        }
        gfx->DrawRect(band, kColWhite);
    } else {
        u8 alpha = kDefaultAlpha;
        Color fill = BandFill(alpha);
        gfx->FillRects(&band, 1, fill, alpha);
    }

    if (!win->readingBarHover && win->readingBarDrag == ReadingBarDrag::None) {
        return;
    }
    Rect close = CloseRect(band);
    if (close.IsEmpty()) {
        return;
    }
    int m = DpiScale(3);
    Point a{close.x + m, close.y + m};
    Point b{close.Right() - m - 1, close.Bottom() - m - 1};
    Point c{close.Right() - m - 1, close.y + m};
    Point d{close.x + m, close.Bottom() - m - 1};
    Color xcol = InvertOn() ? kColWhite : kColBlack;
    gfx->DrawLineAA(a, b, xcol, 1.5f);
    gfx->DrawLineAA(c, d, xcol, 1.5f);
}

bool ReadingBarIsOn(MainWindow* win) {
    return ActiveBarTab(win) != nullptr;
}

void ReadingBarCancelDrag(MainWindow* win) {
    if (!win) {
        return;
    }
    bool dragging = win->readingBarDrag != ReadingBarDrag::None;
    win->readingBarDrag = ReadingBarDrag::None;
    win->readingBarDragOff = 0;
    if (dragging && win->hwndCanvas && GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
}

void ReadingBarHide(MainWindow* win) {
    WindowTab* tab = DocTab(win);
    ReadingBarCancelDrag(win);
    if (tab) {
        tab->readingBar.on = false;
    }
    if (win) {
        win->readingBarHover = false;
    }
    InvalidateCanvas(win);
    if (win && win->hwndCanvas) {
        ReadingAutoScrollRelayout(win->hwndCanvas);
    }
}

void ReadingBarForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    bool wasCurrent = win && win->CurrentTab() == tab;
    tab->readingBar = {};
    tab->readingBar.yFrac = kDefaultYFrac;
    if (wasCurrent) {
        ReadingBarCancelDrag(win);
        win->readingBarHover = false;
        InvalidateCanvas(win);
    }
}

void ReadingBarToggle(MainWindow* win) {
    WindowTab* tab = DocTab(win);
    if (!tab || !tab->AsFixed()) {
        return;
    }
    if (tab->readingBar.on) {
        ReadingBarHide(win);
        return;
    }
    tab->readingBar.on = true;
    InvalidateCanvas(win);
    if (win->hwndCanvas) {
        ReadingAutoScrollRelayout(win->hwndCanvas);
    }
}

void ReadingBarToggleInvert(MainWindow* win) {
    if (!gSettings) {
        return;
    }
    gSettings->readingBar.invert = !gSettings->readingBar.invert;
    ScheduleSaveSettings();
    InvalidateCanvas(win);
}

static void ApplyMove(MainWindow* win, int y) {
    WindowTab* tab = ActiveBarTab(win);
    Rect canvas = CanvasRect(win);
    Rect band = BandRect(win);
    if (!tab || canvas.dy <= 0 || band.IsEmpty()) {
        return;
    }
    int newY = y - win->readingBarDragOff;
    if (newY < 0) {
        newY = 0;
    }
    if (newY + band.dy > canvas.dy) {
        newY = canvas.dy - band.dy;
    }
    SetBandY(tab, newY, canvas.dy);
    InvalidateCanvas(win);
}

static void ApplyResizeTop(MainWindow* win, int y) {
    WindowTab* tab = ActiveBarTab(win);
    Rect canvas = CanvasRect(win);
    Rect band = BandRect(win);
    if (!tab || canvas.dy <= 0 || band.IsEmpty()) {
        return;
    }
    int bottom = band.Bottom();
    int newY = y - win->readingBarDragOff;
    int minH = DpiScale(kMinHeight96);
    if (newY < 0) {
        newY = 0;
    }
    int newH = bottom - newY;
    if (newH < minH) {
        newY = bottom - minH;
        newH = minH;
    }
    int maxH = canvas.dy * 4 / 5;
    if (newH > maxH) {
        newH = maxH;
        newY = bottom - newH;
        if (newY < 0) {
            newY = 0;
            newH = bottom;
        }
    }
    SetBandY(tab, newY, canvas.dy);
    SetHeightPx(newH, false);
    InvalidateCanvas(win);
}

static void ApplyResizeBottom(MainWindow* win, int y) {
    WindowTab* tab = ActiveBarTab(win);
    Rect canvas = CanvasRect(win);
    Rect band = BandRect(win);
    if (!tab || canvas.dy <= 0 || band.IsEmpty()) {
        return;
    }
    int newBottom = y - win->readingBarDragOff;
    int minH = DpiScale(kMinHeight96);
    int newH = newBottom - band.y;
    if (newH < minH) {
        newH = minH;
    }
    int maxH = canvas.dy - band.y;
    int cap = canvas.dy * 4 / 5;
    if (maxH > cap) {
        maxH = cap;
    }
    if (newH > maxH) {
        newH = maxH;
    }
    SetHeightPx(newH, false);
    InvalidateCanvas(win);
}

bool ReadingBarOnLeftDown(MainWindow* win, int x, int y) {
    ReadingBarHit hit = HitTest(win, {x, y});
    if (hit == ReadingBarHit::None) {
        return false;
    }
    if (hit == ReadingBarHit::Close) {
        ReadingBarHide(win);
        return true;
    }
    Rect band = BandRect(win);
    win->readingBarHover = true;
    if (hit == ReadingBarHit::ResizeTop) {
        win->readingBarDrag = ReadingBarDrag::ResizeTop;
        win->readingBarDragOff = y - band.y;
    } else if (hit == ReadingBarHit::ResizeBottom) {
        win->readingBarDrag = ReadingBarDrag::ResizeBottom;
        win->readingBarDragOff = y - band.Bottom();
    } else {
        win->readingBarDrag = ReadingBarDrag::Move;
        win->readingBarDragOff = y - band.y;
    }
    if (win->hwndCanvas) {
        SetCapture(win->hwndCanvas);
    }
    return true;
}

bool ReadingBarOnMouseMove(MainWindow* win, int x, int y) {
    if (win->readingBarDrag == ReadingBarDrag::Move) {
        ApplyMove(win, y);
        return true;
    }
    if (win->readingBarDrag == ReadingBarDrag::ResizeTop) {
        ApplyResizeTop(win, y);
        return true;
    }
    if (win->readingBarDrag == ReadingBarDrag::ResizeBottom) {
        ApplyResizeBottom(win, y);
        return true;
    }
    if (!ActiveBarTab(win)) {
        if (win->readingBarHover) {
            win->readingBarHover = false;
            InvalidateCanvas(win);
        }
        return false;
    }
    bool hover = HitTest(win, {x, y}) != ReadingBarHit::None;
    if (hover != win->readingBarHover) {
        win->readingBarHover = hover;
        InvalidateCanvas(win);
    }
    if (hover && win->hwndCanvas) {
        TrackMouseLeave(win->hwndCanvas);
    }
    return false;
}

bool ReadingBarOnLeftUp(MainWindow* win) {
    if (!win || win->readingBarDrag == ReadingBarDrag::None) {
        return false;
    }
    bool resized =
        win->readingBarDrag == ReadingBarDrag::ResizeTop || win->readingBarDrag == ReadingBarDrag::ResizeBottom;
    if (resized) {
        Rect band = BandRect(win);
        if (!band.IsEmpty()) {
            SetHeightPx(band.dy, true);
        }
    }
    ReadingBarCancelDrag(win);
    return true;
}

bool ReadingBarOnSetCursor(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return false;
    }
    if (win->readingBarDrag == ReadingBarDrag::ResizeTop || win->readingBarDrag == ReadingBarDrag::ResizeBottom) {
        SetCursorCached(IDC_SIZENS);
        return true;
    }
    if (win->readingBarDrag == ReadingBarDrag::Move) {
        SetCursorCached(IDC_SIZEALL);
        return true;
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    ReadingBarHit hit = HitTest(win, pt);
    if (hit == ReadingBarHit::None) {
        return false;
    }
    if (hit == ReadingBarHit::Close) {
        SetCursorCached(IDC_HAND);
        return true;
    }
    if (hit == ReadingBarHit::ResizeTop || hit == ReadingBarHit::ResizeBottom) {
        SetCursorCached(IDC_SIZENS);
        return true;
    }
    SetCursorCached(IDC_SIZEALL);
    return true;
}

void ReadingBarOnMouseLeave(MainWindow* win) {
    if (!win || win->readingBarDrag != ReadingBarDrag::None) {
        return;
    }
    if (win->readingBarHover) {
        win->readingBarHover = false;
        InvalidateCanvas(win);
    }
}

static void NudgeY(MainWindow* win, int dir) {
    WindowTab* tab = ActiveBarTab(win);
    Rect canvas = CanvasRect(win);
    Rect band = BandRect(win);
    if (!tab || canvas.dy <= 0 || band.IsEmpty()) {
        return;
    }
    int step = DpiScale(16);
    SetBandY(tab, band.y + (dir * step), canvas.dy);
    InvalidateCanvas(win);
}

static void NudgeHeight(MainWindow* win, int dir) {
    Rect canvas = CanvasRect(win);
    Rect band = BandRect(win);
    if (canvas.dy <= 0 || band.IsEmpty()) {
        return;
    }
    int step = DpiScale(8);
    int newH = band.dy + (dir * step);
    int minH = DpiScale(kMinHeight96);
    if (newH < minH) {
        newH = minH;
    }
    int maxH = canvas.dy * 4 / 5;
    if (newH > maxH) {
        newH = maxH;
    }
    SetHeightPx(newH, true);
    InvalidateCanvas(win);
}

bool ReadingBarOnKey(MainWindow* win, WPARAM key) {
    if (!ActiveBarTab(win)) {
        return false;
    }
    if (IsFindUIVisible(win)) {
        return false;
    }
    if (IsAltPressed()) {
        return false;
    }
    if (key == VK_ESCAPE && !IsCtrlPressed() && !IsShiftPressed()) {
        if (ReadingAutoScrollIsOn(win)) {
            return false;
        }
        ReadingBarHide(win);
        return true;
    }
    if (!IsCtrlPressed()) {
        return false;
    }
    if (key == VK_UP) {
        if (IsShiftPressed()) {
            NudgeHeight(win, -1);
        } else {
            NudgeY(win, -1);
        }
        return true;
    }
    if (key == VK_DOWN) {
        if (IsShiftPressed()) {
            NudgeHeight(win, 1);
        } else {
            NudgeY(win, 1);
        }
        return true;
    }
    return false;
}

TempStr ReadingBarStateTemp(int* exitCodeOut) {
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
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    bool home = tab && tab->IsNonDocumentTab();
    DisplayModel* dm = tab ? tab->AsFixed() : (win ? win->AsFixed() : nullptr);
    int scrollY = dm ? dm->viewPort.y : -1;
    Rect band = BandRect(win);
    bool on = ActiveBarTab(win) != nullptr;
    int invert = InvertOn() ? 1 : 0;
    int autoOn = ReadingAutoScrollIsOn(win) ? 1 : 0;
    int height = gSettings ? gSettings->readingBar.height : 0;
    float yFrac = (tab && !home) ? tab->readingBar.yFrac : 0;
    out.Append(fmt("OK on=%d invert=%d home=%d auto=%d yFrac=%d height=%d bandY=%d bandH=%d scrollY=%d\n", (int)on,
                   invert, (int)home, autoOn, (int)(yFrac * 1000.0f + 0.5f), height, band.y, band.dy, scrollY));
    return finish(0);
}
