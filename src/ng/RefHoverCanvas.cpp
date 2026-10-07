/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Canvas integration for citation hover: keeps RefHover orchestration out of
// DocCanvas.cpp so the event handler stays wiring-only.

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "RefHover.h"

// Canvas wiring entry points (RefHoverCanvas.cpp) — keep DocCanvas.cpp thin.
bool RefHoverIsInternalLink(IPageElement* el, DisplayModel* dm) {
    if (!el || !el->Is(kindPageElementDest)) {
        return false;
    }
    IPageDestination* dest = el->AsLink();
    if (!dest) {
        return false;
    }
    if (IsLaunchLinkKind(dest->GetKind())) {
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

// ng: orig converts the page rect to screen coordinates because the popup is a
// top-level window. Here the popup lives inside the canvas, so the page rect
// is already in the coordinate space the popup is placed in.
static Rect PageRectOnCanvas(DisplayModel* dm, int srcPage) {
    PageInfo* pi = (srcPage > 0) ? dm->GetPageInfo(srcPage) : nullptr;
    if (!pi || pi->pageOnScreen.IsEmpty()) {
        return Rect{};
    }
    return pi->pageOnScreen;
}

void RefHoverOnCanvasMouseMove(RefHoverState*& s, MainWindow* win, DocController* ctrl, ILinkHandler* linkHandler,
                               DisplayModel* dm, int x, int y, IPageElement* el, int srcPageNo, int hoverDelayMs) {
    if (hoverDelayMs < 0) {
        if (s) {
            RefHoverHide(s);
        }
        return;
    }

    if (!s) {
        s = RefHoverCreate(win);
    }
    if (!s) {
        return;
    }
    s->ctrl = ctrl;
    s->linkHandler = linkHandler;

    bool scheduled = false;
    if (RefHoverIsInternalLink(el, dm)) {
        IPageDestination* dest = el->AsLink();
        int destPage = PageDestGetPageNo(dest);
        RectF destPt = PageDestGetDestPoint(dest);
        float destZoom = dest->GetZoom();
        Point screenPt = Point(x, y);
        int srcPage = el->GetPageNo();
        RectF srcRect = el->GetRect();
        Rect pageScreenRect = PageRectOnCanvas(dm, srcPage);
        RefHoverSchedule(s, hoverDelayMs, screenPt, destPage, destPt.x, destPt.y, destZoom, srcPage, srcRect,
                         pageScreenRect);
        scheduled = true;
    } else if (srcPageNo > 0) {
        PointF pagePtF = dm->CvtFromScreen({x, y}, srcPageNo);
        Point pagePt{(int)pagePtF.x, (int)pagePtF.y};
        int destPage = -1;
        float destX = -1.f, destY = -1.f;
        RectF citationSrcRect{};
        if (RefHoverTryPlainText(s, dm->GetEngine(), srcPageNo, pagePt, destPage, destX, destY, citationSrcRect)) {
            Point screenPt = Point(x, y);
            Rect pageScreenRect = PageRectOnCanvas(dm, srcPageNo);
            RefHoverSchedule(s, hoverDelayMs, screenPt, destPage, destX, destY, 0.f, srcPageNo, citationSrcRect,
                             pageScreenRect);
            scheduled = true;
        }
    }
    if (!scheduled) {
        RefHoverScheduleHide(s, hoverDelayMs);
    }
}

void RefHoverOnCanvasMouseLeave(RefHoverState* s, int hoverDelayMs) {
    if (!s) {
        return;
    }
    RefHoverScheduleHide(s, hoverDelayMs);
}

void RefHoverOnCanvasLeftButtonDown(RefHoverState* s) {
    RefHoverHide(s);
}
