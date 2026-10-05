/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — lifecycle and scheduling. Popup UI, async
// render, region detection, canvas wiring, and plain-text lookup live in
// sibling RefHover*.cpp files.

#include "base/Base.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "RefHover.h"

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
static constexpr int kRefHoverHideMinMs = 250;

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
