/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's popup is a WS_POPUP tool window with its own WndProc that blits
// the rendered strip with GDI. gpui has no child windows, so the popup is an
// absolutely positioned element over the canvas (the AnnotTextPopup pattern)
// and the strip is an ImageEl over the page bitmap. Same background color,
// same border, same placement rules, with the canvas in place of the monitor
// work area as the bound.

#include "gui/GpuiBridge.h"

#include "base/Pixmap.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "RefHover.h"

// orig's WHEEL_DELTA, the unit RefHoverWheelScroll counts notches in
constexpr int kWheelDelta = 120;

// orig's WM_PAINT fills the popup with this before blitting the strip
static Color RefHoverBg() {
    return MkRgb(255, 252, 200);
}

void RefHoverFreeRenderImage(RefHoverState* s) {
    if (!s || !s->renderImage) {
        return;
    }
    auto* img = (gp::RenderImage*)s->renderImage;
    s->renderImage = nullptr;
    gp::RenderImageRelease(img);
}

static bool PopupClientToPagePt(RefHoverState* s, int clientX, int clientY, PointF& ptOut) {
    if (!s || !s->hitEngine || s->displayed.destPage <= 0) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    int border = DpiScale(kRefHoverBorder);
    // When a column-wrap continuation is stitched below displayed.region in
    // the bitmap (see RefHoverRender.cpp's StackPixmapsVertically), a click
    // there falls outside what displayed.region maps to — the formula below
    // would silently produce a page point in the wrong place. Reject clicks
    // past the primary crop's rendered height rather than mis-hit-test.
    float regionPixH = s->displayed.region.dy * zoom;
    if ((float)(clientY - border) > regionPixH) {
        return false;
    }
    ptOut.x = s->displayed.region.x + ((float)(clientX - border) / zoom);
    ptOut.y = s->displayed.region.y + ((float)(clientY - border) / zoom);
    return true;
}

static IPageDestination* LaunchLinkAtPagePt(RefHoverState* s, PointF pagePt) {
    if (!s || !s->hitEngine || s->displayed.destPage <= 0) {
        return nullptr;
    }
    IPageElement* el = s->hitEngine->GetElementAtPos(s->displayed.destPage, pagePt);
    if (!el || !el->Is(kindPageElementDest)) {
        return nullptr;
    }
    IPageDestination* dest = el->AsLink();
    if (!dest || !IsLaunchLinkKind(dest->GetKind())) {
        return nullptr;
    }
    return dest;
}

static IPageDestination* LaunchLinkAtPopupPt(RefHoverState* s, int clientX, int clientY) {
    PointF pagePt;
    if (!PopupClientToPagePt(s, clientX, clientY, pagePt)) {
        return nullptr;
    }
    return LaunchLinkAtPagePt(s, pagePt);
}

// a mouse event's window point as a popup-client point, in canvas pixels
static Point PopupClientPt(RefHoverState* s, float wx, float wy) {
    MainWindow* win = s->win;
    float k = CanvasScale(win);
    if (k <= 0.f) {
        k = 1.f;
    }
    int x = (int)((wx - (float)win->canvasRc.x) / k) - s->popupRc.x;
    int y = (int)((wy - (float)win->canvasRc.y) / k) - s->popupRc.y;
    return Point(x, y);
}

void RefHoverShowPopup(RefHoverState* s, Point screenPt) {
    if (!s || !s->bmp || !s->win) {
        return;
    }
    Size bmpSize = Size(s->bmp->width, s->bmp->height);
    int border = DpiScale(kRefHoverBorder);
    int popupW = bmpSize.dx + (2 * border);
    int popupH = bmpSize.dy + (2 * border);

    // ng: orig bounds the popup by the monitor work area; the canvas is what
    // bounds it here, since the popup is an element inside it
    int leftBound = 0;
    int rightBound = s->win->canvasRc.dx;
    int topBound = 0;
    int bottomBound = s->win->canvasRc.dy;
    Rect pr = s->pending.pageScreenRect;
    if (pr.dy > 0) {
        topBound = std::max(pr.y, topBound);
        bottomBound = std::min(pr.y + pr.dy, bottomBound);
    }
    int boundW = rightBound - leftBound;
    int boundH = bottomBound - topBound;
    popupW = std::min(popupW, boundW);
    popupH = std::min(popupH, boundH);

    int pageCenterX = (pr.dx > 0) ? (pr.x + (pr.dx / 2)) : screenPt.x;
    int anchorX = pageCenterX;
    if (pr.dx > 0) {
        int colWidth = pr.dx / 2;
        if (popupW <= colWidth) {
            anchorX = (screenPt.x >= pageCenterX) ? (pr.x + (pr.dx * 3 / 4)) : (pr.x + (pr.dx / 4));
        }
    }
    int x = anchorX - (popupW / 2);
    int cursorPad = DpiScale(kRefHoverCursorPad);
    int spaceBelow = bottomBound - (screenPt.y + cursorPad);
    int spaceAbove = (screenPt.y - cursorPad) - topBound;
    int y;
    if (spaceBelow >= popupH) {
        y = screenPt.y + cursorPad;
    } else if (spaceAbove >= popupH) {
        y = screenPt.y - popupH - cursorPad;
    } else if (spaceBelow >= spaceAbove) {
        if (spaceBelow > 0) {
            popupH = spaceBelow;
        }
        y = screenPt.y + cursorPad;
    } else {
        if (spaceAbove > 0) {
            popupH = spaceAbove;
        }
        y = screenPt.y - popupH - cursorPad;
    }
    x = std::max(x, leftBound);
    if (x + popupW > rightBound) {
        x = rightBound - popupW;
    }
    y = std::max(y, topBound);
    if (y + popupH > bottomBound) {
        popupH = bottomBound - y;
    }

    if (popupW <= 0 || popupH <= 0) {
        s->visible = false;
        AppShellInvalidate(s->win);
        return;
    }

    s->popupRc = Rect{x, y, popupW, popupH};
    s->visible = true;
    AppShellInvalidate(s->win);
}

bool RefHoverRerenderDisplayedRegion(RefHoverState* s, EngineBase* engine, int page, RectF region) {
    if (!s || !engine || page <= 0) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    s->displayed.destPage = page;
    s->displayed.region = region;
    RefHoverState::RenderRequest req;
    req.pageNo = page;
    req.zoom = zoom;
    req.region = region;
    RefHoverRequestRender(s, engine, req);
    return true;
}

// Re-render the popup at adjusted zoom in response to a mouse-wheel event.
// Popup keeps its size; only the rendered content scales. Positive delta zooms
// in, negative zooms out. Returns true if the zoom changed and a re-render
// happened.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->visible || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float factor = (wheelDelta > 0) ? kRefHoverUserZoomStep : (1.f / kRefHoverUserZoomStep);
    float newZoom = s->displayed.userZoom * factor;
    if (newZoom < kRefHoverMinUserZoom) {
        newZoom = kRefHoverMinUserZoom;
    } else if (newZoom > kRefHoverMaxUserZoom) {
        newZoom = kRefHoverMaxUserZoom;
    }
    if (newZoom == s->displayed.userZoom) {
        return false;
    }
    s->displayed.userZoom = newZoom;

    int border = DpiScale(kRefHoverBorder);
    float clientW = (float)(s->popupRc.dx - (2 * border));
    float clientH = (float)(s->popupRc.dy - (2 * border));
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f || clientW <= 0.f || clientH <= 0.f) {
        return false;
    }

    RectF mediabox = engine->PageMediabox(s->displayed.destPage);
    RectF region = s->displayed.region;
    region.dx = clientW / zoom;
    region.dy = clientH / zoom;
    if (region.x + region.dx > mediabox.dx) {
        region.dx = mediabox.dx - region.x;
    }
    if (region.y + region.dy > mediabox.dy) {
        region.dy = mediabox.dy - region.y;
    }
    if (region.dx <= 0.f || region.dy <= 0.f) {
        return false;
    }

    return RefHoverRerenderDisplayedRegion(s, engine, s->displayed.destPage, region);
}

// Scroll the popup's rendered region by a wheel notch. Positive delta scrolls
// toward earlier content (up); negative scrolls toward later content (down).
// Rolls over to the previous / next page when the viewport hits a page edge
// (continuous scrolling). Popup keeps its size; only the rendered region's Y
// (and possibly page number) changes.
bool RefHoverWheelScroll(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->visible || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    int pageCount = engine->PageCount();
    int page = s->displayed.destPage;
    RectF region = s->displayed.region;
    RectF mediabox = engine->PageMediabox(page);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return false;
    }

    float scrollStep = (float)DpiScale(kRefHoverScrollStepPx);
    float scrollPt = scrollStep * ((float)wheelDelta / (float)kWheelDelta) / zoom;
    float newY = region.y - scrollPt;

    if (newY < 0.f) {
        if (page > 1) {
            float overflow = -newY;
            page--;
            mediabox = engine->PageMediabox(page);
            newY = mediabox.dy - region.dy - overflow;
            newY = std::max(newY, 0.f);
        } else {
            newY = 0.f;
        }
    } else if (newY + region.dy > mediabox.dy) {
        if (page < pageCount) {
            float overflow = (newY + region.dy) - mediabox.dy;
            page++;
            mediabox = engine->PageMediabox(page);
            newY = overflow;
            if (newY + region.dy > mediabox.dy) {
                newY = mediabox.dy - region.dy;
            }
            newY = std::max(newY, 0.f);
        } else {
            newY = mediabox.dy - region.dy;
            newY = std::max(newY, 0.f);
        }
    }

    if (page == s->displayed.destPage && newY == region.y) {
        return false;
    }
    region.y = newY;
    region.dy = std::min(region.dy, mediabox.dy);
    if (region.x + region.dx > mediabox.dx) {
        region.x = mediabox.dx - region.dx;
        if (region.x < 0.f) {
            region.x = 0.f;
            region.dx = mediabox.dx;
        }
    }

    return RefHoverRerenderDisplayedRegion(s, engine, page, region);
}

// --- the popup element ------------------------------------------------------

struct RefHoverView {
    MainWindow* win = nullptr;

    static void OnMove(RefHoverView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnDown(RefHoverView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnWheel(RefHoverView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
};

static gp::Entity<RefHoverView> gRefHoverView;

// ng: orig asks WindowFromPoint in its hide timer; here the popup's own
// mouse-move says the cursor is on it, and the canvas's says it left
void RefHoverView::OnMove(RefHoverView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    RefHoverState* s = self->win ? self->win->refHover : nullptr;
    if (!s || !s->visible) {
        return;
    }
    s->cursorOverPopup = true;
    Point pt = PopupClientPt(s, ev->x, ev->y);
    int cur = LaunchLinkAtPopupPt(s, pt.x, pt.y) ? (int)gp::CursorKind::Pointer : (int)gp::CursorKind::Arrow;
    if (cur != s->popupCursor) {
        s->popupCursor = cur;
        gp::Notify(cx);
    }
}

void RefHoverView::OnDown(RefHoverView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    RefHoverState* s = self->win ? self->win->refHover : nullptr;
    if (!s || !s->visible || ev->button != gp::MouseButton::Left) {
        return;
    }
    Point pt = PopupClientPt(s, ev->x, ev->y);
    IPageDestination* dest = LaunchLinkAtPopupPt(s, pt.x, pt.y);
    if (dest) {
        RefHoverHandlePopupClick(s, dest);
    }
    gp::Notify(cx);
}

// The popup owns wheel input under the cursor. Ctrl zooms; every other wheel
// scrolls, including horizontal wheels synthesized for Shift + wheel.
void RefHoverView::OnWheel(RefHoverView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    RefHoverState* s = self->win ? self->win->refHover : nullptr;
    if (!s || !s->visible || !s->hitEngine) {
        return;
    }
    float notch = gp::WheelNotchPixels(cx->app);
    if (notch <= 0) {
        notch = 1;
    }
    float raw = ev->deltaY != 0 ? ev->deltaY : ev->deltaX;
    int delta = (int)((raw / notch) * kWheelDelta);
    if (delta == 0) {
        return;
    }
    if (ev->modifiers.control) {
        RefHoverWheelZoom(s, s->hitEngine, delta);
    } else {
        RefHoverWheelScroll(s, s->hitEngine, delta);
    }
    gp::WindowStopPropagation(cx);
    gp::Notify(cx);
}

static gp::ImageLoadState RefHoverImgLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* s = (RefHoverState*)user;
    if (!s->renderImage && s->bmp) {
        s->renderImage = RenderImageFromPixmap(pa, s->bmp);
    }
    *imgOut = (gp::RenderImage*)s->renderImage;
    return s->renderImage ? gp::ImageLoadState::Ready : gp::ImageLoadState::Loading;
}

gp::El* RefHoverBuild(MainWindow* win, gp::Ctx* cx) {
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s || !s->visible || !s->bmp || s->popupRc.IsEmpty()) {
        return nullptr;
    }
    if (!gRefHoverView.IsValid()) {
        gRefHoverView = gp::EntityNewState<RefHoverView>(cx->app);
    }
    auto* view = (RefHoverView*)gp::EntityGet(cx->app, gRefHoverView.id);
    view->win = win;

    float k = CanvasScale(win);
    float border = (float)DpiScale(kRefHoverBorder) * k;
    Rect rc = s->popupRc;
    gp::ImageSource src = gp::ImageSource::FromCustom(RefHoverImgLoad, s);
    gp::El* card = gp::Div(cx->a)
                       ->Absolute()
                       ->Left((float)rc.x * k)
                       ->Top((float)rc.y * k)
                       ->W((float)rc.dx * k)
                       ->H((float)rc.dy * k)
                       ->Pad(border)
                       ->Bg(ToGpui(RefHoverBg()))
                       ->Border(1, ToGpui(kColBlack))
                       ->Cursor((gp::CursorKind)s->popupCursor)
                       ->OnMouseMove(gp::ListenTo(gRefHoverView, &RefHoverView::OnMove))
                       ->OnMouseDown(gp::ListenTo(gRefHoverView, &RefHoverView::OnDown))
                       ->OnScrollWheel(gp::ListenTo(gRefHoverView, &RefHoverView::OnWheel));
    card->Child(gp::ImageEl(cx->a, src, GStrL(""))
                    ->W((float)s->bmp->width * k)
                    ->H((float)s->bmp->height * k)
                    ->ObjectFitMode(gp::ObjectFit::Fill));
    return card;
}
