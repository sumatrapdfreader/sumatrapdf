/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"
#include "RefHoverPopupCommon.h"

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

IPageDestination* LaunchLinkAtPopupPt(RefHoverState* s, int clientX, int clientY) {
    PointF pagePt;
    if (!PopupClientToPagePt(s, clientX, clientY, pagePt)) {
        return nullptr;
    }
    return LaunchLinkAtPagePt(s, pagePt);
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
