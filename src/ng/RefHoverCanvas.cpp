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
#include "RefHoverPopupCommon.h"

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

static bool ScheduleLink(RefHoverState* s, DisplayModel* dm, int x, int y, IPageElement* el, int delayMs) {
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
    Point screenPt = Point(x, y);
    int srcPage = el->GetPageNo();
    RectF srcRect = el->GetRect();
    Rect pageScreenRect = PageRectOnCanvas(dm, srcPage);
    RefHoverSchedule(s, delayMs, screenPt, destPage, destPt.x, destPt.y, destZoom, srcPage, srcRect, pageScreenRect);
    s->pending.noteRef = dest->isNoteRef;
    return true;
}

// Show the popup for a click on an EPUB footnote link right away. False if
// there's nothing to show, so the click follows the link.
bool RefHoverShowNoteRef(RefHoverState*& s, MainWindow* win, DocController* ctrl, ILinkHandler* linkHandler,
                         DisplayModel* dm, int x, int y, IPageElement* el) {
    if (!s) {
        s = RefHoverCreate(win);
    }
    if (!s) {
        return false;
    }
    s->ctrl = ctrl;
    s->linkHandler = linkHandler;
    return ScheduleLink(s, dm, x, y, el, 0);
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

void RefHoverOnCanvasMouseMove(RefHoverState*& s, MainWindow* win, DocController* ctrl, ILinkHandler* linkHandler,
                               DisplayModel* dm, int x, int y, IPageElement* el, int srcPageNo, int hoverDelayMs) {
    if (hoverDelayMs < 0) {
        // no hover popups, so a shown one is from a click on an EPUB footnote
        // link: it stays while the cursor is on that link or on the popup. One
        // not shown yet stays too: Windows sends a move without the mouse
        // moving when a window appears under the cursor
        if (!s || !s->visible) {
            return;
        }
        if (IsPopupLink(s, el)) {
            s->hideLeftMs = -1;
            return;
        }
        RefHoverScheduleHide(s, 0);
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
    if (ScheduleLink(s, dm, x, y, el, hoverDelayMs)) {
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
