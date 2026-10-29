/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Canvas integration for citation hover: keeps RefHover orchestration out of
// Canvas.cpp so the WM handler stays wiring-only.

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RefHover.h"
#include "RefHoverPopupCommon.h"

static Rect PageScreenRectToScreen(HWND hwndCanvas, DisplayModel* dm, int srcPage) {
    Rect pageScreenRect{};
    PageInfo* pi = (srcPage > 0) ? dm->GetPageInfo(srcPage) : nullptr;
    if (pi && !pi->pageOnScreen.IsEmpty()) {
        pageScreenRect = pi->pageOnScreen;
        Point topLeft = HwndClientToScreen(hwndCanvas, Point(pageScreenRect.x, pageScreenRect.y));
        pageScreenRect.x = topLeft.x;
        pageScreenRect.y = topLeft.y;
    }
    return pageScreenRect;
}

// schedules the popup for el, an internal link at canvas point (x, y)
bool RefHoverScheduleLink(RefHoverState* s, HWND hwndCanvas, DisplayModel* dm, int x, int y, IPageElement* el,
                          int delayMs) {
    if (!RefHoverIsInternalLink(el, dm)) {
        return false;
    }
    IPageDestination* dest = el->AsLink();
    // chaptered doc (EPUB): the page is known once the dest is resolved
    if (PageDestGetPageNo(dest) < 1) {
        dm->ResolveDest(dest);
    }
    int destPage = PageDestGetPageNo(dest);
    if (!dm->ValidPageNo(destPage)) {
        return false;
    }
    RectF destPt = PageDestGetDestPoint(dest);
    // a footnote's region is detected from its text, not the rest of the page
    float destZoom = dest->isNoteRef ? 0.f : dest->GetZoom();
    Point screenPt = HwndClientToScreen(hwndCanvas, Point(x, y));
    int srcPage = el->GetPageNo();
    RectF srcRect = el->GetRect();
    Rect pageScreenRect = PageScreenRectToScreen(hwndCanvas, dm, srcPage);
    RefHoverSchedule(s, hwndCanvas, delayMs, screenPt, destPage, destPt.x, destPt.y, destZoom, srcPage, srcRect,
                     pageScreenRect);
    s->pending.noteRef = dest->isNoteRef;
    return true;
}

// Show the popup for a click on an EPUB footnote link right away. False if
// there's nothing to show, so the click follows the link.
bool RefHoverShowNoteRef(RefHoverState*& s, HWND hwndCanvas, DocController* ctrl, ILinkHandler* linkHandler,
                         DisplayModel* dm, int x, int y, IPageElement* el) {
    if (!s) {
        s = RefHoverCreate(hwndCanvas);
    }
    if (!s) {
        return false;
    }
    s->ctrl = ctrl;
    s->linkHandler = linkHandler;
    if (!RefHoverScheduleLink(s, hwndCanvas, dm, x, y, el, 0)) {
        return false;
    }
    // tracking a cursor that isn't over the canvas would hide the popup at once
    if (HwndIsCursorOverWindow(hwndCanvas)) {
        TrackMouseLeave(hwndCanvas);
    }
    return true;
}

// is el the link the shown (or pending) popup is for?
static bool IsPopupLink(RefHoverState* s, IPageElement* el) {
    if (!el || !el->Is(kindPageElementDest)) {
        return false;
    }
    int page = el->GetPageNo();
    RectF rect = el->GetRect();
    bool shown = s->displayed.srcPage == page && s->displayed.srcRect == rect;
    bool pending = s->pending.srcPage == page && s->pending.srcRect == rect;
    return shown || pending;
}

void RefHoverOnCanvasMouseMove(RefHoverState*& s, HWND hwndCanvas, DocController* ctrl, ILinkHandler* linkHandler,
                               DisplayModel* dm, int x, int y, IPageElement* el, int srcPageNo, int hoverDelayMs) {
    if (hoverDelayMs < 0) {
        // no hover popups, so a shown one is from a click on an EPUB footnote
        // link: it stays while the cursor is on that link or on the popup
        if (!s) {
            return;
        }
        if (IsPopupLink(s, el)) {
            KillTimer(hwndCanvas, kRefHoverHideTimerID);
            return;
        }
        RefHoverScheduleHide(s, hwndCanvas, 0);
        return;
    }

    if (!s) {
        s = RefHoverCreate(hwndCanvas);
    }
    if (!s) {
        return;
    }
    s->ctrl = ctrl;
    s->linkHandler = linkHandler;

    bool scheduled = false;
    if (RefHoverScheduleLink(s, hwndCanvas, dm, x, y, el, hoverDelayMs)) {
        TrackMouseLeave(hwndCanvas);
        scheduled = true;
    } else if (srcPageNo > 0) {
        PointF pagePtF = dm->CvtFromScreen({x, y}, srcPageNo);
        Point pagePt{(int)pagePtF.x, (int)pagePtF.y};
        int destPage = -1;
        float destX = -1.f, destY = -1.f;
        RectF citationSrcRect{};
        if (RefHoverTryPlainText(s, dm->GetEngine(), srcPageNo, pagePt, destPage, destX, destY, citationSrcRect)) {
            TrackMouseLeave(hwndCanvas);
            Point screenPt = HwndClientToScreen(hwndCanvas, Point(x, y));
            Rect pageScreenRect = PageScreenRectToScreen(hwndCanvas, dm, srcPageNo);
            RefHoverSchedule(s, hwndCanvas, hoverDelayMs, screenPt, destPage, destX, destY, 0.f, srcPageNo,
                             citationSrcRect, pageScreenRect);
            scheduled = true;
        }
    }
    if (!scheduled) {
        RefHoverScheduleHide(s, hwndCanvas, hoverDelayMs);
    }
}

void RefHoverOnCanvasMouseLeave(RefHoverState* s, HWND hwndCanvas, int hoverDelayMs) {
    if (!s) {
        return;
    }
    RefHoverScheduleHide(s, hwndCanvas, hoverDelayMs);
}

void RefHoverOnCanvasLeftButtonDown(RefHoverState* s, HWND hwndCanvas) {
    RefHoverHide(s, hwndCanvas);
}

bool RefHoverOnCanvasTimer(RefHoverState* s, HWND hwndCanvas, DisplayModel* dm, UINT_PTR timerId) {
    if (timerId == kRefHoverTimerID) {
        int destPage = s ? s->pending.destPage : -1;
        if (!dm || !dm->ValidPageNo(destPage)) {
            KillTimer(hwndCanvas, kRefHoverTimerID);
            RefHoverHide(s, hwndCanvas);
            return true;
        }
        float pageZoom = dm->GetZoomReal(destPage);
        KillTimer(hwndCanvas, kRefHoverTimerID);
        RefHoverOnTimer(s, dm->GetEngine(), pageZoom);
        return true;
    }
    if (timerId == kRefHoverHideTimerID) {
        RefHoverOnHideTimer(s, hwndCanvas);
        return true;
    }
    return false;
}
