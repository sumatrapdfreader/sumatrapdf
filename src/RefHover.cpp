/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — lifecycle and scheduling. Popup UI, async
// render, region detection, canvas wiring, and plain-text lookup live in
// sibling RefHover*.cpp files.

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHoverInternal.h"
#include "RefHoverText.h"

RefHoverState* RefHoverCreate(HWND hwndCanvas) {
    auto* s = new RefHoverState();
    if (!RefHoverPopupCreate(s, hwndCanvas)) {
        delete s;
        return nullptr;
    }
    RefHoverRegisterLiveState(s);
    return s;
}

void RefHoverDestroy(RefHoverState* s) {
    if (!s) {
        return;
    }
    RefHoverUnregisterLiveState(s);
    RefHoverDropQueuedRender(s);
    if (s->hwndPopup) {
        DestroyWindow(s->hwndPopup);
        s->hwndPopup = nullptr;
    }
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
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, int delayMs, Point screenPt, int destPage, float destX,
                      float destY, float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s || delayMs < 0) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    KillTimer(hwndCanvas, kRefHoverHideTimerID);

    bool sameSrc = s->displayed.srcPage == srcPage && s->displayed.srcRect == srcRect;
    if (HwndIsVisible(s->hwndPopup) && s->displayed.destPage == destPage && s->displayed.destX == destX &&
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
    if (HwndIsVisible(s->hwndPopup)) {
        delayMs = 0;
    }
    SetTimer(hwndCanvas, kRefHoverTimerID, (UINT)delayMs, nullptr);
}

void RefHoverHide(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    KillTimer(hwndCanvas, kRefHoverHideTimerID);
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (s->hwndPopup && HwndIsVisible(s->hwndPopup)) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        s->displayed.destPage = -1;
    }
}

static constexpr UINT kRefHoverHidePollMs = 150;
static constexpr int kRefHoverHideMinMs = 250;

// Like RefHoverHide but deferred: cancels any pending show immediately, then
// hides the visible popup after delayMs. While the timer is pending, moving
// the cursor onto the popup (e.g. to click a DOI link inside it) keeps it
// alive. Lets the cursor cross the gap between the link and the popup without
// the popup vanishing. Cancelled by a new RefHoverSchedule / RefHoverHide.
void RefHoverScheduleHide(RefHoverState* s, HWND hwndCanvas, int delayMs) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (!s->hwndPopup || !HwndIsVisible(s->hwndPopup)) {
        s->displayed.destPage = -1;
        return;
    }
    delayMs = std::max(delayMs, kRefHoverHideMinMs);
    SetTimer(hwndCanvas, kRefHoverHideTimerID, (UINT)delayMs, nullptr);
}

// Fired by kRefHoverHideTimerID: hides the popup unless the cursor is now
// over it (in which case it re-arms and keeps the popup up).
void RefHoverOnHideTimer(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverHideTimerID);
    if (!s->hwndPopup || !HwndIsVisible(s->hwndPopup)) {
        return;
    }
    POINT pt;
    if (GetCursorPos(&pt)) {
        if (HwndWindowFromPoint(Point(pt.x, pt.y)) == s->hwndPopup) {
            SetTimer(hwndCanvas, kRefHoverHideTimerID, kRefHoverHidePollMs, nullptr);
            return;
        }
    }
    ShowWindow(s->hwndPopup, SW_HIDE);
    s->displayed.destPage = -1;
}

// Open a launch link (external URL / file) hit-tested inside the popup.
void RefHoverHandlePopupClick(RefHoverState* s, IPageDestination* dest) {
    if (!s || !dest || !s->ctrl) {
        return;
    }
    RefHoverHide(s, s->hwndCanvas);
    s->ctrl->HandleLink(dest, s->linkHandler);
}
