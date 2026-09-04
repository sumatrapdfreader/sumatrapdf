/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"

enum class MissingEntry {
    Empty,
    Landscape,
};

static RectF DetectRegion(EngineBase* engine, int pageNo, RectF mediabox, float destX, float destY, RectF* continuation,
                          MissingEntry missingEntry) {
    AutoArenaSavepoint tempScope;

    Rect* coords = nullptr;
    int textLen = 0;
    Str textUtf8 = engine->GetTextForPage(pageNo, &textLen, &coords);
    TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
    WCHAR* cleanText = nullptr;
    Rect* cleanCoords = nullptr;
    Rect* normCoords = coords;
    if (coords && len(text) > 0) {
        cleanText = AllocArrayTemp<WCHAR>(textLen);
        cleanCoords = AllocArrayTemp<Rect>(textLen);
        int cleanLen = StripWatermarkGlyphs(text, coords, cleanText, cleanCoords);
        text = WStr(cleanText, cleanLen);
        normCoords = AllocArrayTemp<Rect>(cleanLen);
        NormalizeGlyphLines(cleanCoords, normCoords, cleanLen);
    }

    RectF region = DetectEquationBox(text, normCoords, mediabox, destX, destY);
    if (region.dx <= 0.f || region.dy <= 0.f) {
        region = DetectEntryBox(text, normCoords, mediabox, destX, destY, continuation);
    }
    if ((region.dx <= 0.f || region.dy <= 0.f) && missingEntry == MissingEntry::Landscape) {
        region = LandscapeBox(mediabox, destX, destY, text, normCoords);
    }
    return region;
}

// pageZoom is the destination page's current display zoom (px-per-pt) —
// used as the initial render zoom so popup text height matches the page.
void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom) {
    KillTimer(hwndCanvas, kRefHoverTimerID);
    if (!s || !engine || s->pending.destPage <= 0) {
        return;
    }
    if (s->hitEngine != engine) {
        if (s->hitEngine) {
            s->hitEngine->Release();
        }
        s->hitEngine = engine;
        engine->AddRef();
    }
    int destPage = s->pending.destPage;
    float destX = s->pending.destX;
    float destY = s->pending.destY;

    RectF mediabox = engine->PageMediabox(destPage);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return;
    }
    if (destY <= 0.f || destY >= mediabox.dy - 1.f) {
        destY = 0.f;
        float resolved = RefHoverResolveDestYFromSourceText(engine, s->pending.srcPage, s->pending.srcRect, destPage);
        if (resolved >= 0.f) {
            destY = resolved;
            destX = std::max(destX, 0.f);
        }
    }

    float linkZoom = s->pending.destZoom;
    bool useLinkZoom = (linkZoom > 0.f);
    if (useLinkZoom && pageZoom > linkZoom) {
        linkZoom = pageZoom;
    }

    RectF region;
    // Set when a bracket-style entry wraps across a 2-column page break (e.g.
    // "[63]"): a second crop, stitched below `region` in the delivered
    // bitmap. Empty (dx/dy <= 0) otherwise.
    RectF continuation{};
    if (useLinkZoom) {
        region = RectF{0.f, destY, mediabox.dx, mediabox.dy - destY};
    } else {
        region = DetectRegion(engine, destPage, mediabox, destX, destY, &continuation, MissingEntry::Empty);
        bool regionEmpty = region.dx <= 0.f || region.dy <= 0.f;
        bool searchNext = regionEmpty || ShouldSearchNextPage(mediabox, destY);
        if (searchNext && !regionEmpty) {
            float currentY = RefHoverResolveDestYFromSourceText(engine, s->pending.srcPage, s->pending.srcRect,
                                                                destPage, RefHoverTextMatch::Best);
            searchNext = currentY < 0.f;
        }

        float nextY = -1.f;
        if (searchNext) {
            int nextPage = destPage + 1;
            if (nextPage <= engine->PageCount()) {
                nextY = RefHoverResolveDestYFromSourceText(engine, s->pending.srcPage, s->pending.srcRect, nextPage,
                                                           RefHoverTextMatch::Best);
            }
            if (nextY >= 0.f) {
                destPage = nextPage;
                destY = nextY;
                destX = std::max(destX, 0.f);
                mediabox = engine->PageMediabox(destPage);
                region = DetectRegion(engine, destPage, mediabox, destX, destY, &continuation, MissingEntry::Landscape);
            }
        }
        if (regionEmpty && nextY < 0.f) {
            region = DetectRegion(engine, destPage, mediabox, destX, destY, &continuation, MissingEntry::Landscape);
        }
    }
    bool hasContinuation = continuation.dx > 0.f && continuation.dy > 0.f;
    s->displayed.userZoom = 1.f;
    float baseZoom = kRefHoverRenderZoom;
    if (useLinkZoom) {
        baseZoom = linkZoom;
    } else if (pageZoom > 0.f) {
        baseZoom = pageZoom;
    }

    int popupWCap = DpiScale(kRefHoverMaxPopupWidth);
    {
        POINT mp = {s->pending.screenPt.x, s->pending.screenPt.y};
        HMONITOR hmon = MonitorFromPoint(mp, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hmon, &mi)) {
            int monW = mi.rcWork.right - mi.rcWork.left;
            int dyn = monW * 95 / 100;
            popupWCap = std::max(dyn, popupWCap);
        }
    }
    // Combined content extent (region stacked above continuation, if any) used
    // for sizing below; req.region itself stays just the primary crop.
    float contentDy = region.dy + (hasContinuation ? continuation.dy : 0.f);
    float contentDx = hasContinuation && continuation.dx > region.dx ? continuation.dx : region.dx;

    int popupHCap;
    int cursorPad = DpiScale(kRefHoverCursorPad);
    if (contentDy > 250.f && s->pending.pageScreenRect.dy > 0) {
        Rect pr = s->pending.pageScreenRect;
        int curY = s->pending.screenPt.y;
        int spaceAbove = curY - pr.y - cursorPad;
        int spaceBelow = (pr.y + pr.dy) - curY - cursorPad;
        int maxSpace = (spaceAbove > spaceBelow) ? spaceAbove : spaceBelow;
        maxSpace = std::max(maxSpace, 0);
        int pageBased = pr.dy * 75 / 100;
        popupHCap = (pageBased > maxSpace) ? pageBased : maxSpace;
    } else {
        popupHCap = DpiScale(kRefHoverMaxPopupHeight);
        if (s->pending.pageScreenRect.dy > 0) {
            int pageBased = s->pending.pageScreenRect.dy * 45 / 100;
            popupHCap = std::min(pageBased, popupHCap);
        }
    }
    int border = DpiScale(kRefHoverBorder);
    float availH = (float)(popupHCap - (2 * border));
    float availW = (float)(popupWCap - (2 * border));
    if (useLinkZoom) {
        float wantW = availW / baseZoom;
        float wantH = availH / baseZoom;
        float maxW = mediabox.dx - region.x;
        float maxH = mediabox.dy - region.y;
        wantW = std::min(wantW, maxW);
        wantH = std::min(wantH, maxH);
        wantW = std::max(wantW, 1.f);
        wantH = std::max(wantH, 1.f);
        region.dx = wantW;
        region.dy = wantH;
    } else {
        if (contentDy > 0.f && contentDy * baseZoom > availH) {
            baseZoom = availH / contentDy;
        }
        if (contentDx > 0.f && contentDx * baseZoom > availW) {
            baseZoom = availW / contentDx;
        }
    }
    baseZoom = std::max(baseZoom, kRefHoverMinUserZoom);
    s->displayed.baseZoom = baseZoom;

    RefHoverState::RenderRequest req;
    req.pageNo = destPage;
    req.zoom = s->displayed.baseZoom * s->displayed.userZoom;
    req.region = region;
    if (hasContinuation) {
        req.continuationRegion = continuation;
    }
    req.showPopup = true;
    req.screenPt = s->pending.screenPt;
    req.destPageRaw = s->pending.destPage;
    req.destXRaw = s->pending.destX;
    req.destYRaw = s->pending.destY;
    req.srcPageRaw = s->pending.srcPage;
    req.srcRectRaw = s->pending.srcRect;
    RefHoverRequestRender(s, engine, req);
}
