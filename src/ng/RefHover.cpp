/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — lifecycle and scheduling. Popup UI, async
// render, region detection, canvas wiring, and plain-text lookup live in
// sibling RefHover*.cpp files.

#include "base/Base.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "RefHover.h"

float CanvasScale(MainWindow* win);

// Tests pass a frame client point. The document hit-test wants canvas pixels.
static bool FrameClientToDoc(MainWindow* win, int px, int py, Point* out) {
    float scale = CanvasScale(win);
    if (scale <= 0.f) {
        scale = 1.f;
    }
    float dipX = (float)px * scale;
    float dipY = (float)py * scale;
    Rect rc = win->canvasRc;
    if (dipX < (float)rc.x || dipY < (float)rc.y || dipX >= (float)(rc.x + rc.dx) || dipY >= (float)(rc.y + rc.dy)) {
        return false;
    }
    *out = Point{(int)((dipX - (float)rc.x) / scale), (int)((dipY - (float)rc.y) / scale)};
    return true;
}

RefHoverState* RefHoverCreate(MainWindow* win) {
    auto* s = new RefHoverState();
    s->win = win;
    RefHoverRegisterLiveState(s);
    return s;
}

void RefHoverDestroy(RefHoverState* s) {
    if (!s) {
        return;
    }
    RefHoverUnregisterLiveState(s);
    RefHoverDropQueuedRender(s);
    RefHoverFreeRenderImage(s);
    FreePixmap(s->bmp);
    s->bmp = nullptr;
    if (s->hitEngine) {
        s->hitEngine->Release();
        s->hitEngine = nullptr;
    }
    RefHoverFreeLookupCache(s);
    delete s;
}

// delayMs: how long the cursor must hover before the popup shows
// (the CitationHoverDelay advanced setting)
void RefHoverSchedule(RefHoverState* s, int delayMs, Point screenPt, int destPage, float destX, float destY,
                      float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s || delayMs < 0) {
        return;
    }
    s->showLeftMs = -1;
    s->hideLeftMs = -1;

    bool sameSrc = s->displayed.srcPage == srcPage && s->displayed.srcRect == srcRect;
    if (s->visible && s->displayed.destPageRaw == destPage && s->displayed.destX == destX &&
        s->displayed.destY == destY && sameSrc) {
        return;
    }
    s->pending.screenPt = screenPt;
    s->pending.destPage = destPage;
    s->pending.destX = destX;
    s->pending.destY = destY;
    s->pending.destZoom = destZoom;
    s->pending.noteRef = false;
    s->pending.srcPage = srcPage;
    s->pending.srcRect = srcRect;
    s->pending.pageScreenRect = pageScreenRect;
    if (s->visible) {
        delayMs = 0;
    }
    s->showLeftMs = delayMs;
}

void RefHoverHide(RefHoverState* s) {
    if (!s) {
        return;
    }
    s->showLeftMs = -1;
    s->hideLeftMs = -1;
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (s->visible) {
        s->visible = false;
        s->displayed.destPage = -1;
        AppShellInvalidate(s->win);
    }
}

static constexpr int kRefHoverHidePollMs = 150;
// Like RefHoverHide but deferred: cancels any pending show immediately, then
// hides the visible popup after delayMs. While the timer is pending, moving
// the cursor onto the popup (e.g. to click a DOI link inside it) keeps it
// alive. Lets the cursor cross the gap between the link and the popup without
// the popup vanishing. Cancelled by a new RefHoverSchedule / RefHoverHide.
void RefHoverScheduleHide(RefHoverState* s, int delayMs) {
    if (!s) {
        return;
    }
    s->showLeftMs = -1;
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (!s->visible) {
        s->displayed.destPage = -1;
        return;
    }
    delayMs = std::max(delayMs, kRefHoverHideMinMs);
    s->hideLeftMs = delayMs;
}

// Fired by the hide timer: hides the popup unless the cursor is now over it
// (in which case it re-arms and keeps the popup up).
void RefHoverOnHideTimer(RefHoverState* s) {
    if (!s) {
        return;
    }
    s->hideLeftMs = -1;
    if (!s->visible) {
        return;
    }
    if (s->cursorOverPopup) {
        s->hideLeftMs = kRefHoverHidePollMs;
        return;
    }
    s->visible = false;
    s->displayed.destPage = -1;
    AppShellInvalidate(s->win);
}

// Open a launch link (external URL / file) hit-tested inside the popup.
void RefHoverHandlePopupClick(RefHoverState* s, IPageDestination* dest) {
    if (!s || !dest || !s->ctrl) {
        return;
    }
    RefHoverHide(s);
    s->ctrl->HandleLink(dest, s->linkHandler);
}

int RefHoverPopupWidthCap(RefHoverState* s, int minWidth) {
    if (!s || !s->win || s->win->canvasRc.dx <= 0) {
        return minWidth;
    }
    int canvasWidth = s->win->canvasRc.dx;
    int width = std::max(canvasWidth * 95 / 100, minWidth);
    return std::min(width, canvasWidth);
}

// ng: orig's kRefHoverTimerID / kRefHoverHideTimerID, counted down by the
// shell's tick (RefHoverOnCanvasTimer in orig's RefHoverCanvas.cpp)
void RefHoverTick(MainWindow* win, int elapsedMs) {
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s) {
        return;
    }
    if (s->hideLeftMs >= 0) {
        s->hideLeftMs -= elapsedMs;
        if (s->hideLeftMs <= 0) {
            RefHoverOnHideTimer(s);
        }
    }
    if (s->showLeftMs < 0) {
        return;
    }
    s->showLeftMs -= elapsedMs;
    if (s->showLeftMs > 0) {
        return;
    }
    s->showLeftMs = -1;
    DisplayModel* dm = win->AsFixed();
    int destPage = s->pending.destPage;
    if (!dm || !dm->ValidPageNo(destPage)) {
        RefHoverHide(s);
        return;
    }
    RefHoverOnTimer(s, dm->GetEngine(), dm->GetZoomReal(destPage));
}

// Citation hover popup state. "show" opens the popup for the link at the
// frame point, because a test cursor cannot hold a hover. tests/issue-6252.ts.
TempStr RefHoverResultTemp(Str action, int x, int y, int* exitCodeOut) {
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (str::Eq(action, StrL("show"))) {
        if (!win || !dm) {
            if (exitCodeOut) {
                *exitCodeOut = 2;
            }
            return str::DupTemp(StrL("NOTREADY no-document"));
        }
        Point doc;
        if (!FrameClientToDoc(win, x, y, &doc)) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            return fmt("ERROR no-link x=%d y=%d", x, y);
        }
        int srcPage = 0;
        IPageElement* el = dm->GetElementAtPos(doc, &srcPage);
        // TryLock lookup misses while a render holds the page lock; wait instead
        if (!el && dm->ValidPageNo(srcPage)) {
            el = EngineMupdfElementAtPosWait(dm->GetEngine(), srcPage, dm->CvtFromScreen(doc, srcPage));
        }
        RefHoverOnCanvasMouseMove(win->refHover, win, win->ctrl, win->linkHandler, dm, doc.x, doc.y, el, srcPage, 0);
        RefHoverState* s = win->refHover;
        if (!s || (s->showLeftMs < 0 && !s->visible)) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            return fmt("ERROR no-link x=%d y=%d", x, y);
        }
        if (s->showLeftMs >= 0) {
            RefHoverTick(win, 1000);
        }
    }
    // frame point of the current page's x-th EPUB footnote link, to click
    if (str::Eq(action, StrL("noteref")) && dm) {
        float k = CanvasScale(win);
        if (k <= 0.f) {
            k = 1.f;
        }
        int pageNo = dm->CurrentPageNo();
        int n = 0;
        for (IPageElement* el : dm->GetEngine()->GetElements(pageNo)) {
            IPageDestination* dest = el && el->Is(kindPageElementDest) ? el->AsLink() : nullptr;
            if (!dest || !dest->isNoteRef) {
                continue;
            }
            // skip the zero-width link mupdf adds at the <a>'s anchor
            RectF r = el->GetRect();
            if (r.dx <= 0.f || n++ < x) {
                continue;
            }
            Point pt = dm->CvtToScreen(pageNo, PointF(r.x + (r.dx / 2), r.y + (r.dy / 2)));
            int fx = (int)((float)win->canvasRc.x / k) + pt.x;
            int fy = (int)((float)win->canvasRc.y / k) + pt.y;
            return fmt("OK noteref=%d,%d page=%d scale=%.4f", fx, fy, pageNo, (double)k);
        }
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return fmt("ERROR no-noteref page=%d", pageNo);
    }
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s || !s->visible) {
        return str::DupTemp(StrL("OK visible=0"));
    }
    auto& d = s->displayed;
    // center of the popup in frame pixels, so a test wheel can land on it
    float k = CanvasScale(win);
    if (k <= 0.f) {
        k = 1.f;
    }
    int popX = (int)((float)win->canvasRc.x / k + (float)s->popupRc.x + (float)s->popupRc.dx / 2.f);
    int popY = (int)((float)win->canvasRc.y / k + (float)s->popupRc.y + (float)s->popupRc.dy / 2.f);
    return fmt("OK visible=1 page=%d y=%d zoom=%d pop=%d,%d scale=%.4f x=%d", d.destPage, (int)d.region.y,
               (int)(d.userZoom * 100), popX, popY, (double)k, (int)d.region.x);
}
