/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Canvas.cpp, the part that draws and drives a DisplayModel:
// DrawDocument, the page frame and shadow, the WM_VSCROLL / WM_HSCROLL
// handlers, the mouse wheel (scroll, page flip, cursor-anchored ctrl+wheel
// zoom), drag panning, middle-button auto-scroll, and (step 8a) the mouse
// handling for links and selection. The win32 half (HDC, capture, timers,
// touch, OLE drag-drop) is gpui here; find and annotations arrive with steps
// 8b and 13.

#include "gui/GpuiBridge.h"
#include "base/Pixmap.h"
#include "base/Timer.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "RenderCache.h"
#include "Commands.h"
#include "VirtKeys.h"
#include "Translations.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "Selection.h"
#include "SelectionToolbar.h"
#include "LinkFollow.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "SelectTextKeyboard.h"
#include "SumatraDialogs.h"
#include "gui/GpuiTheme.h"
#include "OverlayScrollbar.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "ReadAloud.h"
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AnnotPlacement.h"
#include "gui/OleDragDrop.h"
#include "AnnotTextPopup.h"
#include "RefHover.h"
#include "FormFields.h"
#include "Menu.h"
#include "gui/DialogWidgets.h"
#include "gui/DocCanvas.h"

#include "SumatraLog.h"

// the notification for a page that is slow to render goes up after this long
constexpr int kRenderDelayShowNotif = 500;

// one wheel notch, as win32 reports it; gpui reports pixels, and the wheel
// code is orig's, so it converts back
constexpr int kWheelDelta = 120;

// --- the frame being painted ------------------------------------------------

// RenderCache::Paint hands us document coordinates and knows nothing about the
// element it draws into, so the canvas records where that is. Painting only
// ever happens on the UI thread, one element at a time.
struct PaintFrame {
    gp::PaintCtx* ctx = nullptr;
    float x0 = 0; // canvas origin, in dips
    float y0 = 0;
    float k = 1; // dips per document pixel
};

static PaintFrame gFrame;

struct DocCanvasView;

// ng: the canvas state orig keeps on MainWindow; one per window (step 10b)
struct DocCanvasUI {
    gp::Entity<DocCanvasView> view;
    // the last wheel page turn, so queued notches don't skip unread comic pages
    TimeStamp wheelPageTurnTime{};
    bool wheelDidTurnPage = false;
    // the page context menu, built when it is opened (orig's
    // OnWindowContextMenu), and the popup that shows it
    MenuModel* ctxMenu = nullptr;
    gp::Entity<gp::PopupMenuState> ctxPopup;
};

static DocCanvasUI* Ui(MainWindow* win) {
    if (!win->docCanvas) {
        win->docCanvas = new DocCanvasUI();
    }
    return win->docCanvas;
}

void DocCanvasDelete(MainWindow* win) {
    if (win->docCanvas) {
        DeleteMenuModel(win->docCanvas->ctxMenu);
    }
    delete win->docCanvas;
    win->docCanvas = nullptr;
}

static gp::Bounds ToCanvas(Rect r) {
    float k = gFrame.k;
    return gp::Bounds{gFrame.x0 + (float)r.x * k, gFrame.y0 + (float)r.y * k, (float)r.dx * k, (float)r.dy * k};
}

// dips per document pixel: gpui lays out in dips, the document model in
// pixels, and on a 100% display the two are the same
float CanvasScale(MainWindow* win) {
    if (!win || !win->gpuiWin) {
        return 1;
    }
    gp::WinSize ws = gp::WindowSize(win->gpuiWin);
    if (ws.dipW <= 0 || ws.pxW <= 0) {
        return 1;
    }
    return ws.dipW / (float)ws.pxW;
}

// --- page bitmaps -----------------------------------------------------------

// ng: a page bitmap goes to gpui as raw pixels (RenderImageFromBgra: tightly
// packed, premultiplied BGRA). It used to be wrapped in a BMP header and
// decoded, which the browser does asynchronously: the page stayed blank on
// wasm until something else repainted.

static gp::RenderImage* RenderImageFromBgraPixmap(gp::PaintApp* pa, Pixmap* px);

// EngineMupdf hands back an 8-bit palette DIB when a page has few enough
// colors (TryRenderAsPaletteImage). Only the HBITMAP knows how to read those,
// so take a 32bpp copy of it; orig blits the DIB with GDI instead.
gp::RenderImage* RenderImageFromPixmap(gp::PaintApp* pa, Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    if (px->format != PixmapFormat::Native) {
        return RenderImageFromBgraPixmap(pa, px);
    }
#if OS_WIN
    Pixmap* copy = PixmapCopyAs32bppDIB(px);
    if (!copy) {
        return nullptr;
    }
    gp::RenderImage* img = RenderImageFromBgraPixmap(pa, copy);
    FreePixmap(copy);
    return img;
#else
    return nullptr;
#endif
}

static gp::RenderImage* RenderImageFromBgraPixmap(gp::PaintApp* pa, Pixmap* px) {
    if (!pa || !px || !px->data || px->width <= 0 || px->height <= 0) {
        return nullptr;
    }
    bool isBgra = px->format == PixmapFormat::BGRA8;
    if (!isBgra && px->format != PixmapFormat::BGR8) {
        // RGBA8 needs a swizzle and Native only the platform bitmap knows how
        // to read; neither comes out of the page renderers we have
        return nullptr;
    }
    int w = px->width;
    int h = px->height;
    i64 total = (i64)w * h * 4;
    if (total > INT_MAX) {
        return nullptr;
    }
    u8* buf = (u8*)malloc((size_t)total);
    if (!buf) {
        return nullptr;
    }
    // an opaque pixmap may have anything in its alpha bytes (a DIB's are 0),
    // and one with alpha is not always premultiplied
    bool copyRows = isBgra && px->hasAlpha && px->premultiplied;
    for (int y = 0; y < h; y++) {
        const u8* src = px->data + ((size_t)y * px->stride);
        u8* dst = buf + ((size_t)y * w * 4);
        if (copyRows) {
            memcpy(dst, src, (size_t)w * 4);
            continue;
        }
        int srcBpp = isBgra ? 4 : 3;
        for (int x = 0; x < w; x++, src += srcBpp, dst += 4) {
            if (!isBgra || !px->hasAlpha) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 0xff;
                continue;
            }
            int alpha = src[3];
            dst[0] = (u8)((src[0] * alpha + 127) / 255);
            dst[1] = (u8)((src[1] * alpha + 127) / 255);
            dst[2] = (u8)((src[2] * alpha + 127) / 255);
            dst[3] = (u8)alpha;
        }
    }
    gp::RenderImage* img = gp::RenderImageFromBgra(pa, buf, w, h);
    free(buf);
    return img;
}

// how long handing the page bitmaps to gpui has cost so far (a copy each)
static double gBmpWrapMs = 0;
static int gBmpWrapCount = 0;

// ms from process start to the first page tile on screen
static double gFirstTilePaintMs = -1;

void CanvasBmpWrapStats(double& msOut, int& countOut) {
    msOut = gBmpWrapMs;
    countOut = gBmpWrapCount;
}

double CanvasFirstPaintMs() {
    return gFirstTilePaintMs;
}

bool CanvasDrawTile(gp::PaintCtx* ctx, BitmapCacheEntry* entry, Rect target, Rect source) {
    if (!ctx || !entry || !entry->bitmap) {
        return false;
    }
    if (gFirstTilePaintMs < 0) {
        gFirstTilePaintMs = AppElapsedMs();
    }
    if (!entry->renderImage) {
        auto timeStart = TimeGet();
        entry->renderImage = RenderImageFromPixmap(ctx->pa, entry->bitmap);
        gBmpWrapMs += TimeSinceInMs(timeStart);
        gBmpWrapCount++;
        logf("CanvasDrawTile: page %d %dx%d fmt %d -> RenderImage %s in %.2f ms (%d so far, %.0f ms total)\n",
             entry->pageNo, entry->bitmap->width, entry->bitmap->height, (int)entry->bitmap->format,
             entry->renderImage ? StrL("ok") : StrL("FAILED"), TimeSinceInMs(timeStart), gBmpWrapCount, gBmpWrapMs);
    }
    auto* img = (gp::RenderImage*)entry->renderImage;
    if (!img || source.dx <= 0 || source.dy <= 0 || target.dx <= 0 || target.dy <= 0) {
        return false;
    }
    // orig blits source (a region of the bitmap) onto target. RenderImageDraw
    // takes where the *whole* image goes plus a clip, so place the image so
    // that source lands on target and clip to target.
    float sx = (float)target.dx / (float)source.dx;
    float sy = (float)target.dy / (float)source.dy;
    Size bmpSize = Size(entry->bitmap->width, entry->bitmap->height);
    gp::Bounds clip = ToCanvas(target);
    gp::Bounds image;
    image.x = clip.x - ((float)source.x * sx * gFrame.k);
    image.y = clip.y - ((float)source.y * sy * gFrame.k);
    image.w = (float)bmpSize.dx * sx * gFrame.k;
    image.h = (float)bmpSize.dy * sy * gFrame.k;
    gp::RenderImageDraw(ctx, img, clip, image, 0, 0, false);
    return true;
}

void CanvasDrawTileOutline(gp::PaintCtx* ctx, Rect bounds) {
    gp::Bounds b = ToCanvas(bounds);
    gp::CanvasStrokeRound(ctx, b.x, b.y, b.w, b.h, 0, 1, ToGpui(kColYellow));
}

// --- what Selection.cpp and LinkFollow.cpp draw with ------------------------

// orig's Gfx::FillRects: one translucent fill per rect plus, when
// outlineWidth > 0, a 1px outline of the same color at full opacity
void CanvasFillRects(gp::PaintCtx* ctx, const Rect* rects, int n, Color col, u8 alpha, int outlineWidth) {
    if (!ctx || n <= 0) {
        return;
    }
    gp::Rgba fill = ToGpui(col);
    fill.a = alpha;
    gp::Rgba line = ToGpui(col);
    line.a = 255;
    for (int i = 0; i < n; i++) {
        gp::Bounds b = ToCanvas(rects[i]);
        gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, fill);
        if (outlineWidth > 0) {
            gp::CanvasStrokeRound(ctx, b.x, b.y, b.w, b.h, 0, (float)outlineWidth, line);
        }
    }
}

// orig's Gfx::FillQuads: pts holds 4 corners per quad, in order
void CanvasFillQuads(gp::PaintCtx* ctx, const Point* pts, int nQuads, Color col, u8 alpha, int outlineWidth) {
    if (!ctx || nQuads <= 0) {
        return;
    }
    gp::Rgba fill = ToGpui(col);
    fill.a = alpha;
    gp::Rgba line = ToGpui(col);
    line.a = 255;
    for (int i = 0; i < nQuads; i++) {
        const Point* q = pts + (i * 4);
        gp::Path* p = gp::PathNew(ctx, false);
        if (!p) {
            return;
        }
        for (int k = 0; k < 4; k++) {
            gp::Bounds b = ToCanvas(Rect(q[k].x, q[k].y, 0, 0));
            if (k == 0) {
                gp::PathMoveTo(p, b.x, b.y);
            } else {
                gp::PathLineTo(p, b.x, b.y);
            }
        }
        gp::PathClose(p);
        gp::PathFill(ctx, p, fill);
        if (outlineWidth > 0) {
            gp::PathStroke(ctx, p, (float)outlineWidth, line);
        }
        gp::PathFree(p);
    }
}

// orig's Gfx::DrawLineAA
void CanvasDrawLine(gp::PaintCtx* ctx, Point a, Point b, Color col, float width) {
    if (!ctx) {
        return;
    }
    gp::Path* p = gp::PathNew(ctx, false);
    if (!p) {
        return;
    }
    gp::Bounds ba = ToCanvas(Rect(a.x, a.y, 0, 0));
    gp::Bounds bb = ToCanvas(Rect(b.x, b.y, 0, 0));
    gp::PathMoveTo(p, ba.x, ba.y);
    gp::PathLineTo(p, bb.x, bb.y);
    gp::PathStroke(ctx, p, width * (gFrame.k > 0 ? gFrame.k : 1), ToGpui(col));
    gp::PathFree(p);
}

// orig's Gfx::DrawRect: the outline of a rectangle in document coordinates
void CanvasDrawRect(gp::PaintCtx* ctx, Rect r, Color col, float width) {
    if (!ctx) {
        return;
    }
    gp::Bounds b = ToCanvas(r);
    gp::CanvasStrokeRound(ctx, b.x, b.y, b.w, b.h, 0, width * (gFrame.k > 0 ? gFrame.k : 1), ToGpui(col));
}

// orig's Gfx::DrawEllipse / FillEllipse (fill when width <= 0)
void CanvasDrawEllipse(gp::PaintCtx* ctx, Rect r, Color col, float width) {
    if (!ctx || r.dx <= 0 || r.dy <= 0) {
        return;
    }
    gp::Bounds b = ToCanvas(r);
    float k = gFrame.k > 0 ? gFrame.k : 1;
    if (width <= 0) {
        gp::CanvasFillRound(ctx, b.x, b.y, b.w, b.h, std::min(b.w, b.h) / 2, ToGpui(col));
        return;
    }
    gp::CanvasEllipse(ctx, b.x + b.w / 2, b.y + b.h / 2, b.w / 2, b.h / 2, width * k, ToGpui(col));
}

Size CanvasMeasureText(gp::PaintCtx* ctx, Str s, float fontSize) {
    gp::Size sz = gp::MeasureText(ctx, ToGpui(s), fontSize, 0);
    float k = gFrame.k > 0 ? gFrame.k : 1;
    return Size{(int)(sz.w / k), (int)(sz.h / k)};
}

void CanvasDrawText(gp::PaintCtx* ctx, Str s, Point at, Color col, float fontSize) {
    gp::Bounds b = ToCanvas(Rect(at.x, at.y, 0, 0));
    // DrawTextBaseline places the baseline; MeasureText reports the line box
    gp::Size sz = gp::MeasureText(ctx, ToGpui(s), fontSize, 0);
    gp::DrawTextBaseline(ctx, ToGpui(s), b.x, b.y + sz.h * 0.8f, fontSize, ToGpui(col));
}

// --- input state orig reads from win32 --------------------------------------

// the modifiers of the last mouse or key event; orig asks GetKeyState()
static bool gCtrlPressed = false;
static bool gShiftPressed = false;

static void SetCanvasModifiers(const gp::Modifiers& m) {
    gCtrlPressed = m.control;
    gShiftPressed = m.shift;
}

bool CanvasCtrlPressed() {
    return gCtrlPressed;
}

bool CanvasShiftPressed() {
    return gShiftPressed;
}

// A key replaces the mouse event's modifiers. Orig asks GetKeyState, which is
// the keys held now, not whichever button was down on the last click.
void CanvasSetKeyModifiers(bool shift, bool ctrl) {
    gShiftPressed = shift;
    gCtrlPressed = ctrl;
}

// ng: gpui captures the mouse for the window itself while a button is down
// (and releases it on the up), so orig's SetCapture / ReleaseCapture in the
// selection code has nothing to do. Doing it anyway makes win32 send
// WM_CAPTURECHANGED, which gpui turns into a synthetic mouse-up.
void CanvasSetCapture(MainWindow*, bool) {}

void CanvasSetClipboardText(MainWindow* win, Str s) {
    if (win && win->gpuiWin) {
        gp::ClipboardSetText(win->gpuiWin, ToGpui(s));
    }
}

// orig's SetCursorCached: gpui resolves the cursor on every mouse move from the
// element under the pointer, so tell it what we picked and keep its cache in
// sync or it will not set it again
void CanvasSetCursor(MainWindow* win, int cursorKind) {
    win->nativeCursor = NativeCursor::None;
    gp::Window* gw = win ? win->gpuiWin : nullptr;
    auto kind = (gp::CursorKind)cursorKind;
    bool changed = win->canvasCursor != cursorKind;
    win->canvasCursor = cursorKind;
    if (changed) {
        AppShellInvalidate(win);
    }
    if (!gw || gw->cursor == kind) {
        return;
    }
    gw->cursor = kind;
    gp::PlatSetCursor(gw, kind);
    if (!changed) {
        return;
    }
    static SeqStrings kCursorNames = "arrow\0ibeam\0hand\0col-resize\0row-resize\0cross\0";
    logf("CanvasSetCursor: %s\n", SeqStrByIndex(kCursorNames, cursorKind));
}

bool CanvasSetNativeCursor(MainWindow* win, NativeCursor cursor) {
#if OS_WIN
    // ng: the stand-in kind; gpui's Windows backend draws OpenHand as the
    // arrow, and the frame's subclass replaces it (gui/NativeWindow.cpp)
    CanvasSetCursor(win, (int)gp::CursorKind::OpenHand);
    win->nativeCursor = cursor;
    AppShellApplyNativeCursor(win);
    return true;
#else
    (void)win;
    (void)cursor;
    return false;
#endif
}

// the thread that paints; nothing else may touch a RenderImage
static ThreadId gUiThread = 0;

static void ReleaseRenderImage(gp::RenderImage* img) {
    gp::RenderImageRelease(img);
}

// ng: a cache entry is evicted on whichever thread needed the room, which is a
// render worker. A RenderImage owns D2D / D3D resources belonging to the paint
// thread, so the release goes back to it.
void CanvasFreeTileImage(BitmapCacheEntry* entry) {
    if (!entry || !entry->renderImage) {
        return;
    }
    auto* img = (gp::RenderImage*)entry->renderImage;
    entry->renderImage = nullptr;
    if (GetCurrentThreadId() == gUiThread) {
        gp::RenderImageRelease(img);
        return;
    }
    uitask::Post(MkFunc0(ReleaseRenderImage, img), "ReleaseRenderImage");
}

// --- background and page frame ----------------------------------------------

static void PaintCheckerboard(gp::PaintCtx* ctx, Rect rc) {
    constexpr int kCheckerSize = 8;
    gp::Rgba light = ToGpui(kColWhite);
    gp::Rgba dark = ToGpui(MkRgb(204, 204, 204));
    for (int cy = 0; cy < rc.dy; cy += kCheckerSize) {
        for (int cx = 0; cx < rc.dx; cx += kCheckerSize) {
            int cellW = std::min(kCheckerSize, rc.dx - cx);
            int cellH = std::min(kCheckerSize, rc.dy - cy);
            bool isDark = ((cx / kCheckerSize) + (cy / kCheckerSize)) % 2 != 0;
            gp::Bounds b = ToCanvas(Rect(rc.x + cx, rc.y + cy, cellW, cellH));
            gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, isDark ? dark : light);
        }
    }
}

static void FillRect(gp::PaintCtx* ctx, Rect rc, Color col) {
    gp::Bounds b = ToCanvas(rc);
    gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, ToGpui(col));
}

// orig has two versions of this behind DRAW_PAGE_SHADOWS; SumatraPDF ships the
// shadow-less one, which just fills the page rect with the placeholder color
static void PaintPageFrameAndShadow(gp::PaintCtx* ctx, Rect& bounds, Rect&, bool, Color bgCol) {
    FillRect(ctx, Rect(bounds.x, bounds.y, bounds.dx + 1, bounds.dy + 1), bgCol);
}

// a message in the middle of the page, as orig's HdcDrawCenteredText does
static void DrawCenteredText(gp::PaintCtx* ctx, Rect bounds, Str s, Color col) {
    constexpr float kMsgFontSize = 14;
    gp::Str gs = ToGpui(s);
    gp::Size sz = gp::MeasureText(ctx, gs, kMsgFontSize, 0);
    gp::Bounds b = ToCanvas(bounds);
    float x = b.x + ((b.w - (float)sz.w) / 2);
    float y = b.y + ((b.h - (float)sz.h) / 2) + (float)sz.h * 0.8f;
    gp::DrawTextBaseline(ctx, gs, x, y, kMsgFontSize, ToGpui(col));
}

// --- DrawDocument -----------------------------------------------------------

static void GradientColorAt(Color a, Color b, float perc, Color* out) {
    int r = (int)((1 - perc) * GetRed(a) + perc * GetRed(b));
    int g = (int)((1 - perc) * GetGreen(a) + perc * GetGreen(b));
    int bl = (int)((1 - perc) * GetBlue(a) + perc * GetBlue(b));
    *out = MkRgb((u8)r, (u8)g, (u8)bl);
}

static void PaintVerticalGradient(gp::PaintCtx* ctx, Rect rc, Color top, Color bot) {
    gp::Bounds b = ToCanvas(rc);
    gp::Path* p = gp::PathNew(ctx, false);
    if (!p) {
        return;
    }
    gp::PathMoveTo(p, b.x, b.y);
    gp::PathLineTo(p, b.x + b.w, b.y);
    gp::PathLineTo(p, b.x + b.w, b.y + b.h);
    gp::PathLineTo(p, b.x, b.y + b.h);
    gp::PathClose(p);
    gp::PathFillGradientV(ctx, p, b.y, b.y + b.h, ToGpui(top), ToGpui(bot));
    gp::PathFree(p);
}

// --- the canvas overlays (orig Canvas.cpp) ---------------------------------

// CmdToggleImages. Like showLinks this is a debug aid (both live in the debug
// menu, so both are debug / pre-release only), and like it the outlines are
// only drawn, never saved
static bool gShowImages = false;

// CmdToggleImages: outline images the way showLinks outlines links (debug aid)
bool ShowImageOutlines() {
    return gShowImages;
}

void ToggleShowImageOutlines() {
    gShowImages = !gShowImages;
}

// CmdToggleTransparencyGrid: Acrobat-style checkerboard under the page so
// transparent PDFs (white art on a hole) are visible. Session-only, not saved.
static bool gShowTransparencyGrid = false;

bool ShowTransparencyGrid() {
    return gShowTransparencyGrid;
}

void ToggleTransparencyGrid() {
    gShowTransparencyGrid = !gShowTransparencyGrid;
}

// CmdDebugShowFitContentArea. Like gShowImages, a debug-only visualization that
// is drawn but never saved to settings
static bool gShowFitContentArea = false;

void ToggleShowFitContentArea() {
    gShowFitContentArea = !gShowFitContentArea;
}

bool ShowFitContentArea() {
    return gShowFitContentArea;
}

// ng: what the last paint drew, so a scripted run can tell an overlay that is
// on from one that is on and draws nothing (-dbg-control's TestOverlayState)
static int gOverlayShapes = 0;

int CanvasOverlayShapesDrawn() {
    return gOverlayShapes;
}

/* debug code to visualize links and images (can block while rendering) */
static void DebugOutlinePageElements(DisplayModel* dm, gp::PaintCtx* ctx, bool images) {
    Rect viewPortRect(Point(), dm->GetViewPort().Size());
    Kind elementKind = images ? kindPageElementImage : kindPageElementDest;

    // blue for links, green for images, so both can be on at once
    Color col = images ? MkRgb(0x00, 0xa0, 0x00) : kColBlue;

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }

        // don't block the paint (and the whole UI) behind a busy render thread
        // just to outline links; they get drawn on the next repaint
        Vec<IPageElement*> els;
        dm->GetEngine()->TryGetElements(pageNo, &els);

        for (auto& el : els) {
            if (!el->Is(elementKind)) {
                continue;
            }
            Rect rect = dm->CvtToScreen(pageNo, el->GetRect());
            Rect isect = viewPortRect.Intersect(rect);
            if (!isect.IsEmpty()) {
                isect.Inflate(2, 2);
                CanvasDrawRect(ctx, isect, col, 1);
                gOverlayShapes++;
            }
        }
    }
}

static void DebugShowLinks(DisplayModel* dm, gp::PaintCtx* ctx) {
    if (gShowImages) {
        DebugOutlinePageElements(dm, ctx, true);
    }
    if (!gSettings->showLinks) {
        return;
    }
    DebugOutlinePageElements(dm, ctx, false);
}

static Color ColorForPdfPageBox(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Media:
            return MkRgb(0x20, 0x20, 0x20);
        case PdfPageBoxKind::Crop:
            return MkRgb(0xc0, 0x20, 0x20);
        case PdfPageBoxKind::Bleed:
            return MkRgb(0x20, 0x40, 0xc0);
        case PdfPageBoxKind::Trim:
            return MkRgb(0x10, 0x90, 0x20);
        case PdfPageBoxKind::Art:
            return MkRgb(0xc0, 0x80, 0x00);
    }
    return kColBlack;
}

// Place the label so coincident boxes (crop == media, etc.) stay readable.
static Point PdfPageBoxLabelPos(const Rect& r, PdfPageBoxKind kind) {
    constexpr int kPad = 3;
    switch (kind) {
        case PdfPageBoxKind::Media:
            return Point(r.x + kPad, r.y + kPad);
        case PdfPageBoxKind::Crop:
            return Point(r.x + r.dx - kPad, r.y + kPad);
        case PdfPageBoxKind::Bleed:
            return Point(r.x + kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Trim:
            return Point(r.x + r.dx - kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Art:
            return Point(r.x + (r.dx / 2), r.y + kPad);
    }
    return r.TL();
}

// ng: orig passes DT_LEFT / DT_RIGHT / DT_CENTER to DrawText; there is no
// aligned text draw here, so the label rect says how to place the run
enum class PdfBoxLabelAlign {
    Left,
    Right,
    Center
};

static PdfBoxLabelAlign PdfPageBoxLabelAlign(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Crop:
        case PdfPageBoxKind::Trim:
            return PdfBoxLabelAlign::Right;
        case PdfPageBoxKind::Art:
            return PdfBoxLabelAlign::Center;
        case PdfPageBoxKind::Bleed:
        case PdfPageBoxKind::Media:
        default:
            return PdfBoxLabelAlign::Left;
    }
}

constexpr int kPdfBoxLabelDx = 44;
constexpr int kPdfBoxLabelDy = 14;

static Rect PdfPageBoxLabelRect(const Rect& box, PdfPageBoxKind kind) {
    Point p = PdfPageBoxLabelPos(box, kind);
    constexpr int kW = kPdfBoxLabelDx;
    constexpr int kH = kPdfBoxLabelDy;
    switch (kind) {
        case PdfPageBoxKind::Crop:
            return Rect(p.x - kW, p.y, kW, kH);
        case PdfPageBoxKind::Trim:
            // Bottom-right, above p (like Bleed). Drawing below the box clips
            // "trim" off the last/only page (#6005).
            return Rect(p.x - kW, p.y - kH, kW, kH);
        case PdfPageBoxKind::Bleed:
            return Rect(p.x, p.y - kH, kW, kH);
        case PdfPageBoxKind::Art:
            return Rect(p.x - (kW / 2), p.y, kW, kH);
        case PdfPageBoxKind::Media:
        default:
            return Rect(p.x, p.y, kW, kH);
    }
}

// Keep the label fully inside bounds so a box flush with the viewport
// does not clip the last few letters.
static Rect ClampRectTo(const Rect& r, const Rect& bounds) {
    Rect o = r;
    if (o.dx > bounds.dx) {
        o.dx = bounds.dx;
    }
    if (o.dy > bounds.dy) {
        o.dy = bounds.dy;
    }
    if (o.x < bounds.x) {
        o.x = bounds.x;
    }
    if (o.y < bounds.y) {
        o.y = bounds.y;
    }
    int right = bounds.x + bounds.dx;
    int bottom = bounds.y + bounds.dy;
    if (o.x + o.dx > right) {
        o.x = right - o.dx;
    }
    if (o.y + o.dy > bottom) {
        o.y = bottom - o.dy;
    }
    return o;
}

// CmdTogglePageBoxes: outline the PDF boxes this page actually declares
// (MediaBox / CropBox / BleedBox / TrimBox / ArtBox) and label them.
static void PaintPdfPageBoxes(DisplayModel* dm, gp::PaintCtx* ctx) {
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }
    constexpr float kBoxLabelFontSize = 11;
    Rect viewPortRect(Point(), dm->GetViewPort().Size());

    Vec<PdfPageBox> boxes;
    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        engine->GetPdfPageBoxes(pageNo, boxes);
        int n = len(boxes);
        for (int i = 0; i < n; i++) {
            const PdfPageBox& box = boxes[i];
            Rect rect = dm->CvtToScreen(pageNo, box.rect);
            // coincident boxes (crop == media) would paint on top of each
            // other; inset later kinds so every outline stays visible
            rect.Inflate(-(int)box.kind, -(int)box.kind);
            if (rect.dx < 2 || rect.dy < 2) {
                continue;
            }
            Rect vis = viewPortRect.Intersect(rect);
            if (vis.IsEmpty()) {
                continue;
            }
            Color col = ColorForPdfPageBox(box.kind);
            CanvasDrawRect(ctx, rect, col, 1);
            gOverlayShapes++;

            Str name = Str(PdfPageBoxName(box.kind));
            // MediaBox often extends past CropBox (the drawn page); pin the
            // label to the on-screen part so it isn't clipped off-canvas
            Rect labelRc = ClampRectTo(PdfPageBoxLabelRect(vis, box.kind), viewPortRect);
            FillRect(ctx, labelRc, kColWhite);
            Size txt = CanvasMeasureText(ctx, name, kBoxLabelFontSize);
            int x = labelRc.x;
            PdfBoxLabelAlign align = PdfPageBoxLabelAlign(box.kind);
            if (align == PdfBoxLabelAlign::Right) {
                x = labelRc.x + labelRc.dx - txt.dx;
            } else if (align == PdfBoxLabelAlign::Center) {
                x = labelRc.x + ((labelRc.dx - txt.dx) / 2);
            }
            CanvasDrawText(ctx, name, Point(x, labelRc.y), col, kBoxLabelFontSize);
        }
    }
}

/* debug code to visualize the area "Fit Content" zoom would fit to, without
   actually switching the zoom. When no content box is detected we outline the
   whole page, which is the same fallback PageSizeAfterRotation() uses */
static void DebugShowFitContentArea(DisplayModel* dm, gp::PaintCtx* ctx) {
    if (!gShowFitContentArea) {
        return;
    }
    Rect viewPortRect(Point(), dm->GetViewPort().Size());

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        // same cache DisplayModel uses for kZoomFitContent, so we don't
        // re-analyze the page on every repaint
        if (pi->contentBox.IsEmpty()) {
            pi->contentBox = dm->GetEngine()->PageContentBox(pageNo);
        }
        RectF box = pi->contentBox;
        if (box.IsEmpty()) {
            box = dm->PageMediaBox(pageNo);
        }
        Rect rect = dm->CvtToScreen(pageNo, box);
        if (!viewPortRect.Intersect(rect).IsEmpty()) {
            CanvasDrawRect(ctx, rect, kColRed, 2);
            gOverlayShapes++;
        }
    }
}

static bool DrawDocument(MainWindow* win, gp::PaintCtx* ctx, Rect rcArea) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }

    auto* engine = dm->GetEngine();
    bool isImage = engine->isImageCollection;
    bool isEbook = engine->kind == kindEngineMupdf && !str::EqI(engine->defaultExt, StrL(".pdf"));
    bool isPdf = engine->kind == kindEngineMupdf && str::EqI(engine->defaultExt, StrL(".pdf"));
    Color colDocBg;
    Color colDocTxt = ThemeDocumentColors(colDocBg);
    if (isImage) {
        colDocBg = 0x0;
        colDocTxt = 0xffffff;
        // allow ComicBookUI/ImageUI WindowBgCol to override the default black
        ParsedColor* bgOverride = nullptr;
        if (engine->kind == kindEngineComicBooks) {
            bgOverride = GetPrefsColor(gSettings->comicBookUI.windowBgCol);
        } else {
            bgOverride = GetPrefsColor(gSettings->imageUI.windowBgCol);
        }
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    } else if (isEbook) {
        ParsedColor* bgOverride = GetPrefsColor(gSettings->eBookUI.windowBgCol);
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    } else if (isPdf) {
        ParsedColor* bgOverride = GetPrefsColor(gSettings->fixedPageUI.windowBgCol);
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    }

    // per-document background color from FileState overrides everything
    WindowTab* curTab = win->CurrentTab();
    if (curTab && curTab->bgColorCheckered) {
        colDocBg = kColorUnset;
    } else if (curTab && curTab->bgColor != kColorUnset) {
        colDocBg = curTab->bgColor;
    }

    // placeholder painted where a page's bitmap isn't rendered yet
    Color colPlaceholder;
    ThemeDocumentColors(colPlaceholder);
    bool firstDocPaint = curTab && !curTab->everPaintedPage && !isImage;
    if (firstDocPaint) {
        colDocBg = ThemeMainWindowBackgroundColor();
        colDocTxt = ThemeWindowTextColor();
        colPlaceholder = colDocBg;
    }

    bool paintOnBlackWithoutShadow = win->presentation || isImage;
    bool shouldPaint = false;
    auto* gcols = gSettings->fixedPageUI.gradientColors;
    auto nGCols = len(*gcols);

    if (paintOnBlackWithoutShadow || colDocBg == kColorUnset) {
        if (colDocBg == kColorUnset) {
            PaintCheckerboard(ctx, rcArea);
        } else {
            FillRect(ctx, rcArea, colDocBg);
        }
    } else if (0 == nGCols) {
        FillRect(ctx, rcArea, colDocBg);
    } else {
        Color colors[3];
        colors[0] = ParseColor((*gcols)[0], kColWhite);
        if (nGCols == 1) {
            colors[1] = colors[2] = colors[0];
        } else if (nGCols == 2) {
            colors[2] = ParseColor((*gcols)[1], kColWhite);
            colors[1] =
                MkRgb((GetRed(colors[0]) + GetRed(colors[2])) / 2, (GetGreen(colors[0]) + GetGreen(colors[2])) / 2,
                      (GetBlue(colors[0]) + GetBlue(colors[2])) / 2);
        } else {
            colors[1] = ParseColor((*gcols)[1], kColWhite);
            colors[2] = ParseColor((*gcols)[2], kColWhite);
        }
        Size size = dm->GetCanvasSize();
        float percTop = 1.0F * (float)dm->GetViewPort().y / (float)size.dy;
        float percBot = 1.0F * (float)dm->GetViewPort().BR().y / (float)size.dy;
        if (!IsContinuous(dm->GetDisplayMode())) {
            percTop += (float)dm->CurrentPageNo() - 1;
            percTop /= (float)dm->PageCount();
            percBot += (float)dm->CurrentPageNo() - 1;
            percBot /= (float)dm->PageCount();
        }
        Size vp = dm->GetViewPort().Size();
        Color colTop, colBot;
        if (percTop < 0.5F) {
            GradientColorAt(colors[0], colors[1], 2 * percTop, &colTop);
        } else {
            GradientColorAt(colors[1], colors[2], 2 * (percTop - 0.5F), &colTop);
        }
        if (percBot < 0.5F) {
            GradientColorAt(colors[0], colors[1], 2 * percBot, &colBot);
        } else {
            GradientColorAt(colors[1], colors[2], 2 * (percBot - 0.5F), &colBot);
        }
        bool needCenter = percTop < 0.5F && percBot > 0.5F;
        if (!needCenter) {
            PaintVerticalGradient(ctx, Rect(0, 0, vp.dx, vp.dy), colTop, colBot);
        } else {
            // orig draws two gradient meshes meeting where the middle color is
            int yMid = (int)((0.5F - percTop) / (percBot - percTop) * (float)vp.dy);
            PaintVerticalGradient(ctx, Rect(0, 0, vp.dx, yMid), colTop, colors[1]);
            PaintVerticalGradient(ctx, Rect(0, yMid, vp.dx, vp.dy - yMid), colors[1], colBot);
        }
    }

    bool rendering = false;
    Rect screen(Point(), dm->GetViewPort().Size());
    bool anyPageVisible = false;

    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || 0.0F == pi->visibleRatio) {
            continue;
        }
        anyPageVisible = true;
        if (!pi->isShown) {
            continue;
        }

        Rect bounds = pi->pageOnScreen.Intersect(screen);
        // don't paint the frame background for images
        if (!isImage) {
            if (ShowTransparencyGrid()) {
                PaintCheckerboard(ctx, bounds);
            } else {
                Rect r = pi->pageOnScreen;
                auto presMode = win->presentation;
                PaintPageFrameAndShadow(ctx, bounds, r, presMode != PM_DISABLED, colPlaceholder);
            }
        }

        // check if this page is known to have failed rendering
        if (pi->failedToRender) {
            shouldPaint = true;
            DrawCenteredText(ctx, bounds, fmt(Tr("Couldn't render page %d").s, pageNo), colDocTxt);
            continue;
        }

        bool renderOutOfDateCue = false;
        int renderDelay = gRenderCache->Paint(ctx, bounds, dm, pageNo, pi, &renderOutOfDateCue);
        if (renderDelay == 0) {
            shouldPaint = true;
            if (curTab) {
                curTab->everPaintedPage = true;
            }
            // Paint() drew the stale tile and queued a replacement, then
            // returned 0. Without another frame the new bitmap never appears.
            if (renderOutOfDateCue) {
                win->repaintPending = true;
            }
            continue;
        }
        if (renderDelay != kRenderDelayFailed) {
            if (renderDelay >= kRenderDelayShowNotif) {
                // the page is taking a while to render: tell the user about it
                shouldPaint = true;
                DrawCenteredText(ctx, bounds, fmt(Tr("Rendering page %d...").s, pageNo), colDocTxt);
            }
            // ng: orig sets a repaint timer; the shell's tick does it here
            win->repaintPending = true;
            rendering = true;
        } else {
            shouldPaint = true;
            DrawCenteredText(ctx, bounds, fmt(Tr("Couldn't render page %d").s, pageNo), colDocTxt);
        }
    }
    if (!rendering) {
        gOverlayShapes = 0;
        DebugShowLinks(dm, ctx);
        DebugShowFitContentArea(dm, ctx);
        if (win->showPageBoxes) {
            PaintPdfPageBoxes(dm, ctx);
        }
    }

    // Empty viewport (narrow page on a canvas sized by a wider one): the last
    // frame is stale after a reload or jump. Flush the background. Issue #6136.
    if (!anyPageVisible) {
        shouldPaint = true;
    }
    return shouldPaint;
}

// --- scrollbars -------------------------------------------------------------

#if OS_WIN
static void PublishScrollInfo(HWND hwnd, int bar, const CanvasScrollInfo& si) {
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = si.nMin;
    info.nMax = si.nMax;
    info.nPage = (UINT)si.nPage;
    info.nPos = si.nPos;
    SetScrollInfo(hwnd, bar, &info, FALSE);
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (style & (WS_HSCROLL | WS_VSCROLL)) {
        SetWindowLongW(hwnd, GWL_STYLE, style & ~(WS_HSCROLL | WS_VSCROLL));
    }
}
#endif

static void MakeFullScrollbar(CanvasScrollInfo& si) {
    si.nPos = 0;
    si.nMin = 0;
    si.nMax = 99;
    si.nPage = 100;
}

// orig's ControllerCallbackHandler::UpdateScrollbars: the range the window
// scrollbars get. Here it also feeds the overlay bars the canvas draws.
void CanvasUpdateScrollbars(MainWindow* win, DisplayModel* dm, Size canvas) {
    // background tab (Home, another document) must not drive this window's bars
    if (!win || win->AsFixed() != dm) {
        return;
    }
    // every viewport change; the hint recompute waits until scrolling stops
    KeyboardLinkFollowingViewportChanged(win, -1);
    bool hideScrollbar = ScrollbarsAreHidden();
    CanvasScrollInfo si;
    Size viewPort = dm->GetViewPort().Size();

    if (viewPort.dx >= canvas.dx) {
        MakeFullScrollbar(si);
    } else {
        si.nPos = dm->GetViewPort().x;
        si.nMin = 0;
        si.nMax = canvas.dx - 1;
        si.nPage = viewPort.dx;
    }
    si.visible = (viewPort.dx < canvas.dx) && !hideScrollbar;
    win->scrollH = si;

    si = CanvasScrollInfo();
    bool isSinglePageMode = gSettings->scrollbarInSinglePage && (dm->GetDisplayMode() == DisplayMode::SinglePage);
    bool showVScroll = true;
    if (isSinglePageMode) {
        si.nPos = dm->CurrentPageNo() - 1; // 0-based position
        si.nMin = 0;
        si.nMax = dm->PageCount() - 1;
        si.nPage = 1; // one page visible at a time
    } else {
        if (viewPort.dy >= canvas.dy) {
            MakeFullScrollbar(si);
        } else {
            si.nPos = dm->GetViewPort().y;
            si.nMin = 0;
            si.nMax = canvas.dy - 1;
            si.nPage = viewPort.dy;

            if (kZoomFitPage != dm->GetZoomVirtual()) {
                // keep the top/bottom 5% of the previous page visible after paging down/up
                si.nPage = (int)(si.nPage * 0.95);
                si.nMax -= viewPort.dy - si.nPage;
            }
        }
        showVScroll = (viewPort.dy < canvas.dy);
    }
    si.visible = showVScroll && !hideScrollbar;
    win->scrollV = si;

    // orig's OverlayScrollbarSetInfo: a changed position re-reveals the bar
    OverlayScrollbarsNotifyScroll(win);
    AppShellInvalidate(win);
#if OS_WIN
    // Tests read the canvas scroll with GetScrollInfo. gpui has no scroll
    // styles; publish the range and strip the styles SetScrollInfo adds.
    HWND hwnd = AppShellNativeHwnd(win);
    if (hwnd) {
        PublishScrollInfo(hwnd, SB_HORZ, win->scrollH);
        PublishScrollInfo(hwnd, SB_VERT, win->scrollV);
    }
#endif
}

// what SetScrollInfo would have clamped the new position to
static int ClampScrollPos(const CanvasScrollInfo& si, int pos) {
    int maxPos = si.nMax - si.nPage + 1;
    return limitValue(pos, si.nMin, std::max(si.nMin, maxPos));
}

// Empty window: the last document's range would keep a windows-mode bar up.
void CanvasHideScrollbars(MainWindow* win) {
    if (!win) {
        return;
    }
    win->scrollV = {};
    win->scrollH = {};
    OverlayScrollbarHide(win->overlayScrollV);
    OverlayScrollbarHide(win->overlayScrollH);
    AppShellInvalidate(win);
}

int CanvasScrollPosV(MainWindow* win) {
    return win->scrollV.nPos;
}

static int ScrollLineAmount(int configuredAmount) {
    return configuredAmount > 0 ? configuredAmount : 16;
}

// Smooth wheel scrolling: frame-rate-independent exponential chase of the
// target offset (common browser-style "lerp toward destination").
//
// Why not duration + ease-out restarted each tick? Restarting ease-out on every
// wheel notch re-peaks velocity each notch -> visible stutter/pumping while
// spinning the wheel. Updating only the target keeps velocity continuous.
//
// Rate k (1/s): after ~200 ms we close ~95% of remaining (1-e^(-k*0.2)~=0.95).
static const double kSmoothScrollRate = 15.0;
// Snap when this close (pixels) so we do not crawl forever.
static const double kSmoothScrollSnapPx = 0.5;

// true while a wheel event is being turned into line scrolls
static bool gInMouseWheelScroll = false;

void StopSmoothScroll(MainWindow* win) {
    if (win) {
        win->scrollAnimActive = false;
    }
}

// ng: orig runs a 1 ms WM_TIMER with timeBeginPeriod(1); here the shell's tick
// (one per frame) advances the animation with the real elapsed time
static void StartOrUpdateSmoothScrollY(MainWindow* win, int targetY) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    int current = dm->yOffset();
    if (current == targetY && !win->scrollAnimActive) {
        return;
    }
    if (current == targetY && win->scrollAnimActive && fabs(win->scrollAnimY - (double)targetY) < kSmoothScrollSnapPx) {
        StopSmoothScroll(win);
        return;
    }
    win->scrollTargetY = targetY;
    if (win->scrollAnimActive) {
        // only the target changes; scrollAnimY keeps going
        return;
    }
    win->scrollAnimY = (double)current;
    win->scrollAnimActive = true;
    logf("SmoothScroll: %d -> %d\n", current, targetY);
}

void DocCanvasSmoothScrollTick(MainWindow* win, int elapsedMs) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->scrollAnimActive) {
        StopSmoothScroll(win);
        return;
    }
    // real dt so motion is smooth even when tick delivery jitters
    double dtMs = (double)elapsedMs;
    if (dtMs < 0.5) {
        dtMs = 0.5;
    } else if (dtMs > 32.0) {
        // should not jump a full page
        dtMs = 32.0;
    }
    double dt = dtMs / 1000.0;

    int target = win->scrollTargetY;
    int viewY = dm->yOffset();
    if (fabs(win->scrollAnimY - (double)viewY) > 1.5) {
        // something else moved the view
        win->scrollAnimY = (double)viewY;
    }
    double remaining = (double)target - win->scrollAnimY;
    if (fabs(remaining) < kSmoothScrollSnapPx) {
        if (viewY != target) {
            dm->ScrollYTo(target);
        }
        logf("SmoothScroll: settled at %d\n", target);
        StopSmoothScroll(win);
        return;
    }
    // exponential approach: pos += (target-pos) * (1 - e^(-k*dt))
    double a = 1.0 - exp(-kSmoothScrollRate * dt);
    a = std::min(a, 1.0);
    win->scrollAnimY += remaining * a;

    int y = (int)lround(win->scrollAnimY);
    if (y == viewY) {
        return;
    }
    dm->ScrollYTo(y);
    int after = dm->yOffset();
    if (after != y) {
        // ScrollYTo clamped at a document edge
        win->scrollAnimY = (double)after;
        win->scrollTargetY = after;
        StopSmoothScroll(win);
    }
}

void CanvasOnVScroll(MainWindow* win, ScrollMsg msg, int nTrackPos) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    CanvasScrollInfo si = win->scrollV;
    auto* ctrl = win->ctrl;
    bool dmIsSinglePage = (ctrl->GetDisplayMode() == DisplayMode::SinglePage);
    // if true, the scrollbar position is the page number, so the user can page
    // through the document with it even in single page mode
    bool singlePageWithScrollbar = gSettings->scrollbarInSinglePage && dmIsSinglePage;

    int lineHeight = DpiScale(ScrollLineAmount(gSettings->scrollLineAmount));
    bool isFitPage = (kZoomFitPage == ctrl->GetZoomVirtual());
    if (!IsContinuous(ctrl->GetDisplayMode()) && isFitPage) {
        lineHeight = 1;
    }
    // orig's OnVScroll reports the intent so the end-of-document next-file hint
    // shows on a scroll down and goes away on a scroll up
    bool scrollDown = (msg == ScrollMsg::LineDown || msg == ScrollMsg::PageDown || msg == ScrollMsg::HalfPageDown ||
                       msg == ScrollMsg::Bottom);
    bool scrollUp =
        (msg == ScrollMsg::LineUp || msg == ScrollMsg::PageUp || msg == ScrollMsg::HalfPageUp || msg == ScrollMsg::Top);

    // SmoothScroll eases wheel input and arrow-key / scrollbar line steps
    // (issue #4662). Page-up/down and thumb stay instant.
    bool isLineScroll = (msg == ScrollMsg::LineUp || msg == ScrollMsg::LineDown);
    bool useSmoothScroll = gSettings->smoothScroll && (gInMouseWheelScroll || isLineScroll);
    // step from the pending target, not the lagging view position (issue #5857)
    if (useSmoothScroll && win->scrollAnimActive) {
        si.nPos = win->scrollTargetY;
    }

    if (singlePageWithScrollbar) {
        int targetPage = ctrl->CurrentPageNo();
        switch (msg) {
            case ScrollMsg::Top:
                targetPage = 1;
                break;
            case ScrollMsg::Bottom:
                targetPage = ctrl->PageCount();
                break;
            case ScrollMsg::LineUp:
            case ScrollMsg::HalfPageUp:
            case ScrollMsg::PageUp:
                targetPage = std::max(1, targetPage - 1);
                break;
            case ScrollMsg::LineDown:
            case ScrollMsg::HalfPageDown:
            case ScrollMsg::PageDown:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case ScrollMsg::ThumbTrack:
                targetPage = nTrackPos + 1;
                break;
            default:
                break;
        }
        if (targetPage != ctrl->CurrentPageNo()) {
            ctrl->GoToPage(targetPage, true);
        }
        if (scrollDown || scrollUp) {
            OnDocumentVerticalScrollIntent(win, scrollDown);
        }
        return;
    }

    int currPos = si.nPos;
    int halfPage = si.nPage / 2;
    switch (msg) {
        case ScrollMsg::Top:
            si.nPos = si.nMin;
            break;
        case ScrollMsg::Bottom:
            si.nPos = si.nMax;
            break;
        case ScrollMsg::LineUp:
            si.nPos -= lineHeight;
            break;
        case ScrollMsg::LineDown:
            si.nPos += lineHeight;
            break;
        case ScrollMsg::HalfPageUp:
            si.nPos -= halfPage;
            break;
        case ScrollMsg::HalfPageDown:
            si.nPos += halfPage;
            break;
        case ScrollMsg::PageUp:
            si.nPos -= si.nPage;
            break;
        case ScrollMsg::PageDown:
            si.nPos += si.nPage;
            break;
        case ScrollMsg::ThumbTrack:
            si.nPos = nTrackPos;
            break;
        default:
            break;
    }
    si.nPos = ClampScrollPos(si, si.nPos);
    if (useSmoothScroll) {
        // the thumb would run ahead of the view, so it is left to the ticks
        StartOrUpdateSmoothScrollY(win, si.nPos);
    } else {
        StopSmoothScroll(win);
        win->scrollV.nPos = si.nPos;
        if (si.nPos != currPos) {
            dm->ScrollYTo(si.nPos);
        }
    }
    if (scrollDown || scrollUp) {
        OnDocumentVerticalScrollIntent(win, scrollDown);
    }
    ReadAloudOnUserViewChanged(win);
}

void CanvasOnHScroll(MainWindow* win, ScrollMsg msg, int nTrackPos) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    CanvasScrollInfo si = win->scrollH;
    int currPos = si.nPos;
    int lineAmount = DpiScale(ScrollLineAmount(gSettings->scrollLineAmount));
    switch (msg) {
        case ScrollMsg::Left:
            si.nPos = si.nMin;
            break;
        case ScrollMsg::Right:
            si.nPos = si.nMax;
            break;
        case ScrollMsg::LineLeft:
            si.nPos -= lineAmount;
            break;
        case ScrollMsg::LineRight:
            si.nPos += lineAmount;
            break;
        case ScrollMsg::PageLeft:
            si.nPos -= si.nPage;
            break;
        case ScrollMsg::PageRight:
            si.nPos += si.nPage;
            break;
        case ScrollMsg::ThumbTrack:
            si.nPos = nTrackPos;
            break;
        default:
            break;
    }
    si.nPos = ClampScrollPos(si, si.nPos);
    win->scrollH.nPos = si.nPos;
    if (si.nPos != currPos) {
        dm->ScrollXTo(si.nPos);
    }
    ReadAloudOnUserViewChanged(win);
}

// --- mouse wheel ------------------------------------------------------------

// win32 reports the wheel in WHEEL_DELTA units per SPI_GETWHEELSCROLLLINES
// lines; gpui reports pixels for a fixed three lines
static int gDeltaPerLine = kWheelDelta / 3;

void UpdateDeltaPerLine() {
#if OS_WIN
    ULONG ulScrollLines;
    BOOL ok = SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
    if (!ok) {
        return;
    }
    gDeltaPerLine = 0;
    if (ulScrollLines == (ULONG)-1) {
        gDeltaPerLine = -1;
    } else if (ulScrollLines != 0) {
        gDeltaPerLine = kWheelDelta / (int)ulScrollLines;
    }
#endif
}

static int WheelScrollPosOrTarget(MainWindow* win) {
    if (gSettings->smoothScroll && win->scrollAnimActive) {
        return win->scrollTargetY;
    }
    return win->scrollV.nPos;
}

// this does zooming via mouse wheel (with ctrl or the right mouse button)
static void ZoomByMouseWheel(MainWindow* win, int delta, Point pt) {
    StopSmoothScroll(win);
    win->dragStartPending = false;
    // when ZoomIncrement is zero/negative, zoom must step through ZoomLevels (issue #5662)
    bool discreteWheelZoom = gSettings->zoomIncrement <= 0;
    if (discreteWheelZoom) {
        float newZoom = win->ctrl->GetNextZoomStep(delta < 0 ? kZoomMin : kZoomMax);
        SmartZoom(win, newZoom, &pt, false);
        return;
    }

    static TimeStamp lastWheelMsgTime{};
    static bool hadWheelMsg = false;
    static int accumDelta = 0;
    static float initialZoomVirtual = 0;

    // 150 ms is a heuristic: a wheel event that soon after the last one
    // continues the same zoom gesture
    if (!hadWheelMsg || TimeSinceInMs(lastWheelMsgTime) >= 150.0) {
        initialZoomVirtual = win->ctrl->GetZoomVirtual(true);
        accumDelta = 0;
    }
    lastWheelMsgTime = TimeGet();
    hadWheelMsg = true;

    // special case the value coming from a touchpad pinch: WHEEL_DELTA is too
    // fast, so slow the zoom down (10 is a heuristic)
    if (delta == kWheelDelta) {
        delta = 10;
    } else if (delta == -kWheelDelta) {
        delta = -10;
    }
    accumDelta += delta;
    bool negative = accumDelta < 0;
    float factor = (float)std::abs(accumDelta) / 100.F;
    factor = 1.F + factor;
    if (negative) {
        factor = 1.F / factor;
    }
    SmartZoom(win, initialZoomVirtual * factor, &pt, false);
}

// whether a wheel notch in Fit Content should flip a whole page instead of
// scrolling. Always in the non-continuous modes
static bool FitContentWheelFlipsPage(DisplayModel* dm) {
    if (!dm || dm->GetZoomVirtual() != kZoomFitContent) {
        return false;
    }
    return !IsContinuous(dm->GetDisplayMode());
}

// Comics decode slowly. Queued wheel notches after a page turn skip unread
// pages (#6144). Allow another turn only after the current page is on screen
// and a short gap has passed.
constexpr double kWheelPageTurnGapMs = 250;

static bool WheelMayTurnPage(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->GetEngine() || !dm->GetEngine()->isImageCollection) {
        return true;
    }
    DocCanvasUI* ui = Ui(win);
    if (ui->wheelDidTurnPage && TimeSinceInMs(ui->wheelPageTurnTime) < kWheelPageTurnGapMs) {
        return false;
    }
    if (gRenderCache && !gRenderCache->Exists(dm, dm->CurrentPageNo(), dm->GetRotation())) {
        return false;
    }
    return true;
}

// Use the source rect retained by the popup. Engine hit-testing may be busy
// while the popup is rendering and lose later wheel notches.
static bool RefHoverTakesWheel(MainWindow* win, int delta, bool isCtrl, bool isShift, Point pt) {
    RefHoverState* s = win->refHover;
    if (!s || !s->visible || (!isCtrl && !isShift)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    int srcPage = s->displayed.srcPage;
    if (!dm || !dm->ValidPageNo(srcPage)) {
        return false;
    }
    PointF pagePt = dm->CvtFromScreen(pt, srcPage);
    if (!s->displayed.srcRect.Contains(pagePt)) {
        return false;
    }
    if (isCtrl) {
        RefHoverWheelZoom(s, dm->GetEngine(), delta);
    } else {
        RefHoverWheelScroll(s, dm->GetEngine(), delta);
    }
    return true;
}

// ng: orig asks whether the cursor is right of the canvas' client area; the
// vertical scrollbar is an element of its own here and says so itself
static bool gWheelOverVScrollbar = false;

static void CanvasOnMouseWheel(MainWindow* win, int delta, bool isCtrl, bool isShift, bool isAlt, Point pt) {
    // ignore wheel events while middle-button drag-scrolling is active
    if (MouseAction::Scrolling == win->mouseAction) {
        return;
    }
    if (!win->ctrl) {
        return;
    }
    // Mouse-wheel on the citation-hover popup (cursor still on the citation
    // link that opened it). Avoids moving the cursor onto the popup itself,
    // which would dismiss the hover.
    //   shift+wheel -> scroll popup content (rolls over to prev/next page)
    //   ctrl+wheel  -> zoom popup content
    //   plain wheel -> falls through to scroll the main document, as if the
    //                  popup weren't there
    if (RefHoverTakesWheel(win, delta, isCtrl, isShift, pt)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (isCtrl) {
        ZoomByMouseWheel(win, delta, pt);
        return;
    }

    bool hScroll = isShift;
    bool vScroll = !hScroll;
    bool isCont = !IsContinuous(win->ctrl->GetDisplayMode());

    // Alt speeds up scrolling but also triggers showing menu
    // this will suppress next menu trigger to avoid accidental triggering of menu
    if (isAlt) {
        AppShellSuppressAltMenu(win);
    }

    // orig: run the next-file-in-folder tip after any vertical wheel handling
    struct VerticalScrollIntentGuard {
        MainWindow* win = nullptr;
        bool down = false;
        bool armed = false;
        ~VerticalScrollIntentGuard() {
            if (armed && win) {
                OnDocumentVerticalScrollIntent(win, down);
            }
        }
    } scrollIntent;
    if (vScroll) {
        scrollIntent.win = win;
        scrollIntent.down = delta < 0;
        scrollIntent.armed = true;
    }

    // MouseWheelTurnsPage: a wheel notch is a page turn, not a scroll, even
    // when the page is zoomed past the window. Alt + wheel still scrolls
    if (vScroll && !isAlt && gSettings->mouseWheelTurnsPage) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= kWheelDelta) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= kWheelDelta;
        } else if (win->wheelAccumDelta <= -kWheelDelta) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += kWheelDelta;
        }
        return;
    }

    // fit content: flip page on wheel, regardless of scrollbar state
    if (vScroll && dm && FitContentWheelFlipsPage(dm) && IsSingle(dm->GetDisplayMode())) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= kWheelDelta) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= kWheelDelta;
        } else if (win->wheelAccumDelta <= -kWheelDelta) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += kWheelDelta;
        }
        return;
    }

    bool isSinglePageMode =
        gSettings->scrollbarInSinglePage && (win->ctrl->GetDisplayMode() == DisplayMode::SinglePage);

    // in SinglePage mode a document that fits the viewport pages instead
    if (isSinglePageMode && vScroll && (!dm || !dm->NeedVScroll())) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= kWheelDelta) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= kWheelDelta;
        } else if (win->wheelAccumDelta <= -kWheelDelta) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += kWheelDelta;
        }
        return;
    }

    // page-by-page navigation for the other non-continuous modes
    if (vScroll && !isCont && !isSinglePageMode) {
        // in fit content we may show a vertical scrollbar but still want to
        // flip the whole page on the wheel
        bool flipPage = FitContentWheelFlipsPage(dm);
        if (dm && !dm->NeedVScroll()) {
            // if the page(s) fully fit in the window, flip the whole page
            flipPage = true;
        }
        // fit content/page: one wheel click = one page; otherwise 3 clicks
        int pageFlipDelta = flipPage ? kWheelDelta : kWheelDelta * 3;
        if (flipPage) {
            win->wheelAccumDelta += delta;
            if (win->wheelAccumDelta >= pageFlipDelta) {
                win->ctrl->GoToPrevPage();
                win->wheelAccumDelta -= pageFlipDelta;
            } else if (win->wheelAccumDelta <= -pageFlipDelta) {
                win->ctrl->GoToNextPage();
                win->wheelAccumDelta += pageFlipDelta;
            }
            return;
        }
    }

    if (gDeltaPerLine == 0) {
        return;
    }

    // For SinglePage mode with zoomed content, use continuous scrolling with page transitions
    if (isSinglePageMode && vScroll && dm) {
        if (dm->NeedVScroll()) {
            // Use continuous scrolling that handles page transitions at boundaries
            const CanvasScrollInfo& si = hScroll ? win->scrollH : win->scrollV;
            int scrollBy = -(int)(((i64)si.nPage * delta * 30) / kWheelDelta);
            // on sensitive touchpads delta can be very small
            if (scrollBy == 0) {
                return;
            }
            dm->ScrollYBy(scrollBy, gSettings->scrollEdgeTurnsPage);
            // ScrollYBy updates the thumb; also force the thin smart bar to
            // appear for wheel-only reading (#5859).
            if (ScrollbarsUseOverlay()) {
                OverlayScrollbarNotifyScroll(win->overlayScrollV);
            }
            ReadAloudOnUserViewChanged(win);
            return;
        }
    }

    if (gDeltaPerLine < 0 && dm) {
        // scroll by (a fraction of) a page
        const CanvasScrollInfo& si = hScroll ? win->scrollH : win->scrollV;
        int scrollBy = -(int)(((i64)si.nPage * delta) / kWheelDelta);
        if (scrollBy == 0) {
            return;
        }
        if (hScroll) {
            dm->ScrollXBy(scrollBy);
        } else {
            dm->ScrollYBy(scrollBy, gSettings->scrollEdgeTurnsPage);
        }
        if (ScrollbarsUseOverlay()) {
            OverlayScrollbarNotifyScroll(hScroll ? win->overlayScrollH : win->overlayScrollV);
        }
        ReadAloudOnUserViewChanged(win);
        return;
    }

    // alt while scrolling scrolls by half a page per tick, useful for
    // browsing long files
    if (isAlt) {
        CanvasOnVScroll(win, delta > 0 ? ScrollMsg::HalfPageUp : ScrollMsg::HalfPageDown);
        return;
    }

    if (gSettings->fastScrollOverScrollbar) {
        // scroll faster if the cursor is over the scroll bar
        if (gWheelOverVScrollbar) {
            CanvasOnVScroll(win, delta > 0 ? ScrollMsg::HalfPageUp : ScrollMsg::HalfPageDown);
            return;
        }
    }

    win->wheelAccumDelta += delta;
    // while a smooth scroll runs the view lags the target, so "did this notch
    // move anything" has to ask the target (orig's WheelScrollPosOrTarget)
    int prevScrollPos = WheelScrollPosOrTarget(win);

    bool didScrollByLine = false;
    if (win->wheelAccumDelta < 0) {
        while (win->wheelAccumDelta <= -gDeltaPerLine) {
            if (hScroll) {
                CanvasOnHScroll(win, ScrollMsg::LineRight);
            } else {
                CanvasOnVScroll(win, ScrollMsg::LineDown);
            }
            win->wheelAccumDelta += gDeltaPerLine;
            didScrollByLine = true;
        }
    } else {
        while (win->wheelAccumDelta >= gDeltaPerLine) {
            if (hScroll) {
                CanvasOnHScroll(win, ScrollMsg::LineLeft);
            } else {
                CanvasOnVScroll(win, ScrollMsg::LineUp);
            }
            win->wheelAccumDelta -= gDeltaPerLine;
            didScrollByLine = true;
        }
    }
    // in non-continuous mode flip the page if necessary
    if (!vScroll || !isCont || !gSettings->scrollEdgeTurnsPage) {
        return;
    }
    if (!didScrollByLine) {
        return;
    }
    if (WheelScrollPosOrTarget(win) != prevScrollPos) {
        // we don't flip a page if we did scroll by line
        return;
    }
    if (!WheelMayTurnPage(win)) {
        win->wheelAccumDelta = 0;
        return;
    }
    if (delta > 0) {
        win->ctrl->GoToPrevPage(true);
    } else if (dm) {
        // this page turn continues a scroll, so start the new page at its top
        dm->GoToNextPage(false);
    } else {
        win->ctrl->GoToNextPage();
    }
    win->wheelAccumDelta = 0;
    Ui(win)->wheelPageTurnTime = TimeGet();
    Ui(win)->wheelDidTurnPage = true;
}

// --- mouse ------------------------------------------------------------------

// how much slower the document moves than the cursor during middle-button
// auto-scroll (orig's kSelectSmoothScrollSlowDownFactor)
constexpr float kAutoScrollSlowDown = 10.f;

static bool SetLaserPointerCursor(MainWindow* win);

static void StartMouseDrag(MainWindow* win, int x, int y, bool right = false) {
    win->mouseAction = MouseAction::Dragging;
    win->dragRightClick = right;
    win->dragPrevPos = Point(x, y);
    // orig's gCursorDrag
    if (!SetLaserPointerCursor(win)) {
        CanvasSetNativeCursor(win, NativeCursor::Drag);
    }
}

static bool IsRightDragging(MainWindow* win) {
    if (win->mouseAction != MouseAction::Dragging) {
        return false;
    }
    return win->dragRightClick;
}

static void StopMouseDrag(MainWindow* win, int x, int y, bool aborted = false) {
    if (win->mouseAction != MouseAction::Dragging) {
        return;
    }
    win->mouseAction = MouseAction::None;
    if (aborted) {
        return;
    }
    Size drag(x - win->dragPrevPos.x, y - win->dragPrevPos.y);
    win->MoveDocBy(drag.dx, -2 * drag.dy);
}

static void OnMouseMiddleButtonDown(MainWindow* win, int x, int y) {
    if (win->mouseAction == MouseAction::None) {
        win->mouseAction = MouseAction::Scrolling;
        win->dragStartPending = true;
        // the farther the mouse moves from here, the faster the document scrolls
        win->dragStart = Point(x, y);
        win->xScrollSpeed = 0;
        win->yScrollSpeed = 0;
        win->xScrollAccum = 0;
        win->yScrollAccum = 0;
    } else if (win->mouseAction == MouseAction::Scrolling) {
        win->mouseAction = MouseAction::None;
    }
}

static void OnMouseMiddleButtonUp(MainWindow* win) {
    // a middle-click that started auto-scrolling and then moved is a drag, and
    // releasing it ends the scroll; releasing without moving leaves auto-scroll
    // latched on, which is what dragStartPending still being set means
    if (win->mouseAction == MouseAction::Scrolling && !win->dragStartPending) {
        win->mouseAction = MouseAction::None;
    }
}

// CmdStartAutoScroll: start / stop auto-scroll anchored at the cursor, exactly
// as a middle-click there would. orig asks win32 for the cursor position; the
// canvas already tracks it in dragPrevPos.
void StartAutoScrollAtCursor(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return;
    }
    ReadingAutoScrollStop(win);
    Point pt = win->dragPrevPos;
    win->xScrollAccum = 0;
    win->yScrollAccum = 0;
    OnMouseMiddleButtonDown(win, pt.x, pt.y);
    AppShellInvalidate(win);
}

// --- laser pointer (CmdToggleLaserPointer) ----------------------------------

// A laser pointer is a session mode, not a setting: it's turned on to point at
// something during a presentation and off again.
// ng: orig makes a HCURSOR out of it. gpui has a fixed set of cursor shapes and
// no way to supply a bitmap (see "gpui gaps"), so the dot is painted on the
// canvas and the system cursor is hidden under it.
static bool gLaserPointer = false;

constexpr int kLaserPointerSize = 32;

bool IsLaserPointerActive() {
    return gLaserPointer;
}

// ng: where the system cursor can be a bitmap (Windows) the dot is orig's
// cursor; elsewhere it is painted on the canvas under a hidden cursor
constexpr bool kLaserPointerIsCursor = OS_WIN;

static void OnSetCursor(MainWindow* win, Point pt);

void ToggleLaserPointer(MainWindow* win) {
    gLaserPointer = !gLaserPointer;
    logf("ToggleLaserPointer: %d\n", (int)gLaserPointer);
    if (!gLaserPointer) {
        AppShellShowCursor(win, true);
    }
    // change the cursor now rather than on the next mouse move
    if (win->AsFixed()) {
        OnSetCursor(win, win->dragPrevPos);
    }
    AppShellInvalidate(win);
}

// while on, the canvas cursor is the laser dot no matter what is under it:
// links, text and annotations still work, they just don't change the cursor
static bool SetLaserPointerCursor(MainWindow* win) {
    if (!gLaserPointer) {
        return false;
    }
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // the presenter blanked the screen on purpose, don't put a dot on it
        return false;
    }
    return CanvasSetNativeCursor(win, NativeCursor::LaserPointer);
}

// a white-hot center inside a saturated red core, surrounded by a soft halo
static void PaintLaserPointer(MainWindow* win, gp::PaintCtx* ctx) {
    if (!gLaserPointer || kLaserPointerIsCursor) {
        return;
    }
    Point pt = win->dragPrevPos;
    int r = kLaserPointerSize / 2;
    auto dot = [&](int radius, Color col) {
        CanvasDrawEllipse(ctx, Rect(pt.x - radius, pt.y - radius, radius * 2, radius * 2), col, 0);
    };
    dot(r, MkRgba(0xff, 0x20, 0x20, 0x40));
    dot(r / 2, MkRgba(0xff, 0x10, 0x10, 0xd0));
    dot(r / 5, MkRgba(0xff, 0xe0, 0xe0, 0xff));
}

// orig's IsDragDistance, with the system drag thresholds
bool IsDragDistance(int x1, int x2, int y1, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
#if OS_WIN
    return dx > GetSystemMetrics(SM_CXDRAG) || dy > GetSystemMetrics(SM_CYDRAG);
#else
    return dx > DpiScale(4) || dy > DpiScale(4);
#endif
}

// --- links: cursor, tooltip, follow ------------------------------------------

constexpr int kCurArrow = (int)gp::CursorKind::Arrow;
constexpr int kCurIBeam = (int)gp::CursorKind::IBeam;
constexpr int kCurHand = (int)gp::CursorKind::Pointer;
constexpr int kCurCross = (int)gp::CursorKind::Crosshair;
// orig's IDC_SIZEALL (middle-button auto-scroll, a line end or polygon vertex
// handle). ng: gpui has no "move" cursor kind; its ClosedHand is IDC_SIZEALL in
// the Windows backend, a closed hand elsewhere, where the crosshair stays
#if OS_WIN
constexpr int kCurSizeAll = (int)gp::CursorKind::ClosedHand;
#else
constexpr int kCurSizeAll = (int)gp::CursorKind::Crosshair;
#endif

static void SetLinkTooltip(MainWindow* win, Str text, Rect rc) {
    if (str::Eq(win->linkTooltip, text) && win->linkTooltipRc == rc) {
        return;
    }
    str::ReplaceWithCopy(&win->linkTooltip, text);
    win->linkTooltipRc = rc;
    logf("LinkTooltip: '%s' at %d,%d,%d,%d\n", text, rc.x, rc.y, rc.dx, rc.dy);
    AppShellInvalidate(win);
}

void DeleteLinkTooltip(MainWindow* win) {
    if (len(win->linkTooltip) == 0) {
        return;
    }
    str::Free(win->linkTooltip);
    win->linkTooltip = {};
    AppShellInvalidate(win);
}

static void SetTextOrArrowCursor(MainWindow* win, DisplayModel* dm, Point pt) {
    CanvasSetCursor(win, dm->IsOverText(pt) ? kCurIBeam : kCurArrow);
}

// defined with the annotation half below
static ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot);
static int CursorForResizeHandle(ResizeHandle handle);
static Annotation* AnnotationLockingMouse(MainWindow* win);

// orig's OnSetCursorMouseNone: the cursor and the link tooltip while no mouse
// button is down. The laser-pointer branch is step 14.
static void OnSetCursorMouseNone(MainWindow* win, Point pt) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        DeleteLinkTooltip(win);
        return;
    }

    // the cursor-position tip is for measuring: a crosshair points at exactly
    // the spot it reports
    if (GetNotificationForGroup(win, kNotifCursorPos)) {
        CanvasSetCursor(win, kCurCross);
        DeleteLinkTooltip(win);
        return;
    }

    // a form field warrants its own cursor, whatever else is under the pointer
    Annotation* widget = dm->GetWidgetAtPos(pt);
    WidgetCursorKind wk = GetWidgetCursorKind(widget);
    if (wk != WidgetCursorKind::None) {
        CanvasSetCursor(win, wk == WidgetCursorKind::Text ? kCurIBeam : kCurHand);
        DeleteLinkTooltip(win);
        return;
    }

    // the highlighter only selects text: annotations get no hover
    Annotation* selected = win->CurrentTab() ? win->CurrentTab()->selectedAnnotation : nullptr;
    Annotation* annot = IsPlacingHighlighterAnnotation(win) ? nullptr : dm->GetAnnotationAtPos(pt, selected);
    if (annot != win->annotationUnderCursor) {
        win->annotationUnderCursor = annot;
        AppShellInvalidate(win);
    }
    bool annotEditHover = annot && (win->pdfAnnotationsToolbarEnabled || selected);

    int pageNo = 0;
    IPageElement* pageEl = dm->GetElementAtPos(pt, &pageNo);
    if (pageEl && pageEl->Is(kindPageElementDest) && gSettings->disableLinks) {
        pageEl = nullptr;
    }
    if (!pageEl) {
        if (annotEditHover) {
            CanvasSetCursor(win, kCurHand);
        } else {
            SetTextOrArrowCursor(win, dm, pt);
        }
        DeleteLinkTooltip(win);
        return;
    }
    // The Edit PDF hover card and the text popup have the annotation's contents.
    // Do not put the old one-line comment tooltip on top of them.
    bool annotCardShown = win->pdfAnnotationsToolbarEnabled || IsAnnotationTextPopupShownFor(win, annot);
    if (annotCardShown && annot && pageEl->Is(kindPageElementComment)) {
        DeleteLinkTooltip(win);
        CanvasSetCursor(win, kCurHand);
        return;
    }
    Str text = pageEl->GetValue();
    if (!dm->ValidPageNo(pageNo)) {
        logf("OnSetCursorMouseNone: page element '%s' on invalid page %d\n", Str(text), pageNo);
        return;
    }
    Rect rc = dm->CvtToScreen(pageNo, pageEl->GetRect());
    SetLinkTooltip(win, text, rc);
    // keep the hand cursor while editing an annotation, but still show the
    // comment tooltip (issue #5329)
    if (annotEditHover || pageEl->Is(kindPageElementDest)) {
        CanvasSetCursor(win, kCurHand);
    } else {
        SetTextOrArrowCursor(win, dm, pt);
    }
}

// orig's OnSetCursor: the cursor that belongs to the action in progress
static void OnSetCursor(MainWindow* win, Point pt) {
    if (win->mouseAction != MouseAction::None) {
        DeleteLinkTooltip(win);
    }
    if (ReadingBarOnSetCursor(win, pt.x, pt.y)) {
        DeleteLinkTooltip(win);
        return;
    }
    if (AnnotationPlacementOnSetCursor(win)) {
        DeleteLinkTooltip(win);
        return;
    }
    // the laser dot replaces every other cursor, and while pointing at the page
    // during a talk a link tooltip popping up is just in the way
    if (SetLaserPointerCursor(win)) {
        DeleteLinkTooltip(win);
        return;
    }
    if (IsPlacingSignature(win)) {
        CanvasSetCursor(win, kCurCross);
        DeleteLinkTooltip(win);
        return;
    }
    // a resize handle of the selected annotation, before anything else
    WindowTab* selTab = win->CurrentTab();
    Annotation* selected = selTab ? selTab->selectedAnnotation : nullptr;
    if (selected && AnnotationCanBeResized(selected->type)) {
        ResizeHandle handle =
            win->annotationBeingResized ? (ResizeHandle)win->resizeHandle : GetResizeHandleAt(win, pt, selected);
        if (handle != ResizeHandle::None) {
            CanvasSetCursor(win, CursorForResizeHandle(handle));
            DeleteLinkTooltip(win);
            return;
        }
    }
    // an annotation being edited: no hover cursors for anything else
    if (Annotation* locked = AnnotationLockingMouse(win)) {
        DisplayModel* dmLocked = win->AsFixed();
        bool onSelected = dmLocked && dmLocked->GetAnnotationAtPos(pt, locked) == locked;
        CanvasSetCursor(win, onSelected ? kCurHand : kCurArrow);
        DeleteLinkTooltip(win);
        return;
    }
    switch (win->mouseAction) {
        case MouseAction::Dragging:
            // orig's gCursorDrag, the hand of dragcursor.cur
            if (!CanvasSetNativeCursor(win, NativeCursor::Drag)) {
                CanvasSetCursor(win, kCurCross);
            }
            return;
        case MouseAction::Scrolling:
            CanvasSetCursor(win, kCurSizeAll);
            return;
        case MouseAction::SelectingText:
            CanvasSetCursor(win, kCurIBeam);
            return;
        case MouseAction::Selecting:
            if (win->selectionDragEdge != SelectionDragEdge::None) {
                CanvasSetCursor(win, CursorIdForSelectionEdge(win->selectionDragEdge));
                return;
            }
            CanvasSetCursor(win, kCurCross);
            return;
        case MouseAction::None:
            // resize / move cursors over an existing rectangular selection
            if (IsRectangularSelection(win)) {
                SelectionDragEdge edge = HitTestRectangularSelection(win, pt.x, pt.y);
                if (edge != SelectionDragEdge::None) {
                    CanvasSetCursor(win, CursorIdForSelectionEdge(edge));
                    DeleteLinkTooltip(win);
                    return;
                }
            }
            OnSetCursorMouseNone(win, pt);
            return;
    }
}

// --- annotations (step 13a; orig's Canvas.cpp half) -------------------------

// Size of resize handle hit area (in pixels)
constexpr int kResizeHandleSize = 8;

static bool IsLineEndpointHandle(ResizeHandle handle) {
    return handle == ResizeHandle::LineStart || handle == ResizeHandle::LineEnd;
}

static bool IsVertexHandle(ResizeHandle handle) {
    return handle == ResizeHandle::Vertex;
}

static bool IsPolyVertexType(AnnotationType tp) {
    return tp == AnnotationType::PolyLine || tp == AnnotationType::Polygon;
}

// Line annotations: hit-test the two endpoints, not the bounding-box handles.
static ResizeHandle GetLineEndpointHandleAt(DisplayModel* dm, Point pt, Annotation* annot) {
    PointF start, end;
    if (!GetLinePoints(annot, start, end)) {
        return ResizeHandle::None;
    }
    Point startPt = dm->CvtToScreen(annot->pageNo, start);
    Point endPt = dm->CvtToScreen(annot->pageNo, end);
    int hs = kResizeHandleSize;
    auto dist = [&](Point p) { return std::max(abs(pt.x - p.x), abs(pt.y - p.y)); };
    int dStart = dist(startPt);
    int dEnd = dist(endPt);
    if (dStart <= hs && dStart <= dEnd) {
        return ResizeHandle::LineStart;
    }
    if (dEnd <= hs) {
        return ResizeHandle::LineEnd;
    }
    return ResizeHandle::None;
}

// PolyLine / Polygon: hit-test each vertex. Returns index, or -1.
static int GetPolyVertexAt(DisplayModel* dm, Point pt, Annotation* annot) {
    if (!annot || !IsPolyVertexType(annot->type)) {
        return -1;
    }
    Vec<PointF> pts = GetVertices(annot);
    int n = len(pts);
    if (n == 0) {
        return -1;
    }
    int hs = kResizeHandleSize;
    int best = -1;
    int bestDist = hs + 1;
    for (int i = 0; i < n; i++) {
        Point p = dm->CvtToScreen(annot->pageNo, pts[i]);
        int d = std::max(abs(pt.x - p.x), abs(pt.y - p.y));
        if (d <= hs && d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

// Get the resize handle at the given point for the selected annotation
static ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot) {
    if (!annot) {
        return ResizeHandle::None;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return ResizeHandle::None;
    }
    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        return ResizeHandle::None;
    }
    if (annot->type == AnnotationType::Line) {
        return GetLineEndpointHandleAt(dm, pt, annot);
    }
    if (IsPolyVertexType(annot->type)) {
        return GetPolyVertexAt(dm, pt, annot) >= 0 ? ResizeHandle::Vertex : ResizeHandle::None;
    }
    if (annot->type == AnnotationType::Redact && len(GetQuadPointsAsRect(annot)) > 0) {
        // text-selection marks are a set of quads, not a stretchable rect
        return ResizeHandle::None;
    }

    Rect rect = dm->CvtToScreen(pageNo, GetRect(annot));
    int hs = kResizeHandleSize;

    bool nearLeft = pt.x >= rect.x - hs && pt.x <= rect.x + hs;
    bool nearRight = pt.x >= rect.x + rect.dx - hs && pt.x <= rect.x + rect.dx + hs;
    bool nearTop = pt.y >= rect.y - hs && pt.y <= rect.y + hs;
    bool nearBottom = pt.y >= rect.y + rect.dy - hs && pt.y <= rect.y + rect.dy + hs;
    bool betweenX = pt.x >= rect.x + hs && pt.x <= rect.x + rect.dx - hs;
    bool betweenY = pt.y >= rect.y + hs && pt.y <= rect.y + rect.dy - hs;

    // clang-format off
    // corners have priority over edges
    if (nearLeft  && nearTop)    return ResizeHandle::TopLeft;
    if (nearRight && nearTop)    return ResizeHandle::TopRight;
    if (nearRight && nearBottom) return ResizeHandle::BottomRight;
    if (nearLeft  && nearBottom) return ResizeHandle::BottomLeft;
    // edges
    if (betweenX  && nearTop)    return ResizeHandle::Top;
    if (nearRight && betweenY)   return ResizeHandle::Right;
    if (betweenX  && nearBottom) return ResizeHandle::Bottom;
    if (nearLeft  && betweenY)   return ResizeHandle::Left;
    // clang-format on

    return ResizeHandle::None;
}

static int CursorForResizeHandle(ResizeHandle handle) {
    switch (handle) {
        case ResizeHandle::TopLeft:
        case ResizeHandle::BottomRight:
            return (int)gp::CursorKind::ResizeUpLeftDownRight;
        case ResizeHandle::TopRight:
        case ResizeHandle::BottomLeft:
            return (int)gp::CursorKind::ResizeUpRightDownLeft;
        case ResizeHandle::Left:
        case ResizeHandle::Right:
            return (int)gp::CursorKind::ColResize;
        case ResizeHandle::Top:
        case ResizeHandle::Bottom:
            return (int)gp::CursorKind::RowResize;
        case ResizeHandle::LineStart:
        case ResizeHandle::LineEnd:
        case ResizeHandle::Vertex:
            return kCurSizeAll;
        default:
            return kCurArrow;
    }
}

// Edit PDF with an annotation selected (its toolbar is up): the mouse works
// only on that annotation, and a click anywhere else just deselects it
static Annotation* AnnotationLockingMouse(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot)) {
        return nullptr;
    }
    return annot;
}

static void StartAnnotationDrag(MainWindow* win, Annotation* annot, Point pt) {
    win->annotationBeingDragged = annot;
    DisplayModel* dm = win->AsFixed();
    RectF r = GetRect(annot);
    int pageNo = PageNo(annot);
    Rect rScreen = dm->CvtToScreen(pageNo, r);
    win->annotationBeingMovedSize = {rScreen.dx, rScreen.dy};
    win->annotationBeingMovedOffset = Point{rScreen.x - pt.x, rScreen.y - pt.y};
}

// return true if this was annotation dragging
static bool StopDraggingAnnotation(MainWindow* win, int x, int y, bool aborted) {
    Annotation* annot = win->annotationBeingDragged;
    if (!annot) {
        return false;
    }
    win->annotationBeingDragged = nullptr;
    if (aborted) {
        return true;
    }

    DisplayModel* dm = win->AsFixed();
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    // we can only move annotation within the same page
    if (pageNo == PageNo(annot)) {
        Rect rScreen{x, y, 1, 1};
        RectF r = dm->CvtFromScreen(rScreen, pageNo);
        RectF ar = GetRect(annot);
        r.dx = ar.dx;
        r.dy = ar.dy;
        SetRect(annot, r);
        NotifyAnnotationsChanged(win->CurrentTab());
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
        UpdateAnnotFilterToolbar(win);
    }
    return true;
}

constexpr int kNudgeStep = 1;
constexpr int kNudgeStepShift = 10;
constexpr int kAnnotationNudgeDelayMs = 150;

static void FinishAnnotationNudge(MainWindow* win) {
    WindowTab* tab = win ? win->annotationNudgeTab : nullptr;
    int pageNo = win ? win->annotationNudgePageNo : 0;
    if (!tab) {
        return;
    }
    win->annotationNudgeTab = nullptr;
    win->annotationNudgePageNo = 0;
    win->annotationNudgeLeftMs = 0;
    if (!IsWindowTabValid(tab)) {
        return;
    }
    RerenderTabPage(tab, pageNo);
    NotifyAnnotationsChanged(tab);
    ToolbarUpdateStateForWindow(tab->win, true);
}

// Arrow keys nudge the selected annotation by a pixel, Shift+arrow by 10.
bool NudgeSelectedAnnotation(MainWindow* win, int vkey, bool shift) {
    Point dir;
    switch (vkey) {
        case VK_LEFT:
            dir = {-1, 0};
            break;
        case VK_RIGHT:
            dir = {1, 0};
            break;
        case VK_UP:
            dir = {0, -1};
            break;
        case VK_DOWN:
            dir = {0, 1};
            break;
        default:
            return false;
    }

    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!dm || !AnnotationIsLive(annot) || win->annotationBeingDragged) {
        return false;
    }
    if (!AnnotationCanBeMoved(annot->type) || annot->type == AnnotationType::Widget) {
        return false;
    }

    int step = DpiScale(shift ? kNudgeStepShift : kNudgeStep);
    int pageNo = PageNo(annot);
    RectF ar = GetRect(annot);
    Point from = dm->CvtToScreen(pageNo, PointF{ar.x, ar.y});
    Point to{from.x + (dir.x * step), from.y + (dir.y * step)};
    if (dm->GetPageNoByPoint(to) != pageNo) {
        return true;
    }
    PointF pFrom = dm->CvtFromScreen(from, pageNo);
    PointF pTo = dm->CvtFromScreen(to, pageNo);
    RectF r = ar;
    r.x += pTo.x - pFrom.x;
    r.y += pTo.y - pFrom.y;
    SetRect(annot, r);

    AppShellInvalidate(win);
    if (win->annotationNudgeTab != tab || win->annotationNudgePageNo != pageNo) {
        FinishAnnotationNudge(win);
    }
    win->annotationNudgeTab = tab;
    win->annotationNudgePageNo = pageNo;
    win->annotationNudgeLeftMs = kAnnotationNudgeDelayMs;
    return true;
}

void AnnotationNudgeTick(MainWindow* win, int elapsedMs) {
    if (!win || !win->annotationNudgeTab) {
        return;
    }
    win->annotationNudgeLeftMs -= elapsedMs;
    if (win->annotationNudgeLeftMs <= 0) {
        FinishAnnotationNudge(win);
    }
}

// orig's kAnnotationResizeRerenderDelayMs: the bounds follow the pointer,
// the page bitmap waits until the drag pauses
constexpr int kAnnotationResizeRerenderDelayMs = 125;

void AnnotationResizeRerenderTick(MainWindow* win, int elapsedMs) {
    if (!win || win->annotationResizeRerenderLeftMs <= 0) {
        return;
    }
    win->annotationResizeRerenderLeftMs -= elapsedMs;
    if (win->annotationResizeRerenderLeftMs > 0) {
        return;
    }
    win->annotationResizeRerenderLeftMs = 0;
    MainWindowRerender(win);
}

// Helper function to calculate new rectangle during resize
static RectF CalculateResizedRect(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    Annotation* annot = win->annotationBeingDragged;
    int pageNo = PageNo(annot);

    Rect screenPt{x, y, 1, 1};
    RectF pagePt = dm->CvtFromScreen(screenPt, pageNo);

    RectF orig = win->annotationOriginalRect;
    RectF r = orig;

    Point startPt = win->dragStart;
    Rect startScreen{startPt.x, startPt.y, 1, 1};
    RectF startPage = dm->CvtFromScreen(startScreen, pageNo);

    float deltaX = pagePt.x - startPage.x;
    float deltaY = pagePt.y - startPage.y;

    const float minSize = 10.0F;
    auto handle = (ResizeHandle)win->resizeHandle;

    bool moveLeft =
        handle == ResizeHandle::TopLeft || handle == ResizeHandle::Left || handle == ResizeHandle::BottomLeft;
    bool moveRight =
        handle == ResizeHandle::TopRight || handle == ResizeHandle::Right || handle == ResizeHandle::BottomRight;
    bool moveTop = handle == ResizeHandle::TopLeft || handle == ResizeHandle::Top || handle == ResizeHandle::TopRight;
    bool moveBottom =
        handle == ResizeHandle::BottomLeft || handle == ResizeHandle::Bottom || handle == ResizeHandle::BottomRight;

    if (moveLeft) {
        r.x = orig.x + deltaX;
        r.dx = orig.dx - deltaX;
        if (r.dx < minSize) {
            r.x = orig.x + orig.dx - minSize;
            r.dx = minSize;
        }
    }
    if (moveRight) {
        r.dx = orig.dx + deltaX;
        r.dx = std::max(r.dx, minSize);
    }
    if (moveTop) {
        r.y = orig.y + deltaY;
        r.dy = orig.dy - deltaY;
        if (r.dy < minSize) {
            r.y = orig.y + orig.dy - minSize;
            r.dy = minSize;
        }
    }
    if (moveBottom) {
        r.dy = orig.dy + deltaY;
        r.dy = std::max(r.dy, minSize);
    }

    float aspect = win->annotationResizeAspectRatio;
    if (aspect > 0) {
        bool widthDriven = moveLeft || moveRight;
        if (widthDriven && (moveTop || moveBottom)) {
            float widthChange = orig.dx > 0 ? fabsf(r.dx - orig.dx) / orig.dx : 0;
            float heightChange = orig.dy > 0 ? fabsf(r.dy - orig.dy) / orig.dy : 0;
            widthDriven = widthChange >= heightChange;
        }
        if (widthDriven) {
            r.dx = std::max(r.dx, minSize * aspect);
            r.dy = r.dx / aspect;
        } else {
            r.dy = std::max(r.dy, minSize);
            r.dx = r.dy * aspect;
        }

        if (moveLeft) {
            r.x = orig.x + orig.dx - r.dx;
        } else if (moveRight) {
            r.x = orig.x;
        } else {
            r.x = orig.x + ((orig.dx - r.dx) / 2);
        }
        if (moveTop) {
            r.y = orig.y + orig.dy - r.dy;
        } else if (moveBottom) {
            r.y = orig.y;
        } else {
            r.y = orig.y + ((orig.dy - r.dy) / 2);
        }
    }
    return r;
}

static void StartAnnotationResize(MainWindow* win, Annotation* annot, Point pt, ResizeHandle handle) {
    win->annotationResizeRerenderLeftMs = 0;
    // the drag rewrites the annotation on every mouse move; one undo step
    BeginPdfEditOperation(win, "Resize annotation");
    // a finished right-click leaves dragRightClick set; this is a left drag
    win->dragRightClick = false;
    win->annotationBeingDragged = annot;
    win->annotationBeingResized = true;
    win->resizeHandle = (int)handle;
    win->dragStart = pt;
    RectF r = GetRect(annot);
    win->annotationOriginalRect = r;
    win->annotationResizePreviewRect = r;
    // free text lays its text out again on every write; keep the drag to the
    // outline and write the annotation once, when the drag ends
    win->annotationResizeOutlineOnly =
        annot->type == AnnotationType::FreeText && !IsLineEndpointHandle(handle) && !IsVertexHandle(handle);
    win->annotationOriginalLineStart = {};
    win->annotationOriginalLineEnd = {};
    win->annotationLinePreviewStart = {};
    win->annotationLinePreviewEnd = {};
    win->annotationResizeVertexIndex = -1;
    VecReset(win->annotationVertexPreview);
    if (annot->type == AnnotationType::Line) {
        GetLinePoints(annot, win->annotationOriginalLineStart, win->annotationOriginalLineEnd);
        win->annotationLinePreviewStart = win->annotationOriginalLineStart;
        win->annotationLinePreviewEnd = win->annotationOriginalLineEnd;
    } else if (IsPolyVertexType(annot->type)) {
        win->annotationVertexPreview = GetVertices(annot);
        win->annotationResizeVertexIndex = GetPolyVertexAt(win->AsFixed(), pt, annot);
    }
    win->annotationResizeAspectRatio = 0;
    if (annot->type == AnnotationType::Stamp && r.dx > 0 && r.dy > 0) {
        // Rubber stamps are regenerated at a fixed aspect ratio by MuPDF;
        // preserving image-stamp aspect also avoids distortion.
        win->annotationResizeAspectRatio = r.dx / r.dy;
    }
    win->mouseAction = MouseAction::Dragging;
    win->dragPrevPos = pt;
}

static bool StopAnnotationResize(MainWindow* win, bool aborted) {
    if (!win->annotationBeingResized) {
        return false;
    }
    Annotation* annot = win->annotationBeingDragged;
    auto handle = (ResizeHandle)win->resizeHandle;
    PointF lineStart = win->annotationLinePreviewStart;
    PointF lineEnd = win->annotationLinePreviewEnd;
    bool outlineOnly = win->annotationResizeOutlineOnly;
    RectF previewRect = win->annotationResizePreviewRect;
    win->annotationBeingResized = false;
    win->annotationResizeOutlineOnly = false;
    win->annotationBeingDragged = nullptr;
    win->annotationResizeRerenderLeftMs = 0;
    CanvasSetCursor(win, kCurArrow);

    if (aborted || !annot) {
        EndPdfEditOperation(win);
        AppShellInvalidate(win);
        return true;
    }
    if (IsLineEndpointHandle(handle)) {
        SetLinePoints(annot, lineStart, lineEnd);
    } else if (IsVertexHandle(handle)) {
        SetVertices(annot, win->annotationVertexPreview);
    } else if (outlineOnly) {
        SetRect(annot, previewRect);
    }
    // Other rectangle resizes already wrote the annot during mouse move.
    EndPdfEditOperation(win);
    NotifyAnnotationsChanged(win->CurrentTab());
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    UpdateAnnotFilterToolbar(win);
    return true;
}

static void OpenOrSelectEditAnnotation(WindowTab* tab, Annotation* annot, Point clickPt) {
    if (!tab || !annot) {
        return;
    }
    DisplayModel* dm = tab->win ? tab->win->AsFixed() : nullptr;
    if (dm && AnnotationIsTextMarkup(annot->type)) {
        // the toolbar starts at the click, not at the (maybe multi-line) bounds
        SetAnnotEditToolbarClickPos(annot, dm->CvtFromScreen(clickPt, PageNo(annot)));
    }
    SetSelectedAnnotation(tab, annot);
}

// the annotation being resized follows the pointer
static void UpdateAnnotationResize(MainWindow* win, int x, int y, bool isShift) {
    Annotation* annot = win->annotationBeingDragged;
    DisplayModel* dm = win->AsFixed();
    if (!annot || !dm) {
        return;
    }
    auto handle = (ResizeHandle)win->resizeHandle;
    if (IsLineEndpointHandle(handle) && annot->type == AnnotationType::Line) {
        int linePageNo = PageNo(annot);
        Point anchor = (handle == ResizeHandle::LineStart)
                           ? dm->CvtToScreen(linePageNo, win->annotationOriginalLineEnd)
                           : dm->CvtToScreen(linePageNo, win->annotationOriginalLineStart);
        Point end{x, y};
        if (isShift) {
            end = SnapLineEndpoint(anchor, end);
        }
        PointF pagePt = dm->CvtFromScreen(end, linePageNo);
        if (handle == ResizeHandle::LineStart) {
            win->annotationLinePreviewStart = pagePt;
        } else {
            win->annotationLinePreviewEnd = pagePt;
        }
    } else if (IsVertexHandle(handle) && IsPolyVertexType(annot->type)) {
        int polyPageNo = PageNo(annot);
        Vec<PointF>& pts = win->annotationVertexPreview;
        int idx = win->annotationResizeVertexIndex;
        if (VecIsValidIndex(pts, idx)) {
            Point end{x, y};
            int anchor = idx > 0 ? idx - 1 : idx + 1;
            if (isShift && VecIsValidIndex(pts, anchor)) {
                end = SnapLineEndpoint(dm->CvtToScreen(polyPageNo, pts[anchor]), end);
            }
            pts[idx] = dm->CvtFromScreen(end, polyPageNo);
        }
    } else if (win->annotationResizeOutlineOnly) {
        // Outline only: writing the annotation re-lays out its text on every
        // move, so only the marker follows the pointer
        win->annotationResizePreviewRect = CalculateResizedRect(win, x, y);
    } else {
        RectF newRect = CalculateResizedRect(win, x, y);
        SetRect(annot, newRect);
        win->annotationResizePreviewRect = newRect;
        win->annotationResizeRerenderLeftMs = kAnnotationResizeRerenderDelayMs;
    }
    AppShellInvalidate(win);
}

// --- the annotation markers on the page -------------------------------------

constexpr Color kAnnotMarkBlue = MkRgb(0, 80, 200);
constexpr int kAnnotHandleSize = 6;

static void PaintHoveredAnnotationMark(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    Annotation* annot = win ? win->annotationUnderCursor : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || !tab || !annot || annot == tab->selectedAnnotation) {
        return;
    }
    if (!AnnotationIsLive(annot) || !dm->PageVisible(annot->pageNo)) {
        return;
    }
    Rect rect = dm->CvtToScreen(annot->pageNo, GetRect(annot));
    rect.Inflate(DisplayModel::kAnnotMarkPadding, DisplayModel::kAnnotMarkPadding);
    CanvasDrawRect(ctx, rect, kAnnotMarkBlue, 2);
}

static void PaintAnnotHandle(gp::PaintCtx* ctx, Point p) {
    int hs = kAnnotHandleSize;
    Rect r{p.x - hs / 2, p.y - hs / 2, hs, hs};
    CanvasFillRects(ctx, &r, 1, 0xffffff, 0xff, 0);
    CanvasDrawRect(ctx, r, 0x000000, 1);
}

// the blue dotted border and the resize handles of the selected annotation
static void PaintCurrentEditAnnotationMark(WindowTab* tab, gp::PaintCtx* ctx, DisplayModel* dm) {
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!annot || !AnnotationIsLive(annot) || !dm->PageVisible(annot->pageNo)) {
        return;
    }
    int pageNo = annot->pageNo;
    MainWindow* win = tab->win;
    bool canResize = AnnotationCanBeResized(annot->type);
    bool draggingLine = win && win->annotationBeingResized && IsLineEndpointHandle((ResizeHandle)win->resizeHandle) &&
                        annot->type == AnnotationType::Line;
    bool draggingVertices = win && win->annotationBeingResized && IsVertexHandle((ResizeHandle)win->resizeHandle) &&
                            IsPolyVertexType(annot->type);
    // an outline-only resize hasn't touched the annotation yet, so the marker
    // and its handles come from the preview rect instead
    bool draggingOutline =
        win && win->annotationBeingResized && win->annotationResizeOutlineOnly && win->annotationBeingDragged == annot;

    Rect rect = dm->CvtToScreen(pageNo, draggingOutline ? win->annotationResizePreviewRect : GetRect(annot));
    if (!tab->didScrollToSelectedAnnotation) {
        dm->ScrollScreenToRect(pageNo, rect);
        tab->didScrollToSelectedAnnotation = true;
    }
    rect.Inflate(DisplayModel::kAnnotMarkPadding, DisplayModel::kAnnotMarkPadding);

    auto drawVertexHandles = [&](const Vec<PointF>& pts) {
        for (int i = 0; i < len(pts); i++) {
            PaintAnnotHandle(ctx, dm->CvtToScreen(pageNo, pts[i]));
        }
    };
    auto drawVertexPath = [&](const Vec<PointF>& pts, bool closed) {
        int n = len(pts);
        if (n == 0) {
            return;
        }
        float w = (float)std::max(DpiScale(2), 1);
        Point prev = dm->CvtToScreen(pageNo, pts[0]);
        for (int i = 1; i < n; i++) {
            Point cur = dm->CvtToScreen(pageNo, pts[i]);
            CanvasDrawLine(ctx, prev, cur, kAnnotMarkBlue, w);
            prev = cur;
        }
        if (closed && n >= 2) {
            CanvasDrawLine(ctx, prev, dm->CvtToScreen(pageNo, pts[0]), kAnnotMarkBlue, w);
        }
    };

    if (draggingLine) {
        Point startPt = dm->CvtToScreen(pageNo, win->annotationLinePreviewStart);
        Point endPt = dm->CvtToScreen(pageNo, win->annotationLinePreviewEnd);
        CanvasDrawLine(ctx, startPt, endPt, kAnnotMarkBlue, (float)std::max(DpiScale(2), 1));
        PaintAnnotHandle(ctx, startPt);
        PaintAnnotHandle(ctx, endPt);
        return;
    }
    if (draggingVertices) {
        drawVertexPath(win->annotationVertexPreview, annot->type == AnnotationType::Polygon);
        drawVertexHandles(win->annotationVertexPreview);
        return;
    }

    CanvasDrawRect(ctx, rect, kAnnotMarkBlue, 2);
    if (!canResize) {
        return;
    }
    PointF lineStart, lineEnd;
    if (annot->type == AnnotationType::Line && GetLinePoints(annot, lineStart, lineEnd)) {
        PaintAnnotHandle(ctx, dm->CvtToScreen(pageNo, lineStart));
        PaintAnnotHandle(ctx, dm->CvtToScreen(pageNo, lineEnd));
        return;
    }
    if (IsPolyVertexType(annot->type)) {
        drawVertexHandles(GetVertices(annot));
        return;
    }
    if (annot->type == AnnotationType::Redact && len(GetQuadPointsAsRect(annot)) > 0) {
        return;
    }
    int left = rect.x;
    int midX = rect.x + (rect.dx / 2);
    int right = rect.x + rect.dx;
    int top = rect.y;
    int midY = rect.y + (rect.dy / 2);
    int bottom = rect.y + rect.dy;
    PaintAnnotHandle(ctx, Point{left, top});
    PaintAnnotHandle(ctx, Point{right, top});
    PaintAnnotHandle(ctx, Point{right, bottom});
    PaintAnnotHandle(ctx, Point{left, bottom});
    PaintAnnotHandle(ctx, Point{midX, top});
    PaintAnnotHandle(ctx, Point{right, midY});
    PaintAnnotHandle(ctx, Point{midX, bottom});
    PaintAnnotHandle(ctx, Point{left, midY});
}

// ng: orig drags an annotation with an XOR'd "move pattern" drawn straight on
// the canvas DC; here the outline is painted with everything else
static void PaintAnnotationMove(MainWindow* win, gp::PaintCtx* ctx) {
    if (!win->annotationBeingDragged || win->annotationBeingResized) {
        return;
    }
    Point p = win->dragPrevPos;
    Size sz = win->annotationBeingMovedSize;
    Rect r{p.x + win->annotationBeingMovedOffset.x, p.y + win->annotationBeingMovedOffset.y, sz.dx, sz.dy};
    CanvasDrawRect(ctx, r, kAnnotMarkBlue, 2);
}

#if OS_WIN
static bool IsFullPageImage(DisplayModel* dm, IPageElement* el, int pageNo) {
    // in image documents every page is a full-page image and dragging
    // it out to another app is the expected behavior
    Kind k = dm->GetEngine()->kind;
    if (k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks) {
        return false;
    }
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    RectF pageRc = dm->GetEngine()->PageMediabox(pageNo);
    float pageArea = pageRc.dx * pageRc.dy;
    if (pageArea <= 0) {
        return false;
    }
    RectF imgRc = el->GetRect();
    float imgArea = imgRc.dx * imgRc.dy;
    return imgArea >= 0.8f * pageArea;
}
#endif

static bool IsPointInSelection(MainWindow* win, Point pt) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.Contains(pt)) {
            return true;
        }
    }
    return false;
}

// --- left button ------------------------------------------------------------

// orig's OnMouseLeftButtonDown, minus the annotation, form-field, signature,
// touch and reading-bar branches (steps 12 / 13)
// ng: orig's IsTripleClick() compares the time and place of this press with the
// double-click that selected a word. gpui counts the clicks of a sequence, so
// the third (fifth, ...) press of one whose double-click selected a word is it
static bool gWordSelectedByDblClk = false;
static bool gIsTripleClick = false;

static bool IsTripleClick() {
    return gIsTripleClick && gWordSelectedByDblClk;
}

static void OnMouseLeftButtonDown(MainWindow* win, int x, int y) {
    // orig closes a pinned hover menu when the page is pressed
    HideToolbarHoverDropdown(win);
    win->pressOnlyDeselected = false;
    RefHoverOnCanvasLeftButtonDown(win->refHover);
    if (ReadingBarOnLeftDown(win, x, y)) {
        return;
    }
    // placing a new signature: the next drag draws the box, a click puts a
    // default-size one at the pointer (issue #5967). Consume the press so it
    // doesn't toggle a form field or start a text selection.
    if (IsPlacingSignature(win)) {
        win->dragStartPending = true;
        win->dragStart = Point{x, y};
        OnSelectionStart(win, x, y, true);
        return;
    }
    if (AnnotationPlacementOnLeftDown(win, Point{x, y}, CanvasShiftPressed(), CanvasCtrlPressed())) {
        return;
    }
    if (IsRightDragging(win)) {
        return;
    }
    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
        return;
    }
    if (win->mouseAction != MouseAction::None) {
        logf("OnMouseLeftButtonDown: win->mouseAction=%d\n", (int)win->mouseAction);
        win->mouseAction = MouseAction::None;
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    Point pt{x, y};
    WindowTab* tab = win->CurrentTab();

    // orig's WM_KILLFOCUS: a click on the page is "done", and that click
    // must not also drop the annotation the text was written to
    if (IsEditingAnnotContents(win)) {
        EndAnnotContentsEdit(true);
    }

    // Edit PDF with an annotation selected: a press anywhere but on that
    // annotation or its resize handles only deselects it
    Annotation* locked = AnnotationLockingMouse(win);
    if (locked) {
        bool onHandle =
            AnnotationCanBeResized(locked->type) && GetResizeHandleAt(win, pt, locked) != ResizeHandle::None;
        bool onLocked = dm->GetAnnotationAtPos(pt, locked) == locked;
        if (!onHandle && !onLocked) {
            if (!AnnotContentsEditJustEnded()) {
                SetSelectedAnnotation(tab, nullptr);
            }
            win->pressOnlyDeselected = true;
            return;
        }
        // one that can't be moved has nothing to drag. Text markup is the
        // exception: a press on it still selects the text under it (#6166)
        if (!onHandle && !AnnotationCanBeMoved(locked->type) && !AnnotationIsTextMarkup(locked->type)) {
            return;
        }
    }

    // PDF form filling: clicking a checkbox / radio-button toggles it; clicking
    // a text or choice field starts in-place editing. Widgets are hit-tested on
    // their own list, separate from markup annotations.
    Annotation* widget = locked ? nullptr : dm->GetWidgetAtPos(pt);
    if (ToggleFormButton(widget)) {
        MainWindowRerender(win);
        win->mouseAction = MouseAction::None;
        return;
    }
    if (StartFormFieldEdit(win, widget)) {
        win->mouseAction = MouseAction::None;
        return;
    }
    if (StartSignatureFieldSigning(win, widget)) {
        win->mouseAction = MouseAction::None;
        return;
    }
    CommitFormFieldEdit(true);

    // Resize handles sit outside the selected annotation's rect. Check them
    // before hit-testing other annotations: otherwise an overlapping annot
    // steals the click, selection jumps, and we resize the wrong one (#5818).
    ResizeHandle resizeHandle = ResizeHandle::None;
    if (tab && tab->selectedAnnotation && AnnotationCanBeResized(tab->selectedAnnotation->type)) {
        resizeHandle = GetResizeHandleAt(win, pt, tab->selectedAnnotation);
    }
    if (resizeHandle != ResizeHandle::None) {
        StartAnnotationResize(win, tab->selectedAnnotation, pt, resizeHandle);
        win->dragStartPending = true;
        win->dragStart = pt;
        win->textDragPending = false;
        return;
    }

    // the highlighter only selects text: a click never picks an annotation
    Annotation* annot = IsPlacingHighlighterAnnotation(win)
                            ? nullptr
                            : dm->GetAnnotationAtPos(pt, tab ? tab->selectedAnnotation : nullptr);
    if (CanvasCtrlPressed() && annot && tab) {
        EnablePdfAnnotationsToolbar(win);
    }
    bool editPdf = win->pdfAnnotationsToolbarEnabled;
    if (editPdf && annot && !AnnotationCanBeMoved(annot->type)) {
        // highlight / underline / squiggly / strike-out sit on text. A plain
        // click starts a selection (issue #6166); Ctrl+click still selects.
        bool clickThrough = AnnotationIsTextMarkup(annot->type) && !CanvasCtrlPressed();
        if (!clickThrough) {
            OpenOrSelectEditAnnotation(tab, annot, pt);
            win->textDragPending = false;
            return;
        }
    }
    bool isMoveableAnnot = annot && AnnotationCanBeMoved(annot->type) && annot->type != AnnotationType::Widget;
    // Selecting / dragging an annotation is Edit PDF (Ctrl+click turns that on
    // above). An annotation already selected (just created) can still be dragged.
    if (isMoveableAnnot && !editPdf && annot != (tab ? tab->selectedAnnotation : nullptr)) {
        isMoveableAnnot = false;
    }
    if (isMoveableAnnot && tab && annot != tab->selectedAnnotation) {
        SetSelectedAnnotation(tab, annot);
    }
    if (isMoveableAnnot) {
        StartAnnotationDrag(win, annot, pt);
        win->mouseAction = MouseAction::Dragging;
        win->dragStartPending = true;
        win->dragStart = pt;
        win->dragPrevPos = pt;
        win->textDragPending = false;
        return;
    }
    if (tab && tab->selectedAnnotation) {
        // a click that ended a contents edit is spent on ending it; the
        // annotation the text was written to stays selected
        bool keepSelected = AnnotContentsEditJustEnded();
        if (!keepSelected) {
            SetSelectedAnnotation(tab, nullptr);
        }
        // over text, keep going so the press can start a selection even if a
        // highlight was selected (issue #6166)
        if (keepSelected || !dm->IsOverText(pt)) {
            return;
        }
    }

    ReportIf(win->linkOnLastButtonDown);
    IPageElement* pageEl = dm->GetElementAtPos(pt, nullptr);
    if (pageEl && pageEl->Is(kindPageElementDest) && !gSettings->disableLinks) {
        win->linkOnLastButtonDown = pageEl;
    }

    win->dragStartPending = true;
    win->dragStart = pt;
    win->textDragPending = false;

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    bool isShift = CanvasShiftPressed();
    bool isCtrl = CanvasCtrlPressed();
    bool canCopy = HasPermission(Perm::CopySelection);
    bool isOverText = dm->IsOverText(pt);

    // triple-click selects the whole line (issue #694). Must come before the
    // "already selected text" check below, because the 3rd click lands inside
    // the word that the 2nd click (double-click) just selected.
    if (canCopy && !isShift && !isCtrl && isOverText && IsTripleClick()) {
        int pageNo = dm->GetPageNoByPoint(pt);
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF ptf = dm->CvtFromScreen(pt, pageNo);
            dm->textSelection->SelectLineAt(pageNo, ptf.x, ptf.y);
            UpdateTextSelection(win, false);
            win->selectingByWord = false; // a drag now extends by glyph, not word
            win->showSelection = true;
            win->selectionRect = Rect(x, y, 0, 0);
            win->mouseAction = MouseAction::SelectingText;
            win->dragStartPending = false;
            CanvasSetCapture(win, true);
            AppShellInvalidate(win);
            gWordSelectedByDblClk = false; // so a 4th click doesn't re-trigger
        }
        return;
    }

    // Move / resize an existing rectangular (Ctrl+drag) selection. Before the
    // "click inside the selection" check below: a rectangle is usually drawn
    // over text, and that check would claim every press inside it.
    if (canCopy && !isShift && !isCtrl && IsRectangularSelection(win)) {
        SelectionDragEdge edge = HitTestRectangularSelection(win, x, y);
        if (edge != SelectionDragEdge::None && StartRectangularSelectionEdit(win, x, y, edge)) {
            return;
        }
    }

    // if clicking on already selected text, prepare for drag-out instead of new selection
    // ng: the drag-out is OLE's, so Windows only (gpui has no drag out of a
    // window); elsewhere the press is only remembered and the release clears
    // the selection, which is what orig does for a click without a drag
    if (canCopy && !isShift && !isCtrl && isOverText && win->showSelection && IsPointInSelection(win, pt)) {
        win->textDragPending = true;
        win->linkOnLastButtonDown = nullptr;
        CanvasSetCapture(win, true);
        return;
    }

#if OS_WIN
    // if clicking on an image, prepare for image drag-out. skip full-page
    // images (e.g. scanned pages), where click-and-drag should pan instead.
    if (canCopy && !isShift && !isCtrl && !isOverText) {
        int elPageNo = -1;
        IPageElement* imageEl = dm->GetElementAtPos(pt, &elPageNo);
        if (imageEl && imageEl->Is(kindPageElementImage) && !IsFullPageImage(dm, imageEl, elPageNo)) {
            win->imageDragPending = true;
            win->imageDragElement = imageEl;
            win->imageDragPageNo = elPageNo;
            win->linkOnLastButtonDown = nullptr;
            CanvasSetCapture(win, true);
            return;
        }
    }
#endif

    bool startDrag = !canCopy || ((isShift || !isOverText) && !isCtrl);
    logf("OnMouseLeftButtonDown: %d,%d overText %d ctrl %d shift %d -> %s\n", x, y, isOverText ? 1 : 0, isCtrl ? 1 : 0,
         isShift ? 1 : 0, startDrag ? StrL("drag") : StrL("select"));
    if (startDrag) {
        StartMouseDrag(win, x, y);
    } else {
        OnSelectionStart(win, x, y);
    }
}

// the 3rd click of a triple click: select the whole line (issue #694)
// https://github.com/sumatrapdfreader/sumatrapdf/issues/5289: CmdToggleInverseSearch
// turns the double-click inverse search off for a session
bool gDisableInteractiveInverseSearch = false;

// orig's OnMouseLeftButtonDblClk. False when the press was not a double-click
// action, so the caller can still treat it as an ordinary click.
static bool OnMouseLeftButtonDblClk(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    if (AnnotationPlacementOnLeftDblClk(win, Point{x, y})) {
        return true;
    }
    if (win->pressOnlyDeselected) {
        win->pressOnlyDeselected = false;
        // the click that only deselected has no follow-up. A later click that
        // gpui still counts as the second of that pair is a new press: an
        // annotation created since then has to be deselected by it.
        if (!AnnotationLockingMouse(win)) {
            return true;
        }
        return false;
    }
    // free text edits in place on a double-click. Any other press still starts
    // a drag: gpui counts it as a double-click right after the selection click
    Annotation* locked = AnnotationLockingMouse(win);
    if (locked) {
        Annotation* hit = dm->GetAnnotationAtPos(Point{x, y}, locked);
        if (hit == locked && Type(locked) == AnnotationType::FreeText) {
            StartFreeTextInPlaceEdit(win, locked);
            return true;
        }
        // the page's annot list can miss the selected free text, so a click
        // inside its rect still edits it
        if (Type(locked) == AnnotationType::FreeText) {
            PointF p = dm->CvtFromScreen(Point{x, y}, PageNo(locked));
            if (GetRect(locked).Contains(p) && StartFreeTextInPlaceEdit(win, locked)) {
                return true;
            }
        }
        return false;
    }
    // a double-click on free text edits its text where it sits on the page
    if (!IsPlacingHighlighterAnnotation(win) && StartFreeTextInPlaceEditAt(win, Point{x, y})) {
        return true;
    }
    if (gSettings->enableTeXEnhancements && !gDisableInteractiveInverseSearch) {
        bool dontSelect = OnInverseSearch(win, x, y);
        if (dontSelect) {
            return true;
        }
    }

    Point mousePos = Point(x, y);
    bool isOverText = dm->IsOverText(mousePos);

    if (win->presentation || win->isFullScreen) {
        // in fullscreen we allow to exit by tapping in upper right corner
        constexpr int kCornerSize = 64;
        Rect r = dm->GetViewPort();
        if (!isOverText && (x >= (r.dx - kCornerSize)) && (y < kCornerSize)) {
            ExitFullScreen(win);
            return true;
        }
    }

    int elementPageNo = -1;
    IPageElement* pageEl = dm->GetElementAtPos(mousePos, &elementPageNo);
    if (isOverText) {
        int pageNo = dm->GetPageNoByPoint(mousePos);
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(mousePos, pageNo);
            dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(win, false);
            gWordSelectedByDblClk = true;
            // keep the gesture active so dragging after the double-click extends
            // the selection a word at a time (issue #4761). dragStartPending is
            // cleared so that releasing without dragging keeps the whole word.
            win->selectingByWord = true;
            win->showSelection = true;
            win->selectionRect = Rect(x, y, 0, 0);
            win->mouseAction = MouseAction::SelectingText;
            win->dragStartPending = false;
            CanvasSetCapture(win, true);
            AppShellInvalidate(win);
        }
        return true;
    }

    if (!pageEl) {
        return false;
    }
    if (pageEl->Is(kindPageElementDest)) {
        if (gSettings->disableLinks) {
            return true;
        }
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y);
    } else if (pageEl->Is(kindPageElementImage)) {
        // select an image that could be copied to the clipboard
        Rect rc = dm->CvtToScreen(elementPageNo, pageEl->GetRect());

        DeleteOldSelectionInfo(win, true);
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
        win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        AppShellInvalidate(win);
    }
    return true;
}

// orig's OnMouseLeftButtonUp, minus the annotation, touch, presentation and
// forward-search branches
static void OnMouseLeftButtonUp(MainWindow* win, int x, int y) {
    if (ReadingBarOnLeftUp(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    if (AnnotationPlacementOnLeftUp(win, Point{x, y}, CanvasShiftPressed())) {
        return;
    }
    // ng: orig's press that only deselected leaves no mouse action behind; the
    // flag stays up for the double-click it may be the first half of
    if (win->pressOnlyDeselected) {
        win->mouseAction = MouseAction::None;
        return;
    }

    // click on selected text without dragging: clear selection
    if (win->textDragPending) {
        win->textDragPending = false;
        win->dragStartPending = false;
        CanvasSetCapture(win, false);
        DeleteOldSelectionInfo(win, true);
        AppShellInvalidate(win);
        return;
    }

    // click on image without dragging: just cancel
    if (win->imageDragPending) {
        win->imageDragPending = false;
        win->imageDragElement = nullptr;
        win->imageDragPageNo = -1;
        win->dragStartPending = false;
        CanvasSetCapture(win, false);
        return;
    }

    auto ma = win->mouseAction;
    if (MouseAction::None == ma || IsRightDragging(win)) {
        return;
    }

    // Left-up during middle-click auto-scroll mode stops auto-scroll
    if (MouseAction::Scrolling == ma) {
        win->mouseAction = MouseAction::None;
        win->xScrollSpeed = 0;
        win->yScrollSpeed = 0;
        win->xScrollAccum = 0;
        win->yScrollAccum = 0;
        CanvasSetCursor(win, kCurArrow);
        return;
    }

    // Click without move: dragStartPending is still true, so this is a click not a drag.
    bool didDragMouse = !win->dragStartPending || IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    if (MouseAction::Dragging == ma) {
        if (win->annotationBeingResized) {
            if (didDragMouse) {
                UpdateAnnotationResize(win, x, y, CanvasShiftPressed());
            }
            StopAnnotationResize(win, !didDragMouse);
            win->mouseAction = MouseAction::None;
            OnSetCursor(win, Point(x, y));
            return;
        }
        if (win->annotationBeingDragged) {
            StopDraggingAnnotation(win, x, y, !didDragMouse);
            win->mouseAction = MouseAction::None;
            CanvasSetCapture(win, false);
            if (didDragMouse) {
                return;
            }
        } else {
            StopMouseDrag(win, x, y);
        }
    } else {
        OnSelectionStop(win, x, y, !didDragMouse);
        if (MouseAction::Selecting == ma && win->showSelection) {
            win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
        }
        AnnotationPlacementOnSelectionStop(win);
        if (FinishSignaturePlacement(win, x, y, !didDragMouse)) {
            win->mouseAction = MouseAction::None;
            return;
        }
    }

    win->mouseAction = MouseAction::None;

    Point pt(x, y);
    int pageNo = dm->GetPageNoByPoint(pt);
    PointF ptPage = dm->CvtFromScreen(pt, pageNo);

    IPageElement* link = win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;

    WindowTab* tab = win->CurrentTab();
    if (didDragMouse) {
        return;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        /* return from white/black screens in presentation mode */
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    // Hit-test the click, not annotationUnderCursor: that is last-move hover
    // and is stale when the mouse move did not run (or returned early because
    // dragStartPending was still set from the create gesture). Using it
    // re-selected the new stamp when clicking empty page (issue #5933).
    // the highlighter only selects text: a click never picks an annotation
    Annotation* clickedAnnot = IsPlacingHighlighterAnnotation(win)
                                   ? nullptr
                                   : dm->GetAnnotationAtPos(pt, tab ? tab->selectedAnnotation : nullptr);
    if (CanvasCtrlPressed() && clickedAnnot && tab) {
        EnablePdfAnnotationsToolbar(win);
    }
    bool editPdf = win->pdfAnnotationsToolbarEnabled;

    // In Edit PDF mode a click selects the annotation and shows its toolbar.
    // Text markup clicks through on button-down so that a drag still selects
    // the glyphs underneath (issue #6166), but a plain click ends up here.
    if (clickedAnnot && tab && editPdf) {
        OpenOrSelectEditAnnotation(tab, clickedAnnot, pt);
        return;
    }

    IPageDestination* dest = link ? link->AsLink() : nullptr;
    Kind destKind = dest ? dest->GetKind() : nullptr;
    // FileAttachment is also a dest; open it instead of the #4790 comment card.
    bool openEmbedded = destKind == kindDestinationLaunchEmbedded;

    // Outside Edit PDF, select text markup so Delete has a target. A drag
    // returned above. The comment popup still opens; selection blocks a link
    // drawn under the highlight.
    if (clickedAnnot && tab && AnnotationIsTextMarkup(clickedAnnot->type)) {
        SetSelectedAnnotation(tab, clickedAnnot);
    }

    // Show its text, so a long comment can be read without the editing UI
    // (issue #4790)
    if (!openEmbedded && clickedAnnot && tab && !CanvasCtrlPressed() && AnnotationHasText(clickedAnnot)) {
        if (ShowAnnotationTextPopup(win, clickedAnnot)) {
            return;
        }
    }

    if (clickedAnnot && tab && clickedAnnot == tab->selectedAnnotation) {
        return;
    }

    if (link && link->GetRect().Contains(ptPage)) {
        /* follow an active link */
        // highlight the clicked link (as a reminder of the last action once the
        // user returns)
        Kind kind = dest ? dest->GetKind() : nullptr;
        if (IsLaunchLinkKind(kind)) {
            DeleteOldSelectionInfo(win, true);
            tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(pageNo, link->GetRect()));
            win->showSelection = tab->selectionOnPage != nullptr;
        }
        CanvasSetCursor(win, kCurArrow);
        logf("FollowLink: '%s' kind %s, ctrl %d\n", link->GetValue(), Str(kind), (int)CanvasCtrlPressed());

        // Ctrl+click on an internal link: open in a new tab and navigate there
        bool isInternal = !IsLaunchLinkKind(kind) && kindDestinationJsMenu != kind;
        if (CanvasCtrlPressed() && dest && isInternal && len(tab->filePath) > 0) {
            MainWindow* newWin = LoadDocument(win, tab->filePath);
            if (newWin && newWin->IsDocLoaded()) {
                newWin->linkHandler->ScrollTo(dest);
            }
            AppShellInvalidate(win);
            return;
        }

        win->ctrl->HandleLink(dest, win->linkHandler);
        AppShellInvalidate(win);
        return;
    }

    if (win->showSelection) {
        // A click that wasn't a drag, on empty space (clicking text starts a new
        // selection instead): drop the selection, like every other text UI does.
        DeleteOldSelectionInfo(win, true);
        AppShellInvalidate(win);
        return;
    }

    if (win->fwdSearchMark.show && gSettings->forwardSearch.highlightPermanent) {
        /* if there's a permanent forward search mark, hide it */
        win->fwdSearchMark.show = false;
        AppShellInvalidate(win);
        return;
    }

    // Click the left/right fifth of the canvas to turn the page (issue #1203).
    // Presentation mode has its own click-to-turn below. Manga (R2L) reverses
    // the sides so left still advances.
    if (gSettings->clickEdgeToTurnPage && tab && tab->ctrl && PM_ENABLED != win->presentation) {
        int canvasDx = dm->GetViewPort().dx;
        if (canvasDx > 0) {
            int edgeDx = canvasDx / 5;
            bool r2l = dm->GetDisplayR2L();
            bool goPrev = x < edgeDx;
            bool goNext = x >= canvasDx - edgeDx;
            if (r2l) {
                goPrev = x >= canvasDx - edgeDx;
                goNext = x < edgeDx;
            }
            if (goPrev) {
                tab->ctrl->GoToPrevPage();
                return;
            }
            if (goNext) {
                tab->ctrl->GoToNextPage();
                return;
            }
        }
    }

    if (PM_ENABLED == win->presentation && tab && tab->ctrl) {
        /* in presentation mode, change pages on left/right-clicks */
        if (CanvasShiftPressed()) {
            tab->ctrl->GoToPrevPage();
        } else {
            tab->ctrl->GoToNextPage();
        }
    }
}

// orig's CancelDrag: Escape abandons whatever the mouse was doing
void CanvasCancelDrag(MainWindow* win) {
    Point pt = win->dragPrevPos;
    if (StopAnnotationResize(win, true)) {
        win->mouseAction = MouseAction::None;
        win->dragStartPending = false;
        return;
    }
    if (win->annotationBeingDragged) {
        StopDraggingAnnotation(win, pt.x, pt.y, true);
        win->mouseAction = MouseAction::None;
        win->dragStartPending = false;
        AppShellInvalidate(win);
        return;
    }
    if (win->mouseAction == MouseAction::Selecting || win->mouseAction == MouseAction::SelectingText) {
        OnSelectionStop(win, pt.x, pt.y, true);
    } else {
        StopMouseDrag(win, pt.x, pt.y);
    }
    win->mouseAction = MouseAction::None;
    win->dragStartPending = false;
    win->textDragPending = false;
    win->linkOnLastButtonDown = nullptr;
    CanvasSetCapture(win, false);
    CanvasSetCursor(win, kCurArrow);
}

// orig arms kHideCursorTimerID for this long after every move in presentation
constexpr int kHideCursorDelayInMs = 3000;

static void OnMouseMove(MainWindow* win, int x, int y) {
    if (ReadingBarOnMouseMove(win, x, y)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    // orig's presentation branch: the cursor comes back when the mouse moves
    // and hides itself again shortly after it stops
    if (win->InPresentation()) {
        if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
            // the presenter blanked the screen on purpose
            AppShellShowCursor(win, false);
            return;
        }
        if (!win->dragPrevPos.Eq(x, y)) {
            AppShellShowCursor(win, true);
            // hack: hide the cursor immediately the first time (EnterFullScreen)
            win->presCursorHideLeftMs = win->dragPrevPos.Eq(-2, -3) ? 1 : kHideCursorDelayInMs;
        }
    }
    if (gLaserPointer && !kLaserPointerIsCursor) {
        AppShellShowCursor(win, false);
        AppShellInvalidate(win);
    }
    if (AnnotationPlacementOnMouseMove(win, Point{x, y}, CanvasShiftPressed(), win->annotPlacement.mouseDown)) {
        win->dragPrevPos = Point{x, y};
        return;
    }
    Point pos{x, y};
    int pageNo = dm->GetPageNoByPoint(pos);
    if (dm->ValidPageNo(pageNo)) {
        dm->GetEngine()->RequestTextExtraction(pageNo);
    }

    if (win->textDragPending) {
#if OS_WIN
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        // threshold met: initiate OLE drag-drop of selected text
        win->textDragPending = false;
        win->dragStartPending = false;
        CanvasSetCapture(win, false);
        StartTextDragDrop(win);
#endif
        // ng: no drag out of the window off Windows; the release clears the selection
        return;
    }

#if OS_WIN
    if (win->imageDragPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->imageDragPending = false;
        win->dragStartPending = false;
        CanvasSetCapture(win, false);
        // ng: the drag is posted; it clears imageDragElement when it is over
        StartImageDragDrop(win);
        return;
    }
#endif

    if (win->dragStartPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->dragStartPending = false;
        win->linkOnLastButtonDown = nullptr;
    }
    switch (win->mouseAction) {
        case MouseAction::None: {
            OnSetCursor(win, pos);
            // orig calls this from the same WM_MOUSEMOVE branch (Canvas.cpp)
            int hoverPageNo = 0;
            IPageElement* hoverEl = dm->GetElementAtPos(pos, &hoverPageNo);
            if (hoverEl && hoverEl->Is(kindPageElementDest) && gSettings->disableLinks) {
                hoverEl = nullptr;
            }
            RefHoverOnCanvasMouseMove(win->refHover, win, win->ctrl, win->linkHandler, dm, x, y, hoverEl, pageNo,
                                      gSettings->citationHoverDelay);
            // the highlighter only selects text: annotations get no hover
            bool editPdf = win->pdfAnnotationsToolbarEnabled && !IsPlacingHighlighterAnnotation(win);
            if (editPdf) {
                UpdateAnnotationHoverOverlay(win, pos);
            } else {
                HideAnnotationHoverOverlay(win);
            }
            break;
        }
        case MouseAction::Scrolling:
            HideAnnotationHoverOverlay(win);
            win->yScrollSpeed = (float)(y - win->dragStart.y) / kAutoScrollSlowDown;
            win->xScrollSpeed = (float)(x - win->dragStart.x) / kAutoScrollSlowDown;
            break;
        case MouseAction::SelectingText:
        case MouseAction::Selecting: {
            HideAnnotationHoverOverlay(win);
            if (win->selectionDragEdge != SelectionDragEdge::None) {
                // move / resize existing rectangular selection
                UpdateRectangularSelectionEdit(win, x, y);
            } else {
                // creating a new selection from the start corner
                win->selectionRect.dx = x - win->selectionRect.x;
                win->selectionRect.dy = y - win->selectionRect.y;
                win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
            }
            OnSetCursor(win, pos);
            OnSelectionEdgeAutoscroll(win, x, y);
            AppShellInvalidate(win);
            break;
        }
        case MouseAction::Dragging:
            if (win->annotationBeingResized) {
                UpdateAnnotationResize(win, x, y, CanvasShiftPressed());
                break;
            }
            if (win->annotationBeingDragged) {
                // the move outline follows the pointer; the annotation is
                // written when the drag ends
                AppShellInvalidate(win);
                break;
            }
            win->MoveDocBy(win->dragPrevPos.x - x, win->dragPrevPos.y - y);
            OnSetCursor(win, pos);
            break;
    }
    win->dragPrevPos = pos;

    NotificationWnd* cursorPosNotif = GetNotificationForGroup(win, kNotifCursorPos);
    if (cursorPosNotif) {
        UpdateCursorPositionHelper(win, pos, cursorPosNotif);
    }
}

// orig's kHideCursorTimerID. A laser pointer that disappears when you stop
// moving it would be useless, so it opts out of the hiding.
void CanvasTickPresentation(MainWindow* win, int elapsedMs) {
    if (win->presCursorHideLeftMs < 0) {
        return;
    }
    win->presCursorHideLeftMs -= elapsedMs;
    if (win->presCursorHideLeftMs > 0) {
        return;
    }
    win->presCursorHideLeftMs = -1;
    if (win->InPresentation() && !IsLaserPointerActive()) {
        AppShellShowCursor(win, false);
    }
}

// orig's kSelectSmoothScrollTimerID period
constexpr int kSelectSmoothScrollDelayInMs = 20;

// the auto-scroll tick: xScrollSpeed / yScrollSpeed are pixels per 20 ms
void DocCanvasAutoScrollTick(MainWindow* win, int elapsedMs) {
    // the debounced selection toolbar and the link-hint recompute are timers in
    // orig; here the shell's tick drives them
    SelectionToolbarOnShowTimer(win, elapsedMs);
    KeyboardLinkFollowingViewportChanged(win, elapsedMs);

    MouseAction ma = win->mouseAction;
    if (ma == MouseAction::Selecting || ma == MouseAction::SelectingText) {
        // orig's kSelectSmoothScrollTimerID fires every 20 ms
        static int sinceLastMs = 0;
        sinceLastMs += elapsedMs;
        if (sinceLastMs < kSelectSmoothScrollDelayInMs) {
            return;
        }
        sinceLastMs = 0;
        Point p = win->dragPrevPos;
        if (NeedsSelectionEdgeAutoscroll(win, p.x, p.y)) {
            OnSelectionEdgeAutoscroll(win, p.x, p.y);
            AppShellInvalidate(win);
        }
        return;
    }
    if (ma != MouseAction::Scrolling) {
        return;
    }
    // stop middle-button auto-scroll when the canvas loses focus, e.g. the
    // user clicked the bookmarks/menu or minimized the window (#3203). ng: no
    // WM_KILLFOCUS; the tick sees the frame without the focus, or inactive
    bool lostFocus = !AppShellIsFrameFocused(win) || (win->gpuiWin && !win->gpuiWin->active);
    if (lostFocus) {
        win->mouseAction = MouseAction::None;
        win->xScrollSpeed = 0;
        win->yScrollSpeed = 0;
        win->xScrollAccum = 0;
        win->yScrollAccum = 0;
        CanvasSetCursor(win, kCurArrow);
        return;
    }
    float scale = (float)elapsedMs / 20.0f;
    win->xScrollAccum += win->xScrollSpeed * scale;
    win->yScrollAccum += win->yScrollSpeed * scale;
    int dx = (int)win->xScrollAccum;
    int dy = (int)win->yScrollAccum;
    win->xScrollAccum -= (float)dx;
    win->yScrollAccum -= (float)dy;
    if (dx != 0 || dy != 0) {
        win->MoveDocBy(dx, dy);
    }
}

// --- the element ------------------------------------------------------------

struct DocCanvasView {
    MainWindow* win = nullptr;

    static void OnPaint(gp::PaintCtx* ctx, gp::El* e, void* user);
    static void OnWheel(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnWheelOverScrollbar(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnWheelOverHScrollbar(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnDown(DocCanvasView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnUp(DocCanvasView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnHover(DocCanvasView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnMove(DocCanvasView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnMenuAction(DocCanvasView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
};

// window coordinates (dips) -> document coordinates (pixels), or false when
// the point isn't on the canvas
static bool ToDoc(MainWindow* win, float wx, float wy, Point* out) {
    Rect rc = win->canvasRc;
    float s = 1.f / CanvasScale(win);
    int x = (int)((wx - (float)rc.x) * s);
    int y = (int)((wy - (float)rc.y) * s);
    *out = Point(x, y);
    return wx >= (float)rc.x && wy >= (float)rc.y && wx < (float)(rc.x + rc.dx) && wy < (float)(rc.y + rc.dy);
}

void DocCanvasView::OnPaint(gp::PaintCtx* ctx, gp::El* e, void* user) {
    auto* win = (MainWindow*)user;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    static bool loggedThread = false;
    if (!loggedThread) {
        loggedThread = true;
        logf("DocCanvas: painting on tid %d\n", (int)GetCurrentThreadId());
    }
    gp::Bounds b = e->Bounds();
    gFrame.ctx = ctx;
    gFrame.x0 = b.x;
    gFrame.y0 = b.y;
    gFrame.k = CanvasScale(win);
    gp::CanvasPushClip(ctx, b.x, b.y, b.w, b.h);
    Size vp = win->AsFixed()->GetViewPort().Size();
    // presentation mode's blank screen: nothing else is on it
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        Color col = PM_BLACK_SCREEN == win->presentation ? kColBlack : kColWhite;
        FillRect(ctx, Rect(0, 0, vp.dx, vp.dy), col);
        gp::CanvasPopClip(ctx);
        gFrame.ctx = nullptr;
        return;
    }
    DrawDocument(win, ctx, Rect(0, 0, vp.dx, vp.dy));
    if (ShowPageGrid()) {
        PaintPageGrid(win->AsFixed(), ctx);
    }
    PaintFormFieldHighlights(win, ctx);
    // draw a highlight rectangle around the element the context menu is on
    DisplayModel* dmHl = win->AsFixed();
    if (win->contextMenuHighlightPageNo > 0 && dmHl->PageVisible(win->contextMenuHighlightPageNo)) {
        Rect rc = dmHl->CvtToScreen(win->contextMenuHighlightPageNo, win->contextMenuHighlightRect);
        CanvasDrawRect(ctx, rc, MkRgb(0, 100, 255), 2);
    }
    if (win->showSelection) {
        PaintSelection(win, ctx);
    }
    PaintAnnotationPlacement(win, ctx, win->AsFixed());
    PaintHoveredAnnotationMark(win, ctx, win->AsFixed());
    PaintCurrentEditAnnotationMark(win->CurrentTab(), ctx, win->AsFixed());
    PaintAnnotationMove(win, ctx);
    ReadingBarPaint(win, ctx);
    PaintReadAloudHighlight(win, ctx);
    PaintAllFindMatches(win, ctx);
    if (win->fwdSearchMark.show) {
        PaintForwardSearchMark(win, ctx);
    }
    PaintKeyboardTextCaret(win, ctx);
    PaintKeyboardLinkTargets(win, ctx);
    PaintLaserPointer(win, ctx);
    UpdateSelectionToolbarPosition(win);
    // The flip swapchain keeps three buffers, and an identical scene does not
    // present. A fresh prim each paint fills every buffer with this frame.
    static unsigned paintNonce = 0;
    gp::Rgba mark{};
    mark.r = (uint8_t)(++paintNonce);
    gp::CanvasFillRect(ctx, 0, 0, 1, 1, mark);
    gp::CanvasPopClip(ctx);
    gFrame.ctx = nullptr;
}

void DocCanvasView::OnWheel(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    float notch = gp::WheelNotchPixels(cx->app);
    if (notch <= 0) {
        notch = 1;
    }
    float raw = ev->deltaY != 0 ? ev->deltaY : ev->deltaX;
    bool isShift = ev->modifiers.shift || ev->deltaY == 0;
    int delta = (int)((raw / notch) * kWheelDelta);
    if (delta == 0) {
        return;
    }
    Point pt;
    ToDoc(win, ev->x, ev->y, &pt);
    gInMouseWheelScroll = true;
    // orig's CanvasOnMouseHWheel scrolls sideways whatever the modifiers:
    // Ctrl + a horizontal wheel does not zoom
    bool isCtrl = ev->modifiers.control && ev->deltaY != 0;
    // orig: isZooming = isCtrl || isRightButton. ng: the wheel event has no
    // button state; the window knows which button is down
    bool isRightButton = cx->win && cx->win->mouseDown && cx->win->pressedButton == gp::MouseButton::Right;
    if ((isRightButton || IsRightDragging(win)) && ev->deltaY != 0) {
        isCtrl = true;
    }
    CanvasOnMouseWheel(win, delta, isCtrl, isShift, ev->modifiers.alt, pt);
    gInMouseWheelScroll = false;
    gp::Notify(cx);
}

void DocCanvasWheelFromFrame(MainWindow* win, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    if (!IsMainWindowValid(win) || !win->AsFixed() || !win->docCanvas) {
        return;
    }
    auto* view = (DocCanvasView*)gp::EntityGet(cx->app, Ui(win)->view.id);
    if (view) {
        DocCanvasView::OnWheel(view, cx, ev);
    }
}

// orig's overlay bar forwards the wheel to the canvas (OverlayScrollbar.cpp),
// and a win32 bar is part of the canvas window: the wheel over the vertical
// bar is a canvas wheel, with the FastScrollOverScrollbar branch reachable
void DocCanvasView::OnWheelOverScrollbar(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    gWheelOverVScrollbar = true;
    OnWheel(self, cx, ev);
    gWheelOverVScrollbar = false;
    // or the scrollbar scrolls by its own step from where it was
    const_cast<gp::ScrollWheelEvent*>(ev)->propagate = false;
}

void DocCanvasView::OnWheelOverHScrollbar(DocCanvasView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    OnWheel(self, cx, ev);
    const_cast<gp::ScrollWheelEvent*>(ev)->propagate = false;
}

// orig's OnWindowContextMenu: the menu is built for the element under (x, y)
// and shown there
static void OnWindowContextMenu(MainWindow* win, gp::Ctx* cx, int x, int y) {
    DocCanvasUI* ui = Ui(win);
    DeleteMenuModel(ui->ctxMenu);
    ui->ctxMenu = BuildWindowContextMenu(win, Point{x, y});
    AppShellInvalidate(win);
    if (!ui->ctxMenu) {
        return;
    }
    float k = CanvasScale(win);
    OpenPopupMenuAt(cx, ui->ctxPopup, (float)x * k, (float)y * k);
}

void DocCanvasContextMenuFromKey(MainWindow* win, gp::Ctx* cx) {
    if (!IsMainWindowValid(win) || !win->AsFixed() || !cx->win) {
        return;
    }
    // if invoked with a keyboard (shift-F10) use current mouse position
    Point pt;
    ToDoc(win, cx->win->mouseX, cx->win->mouseY, &pt);
    // super defensive
    int x = std::max(pt.x, 0);
    int y = std::max(pt.y, 0);
    OnWindowContextMenu(win, cx, x, y);
}

static void OnMouseRightButtonDown(MainWindow* win, int x, int y) {
    if (AnnotationPlacementOnRightDown(win)) {
        return;
    }
    // while an annotation is selected, only it has a context menu
    Annotation* locked = AnnotationLockingMouse(win);
    if (locked && win->AsFixed() && win->AsFixed()->GetAnnotationAtPos(Point{x, y}, locked) != locked) {
        return;
    }
    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
    } else if (win->mouseAction != MouseAction::None) {
        return;
    }

    AppShellFocusFrame(win);

    win->dragStartPending = true;
    win->dragStart = Point(x, y);

    StartMouseDrag(win, x, y, true);
}

static void OnMouseRightButtonUp(MainWindow* win, gp::Ctx* cx, int x, int y, bool isCtrl, bool isShift) {
    if (!IsRightDragging(win)) {
        return;
    }

    int isDragXOrY = IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    bool didDragMouse = !win->dragStartPending || isDragXOrY;
    StopMouseDrag(win, x, y, !didDragMouse);

    win->mouseAction = MouseAction::None;
    win->dragStartPending = false;
    CanvasSetCursor(win, kCurArrow);

    if (didDragMouse) {
        /* pass */;
    } else if (PM_ENABLED == win->presentation) {
        if (isCtrl) {
            OnWindowContextMenu(win, cx, x, y);
        } else if (isShift) {
            win->ctrl->GoToNextPage();
        } else {
            win->ctrl->GoToPrevPage();
        }
        ReadAloudOnUserViewChanged(win);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        win->ChangePresentationMode(PM_ENABLED);
    } else {
        OnWindowContextMenu(win, cx, x, y);
    }
}

void DocCanvasView::OnDown(DocCanvasView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    Point pt;
    if (!ToDoc(win, ev->x, ev->y, &pt)) {
        return;
    }
    SetCanvasModifiers(ev->modifiers);
    if (IsContextClick(ev->button, ev->modifiers)) {
        // ng: gpui's ContextMenu would open from this press; orig's menu
        // opens when the button comes up without having dragged
        gp::WindowStopPropagation(cx);
        // orig's OnMouseRightButtonDblClick: the second press of a pair only
        // counts in presentation mode, where two quick right clicks turn two
        // pages
        bool isDblClick = ev->clickCount > 0 && (ev->clickCount % 2) == 0;
        if (isDblClick && !(win->presentation && !ev->modifiers.Modified())) {
            return;
        }
        OnMouseRightButtonDown(win, pt.x, pt.y);
        gp::Notify(cx);
        return;
    }
    if (ev->button == gp::MouseButton::Middle) {
        // no auto-scroll while an annotation is selected; a running one can still be stopped
        if (AnnotationLockingMouse(win) && win->mouseAction != MouseAction::Scrolling) {
            return;
        }
        ReadingAutoScrollStop(win);
        OnMouseMiddleButtonDown(win, pt.x, pt.y);
    } else if (ev->button == gp::MouseButton::Left) {
        // ng: win32 sends WM_LBUTTONDBLCLK for the second click of a pair and
        // orig detects the third one by time and distance; gpui counts them
        // every second press is a WM_LBUTTONDBLCLK, so a 4th click selects
        // the word again; the 3rd goes through the whole press handler
        if (ev->clickCount > 0 && (ev->clickCount % 2) == 0) {
            // gpui counts a later WM_LBUTTONDOWN at this point as a double-click.
            // Orig only does that for WM_LBUTTONDBLCLK. A press that picked
            // nothing still selects an annotation.
            if (!OnMouseLeftButtonDblClk(win, pt.x, pt.y)) {
                OnMouseLeftButtonDown(win, pt.x, pt.y);
            }
        } else {
            if (ev->clickCount <= 1) {
                gWordSelectedByDblClk = false;
            }
            gIsTripleClick = ev->clickCount >= 3;
            OnMouseLeftButtonDown(win, pt.x, pt.y);
            gIsTripleClick = false;
        }
    }
    gp::Notify(cx);
}

// The window listener handles a drag first. The canvas element then sees the
// same point and must not run the gesture a second time.
static float gRoutedX = 1e30f;
static float gRoutedY = 1e30f;
static bool gRouteHandled = false;

static bool RoutedPoint(float x, float y) {
    return x == gRoutedX && y == gRoutedY;
}

static bool SkipRouted(float x, float y) {
    if (!gRouteHandled || !RoutedPoint(x, y)) {
        return false;
    }
    gRouteHandled = false;
    gRoutedX = 1e30f;
    gRoutedY = 1e30f;
    return true;
}

void DocCanvasView::OnUp(DocCanvasView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    if (SkipRouted(ev->x, ev->y)) {
        return;
    }
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    Point pt;
    ToDoc(win, ev->x, ev->y, &pt);
    SetCanvasModifiers(ev->modifiers);
    if (ev->button == gp::MouseButton::Middle) {
        OnMouseMiddleButtonUp(win);
    } else if (IsContextClick(ev->button, ev->modifiers)) {
        OnMouseRightButtonUp(win, cx, pt.x, pt.y, ev->modifiers.control, ev->modifiers.shift);
    } else if (ev->button == gp::MouseButton::Left) {
        OnMouseLeftButtonUp(win, pt.x, pt.y);
    }
    gp::Notify(cx);
}

// orig's WM_MOUSELEAVE
static void CanvasOnMouseLeave(MainWindow* win) {
    if (win->annotationUnderCursor) {
        win->annotationUnderCursor = nullptr;
        AppShellInvalidate(win);
    }
    HideAnnotationHoverOverlay(win);
    ReadingBarOnMouseLeave(win);
}

// ng: gpui tells an element that the pointer left it through its hover
// listener; a move over another element never reaches the canvas
void DocCanvasView::OnHover(DocCanvasView* self, gp::Ctx*, const gp::HoverEvent* ev) {
    MainWindow* win = self->win;
    if (ev->hovered || !IsMainWindowValid(win) || !win->AsFixed() || win->mouseAction != MouseAction::None) {
        return;
    }
    CanvasOnMouseLeave(win);
    DeleteLinkTooltip(win);
    RefHoverOnCanvasMouseLeave(win->refHover, gSettings->citationHoverDelay);
    // orig's overlay bar poll hides a thick bar once the cursor left the canvas
    OverlayScrollbarsOnMouse(win, 0, 0, false);
}

void DocCanvasView::OnMove(DocCanvasView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    if (SkipRouted(ev->x, ev->y)) {
        return;
    }
    MainWindow* win = self->win;
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return;
    }
    Point pt;
    bool onCanvas = ToDoc(win, ev->x, ev->y, &pt);
    SetCanvasModifiers(ev->modifiers);
    // the popup is on top of the canvas, so a move that reaches here is a move
    // off it (orig asks WindowFromPoint in the hide timer instead)
    if (win->refHover) {
        win->refHover->cursorOverPopup = false;
    }
    // orig's canvas does the same: the overlay toolbar shows while the cursor
    // is in the band at the top (or bottom) of the canvas
    UpdateOverlayToolbarForMouse(win, Point{(int)ev->x, (int)ev->y});
    OverlayScrollbarsOnMouse(win, pt.x, pt.y, onCanvas);
    if (!onCanvas && win->mouseAction == MouseAction::None) {
        // the laser dot and the presentation auto-hide only own the canvas
        AppShellShowCursor(win, true);
        CanvasOnMouseLeave(win);
        DeleteLinkTooltip(win);
        // ng: gpui has no WM_MOUSELEAVE; leaving the canvas is what orig's
        // TrackMouseLeave reports
        RefHoverOnCanvasMouseLeave(win->refHover, gSettings->citationHoverDelay);
        return;
    }
    OnMouseMove(win, pt.x, pt.y);
    // ng: no Notify here - a plain hover must not rebuild the element tree on
    // every move. The handlers that changed something invalidate themselves.
    (void)cx;
}

// orig's bars are the window's own (the "windows" mode) or the overlay ones;
// here both are the port's OverlayScrollbar, in the mode the prefs name
static OverlayScrollbar* CanvasScrollbar(MainWindow* win, bool vert) {
    OverlayScrollbarsSyncMode(win);
    return vert ? win->overlayScrollV : win->overlayScrollH;
}

bool DocCanvasWantsRepaint(MainWindow* win) {
    bool res = win->repaintPending;
    win->repaintPending = false;
    return res;
}

// gpui delivers a move to the element under the pointer, and a press does not
// capture it. A resize that crosses the edit toolbar (or leaves the canvas)
// would otherwise never update. While a gesture is active, the window sees
// every move and up first.
static bool CanvasGestureActive(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->AsFixed()) {
        return false;
    }
    return win->mouseAction != MouseAction::None || win->annotPlacement.mouseDown;
}

static void OnWindowMove(DocCanvasView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    OverlayScrollbarOnWindowMove(cx->win, ev->x, ev->y);
    MainWindow* win = self->win;
    if (!CanvasGestureActive(win)) {
        gRouteHandled = false;
        return;
    }
    gRoutedX = ev->x;
    gRoutedY = ev->y;
    gRouteHandled = false;
    DocCanvasView::OnMove(self, cx, ev);
    gRouteHandled = true;
}

static void OnWindowUp(DocCanvasView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    OverlayScrollbarOnWindowUp(cx->win, ev->button);
    MainWindow* win = self->win;
    if (!CanvasGestureActive(win)) {
        return;
    }
    gRoutedX = ev->x;
    gRoutedY = ev->y;
    gRouteHandled = false;
    DocCanvasView::OnUp(self, cx, ev);
    gRouteHandled = true;
}

void DocCanvasHookWindow(MainWindow* win, gp::Window* gw) {
    gUiThread = GetCurrentThreadId();
    gp::App* app = gw->app;
    DocCanvasUI* ui = Ui(win);
    ui->view = gp::EntityNewState<DocCanvasView>(app);
    auto* view = (DocCanvasView*)gp::EntityGet(app, ui->view.id);
    view->win = win;
    gw->onMouseMove = gp::ListenTo(ui->view, &OnWindowMove);
    gw->onMouseUp = gp::ListenTo(ui->view, &OnWindowUp);
    UpdateDeltaPerLine();
}

// orig strips the tooltip's visual style (TooltipApplyColors), which leaves the
// classic look: a plain rectangle with a 1 px COLOR_WINDOWFRAME edge
constexpr Color kTooltipFrameColor = MkRgb(0x64, 0x64, 0x64);

// ng: orig shows the link target in a win32 tooltip below the link. gpui's
// Tooltip components attach to an element, and the link is a painted rect, not
// one, so the card is drawn here, placed the way orig places the tooltip.
static gp::El* LinkTooltipBuild(MainWindow* win, gp::Ctx* cx) {
    if (len(win->linkTooltip) == 0) {
        return nullptr;
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float k = CanvasScale(win);
    Rect rc = win->linkTooltipRc;
    float x = (float)rc.x * k;
    float y = ((float)(rc.y + rc.dy) + 4) * k;
    float maxW = (float)win->canvasRc.dx - 16;
    if (x > maxW - 40) {
        x = std::max(0.f, maxW - 40);
    }
    if (y > (float)win->canvasRc.dy - 24) {
        y = ((float)rc.y - 22) * k;
    }
    y = std::max(0.f, y);
    return gp::Div(cx->a)
        ->Absolute()
        ->Left(x)
        ->Top(y)
        ->MaxW(maxW)
        ->PadX(3)
        ->PadY(1)
        ->Bg(th.tokens.popover)
        ->Border(1, ToGpui(kTooltipFrameColor))
        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, win->linkTooltip))->Font(12)->Fg(th.popoverFg));
}

// --- the page context menu --------------------------------------------------

static uint32_t ActCanvasMenu() {
    static uint32_t act = gp::ActionOf(GStrL("sumatra::CanvasMenu"));
    return act;
}

// ng: a command run from a gpui popup runs while the popup still holds the
// keyboard and the mouse, so it goes through the ui task queue (as the tab
// context menu does); orig's TrackPopupMenu returns after the menu is gone
struct CanvasMenuCmd {
    MainWindow* win;
    int cmdId;
};

static void RunCanvasMenuCmd(CanvasMenuCmd* c) {
    if (IsMainWindowValidAndNotClosing(c->win)) {
        WindowContextMenuCommand(c->win, c->cmdId);
    }
    delete c;
}

void DocCanvasView::OnMenuAction(DocCanvasView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    auto* c = new CanvasMenuCmd{self->win, (int)ev->arg};
    uitask::Post(MkFunc0<CanvasMenuCmd>(RunCanvasMenuCmd, c), "CanvasMenuCmd");
    gp::Notify(cx);
}

static gpc::PopupMenu* CanvasPopupFromModel(gp::Ctx* cx, MenuModel* model, Str id) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(220);
    if (!model) {
        return menu;
    }
    int i = -1;
    for (const MenuItemModel& it : model->items) {
        i++;
        if (it.separator) {
            menu->Separator();
            continue;
        }
        gp::Str label = GpuiDup(cx->a, ParseMenuAccelTextTemp(it.title).display);
        if (it.submenu) {
            TempStr subId = fmt("%s-%d", id, i);
            menu->Submenu(label, CanvasPopupFromModel(cx, it.submenu, subId));
            menu->Disabled(it.disabled);
            continue;
        }
        menu->MenuWithAction(label, ActCanvasMenu(), (intptr_t)it.cmdId);
        if (len(it.accel) > 0) {
            menu->Kbd(GpuiDup(cx->a, it.accel));
        }
        menu->Disabled(it.disabled);
        menu->Checked(it.checked);
    }
    return menu;
}

gp::El* DocCanvasBuild(MainWindow* win, gp::Ctx* cx) {
    DisplayModel* dm = win->AsFixed();
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* root = gp::Div(cx->a)->Flex1()->W(gp::kFill)->Bg(ThemeGpuiCanvasBg());
    if (!dm) {
        return root;
    }

    float k = CanvasScale(win);
    gp::El* canvas = gp::Div(cx->a)
                         ->SizeFull()
                         ->Id(GStrL("doc-canvas"))
                         ->Click(gp::HashClickId(GStrL("doc-canvas")))
                         ->Cursor((gp::CursorKind)win->canvasCursor)
                         ->OnMouseDown(gp::ListenTo(Ui(win)->view, &DocCanvasView::OnDown))
                         ->OnMouseUp(gp::ListenTo(Ui(win)->view, &DocCanvasView::OnUp))
                         ->OnMouseMove(gp::ListenTo(Ui(win)->view, &DocCanvasView::OnMove))
                         ->OnHover(gp::ListenTo(Ui(win)->view, &DocCanvasView::OnHover))
                         ->OnScrollWheel(gp::ListenTo(Ui(win)->view, &DocCanvasView::OnWheel));
    canvas->customPaint = &DocCanvasView::OnPaint;
    canvas->customUser = win;
    // ng: ContextMenu::IntoEl() returns the child it was given with its own
    // click path on it, so the canvas goes inside a wrapper instead of being
    // the child itself (its click path is what the scroll paths hang off)
    gp::El* canvasWrap = gp::Div(cx->a)->SizeFull()->Child(canvas);
    gpc::PopupMenu* ctxPopup = TrackPopup(cx, CanvasPopupFromModel(cx, Ui(win)->ctxMenu, StrL("canvas-ctx-menu")));
    Ui(win)->ctxPopup = ctxPopup->state;
    gp::El* canvasBox = gpc::ContextMenu::New(cx, GStrL("canvas-ctx"))->Child(canvasWrap)->Menu(ctxPopup)->IntoEl();
    canvasBox->OnAction(ActCanvasMenu(), gp::ListenTo(Ui(win)->view, &DocCanvasView::OnMenuAction));
    root->Child(canvasBox);

    gp::El* tip = LinkTooltipBuild(win, cx);
    if (tip) {
        root->Child(tip);
    }
    gp::El* selBar = SelectionToolbarBuild(win, cx);
    if (selBar) {
        root->Child(selBar);
    }
    gp::El* autoBar = ReadingAutoScrollBarBuild(win, cx);
    if (autoBar) {
        root->Child(autoBar);
    }
    if (gp::El* readAloudBar = ReadAloudPlaybackBarBuild(win, cx)) {
        root->Child(readAloudBar);
    }
    if (gp::El* formEdit = FormFieldEditBuild(win, cx)) {
        root->Child(formEdit);
    }
    if (gp::El* freeText = FreeTextInPlaceEditBuild(win, cx)) {
        root->Child(freeText);
    }
    if (gp::El* annotBar = AnnotEditToolbarBuild(win, cx)) {
        root->Child(annotBar);
    }
    if (gp::El* annotPopup = AnnotTextPopupBuild(win, cx)) {
        root->Child(annotPopup);
    }
    if (gp::El* annotHover = AnnotationHoverOverlayBuild(win, cx)) {
        root->Child(annotHover);
    }
    if (gp::El* refPopup = RefHoverBuild(win, cx)) {
        root->Child(refPopup);
    }

    // orig's DrawCanvasKeyboardFocusIfNeeded: a focus ring on the canvas when
    // the document has the keyboard focus, gated by ShowDocumentFocusIndicator
    // (default off; #4644)
    bool showFocus = gSettings->showDocumentFocusIndicator && !win->presentation && !win->isFullScreen &&
                     AppShellIsFrameFocused(win) && gp::WindowIsActive(cx);
    if (showFocus) {
        // inset so the dashed rect is fully inside the client area
        root->Child(gp::Div(cx->a)
                        ->Absolute()
                        ->Left(1)
                        ->Top(1)
                        ->W((float)win->canvasRc.dx - 2)
                        ->H((float)win->canvasRc.dy - 2)
                        ->Border(1, th.foreground)
                        ->Dashed());
    }

    OverlayScrollbar* sbV = CanvasScrollbar(win, true);
    OverlayScrollbar* sbH = CanvasScrollbar(win, false);
    bool showV = win->scrollV.visible && IsOverlayScrollbarVisible(sbV);
    bool showH = win->scrollH.visible && IsOverlayScrollbarVisible(sbH);
    // orig's OverlayScrollbarUpdatePos: two thick bars leave the corner free
    bool bothThick = showV && showH && IsOverlayScrollbarThick(sbV) && IsOverlayScrollbarThick(sbH);
    int canvasDx = (int)((float)win->canvasRc.dx / k);
    int canvasDy = (int)((float)win->canvasRc.dy / k);
    if (showV) {
        const CanvasScrollInfo& si = win->scrollV;
        OverlayScrollbarSetInfo(sbV, si.nMin, si.nMax, si.nPage, si.nPos);
        int siblingInset = bothThick ? OverlayScrollbarWidth(sbH) : 0;
        gp::Listener onWheel = gp::ListenTo(Ui(win)->view, &DocCanvasView::OnWheelOverScrollbar);
        gp::El* bar = OverlayScrollbarBuild(cx, sbV, StrL("doc-scroll-v"), canvasDy - siblingInset, k, &onWheel);
        root->Child(bar->Absolute()->Right(0)->Top(0));
    }
    if (showH) {
        const CanvasScrollInfo& si = win->scrollH;
        OverlayScrollbarSetInfo(sbH, si.nMin, si.nMax, si.nPage, si.nPos);
        int siblingInset = bothThick ? OverlayScrollbarWidth(sbV) : 0;
        gp::Listener onWheel = gp::ListenTo(Ui(win)->view, &DocCanvasView::OnWheelOverHScrollbar);
        gp::El* bar = OverlayScrollbarBuild(cx, sbH, StrL("doc-scroll-h"), canvasDx - siblingInset, k, &onWheel);
        root->Child(bar->Absolute()->Left(0)->Bottom(0));
    }
    return root;
}
