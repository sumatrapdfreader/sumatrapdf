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

// Canvas wiring entry points (RefHoverCanvas.cpp) — keep Canvas.cpp thin.
bool RefHoverIsInternalLink(IPageElement* el, DisplayModel* dm) {
    if (!el || !el->Is(kindPageElementDest)) {
        return false;
    }
    IPageDestination* dest = el->AsLink();
    if (!dest) {
        return false;
    }
    Kind k = dest->GetKind();
    if (k == kindDestinationLaunchURL || k == kindDestinationLaunchFile) {
        return false;
    }
    int destPage = PageDestGetPageNo(dest);
    if (dm && dm->ValidPageNo(destPage)) {
        return true;
    }
    // chaptered doc: destPage stays -1 until clicked, but a dest with a
    // chapter to resolve lazily is still an internal link
    return dm && destPage < 1 && dest->loc.chapter >= 1;
}

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

void RefHoverOnCanvasMouseMove(RefHoverState*& s, HWND hwndCanvas, DocController* ctrl, ILinkHandler* linkHandler,
                               DisplayModel* dm, int x, int y, IPageElement* el, int srcPageNo, int hoverDelayMs) {
    if (hoverDelayMs < 0) {
        if (s) {
            RefHoverHide(s, hwndCanvas);
        }
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
    if (RefHoverIsInternalLink(el, dm)) {
        TrackMouseLeave(hwndCanvas);
        IPageDestination* dest = el->AsLink();
        int destPage = PageDestGetPageNo(dest);
        RectF destPt = PageDestGetDestPoint(dest);
        float destZoom = PageDestGetZoom(dest);
        Point screenPt = HwndClientToScreen(hwndCanvas, Point(x, y));
        int srcPage = el->GetPageNo();
        RectF srcRect = el->GetRect();
        Rect pageScreenRect = PageScreenRectToScreen(hwndCanvas, dm, srcPage);
        RefHoverSchedule(s, hwndCanvas, hoverDelayMs, screenPt, destPage, destPt.x, destPt.y, destZoom, srcPage,
                         srcRect, pageScreenRect);
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
        RefHoverOnTimer(s, hwndCanvas, dm->GetEngine(), pageZoom);
        return true;
    }
    if (timerId == kRefHoverHideTimerID) {
        RefHoverOnHideTimer(s, hwndCanvas);
        return true;
    }
    return false;
}
