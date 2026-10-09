/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui half of orig's Canvas.cpp - the DisplayModel-driven part:
// DrawDocument, the page frame / shadow, scrolling, wheel, drag panning and
// the overlay scrollbars. Links, selection, find, annotations and the touch /
// gesture handlers arrive with steps 8 and 13.

namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
struct PaintApp;
struct RenderImage;
struct Window;
struct ScrollWheelEvent;
} // namespace gpui

struct Pixmap;

struct MainWindow;
struct DisplayModel;
enum class NativeCursor;
struct BitmapCacheEntry;

// orig sends WM_VSCROLL / WM_HSCROLL with an SB_* code. There are no window
// messages here, so the same actions are an enum with orig's names.
enum class ScrollMsg {
    None = 0,
    Top,
    Bottom,
    LineUp,
    LineDown,
    HalfPageUp,
    HalfPageDown,
    PageUp,
    PageDown,
    ThumbTrack,
    Left,
    Right,
    LineLeft,
    LineRight,
    PageLeft,
    PageRight,
};

gpui::El* DocCanvasBuild(MainWindow* win, gpui::Ctx* cx);
// orig's WM_CONTEXTMENU from the keyboard (the Apps key, Shift + F10): the
// page context menu at the mouse position
void DocCanvasContextMenuFromKey(MainWindow* win, gpui::Ctx* cx);
// orig's frame passes a WM_MOUSEWHEEL no child took to the canvas, so the
// document scrolls with the wheel over the tab strip or the toolbar
void DocCanvasWheelFromFrame(MainWindow* win, gpui::Ctx* cx, const gpui::ScrollWheelEvent* ev);
// gpui lays out in dips, the document model in pixels: dips per document pixel
float CanvasScale(MainWindow* win);
// the mouse handlers live on the window, not the element, so a drag that
// leaves the canvas still arrives (orig captures the mouse for the same reason)
void DocCanvasHookWindow(MainWindow* win, gpui::Window* gw);
void DocCanvasDelete(MainWindow* win);
// the shell's tick: repaint while a page is still rendering
bool DocCanvasWantsRepaint(MainWindow* win);
// middle-button auto-scroll moves the document a bit on every tick
void DocCanvasAutoScrollTick(MainWindow* win, int elapsedMs);
// orig's SmoothScroll: the exponential chase of the wheel / line-scroll target
void DocCanvasSmoothScrollTick(MainWindow* win, int elapsedMs);
void StopSmoothScroll(MainWindow* win);

// `nTrackPos` is the thumb position of a ScrollMsg::ThumbTrack
void CanvasOnVScroll(MainWindow* win, ScrollMsg msg, int nTrackPos = 0);
void CanvasOnHScroll(MainWindow* win, ScrollMsg msg, int nTrackPos = 0);
int CanvasScrollPosV(MainWindow* win);

// DocControllerCallback, forwarded by MainWindow.cpp
void CanvasUpdateScrollbars(MainWindow* win, DisplayModel* dm, Size canvas);

// RenderCache::PaintTile blits a cached tile through these (RenderCache lives
// in the `app` lib, which knows nothing about gpui)
bool CanvasDrawTile(gpui::PaintCtx* ctx, BitmapCacheEntry* entry, Rect target, Rect source);
// what wrapping cached pixmaps into gpui RenderImages has cost so far
void CanvasBmpWrapStats(double& msOut, int& countOut);
// ms from process start to the first page tile on screen, -1 before that
double CanvasFirstPaintMs();
void CanvasDrawTileOutline(gpui::PaintCtx* ctx, Rect bounds);
// drops the RenderImage a cache entry carries, when the entry is evicted
void CanvasFreeTileImage(BitmapCacheEntry* entry);
// a page bitmap as something gpui can draw (see "gpui gaps": no RenderImage
// from raw pixels, so this goes through a BMP header and RenderImageDecode)
gpui::RenderImage* RenderImageFromPixmap(gpui::PaintApp* pa, Pixmap* px);

// --- what Selection.cpp / LinkFollow.cpp draw and ask with -----------------

// orig's Gfx::FillRects / FillQuads: document coordinates, one color with an
// alpha and an optional 1px outline of the same color at full opacity
void CanvasFillRects(gpui::PaintCtx* ctx, const Rect* rects, int n, Color col, u8 alpha, int outlineWidth);
void CanvasFillQuads(gpui::PaintCtx* ctx, const Point* pts, int nQuads, Color col, u8 alpha, int outlineWidth);
void CanvasDrawLine(gpui::PaintCtx* ctx, Point a, Point b, Color col, float width);
void CanvasDrawRect(gpui::PaintCtx* ctx, Rect r, Color col, float width);
// width <= 0 fills the ellipse
void CanvasDrawEllipse(gpui::PaintCtx* ctx, Rect r, Color col, float width);
Size CanvasMeasureText(gpui::PaintCtx* ctx, Str s, float fontSize);
void CanvasDrawText(gpui::PaintCtx* ctx, Str s, Point at, Color col, float fontSize);

// orig's SetCapture / ReleaseCapture on the canvas HWND
void CanvasSetCapture(MainWindow* win, bool capture);
void CanvasSetClipboardText(MainWindow* win, Str s);
// what orig reads with GetKeyState; here it is the last mouse or key event
bool CanvasCtrlPressed();
bool CanvasShiftPressed();
void CanvasSetKeyModifiers(bool shift, bool ctrl);
// orig's IsDragDistance: has the pointer moved far enough to be a drag?
bool IsDragDistance(int x1, int x2, int y1, int y2);
// the cursor the canvas shows, as a gpui CursorKind cast to int
void CanvasSetCursor(MainWindow* win, int cursorKind);
// one of orig's cursors that are bitmaps; false where there is none (off
// Windows), and the caller sets a gpui cursor instead
bool CanvasSetNativeCursor(MainWindow* win, NativeCursor cursor);
// orig's CancelDrag: Escape abandons whatever the mouse was doing
void CanvasCancelDrag(MainWindow* win);
bool NudgeSelectedAnnotation(MainWindow*, int vkey, bool shift);
void AnnotationNudgeTick(MainWindow*, int elapsedMs);
void AnnotationResizeRerenderTick(MainWindow*, int elapsedMs);
void DeleteLinkTooltip(MainWindow* win);
// presentation mode: count the auto-hide cursor timer down (shell tick)
void CanvasTickPresentation(MainWindow* win, int ms);
// orig's StartAutoScrollAtCursor (CmdStartAutoScroll)
void StartAutoScrollAtCursor(MainWindow* win);
// CmdToggleLaserPointer
void ToggleLaserPointer(MainWindow* win);
bool IsLaserPointerActive();

// the canvas overlays (orig's Canvas.cpp): CmdToggleImages,
// CmdToggleTransparencyGrid and CmdDebugShowFitContentArea. Session-only, the
// way orig keeps them: drawn, never saved to the settings
bool ShowImageOutlines();
void ToggleShowImageOutlines();
bool ShowTransparencyGrid();
void ToggleTransparencyGrid();
bool ShowFitContentArea();
void ToggleShowFitContentArea();
// how many outlines the overlays drew in the last paint (-dbg-control)
int CanvasOverlayShapesDrawn();
