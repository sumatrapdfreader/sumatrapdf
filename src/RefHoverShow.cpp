/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHoverDetect.h"
#include "RefHoverInternal.h"
#include "RefHoverText.h"

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
            if (destX < 0.f) {
                destX = 0.f;
            }
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
        Rect* coords = nullptr;
        int textLen = 0;
        Str textUtf8 = engine->GetTextForPage(destPage, &textLen, &coords);
        TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
        WCHAR* cleanText = nullptr;
        Rect* cleanCoords = nullptr;
        Rect* normCoords = coords;
        if (coords && len(text) > 0) {
            // Strip the page watermark on the raw glyphs first (its true height
            // is only visible pre-normalization), then normalize the survivors
            // so the detectors below see clean, baseline-flattened text.
            cleanText = AllocArray<WCHAR>(textLen);
            cleanCoords = AllocArray<Rect>(textLen);
            int cleanLen = StripWatermarkGlyphs(text, coords, cleanText, cleanCoords);
            text = WStr(cleanText, cleanLen);
            normCoords = AllocArray<Rect>(cleanLen);
            NormalizeGlyphLines(cleanCoords, normCoords, cleanLen);
        }
        region = DetectEquationBox(text, normCoords, mediabox, destX, destY);
        if (region.dx <= 0.f || region.dy <= 0.f) {
            region = DetectEntryBox(text, normCoords, mediabox, destX, destY, &continuation);
        }
        if (normCoords != coords) {
            free(normCoords);
        }
        free(cleanText);
        free(cleanCoords);
    }
    bool hasContinuation = continuation.dx > 0.f && continuation.dy > 0.f;
    s->displayed.userZoom = 1.f;
    float baseZoom = useLinkZoom ? linkZoom : ((pageZoom > 0.f) ? pageZoom : kRefHoverRenderZoom);

    int popupWCap = DpiScale(s->hwndPopup, kRefHoverMaxPopupWidth);
    {
        POINT mp = {s->pending.screenPt.x, s->pending.screenPt.y};
        HMONITOR hmon = MonitorFromPoint(mp, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hmon, &mi)) {
            int monW = mi.rcWork.right - mi.rcWork.left;
            int dyn = monW * 95 / 100;
            if (dyn > popupWCap) {
                popupWCap = dyn;
            }
        }
    }
    // Combined content extent (region stacked above continuation, if any) used
    // for sizing below; req.region itself stays just the primary crop.
    float contentDy = region.dy + (hasContinuation ? continuation.dy : 0.f);
    float contentDx = hasContinuation && continuation.dx > region.dx ? continuation.dx : region.dx;

    int popupHCap;
    int cursorPad = DpiScale(s->hwndPopup, kRefHoverCursorPad);
    if (contentDy > 250.f && s->pending.pageScreenRect.dy > 0) {
        Rect pr = s->pending.pageScreenRect;
        int curY = s->pending.screenPt.y;
        int spaceAbove = curY - pr.y - cursorPad;
        int spaceBelow = (pr.y + pr.dy) - curY - cursorPad;
        int maxSpace = (spaceAbove > spaceBelow) ? spaceAbove : spaceBelow;
        if (maxSpace < 0) {
            maxSpace = 0;
        }
        int pageBased = pr.dy * 75 / 100;
        popupHCap = (pageBased > maxSpace) ? pageBased : maxSpace;
    } else {
        popupHCap = DpiScale(s->hwndPopup, kRefHoverMaxPopupHeight);
        if (s->pending.pageScreenRect.dy > 0) {
            int pageBased = s->pending.pageScreenRect.dy * 45 / 100;
            if (pageBased < popupHCap) {
                popupHCap = pageBased;
            }
        }
    }
    int border = DpiScale(s->hwndPopup, kRefHoverBorder);
    float availH = (float)(popupHCap - 2 * border);
    float availW = (float)(popupWCap - 2 * border);
    if (useLinkZoom) {
        float wantW = availW / baseZoom;
        float wantH = availH / baseZoom;
        float maxW = mediabox.dx - region.x;
        float maxH = mediabox.dy - region.y;
        if (wantW > maxW) {
            wantW = maxW;
        }
        if (wantH > maxH) {
            wantH = maxH;
        }
        if (wantW < 1.f) {
            wantW = 1.f;
        }
        if (wantH < 1.f) {
            wantH = 1.f;
        }
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
    if (baseZoom < kRefHoverMinUserZoom) {
        baseZoom = kRefHoverMinUserZoom;
    }
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
    req.destXRaw = s->pending.destX;
    req.destYRaw = s->pending.destY;
    req.srcPageRaw = s->pending.srcPage;
    req.srcRectRaw = s->pending.srcRect;
    RefHoverRequestRender(s, engine, req);
}
