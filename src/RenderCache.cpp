/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "RenderCache.h"
#include "Scopes.h"
#include "WinUtil.h"

/* Define if you want to conserve memory by always freeing cached bitmaps
   for pages not visible. Disabling this might lead to pages not rendering
   due to insufficient (GDI) memory. */
#define CONSERVE_MEMORY

RenderCache::RenderCache()
    : cacheCount(0), requestCount(0), invertColors(false),
      maxTileSize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN))
{
    InitializeCriticalSection(&cacheAccess);
    InitializeCriticalSection(&requestAccess);

    startRendering = CreateEvent(NULL, FALSE, FALSE, NULL);
    renderThread = CreateThread(NULL, 0, RenderCacheThread, this, 0, 0);
    assert(NULL != renderThread);
}

RenderCache::~RenderCache()
{
    EnterCriticalSection(&requestAccess);
    EnterCriticalSection(&cacheAccess);

    CloseHandle(renderThread);
    CloseHandle(startRendering);
    assert(!curReq && 0 == requestCount && 0 == cacheCount);

    LeaveCriticalSection(&cacheAccess);
    DeleteCriticalSection(&cacheAccess);
    LeaveCriticalSection(&requestAccess);
    DeleteCriticalSection(&requestAccess);
}

/* Find a bitmap for a page defined by <dm> and <pageNo> and optionally also
   <rotation> and <zoom> in the cache - call DropCacheEntry when you
   no longer need a found entry. */
BitmapCacheEntry *RenderCache::Find(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition *tile)
{
    ScopedCritSec scope(&cacheAccess);
    BitmapCacheEntry *entry;
    rotation = normalizeRotation(rotation);
    for (int i = 0; i < cacheCount; i++) {
        entry = cache[i];
        if ((dm == entry->dm) && (pageNo == entry->pageNo) && (rotation == entry->rotation) &&
            (INVALID_ZOOM == zoom || zoom == entry->zoom) && (!tile || entry->tile == *tile)) {
            entry->refs++;
            return entry;
        }
    }
    return NULL;
}

bool RenderCache::Exists(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition *tile)
{
    BitmapCacheEntry *entry = Find(dm, pageNo, rotation, zoom, tile);
    if (entry)
        DropCacheEntry(entry);
    return entry != NULL;
}

void RenderCache::DropCacheEntry(BitmapCacheEntry *entry)
{
    ScopedCritSec scope(&cacheAccess);
    assert(entry);
    if (!entry) return;
    if (0 == --entry->refs) {
        delete entry->bitmap;
        free(entry);
    }
}

void RenderCache::Add(PageRenderRequest &req, RenderedBitmap *bitmap)
{
    ScopedCritSec scope(&cacheAccess);
    assert(req.dm);

    req.rotation = normalizeRotation(req.rotation);
    DBG_OUT("RenderCache::Add(pageNo=%d, rotation=%d, zoom=%.2f%%)\n", req.pageNo, req.rotation, req.zoom);
    assert(cacheCount <= MAX_BITMAPS_CACHED);

    /* It's possible there still is a cached bitmap with different zoom/rotation */
    FreePage(req.dm, req.pageNo, &req.tile);

    if (cacheCount >= MAX_BITMAPS_CACHED) {
        // free an invisible page of the same DisplayModel ...
        for (int i = 0; i < cacheCount; i++) {
            if (cache[i]->dm == req.dm && !req.dm->PageVisibleNearby(cache[i]->pageNo)) {
                DropCacheEntry(cache[i]);
                cacheCount--;
                memmove(&cache[i], &cache[i + 1], (cacheCount - i) * sizeof(cache[0]));
                break;
            }
        }
        // ... or just the oldest cached page
        if (cacheCount >= MAX_BITMAPS_CACHED) {
            DropCacheEntry(cache[0]);
            cacheCount--;
            memmove(&cache[0], &cache[1], cacheCount * sizeof(cache[0]));
        }
    }

    // Copy the PageRenderRequest as it will be reused
    BitmapCacheEntry entry = { req.dm, req.pageNo, req.rotation, req.zoom, req.tile, bitmap, 1 };
    cache[cacheCount] = (BitmapCacheEntry *)_memdup(&entry);
    assert(cache[cacheCount]);
    if (!cache[cacheCount])
        delete bitmap;
    else
        cacheCount++;
    // allow rendering engines to free more memory needed for rendering
    if (req.dm->engine)
        req.dm->engine->RunGC();
}

// get the (user) coordinates of a specific tile
static RectD GetTileRect(BaseEngine *engine, int pageNo, int rotation, float zoom, TilePosition tile)
{
    RectD mediabox = engine->PageMediabox(pageNo);

    if (tile.res && tile.res != INVALID_TILE_RES) {
        double width = mediabox.dx / (1 << tile.res);
        mediabox.x += tile.col * width;
        mediabox.dx = width;
        double height = mediabox.dy / (1 << tile.res);
        mediabox.y += ((1 << tile.res) - tile.row - 1) * height;
        mediabox.dy = height;
    }

    RectD pixelbox = engine->Transform(mediabox, pageNo, zoom, rotation);
    pixelbox = pixelbox.Round().Convert<double>();
    mediabox = engine->Transform(pixelbox, pageNo, zoom, rotation, true);

    return mediabox;
}

static RectI GetTileOnScreen(BaseEngine *engine, int pageNo, int rotation, float zoom, TilePosition tile, RectI pageOnScreen)
{
    RectD mediabox = GetTileRect(engine, pageNo, rotation, zoom, tile);
    RectI bbox = engine->Transform(mediabox, pageNo, zoom, rotation).Round();
    bbox.Offset(pageOnScreen.x, pageOnScreen.y);
    return bbox;
}

static bool IsTileVisible(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition tile, float fuzz=0)
{
    if (!dm) return false;
    PageInfo *pageInfo = dm->GetPageInfo(pageNo);
    if (!dm->engine || !pageInfo) return false;
    RectI tileOnScreen = GetTileOnScreen(dm->engine, pageNo, rotation, zoom, tile, pageInfo->pageOnScreen);
    // consider nearby tiles visible depending on the fuzz factor
    tileOnScreen.x -= (int)(tileOnScreen.dx * fuzz * 0.5);
    tileOnScreen.dx = (int)(tileOnScreen.dx * (fuzz + 1));
    tileOnScreen.y -= (int)(tileOnScreen.dy * fuzz * 0.5);
    tileOnScreen.dy = (int)(tileOnScreen.dy * (fuzz + 1));
    RectI screen(PointI(), dm->viewPort.Size());
    return !tileOnScreen.Intersect(screen).IsEmpty();
}

/* Free all bitmaps in the cache that are of a specific page (or all pages
   of the given DisplayModel, or even all invisible pages). Returns TRUE if freed
   at least one item. */
bool RenderCache::FreePage(DisplayModel *dm, int pageNo, TilePosition *tile)
{
    ScopedCritSec scope(&cacheAccess);
    int cacheCountTmp = cacheCount;
    bool freedSomething = false;
    int curPos = 0;

    for (int i = 0; i < cacheCountTmp; i++) {
        BitmapCacheEntry* entry = cache[i];
        bool shouldFree;
        if (dm && pageNo != INVALID_PAGE_NO) {
            // a specific page
            shouldFree = (entry->dm == dm) && (entry->pageNo == pageNo);
            if (tile)
                // a given tile of the page or all tiles not rendered at a given resolution
                // (and at resolution 0 for quick zoom previews)
                shouldFree = shouldFree && (entry->tile == *tile ||
                    tile->row == (USHORT)-1 && entry->tile.res > 0 && entry->tile.res != tile->res);
        } else if (dm) {
            // all pages of this DisplayModel
            shouldFree = (cache[i]->dm == dm);
        } else {
            // all invisible pages resp. page tiles
            shouldFree = !entry->dm->PageVisibleNearby(entry->pageNo);
            if (!shouldFree && entry->tile.res > 1)
                shouldFree = !IsTileVisible(entry->dm, entry->pageNo, entry->rotation,
                                            entry->zoom, entry->tile, 2.0);
        }

        if (shouldFree) {
            if (!freedSomething)
                DBG_OUT("RenderCache::FreePage(%#x, %d) ", dm, pageNo);
            DBG_OUT("freed %d ", entry->pageNo);
            freedSomething = true;
            DropCacheEntry(entry);
            cache[i] = NULL;
            cacheCount--;
        }

        if (curPos != i)
            cache[curPos] = cache[i];
        if (!shouldFree)
            curPos++;
    }

    if (freedSomething)
        DBG_OUT("\n");
    return freedSomething;
}

// keep the cached bitmaps for visible pages to avoid flickering during a reload.
// mark invisible pages as out-of-date to prevent inconsistencies
void RenderCache::KeepForDisplayModel(DisplayModel *oldDm, DisplayModel *newDm)
{
    ScopedCritSec scope(&cacheAccess);
    for (int i = 0; i < cacheCount; i++) {
        if (cache[i]->dm == oldDm && cache[i]->bitmap) {
            if (oldDm->PageVisible(cache[i]->pageNo))
                cache[i]->dm = newDm;
            // make sure that the page is rerendered eventually
            cache[i]->zoom = INVALID_ZOOM;
            cache[i]->bitmap->outOfDate = true;
        }
    }
}

/* Free all bitmaps cached for a given <dm>. Returns TRUE if freed
   at least one item. */
bool RenderCache::FreeForDisplayModel(DisplayModel *dm)
{
    return FreePage(dm);
}

/* Free all bitmaps in the cache that are not visible. Returns TRUE if freed
   at least one item. */
bool RenderCache::FreeNotVisible()
{
    return FreePage();
}

// determine the count of tiles required for a page at a given zoom level
USHORT RenderCache::GetTileRes(DisplayModel *dm, int pageNo)
{
    RectD mediabox = dm->engine->PageMediabox(pageNo);
    RectD pixelbox = dm->engine->Transform(mediabox, pageNo, dm->ZoomReal(), dm->Rotation());

    float factorW = (float)pixelbox.dx / (maxTileSize.dx + 1);
    float factorH = (float)pixelbox.dy / (maxTileSize.dy + 1);
    float factorMax = max(factorW, factorH);

    // use larger tiles when fitting page or width or when a page is smaller
    // than the visible canvas width/height or when rendering pages
    // containing a single image (MuPDF isn't that much faster for rendering
    // individual tiles than for rendering the whole image in a single pass)
    if (dm->ZoomVirtual() == ZOOM_FIT_PAGE || dm->ZoomVirtual() == ZOOM_FIT_WIDTH ||
        pixelbox.dx <= dm->viewPort.dx || pixelbox.dy < dm->viewPort.dy ||
        dm->engine->IsImagePage(pageNo)) {
        factorMax /= 2.0;
    }

    USHORT res = 0;
    if (factorMax > 1.5)
        res = (USHORT)ceilf(log(factorMax) / log(2.0f));
    return res;
}

// reduce the size of tiles in order to hopefully use less memory overall
bool RenderCache::ReduceTileSize()
{
    if (maxTileSize.dx < 200 || maxTileSize.dy < 200)
        return false;

    ScopedCritSec scope1(&requestAccess);
    ScopedCritSec scope2(&cacheAccess);

    if (maxTileSize.dx > maxTileSize.dy)
        (int)maxTileSize.dx /= 2;
    else
        (int)maxTileSize.dy /= 2;

    // invalidate all rendered bitmaps and all requests
    while (cacheCount > 0)
        FreeForDisplayModel(cache[0]->dm);
    while (requestCount > 0)
        ClearQueueForDisplayModel(requests[0].dm);
    if (curReq)
        curReq->abort = true;

    return true;
}

void RenderCache::Render(DisplayModel *dm, int pageNo, RenderingCallback *callback)
{
    TilePosition tile = { GetTileRes(dm, pageNo), 0, 0 };
    Render(dm, pageNo, tile, true, callback);

    // render both tiles of the first row when splitting a page in four
    // (which always happens on larger displays for Fit Width)
    if (tile.res == 1 && !IsRenderQueueFull()) {
        tile.col = 1;
        Render(dm, pageNo, tile, false);
    }
}

/* Render a bitmap for page <pageNo> in <dm>. */
void RenderCache::Render(DisplayModel *dm, int pageNo, TilePosition tile, bool clearQueue, RenderingCallback *callback)
{
    DBG_OUT("RenderCache::Render(pageNo=%d)\n", pageNo);
    assert(dm);

    ScopedCritSec scope(&requestAccess);
    bool ok = false;
    if (!dm || dm->dontRenderFlag) goto Exit;

    int rotation = normalizeRotation(dm->Rotation());
    float zoom = dm->ZoomReal(pageNo);

    if (curReq && (curReq->pageNo == pageNo) && (curReq->dm == dm) && (curReq->tile == tile)) {
        if ((curReq->zoom != zoom) || (curReq->rotation != rotation)) {
            /* Currently rendered page is for the same page but with different zoom
            or rotation, so abort it */
            DBG_OUT("  aborting rendering\n");
            curReq->abort = true;
        } else {
            /* we're already rendering exactly the same page */
            DBG_OUT("  already rendering this page\n");
            goto Exit;
        }
    }

    // clear requests for tiles of different resolution and invisible tiles
    if (clearQueue)
        ClearQueueForDisplayModel(dm, pageNo, &tile);

    for (int i = 0; i < requestCount; i++) {
        PageRenderRequest* req = &(requests[i]);
        if ((req->pageNo == pageNo) && (req->dm == dm) && (req->tile == tile)) {
            if ((req->zoom == zoom) && (req->rotation == rotation)) {
                /* Request with exactly the same parameters already queued for
                   rendering. Move it to the top of the queue so that it'll
                   be rendered faster. */
                PageRenderRequest tmp;
                tmp = requests[requestCount-1];
                requests[requestCount-1] = *req;
                *req = tmp;
                DBG_OUT("  already queued\n");
                goto Exit;
            } else {
                /* There was a request queued for the same page but with different
                   zoom or rotation, so only replace this request */
                DBG_OUT("Replacing request for page %d with new request\n", req->pageNo);
                req->zoom = zoom;
                req->rotation = rotation;

                goto Exit;
            }
        }
    }

    if (Exists(dm, pageNo, rotation, zoom, &tile)) {
        /* This page has already been rendered in the correct dimensions
           and isn't about to be rerendered in different dimensions */
        goto Exit;
    }

    ok = Render(dm, pageNo, rotation, zoom, &tile, NULL, callback);

Exit:
    if (!ok && callback)
        callback->Callback();
}

void RenderCache::Render(DisplayModel *dm, int pageNo, int rotation, float zoom, RectD pageRect, RenderingCallback& callback)
{
    bool ok = Render(dm, pageNo, rotation, zoom, NULL, &pageRect, &callback);
    if (!ok)
        callback.Callback();
}

bool RenderCache::Render(DisplayModel *dm, int pageNo, int rotation, float zoom, 
                         TilePosition *tile, RectD *pageRect, RenderingCallback *renderCb)
{
    assert(dm);
    if (!dm || dm->dontRenderFlag)
        return false;

    assert(tile || pageRect && renderCb);
    if (!tile && !(pageRect && renderCb))
        return false;

    ScopedCritSec scope(&requestAccess);
    PageRenderRequest* newRequest;

    /* add request to the queue */
    if (requestCount == MAX_PAGE_REQUESTS) {
        /* queue is full -> remove the oldest items on the queue */
        if (requests[0].renderCb)
            requests[0].renderCb->Callback();
        memmove(&(requests[0]), &(requests[1]), sizeof(PageRenderRequest) * (MAX_PAGE_REQUESTS - 1));
        newRequest = &(requests[MAX_PAGE_REQUESTS-1]);
    } else {
        newRequest = &(requests[requestCount]);
        requestCount++;
    }
    assert(requestCount <= MAX_PAGE_REQUESTS);

    newRequest->dm = dm;
    newRequest->pageNo = pageNo;
    newRequest->rotation = rotation;
    newRequest->zoom = zoom;
    if (tile) {
        newRequest->pageRect = GetTileRect(dm->engine, pageNo, rotation, zoom, *tile);
        newRequest->tile = *tile;
    }
    else if (pageRect) {
        newRequest->pageRect = *pageRect;
        // can't cache bitmaps that aren't for a given tile
        assert(renderCb);
    }
    else
        assert(0);
    newRequest->abort = false;
    newRequest->timestamp = GetTickCount();
    newRequest->renderCb = renderCb;

    SetEvent(startRendering);

    return true;
}

UINT RenderCache::GetRenderDelay(DisplayModel *dm, int pageNo, TilePosition tile)
{
    ScopedCritSec scope(&requestAccess);

    if (curReq && curReq->pageNo == pageNo && curReq->dm == dm && curReq->tile == tile)
        return GetTickCount() - curReq->timestamp;

    for (int i = 0; i < requestCount; i++)
        if (requests[i].pageNo == pageNo && requests[i].dm == dm && requests[i].tile == tile)
            return GetTickCount() - requests[i].timestamp;

    return RENDER_DELAY_UNDEFINED;
}

bool RenderCache::GetNextRequest(PageRenderRequest *req)
{
    ScopedCritSec scope(&requestAccess);

    if (requestCount == 0)
        return false;

    assert(requestCount > 0);
    assert(requestCount <= MAX_PAGE_REQUESTS);
    requestCount--;
    *req = requests[requestCount];
    curReq = req;
    assert(requestCount >= 0);
    assert(!req->abort);

    return true;
}

bool RenderCache::ClearCurrentRequest()
{
    ScopedCritSec scope(&requestAccess);
    curReq = NULL;

    bool isQueueEmpty = requestCount == 0;
    return isQueueEmpty;
}

/* Wait until rendering of a page beloging to <dm> has finished. */
/* TODO: this might take some time, would be good to show a dialog to let the
   user know he has to wait until we finish */
void RenderCache::CancelRendering(DisplayModel *dm)
{
    DBG_OUT("RenderCache::CancelRendering()\n");
    ClearQueueForDisplayModel(dm);

    for (;;) {
        EnterCriticalSection(&requestAccess);
        if (!curReq || (curReq->dm != dm)) {
            // to be on the safe side
            ClearQueueForDisplayModel(dm);
            LeaveCriticalSection(&requestAccess);
            return;
        }

        curReq->abort = true;
        LeaveCriticalSection(&requestAccess);

        /* TODO: busy loop is not good, but I don't have a better idea */
        Sleep(50);
    }
}

void RenderCache::ClearQueueForDisplayModel(DisplayModel *dm, int pageNo, TilePosition *tile)
{
    ScopedCritSec scope(&requestAccess);
    int reqCount = requestCount;
    int curPos = 0;
    for (int i = 0; i < reqCount; i++) {
        PageRenderRequest *req = &(requests[i]);
        bool shouldRemove = req->dm == dm && (pageNo == INVALID_PAGE_NO || req->pageNo == pageNo) &&
            (!tile || req->tile.res != tile->res || !IsTileVisible(dm, req->pageNo, req->rotation, req->zoom, *tile, 0.5));
        if (i != curPos)
            requests[curPos] = requests[i];
        if (shouldRemove) {
            if (req->renderCb)
                req->renderCb->Callback();
            requestCount--;
        } else
            curPos++;
    }
}

DWORD WINAPI RenderCache::RenderCacheThread(LPVOID data)
{
    RenderCache *cache = (RenderCache *)data;
    PageRenderRequest   req;
    RenderedBitmap *    bmp;

    DBG_OUT("RenderCacheThread() started\n");
    for (;;) {
        //DBG_OUT("Worker: wait\n");
        if (cache->ClearCurrentRequest()) {
            DWORD waitResult = WaitForSingleObject(cache->startRendering, INFINITE);
            // Is it not a page render request?
            if (WAIT_OBJECT_0 != waitResult) {
                DBG_OUT("  WaitForSingleObject() failed\n");
                continue;
            }
        }

        if (!cache->GetNextRequest(&req))
            continue;
        DBG_OUT("RenderCacheThread(): dequeued %d\n", req.pageNo);
        if (!req.dm->PageVisibleNearby(req.pageNo) && !req.renderCb) {
            DBG_OUT("RenderCacheThread(): not rendering because not visible\n");
            continue;
        }
        if (req.dm->dontRenderFlag) {
            DBG_OUT("RenderCacheThread(): not rendering because of _dontRenderFlag\n");
            if (req.renderCb)
                req.renderCb->Callback();
            continue;
        }

        bmp = req.dm->engine->RenderBitmap(req.pageNo, req.zoom, req.rotation, &req.pageRect);
        if (req.abort) {
            delete bmp;
            if (req.renderCb)
                req.renderCb->Callback();
            continue;
        }

        if (bmp)
            DBG_OUT("RenderCacheThread(): finished rendering %d\n", req.pageNo);
        else
            DBG_OUT("RenderCacheThread(): failed to render a bitmap of page %d\n", req.pageNo);
        if (req.renderCb) {
            // the callback must free the RenderedBitmap
            req.renderCb->Callback(bmp);
            req.renderCb = (RenderingCallback *)1; // will crash if accessed again, which should not happen
        }
        else {
            if (bmp && cache->invertColors)
                InvertBitmapColors(bmp->GetBitmap());
            cache->Add(req, bmp);
#ifdef CONSERVE_MEMORY
            cache->FreeNotVisible();
#endif
            req.dm->RepaintDisplay();
        }
    }

    DBG_OUT("RenderCacheThread() finished\n");
    return 0;
}

// TODO: conceptually, RenderCache is not the right place for code that paints
//       (this is the only place that knows about Tiles, though)
UINT RenderCache::PaintTile(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                            TilePosition tile, RectI *tileOnScreen, bool renderMissing,
                            bool *renderOutOfDateCue, bool *renderedReplacement)
{
    BitmapCacheEntry *entry = Find(dm, pageNo, dm->Rotation(), dm->ZoomReal(), &tile);
    UINT renderDelay = 0;

    if (!entry) {
        if (renderedReplacement)
            *renderedReplacement = true;
        entry = Find(dm, pageNo, dm->Rotation(), INVALID_ZOOM, &tile);
        renderDelay = GetRenderDelay(dm, pageNo, tile);
        if (renderMissing && RENDER_DELAY_UNDEFINED == renderDelay && !IsRenderQueueFull())
            Render(dm, pageNo, tile);
    }
    RenderedBitmap *renderedBmp = entry ? entry->bitmap : NULL;
    HBITMAP hbmp = renderedBmp ? renderedBmp->GetBitmap() : NULL;

    if (!hbmp) {
        if (entry && !(renderedBmp && ReduceTileSize()))
            renderDelay = RENDER_DELAY_FAILED;
        else if (0 == renderDelay)
            renderDelay = 1;
        if (entry)
            DropCacheEntry(entry);
        return renderDelay;
    }

    DBG_OUT("page %d ", pageNo);

    HDC bmpDC = CreateCompatibleDC(hdc);
    if (bmpDC) {
        SizeI bmpSize = renderedBmp->Size();
        int xSrc = -min(tileOnScreen->x, 0);
        int ySrc = -min(tileOnScreen->y, 0);
        float factor = min(1.0f * bmpSize.dx / tileOnScreen->dx, 1.0f * bmpSize.dy / tileOnScreen->dy);

        SelectObject(bmpDC, hbmp);
        if (factor != 1.0f)
            StretchBlt(hdc, bounds->x, bounds->y, bounds->dx, bounds->dy,
                bmpDC, (int)(xSrc * factor), (int)(ySrc * factor),
                (int)(bounds->dx * factor), (int)(bounds->dy * factor), SRCCOPY);
        else
            BitBlt(hdc, bounds->x, bounds->y, bounds->dx, bounds->dy,
                bmpDC, xSrc, ySrc, SRCCOPY);

        DeleteDC(bmpDC);
    }

    if (renderOutOfDateCue)
        *renderOutOfDateCue = renderedBmp->outOfDate;

    DropCacheEntry(entry);
    return 0;
}

UINT RenderCache::PaintTiles(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                             RectI *pageOnScreen, USHORT tileRes, bool renderMissing,
                             bool *renderOutOfDateCue, bool *renderedReplacement)
{
    int rotation = dm->Rotation();
    float zoom = dm->ZoomReal();
    int tileCount = 1 << tileRes;

    TilePosition tile = { tileRes, 0, 0 };

    UINT renderTimeMin = (UINT)-1;
    for (tile.row = 0; tile.row < tileCount; tile.row++) {
        for (tile.col = 0; tile.col < tileCount; tile.col++) {
            RectI tileOnScreen = GetTileOnScreen(dm->engine, pageNo, rotation, zoom, tile, *pageOnScreen);
            tileOnScreen = pageOnScreen->Intersect(tileOnScreen);
            RectI isect = bounds->Intersect(tileOnScreen);
            if (!isect.IsEmpty()) {
                UINT renderTime = PaintTile(hdc, &isect, dm, pageNo, tile, &tileOnScreen, renderMissing, renderOutOfDateCue, renderedReplacement);
                renderTimeMin = min(renderTime, renderTimeMin);
            }
        }
    }

    return renderTimeMin;
}

UINT RenderCache::Paint(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                        PageInfo *pageInfo, bool *renderOutOfDateCue)
{
    assert(pageInfo->shown && 0.0 != pageInfo->visibleRatio);

    USHORT tileRes = GetTileRes(dm, pageNo);
    bool renderedReplacement;
    UINT renderTime = RENDER_DELAY_UNDEFINED, renderTimeMin = renderTime = RENDER_DELAY_UNDEFINED;
    for (int res = 0; res <= tileRes; res++) {
        renderedReplacement = false;
        renderTime = PaintTiles(hdc, bounds, dm, pageNo, &pageInfo->pageOnScreen,
                                res, res == tileRes, renderOutOfDateCue, &renderedReplacement);
        renderTimeMin = min(renderTime, renderTimeMin);
    }

#ifdef CONSERVE_MEMORY
    if (0 == renderTime && !renderedReplacement) {
        // free tiles with different resolution
        TilePosition tile = { tileRes, -1, 0 };
        FreePage(dm, pageNo, &tile);
    }
#endif

    return renderTimeMin;
}
