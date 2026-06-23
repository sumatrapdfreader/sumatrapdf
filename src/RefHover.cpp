/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — lifecycle and scheduling. Popup UI, async
// render, region detection, canvas wiring, and plain-text lookup live in
// sibling RefHover*.cpp files.

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

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
    delete s->bmp;
    s->bmp = nullptr;
    if (s->hitEngine) {
        s->hitEngine->Release();
        s->hitEngine = nullptr;
    }
    RefHoverFreeLookupCache(s);
    delete s;
}

void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, int delayMs, Point screenPt, int destPage, float destX,
                      float destY, float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s || delayMs < 0) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    KillTimer(hwndCanvas, kRefHoverHideTimerID);

    bool sameSrc = s->displayed.srcPage == srcPage && s->displayed.srcRect == srcRect;
    if (IsWindowVisible(s->hwndPopup) && s->displayed.destPage == destPage && s->displayed.destX == destX &&
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
    if (IsWindowVisible(s->hwndPopup)) {
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
    if (s->hwndPopup && IsWindowVisible(s->hwndPopup)) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        s->displayed.destPage = -1;
    }
}

static constexpr UINT kRefHoverHidePollMs = 150;
static constexpr int kRefHoverHideMinMs = 250;

void RefHoverScheduleHide(RefHoverState* s, HWND hwndCanvas, int delayMs) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (!s->hwndPopup || !IsWindowVisible(s->hwndPopup)) {
        s->displayed.destPage = -1;
        return;
    }
    if (delayMs < kRefHoverHideMinMs) {
        delayMs = kRefHoverHideMinMs;
    }
    SetTimer(hwndCanvas, kRefHoverHideTimerID, (UINT)delayMs, nullptr);
}

void RefHoverOnHideTimer(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverHideTimerID);
    if (!s->hwndPopup || !IsWindowVisible(s->hwndPopup)) {
        return;
    }
    POINT pt;
    if (GetCursorPos(&pt)) {
        if (WindowFromPoint(pt) == s->hwndPopup) {
            SetTimer(hwndCanvas, kRefHoverHideTimerID, kRefHoverHidePollMs, nullptr);
            return;
        }
    }
    ShowWindow(s->hwndPopup, SW_HIDE);
    s->displayed.destPage = -1;
}

void RefHoverHandlePopupClick(RefHoverState* s, IPageDestination* dest) {
    if (!s || !dest || !s->ctrl) {
        return;
    }
    RefHoverHide(s, s->hwndCanvas);
    s->ctrl->HandleLink(dest, s->linkHandler);
}