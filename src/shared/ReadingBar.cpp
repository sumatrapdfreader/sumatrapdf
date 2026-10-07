/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if defined(SUMATRA_NG)
#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#else
#include "base/Base.h"
#include "base/Win.h"
#include "gui/Gfx.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

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
#if defined(SUMATRA_NG)
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#endif
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"

#if defined(SUMATRA_NG)
#include "SumatraLog.h"
#endif

// Horizontal reading guide: a viewport band (Skim / #5771 / #3389). Highlight
// fills it; Invert dims the rest. Per-tab on/yFrac; color, invert, height in
// gSettings->readingBar.
// ng: orig's canvas is an HWND whose client rect is the viewport and which
// captures the mouse; here the band is painted by DocCanvas' customPaint in
// viewport coordinates and the canvas forwards the mouse to the handlers below.

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
    return DpiScale(std::max(h, kMinHeight96));
}

static void SetHeightPx(int px, bool save) {
    if (!gSettings) {
        return;
    }
    int dpi = DpiGet();
    int unscaled = (dpi > 0) ? (px * 96) / dpi : px;
    unscaled = limitValue(unscaled, kMinHeight96, 400);
    if (gSettings->readingBar.height != unscaled) {
        gSettings->readingBar.height = unscaled;
    }
    if (save) {
        ScheduleSaveSettings();
    }
}

static Rect CanvasRect(MainWindow* win) {
    if (!win) {
        return {};
    }
    Size vp = win->GetViewPortSize();
    return Rect{0, 0, vp.dx, vp.dy};
}

static int MaxHeight(int canvasDy) {
    int maxH = canvasDy * 4 / 5;
    return maxH < DpiScale(kMinHeight96) ? canvasDy : maxH;
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
    int h = std::min(HeightPx(), MaxHeight(canvas.dy));
    float frac = limitValue(tab->readingBar.yFrac, 0.f, 1.f);
    int y = limitValue((int)(frac * (float)canvas.dy + 0.5f), 0, canvas.dy - h);
    return {0, y, canvas.dx, h};
}

static void SetBandY(WindowTab* tab, int y, int canvasDy) {
    if (!tab || canvasDy <= 0) {
        return;
    }
    y = limitValue(y, 0, canvasDy);
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
    int edge = std::min(DpiScale(kEdgeHit96), std::max(1, band.dy / 3));
    if (pt.y < band.y + edge) {
        return ReadingBarHit::ResizeTop;
    }
    if (pt.y >= band.Bottom() - edge) {
        return ReadingBarHit::ResizeBottom;
    }
    return ReadingBarHit::Band;
}

static void InvalidateCanvas(MainWindow* win) {
    if (win) {
        win->RedrawCanvas();
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

template <typename Fill, typename Outline, typename Line>
static void PaintBar(MainWindow* win, Fill fill, Outline outline, Line line) {
    Rect band = BandRect(win);
    if (band.IsEmpty()) {
        return;
    }
    Rect canvas = CanvasRect(win);
    if (InvertOn()) {
        Rect above{0, 0, canvas.dx, band.y};
        Rect below{0, band.Bottom(), canvas.dx, canvas.dy - band.Bottom()};
        if (!above.IsEmpty()) {
            fill(above, kColBlack, kMaskAlpha);
        }
        if (!below.IsEmpty()) {
            fill(below, kColBlack, kMaskAlpha);
        }
        outline(band, kColWhite);
    } else {
        u8 alpha = kDefaultAlpha;
        Color fillCol = BandFill(alpha);
        fill(band, fillCol, alpha);
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
    line(a, b, xcol);
    line(c, d, xcol);
}

#if defined(SUMATRA_NG)
void ReadingBarPaint(MainWindow* win, gp::PaintCtx* ctx) {
    if (!win || !ctx) {
        return;
    }
    auto fill = [ctx](const Rect& rect, Color col, u8 alpha) { CanvasFillRects(ctx, &rect, 1, col, alpha, 0); };
    auto outline = [ctx](const Rect& rect, Color col) { CanvasFillRects(ctx, &rect, 1, col, 0, 1); };
    auto line = [ctx](Point a, Point b, Color col) { CanvasDrawLine(ctx, a, b, col, 1.5f); };
    PaintBar(win, fill, outline, line);
}
#else
void ReadingBarPaint(MainWindow* win, Gfx* gfx) {
    if (!win || !gfx) {
        return;
    }
    auto fill = [gfx](const Rect& rect, Color col, u8 alpha) { gfx->FillRects(&rect, 1, col, alpha); };
    auto outline = [gfx](const Rect& rect, Color col) { gfx->DrawRect(rect, col); };
    auto line = [gfx](Point a, Point b, Color col) { gfx->DrawLineAA(a, b, col, 1.5f); };
    PaintBar(win, fill, outline, line);
}
#endif

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
    if (dragging) {
#if defined(SUMATRA_NG)
        CanvasSetCapture(win, false);
#else
        if (win->hwndCanvas && GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
#endif
    }
}

void ReadingBarHide(MainWindow* win) {
#if defined(SUMATRA_NG)
    logf("ReadingBar: off\n");
#endif
    WindowTab* tab = DocTab(win);
    ReadingBarCancelDrag(win);
    if (tab) {
        tab->readingBar.on = false;
    }
    if (win) {
        win->readingBarHover = false;
    }
    InvalidateCanvas(win);
#if !defined(SUMATRA_NG)
    if (win && win->hwndCanvas) {
        ReadingAutoScrollRelayout(win->hwndCanvas);
    }
#endif
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
#if defined(SUMATRA_NG)
    Rect band = BandRect(win);
    logf("ReadingBar: on, band %d,%d %dx%d, invert %d\n", band.x, band.y, band.dx, band.dy, (int)InvertOn());
#else
    if (win->hwndCanvas) {
        ReadingAutoScrollRelayout(win->hwndCanvas);
    }
#endif
}

void ReadingBarToggleInvert(MainWindow* win) {
    if (!gSettings) {
        return;
    }
    gSettings->readingBar.invert = !gSettings->readingBar.invert;
#if defined(SUMATRA_NG)
    logf("ReadingBar: invert %d\n", (int)gSettings->readingBar.invert);
#endif
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
    int newY = limitValue(y - win->readingBarDragOff, 0, canvas.dy - band.dy);
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
    int maxH = std::min(bottom, MaxHeight(canvas.dy));
    int minH = std::min(DpiScale(kMinHeight96), maxH);
    int newH = limitValue(bottom - (y - win->readingBarDragOff), minH, maxH);
    int newY = bottom - newH;
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
    int maxH = std::min(canvas.dy - band.y, MaxHeight(canvas.dy));
    int minH = std::min(DpiScale(kMinHeight96), maxH);
    int newH = limitValue(newBottom - band.y, minH, maxH);
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
#if defined(SUMATRA_NG)
    CanvasSetCapture(win, true);
#else
    if (win->hwndCanvas) {
        SetCapture(win->hwndCanvas);
    }
#endif
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
#if !defined(SUMATRA_NG)
    if (hover && win->hwndCanvas) {
        TrackMouseLeave(win->hwndCanvas);
    }
#endif
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

static ReadingBarHit CursorHit(MainWindow* win, Point pt) {
    if (win->readingBarDrag == ReadingBarDrag::ResizeTop || win->readingBarDrag == ReadingBarDrag::ResizeBottom) {
        return ReadingBarHit::ResizeTop;
    }
    if (win->readingBarDrag == ReadingBarDrag::Move) {
        return ReadingBarHit::Band;
    }
    return HitTest(win, pt);
}

#if defined(SUMATRA_NG)
bool ReadingBarOnSetCursor(MainWindow* win, int x, int y) {
    if (!win) {
        return false;
    }
    ReadingBarHit hit = CursorHit(win, {x, y});
    if (hit == ReadingBarHit::None) {
        return false;
    }
    bool resize = hit == ReadingBarHit::ResizeTop || hit == ReadingBarHit::ResizeBottom;
    gp::CursorKind cursor = hit == ReadingBarHit::Close ? gp::CursorKind::Pointer
                            : resize                    ? gp::CursorKind::RowResize
                                                        : gp::CursorKind::ClosedHand;
    CanvasSetCursor(win, (int)cursor);
    return true;
}
#else
bool ReadingBarOnSetCursor(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return false;
    }
    ReadingBarHit hit = CursorHit(win, HwndGetCursorPos(win->hwndCanvas));
    if (hit == ReadingBarHit::None) {
        return false;
    }
    bool resize = hit == ReadingBarHit::ResizeTop || hit == ReadingBarHit::ResizeBottom;
    LPWSTR cursor = hit == ReadingBarHit::Close ? IDC_HAND : resize ? IDC_SIZENS : IDC_SIZEALL;
    SetCursorCached(cursor);
    return true;
}
#endif

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
    int maxH = MaxHeight(canvas.dy);
    int minH = std::min(DpiScale(kMinHeight96), maxH);
    int newH = limitValue(band.dy + (dir * step), minH, maxH);
    SetHeightPx(newH, true);
    InvalidateCanvas(win);
}

static bool ReadingBarOnKeyImpl(MainWindow* win, int key, bool ctrl, bool shift, bool alt) {
    if (!ActiveBarTab(win)) {
        return false;
    }
    if (IsFindUIVisible(win)) {
        return false;
    }
    if (alt) {
        return false;
    }
    if (key == VK_ESCAPE && !ctrl && !shift) {
        if (ReadingAutoScrollIsOn(win)) {
            return false;
        }
        ReadingBarHide(win);
        return true;
    }
    if (!ctrl) {
        return false;
    }
    if (key == VK_UP) {
        if (shift) {
            NudgeHeight(win, -1);
        } else {
            NudgeY(win, -1);
        }
        return true;
    }
    if (key == VK_DOWN) {
        if (shift) {
            NudgeHeight(win, 1);
        } else {
            NudgeY(win, 1);
        }
        return true;
    }
    return false;
}

#if defined(SUMATRA_NG)
bool ReadingBarOnKey(MainWindow* win, int key, bool ctrl, bool shift, bool alt) {
    return ReadingBarOnKeyImpl(win, key, ctrl, shift, alt);
}
#else
bool ReadingBarOnKey(MainWindow* win, WPARAM key) {
    return ReadingBarOnKeyImpl(win, (int)key, IsCtrlPressed(), IsShiftPressed(), IsAltPressed());
}
#endif

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
