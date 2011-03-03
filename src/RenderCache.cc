/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base_util.h"
#include "RenderCache.h"

/* Define if you want to conserve memory by always freeing cached bitmaps
   for pages not visible. Disabling this might lead to pages not rendering
   due to insufficient (GDI) memory. */
#define CONSERVE_MEMORY

static DWORD WINAPI RenderCacheThread(LPVOID data);

RenderCache::RenderCache(void)
    : _cacheCount(0), _requestCount(0), invertColors(NULL), useGdiRenderer(NULL),
      maxTileSize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN))
{
    InitializeCriticalSection(&_cacheAccess);
    InitializeCriticalSection(&_requestAccess);

    startRendering = CreateEvent(NULL, FALSE, FALSE, NULL);
    _renderThread = CreateThread(NULL, 0, RenderCacheThread, this, 0, 0);
    assert(NULL != _renderThread);
}

RenderCache::~RenderCache(void)
{
    EnterCriticalSection(&_requestAccess);
    EnterCriticalSection(&_cacheAccess);

    CloseHandle(_renderThread);
    CloseHandle(startRendering);
    assert(!_curReq && 0 == _requestCount && 0 == _cacheCount);

    LeaveCriticalSection(&_cacheAccess);
    DeleteCriticalSection(&_cacheAccess);
    LeaveCriticalSection(&_requestAccess);
    DeleteCriticalSection(&_requestAccess);
}

/* Find a bitmap for a page defined by <dm> and <pageNo> and optionally also
   <rotation> and <zoom> in the cache - call DropCacheEntry when you
   no longer need a found entry. */
BitmapCacheEntry *RenderCache::Find(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition *tile)
{
    CritSecScoped scope(&_cacheAccess);
    BitmapCacheEntry *entry;
    normalizeRotation(&rotation);
    for (int i = 0; i < _cacheCount; i++) {
        entry = _cache[i];
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
    CritSecScoped scope(&_cacheAccess);
    assert(entry);
    if (!entry) return;
    if (0 == --entry->refs) {
        delete entry->bitmap;
        free(entry);
    }
}

void RenderCache::Add(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition tile, RenderedBitmap *bitmap)
{
    CritSecScoped scope(&_cacheAccess);
    assert(dm);
    assert(validRotation(rotation));

    normalizeRotation(&rotation);
    DBG_OUT("BitmapCache_Add(pageNo=%d, rotation=%d, zoom=%.2f%%)\n", pageNo, rotation, zoom);
    assert(_cacheCount <= MAX_BITMAPS_CACHED);

    /* It's possible there still is a cached bitmap with different zoom/rotation */
    FreePage(dm, pageNo, &tile);

    if (_cacheCount >= MAX_BITMAPS_CACHED) {
        // free an invisible page of the same DisplayModel ...
        for (int i = 0; i < _cacheCount; i++) {
            if (_cache[i]->dm == dm && !dm->pageVisibleNearby(_cache[i]->pageNo)) {
                DropCacheEntry(_cache[i]);
                _cacheCount--;
                memmove(&_cache[i], &_cache[i + 1], (_cacheCount - i) * sizeof(_cache[0]));
                break;
            }
        }
        // ... or just the oldest cached page
        if (_cacheCount >= MAX_BITMAPS_CACHED) {
            DropCacheEntry(_cache[0]);
            _cacheCount--;
            memmove(&_cache[0], &_cache[1], _cacheCount * sizeof(_cache[0]));
        }
    }

    BitmapCacheEntry entry = { dm, pageNo, rotation, zoom, bitmap, tile, 1 };
    _cache[_cacheCount] = (BitmapCacheEntry *)_memdup(&entry);
    assert(_cache[_cacheCount]);
    if (!_cache[_cacheCount])
        delete bitmap;
    else
        _cacheCount++;
    dm->ageStore();
}

// get the (user) coordinates of a specific tile
static fz_rect GetTileRect(PdfEngine *engine, int pageNo, int rotation, float zoom, TilePosition tile)
{
    fz_rect mediabox = engine->pageMediabox(pageNo);

    if (tile.res && tile.res != INVALID_TILE_RES) {
        float width = (mediabox.x1 - mediabox.x0) / (1 << tile.res);
        mediabox.x0 += tile.col * width;
        mediabox.x1 = mediabox.x0 + width;
        float height = (mediabox.y1 - mediabox.y0) / (1 << tile.res);
        mediabox.y0 += ((1 << tile.res) - tile.row - 1) * height;
        mediabox.y1 = mediabox.y0 + height;
    }

    fz_matrix ctm = engine->viewctm(pageNo, zoom, rotation);
    fz_bbox pixelbox = fz_roundrect(fz_transformrect(ctm, mediabox));

    mediabox = fz_transformrect(fz_invertmatrix(ctm), fz_bboxtorect(pixelbox));
    return mediabox;
}

static RectI GetTileOnScreen(PdfEngine *engine, int pageNo, int rotation, float zoom, TilePosition tile, fz_matrix ctm)
{
    fz_rect mediabox = GetTileRect(engine, pageNo, rotation, zoom, tile);
    fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, mediabox));
    RectI tileOnScreen = { bbox.x0, bbox.y0, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0 };
    return tileOnScreen;
}

static bool IsTileVisible(DisplayModel *dm, int pageNo, int rotation, float zoom, TilePosition tile, float fuzz=0)
{
    PdfPageInfo *pageInfo = dm->getPageInfo(pageNo);
    fz_matrix ctm = dm->pdfEngine->viewctm(pageNo, zoom, rotation);
    ctm = fz_concat(ctm, fz_translate((float)pageInfo->pageOnScreen.x, (float)pageInfo->pageOnScreen.y));
    RectI tileOnScreen = GetTileOnScreen(dm->pdfEngine, pageNo, rotation, zoom, tile, ctm);
    // consider nearby tiles visible depending on the fuzz factor
    tileOnScreen.x -= (int)(tileOnScreen.dx * fuzz * 0.5);
    tileOnScreen.dx = (int)(tileOnScreen.dx * (fuzz + 1));
    tileOnScreen.y -= (int)(tileOnScreen.dy * fuzz * 0.5);
    tileOnScreen.dy = (int)(tileOnScreen.dy * (fuzz + 1));
    RectI screen = { 0, 0, dm->drawAreaSize.dx, dm->drawAreaSize.dy };
    return RectI_Intersect(&tileOnScreen, &screen, NULL) != 0;
}

/* Free all bitmaps in the cache that are of a specific page (or all pages
   of the given DisplayModel, or even all invisible pages). Returns TRUE if freed
   at least one item. */
bool RenderCache::FreePage(DisplayModel *dm, int pageNo, TilePosition *tile)
{
    CritSecScoped scope(&_cacheAccess);
    int cacheCount = _cacheCount;
    bool freedSomething = false;
    int curPos = 0;

    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* entry = _cache[i];
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
            shouldFree = (_cache[i]->dm == dm);
        } else {
            // all invisible pages resp. page tiles
            shouldFree = !entry->dm->pageVisibleNearby(entry->pageNo);
            if (!shouldFree && entry->tile.res > 1)
                shouldFree = !IsTileVisible(entry->dm, entry->pageNo, entry->rotation,
                                            entry->zoom, entry->tile, 2.0);
        }

        if (shouldFree) {
            if (!freedSomething)
                DBG_OUT("BitmapCache_FreePage(%#x, %d) ", dm, pageNo);
            DBG_OUT("freed %d ", entry->pageNo);
            freedSomething = true;
            DropCacheEntry(entry);
            _cache[i] = NULL;
            _cacheCount--;
        }

        if (curPos != i)
            _cache[curPos] = _cache[i];
        if (!shouldFree)
            curPos++;
    }

    if (freedSomething)
        DBG_OUT("\n");
    return freedSomething;
}

void RenderCache::KeepForDisplayModel(DisplayModel *oldDm, DisplayModel *newDm)
{
    CritSecScoped scope(&_cacheAccess);
    for (int i = 0; i < _cacheCount; i++) {
        // keep the cached bitmaps for visible pages to avoid flickering during a reload
        if (_cache[i]->dm == oldDm && oldDm->pageVisible(_cache[i]->pageNo) && _cache[i]->bitmap) {
            _cache[i]->dm = newDm;
            // make sure that the page is rerendered eventually
            _cache[i]->zoom = INVALID_ZOOM;
            _cache[i]->bitmap->outOfDate = true;
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
bool RenderCache::FreeNotVisible(void)
{
    return FreePage();
}


// determine the count of tiles required for a page at a given zoom level
USHORT RenderCache::GetTileRes(DisplayModel *dm, int pageNo)
{
    fz_rect mediabox = dm->pdfEngine->pageMediabox(pageNo);
    fz_matrix ctm = dm->pdfEngine->viewctm(pageNo, dm->zoomReal(), dm->rotation());
    fz_rect pixelbox = fz_transformrect(ctm, mediabox);

    float factorW = (pixelbox.x1 - pixelbox.x0) / (maxTileSize.dx + 1);
    float factorH = (pixelbox.y1 - pixelbox.y0) / (maxTileSize.dy + 1);
    float factorMax = max(factorW, factorH);

    // use larger tiles when fitting page or width or when a page is smaller
    // than the visible canvas width/height or when rendering pages
    // containing a single image (MuPDF isn't that much faster for rendering
    // individual tiles than for rendering the whole image in a single pass)
    if (dm->zoomVirtual() == ZOOM_FIT_PAGE || dm->zoomVirtual() == ZOOM_FIT_WIDTH ||
        pixelbox.x1 - pixelbox.x0 <= dm->drawAreaSize.dx || pixelbox.y1 - pixelbox.y0 < dm->drawAreaSize.dy ||
        dm->pdfEngine->isImagePage(pageNo)) {
        factorMax /= 2.0;
    }

    USHORT res = 0;
    if (factorMax > 1.5)
        res = (USHORT)ceilf(log(factorMax) / log(2.0f));
    return res;
}

void RenderCache::Render(DisplayModel *dm, int pageNo, UIThreadWorkItem *finishedWorkItem)
{
    TilePosition tile = { GetTileRes(dm, pageNo), 0, 0 };
    Render(dm, pageNo, tile, true, finishedWorkItem);

    // render both tiles of the first row when splitting a page in four
    // (which always happens on larger displays for Fit Width)
    if (tile.res == 1 && !IsRenderQueueFull()) {
        tile.col = 1;
        Render(dm, pageNo, tile, false);
    }
}

/* Render a bitmap for page <pageNo> in <dm>. */
void RenderCache::Render(DisplayModel *dm, int pageNo, TilePosition tile, bool clearQueue, UIThreadWorkItem *finishedWorkItem)
{
    DBG_OUT("RenderQueue_Add(pageNo=%d)\n", pageNo);
    assert(dm);
    bool addRequest = false;

    CritSecScoped scope(&_requestAccess);
    if (!dm || dm->_dontRenderFlag) goto Exit;

    int rotation = dm->rotation();
    normalizeRotation(&rotation);
    float zoom = dm->zoomReal(pageNo);

    if (_curReq && (_curReq->pageNo == pageNo) && (_curReq->dm == dm) && (_curReq->tile == tile)) {
        if ((_curReq->zoom != zoom) || (_curReq->rotation != rotation)) {
            /* Currently rendered page is for the same page but with different zoom
            or rotation, so abort it */
            DBG_OUT("  aborting rendering\n");
            _curReq->abort = TRUE;
        } else {
            /* we're already rendering exactly the same page */
            DBG_OUT("  already rendering this page\n");
            goto Exit;
        }
    }

    // clear requests for tiles of different resolution and invisible tiles
    if (clearQueue)
        ClearQueueForDisplayModel(dm, pageNo, &tile);

    for (int i=0; i < _requestCount; i++) {
        PageRenderRequest* req = &(_requests[i]);
        if ((req->pageNo == pageNo) && (req->dm == dm) && (req->tile == tile)) {
            if ((req->zoom == zoom) && (req->rotation == rotation)) {
                /* Request with exactly the same parameters already queued for
                   rendering. Move it to the top of the queue so that it'll
                   be rendered faster. */
                PageRenderRequest tmp;
                tmp = _requests[_requestCount-1];
                _requests[_requestCount-1] = *req;
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

    PageRenderRequest* newRequest;
    /* add request to the queue */
    if (_requestCount == MAX_PAGE_REQUESTS) {
        /* queue is full -> remove the oldest items on the queue */
        memmove(&(_requests[0]), &(_requests[1]), sizeof(PageRenderRequest)*(MAX_PAGE_REQUESTS-1));
        newRequest = &(_requests[MAX_PAGE_REQUESTS-1]);
    } else {
        newRequest = &(_requests[_requestCount]);
        _requestCount++;
    }
    assert(_requestCount <= MAX_PAGE_REQUESTS);
    newRequest->dm = dm;
    newRequest->pageNo = pageNo;
    newRequest->rotation = rotation;
    newRequest->zoom = zoom;
    newRequest->tile = tile;
    newRequest->abort = FALSE;
    newRequest->timestamp = GetTickCount();
    newRequest->finishedWorkItem = finishedWorkItem;
    addRequest = true;

Exit:
    if (addRequest)
        SetEvent(startRendering);
    else if (finishedWorkItem)
        MarshallOnUIThread(finishedWorkItem);
}

UINT RenderCache::GetRenderDelay(DisplayModel *dm, int pageNo, TilePosition tile)
{
    bool foundReq = false;
    DWORD timestamp;

    CritSecScoped scope(&_requestAccess);
    if (_curReq && _curReq->pageNo == pageNo && _curReq->dm == dm && _curReq->tile == tile) {
        timestamp = _curReq->timestamp;
        foundReq = true;
    }
    for (int i = 0; !foundReq && i < _requestCount; i++) {
        if (_requests[i].pageNo == pageNo && _requests[i].dm == dm && _requests[i].tile == tile) {
            timestamp = _requests[i].timestamp;
            foundReq = true;
        }
    }

    if (!foundReq)
        return RENDER_DELAY_UNDEFINED;
    return GetTickCount() - timestamp;
}

bool RenderCache::GetNextRequest(PageRenderRequest *req)
{
    CritSecScoped scope(&_requestAccess);

    if (_requestCount == 0)
        return false;

    assert(_requestCount > 0);
    assert(_requestCount <= MAX_PAGE_REQUESTS);
    _requestCount--;
    *req = _requests[_requestCount];
    _curReq = req;
    assert(_requestCount >= 0);
    assert(!req->abort);

    return true;
}

bool RenderCache::ClearCurrentRequest(void)
{
    CritSecScoped scope(&_requestAccess);
    _curReq = NULL;

    bool isQueueEmpty = _requestCount == 0;
    return isQueueEmpty;
}

/* Wait until rendering of a page beloging to <dm> has finished. */
/* TODO: this might take some time, would be good to show a dialog to let the
   user know he has to wait until we finish */
void RenderCache::CancelRendering(DisplayModel *dm)
{
    DBG_OUT("cancelRenderingForDisplayModel()\n");
    ClearQueueForDisplayModel(dm);

    for (;;) {
        EnterCriticalSection(&_requestAccess);
        if (!_curReq || (_curReq->dm != dm)) {
            // to be on the safe side
            ClearQueueForDisplayModel(dm);
            LeaveCriticalSection(&_requestAccess);
            return;
        }

        _curReq->abort = TRUE;
        LeaveCriticalSection(&_requestAccess);

        /* TODO: busy loop is not good, but I don't have a better idea */
        Sleep(50);
    }
}

void RenderCache::ClearQueueForDisplayModel(DisplayModel *dm, int pageNo, TilePosition *tile)
{
    CritSecScoped scope(&_requestAccess);
    int reqCount = _requestCount;
    int curPos = 0;
    for (int i = 0; i < reqCount; i++) {
        PageRenderRequest *req = &(_requests[i]);
        bool shouldRemove = req->dm == dm && (pageNo == INVALID_PAGE_NO || req->pageNo == pageNo) &&
            (!tile || req->tile.res != tile->res || !IsTileVisible(dm, req->pageNo, req->rotation, req->zoom, *tile, 0.5));
        if (i != curPos)
            _requests[curPos] = _requests[i];
        if (shouldRemove)
            _requestCount--;
        else
            curPos++;
    }
}

static DWORD WINAPI RenderCacheThread(LPVOID data)
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
        if (!req.dm->pageVisibleNearby(req.pageNo) && req.finishedWorkItem == NULL) {
            DBG_OUT("RenderCacheThread(): not rendering because not visible\n");
            continue;
        }
        if (req.dm->_dontRenderFlag) {
            DBG_OUT("RenderCacheThread(): not rendering because of _dontRenderFlag\n");
            continue;
        }

        fz_rect pageRect = GetTileRect(req.dm->pdfEngine, req.pageNo, req.rotation, req.zoom, req.tile);
        bmp = req.dm->renderBitmap(req.pageNo, req.zoom, req.rotation, &pageRect,
                                   Target_View, cache->useGdiRenderer && *cache->useGdiRenderer);
        if (req.abort) {
            delete bmp;
            continue;
        }

        if (bmp && cache->invertColors && *cache->invertColors)
            bmp->invertColors();
        if (bmp)
            DBG_OUT("RenderCacheThread(): finished rendering %d\n", req.pageNo);
        else
            DBG_OUT("PageRenderThread(): failed to render a bitmap of page %d\n", req.pageNo);
        cache->Add(req.dm, req.pageNo, req.rotation, req.zoom, req.tile, bmp);
#ifdef CONSERVE_MEMORY
        cache->FreeNotVisible();
#endif
        if (req.finishedWorkItem) {
            MarshallOnUIThread(req.finishedWorkItem);
            req.finishedWorkItem = (UIThreadWorkItem*)1; // will crash if accessed again, which should not happen
        }
        else
            req.dm->RepaintDisplay();
    }

    DBG_OUT("PageRenderThread() finished\n");
    return 0;
}

// TODO: conceptually, RenderCache is not the right place for code that paints
UINT RenderCache::PaintTile(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                            TilePosition tile, RectI *tileOnScreen, bool renderMissing,
                            bool *renderOutOfDateCue, bool *renderedReplacement)
{
    BitmapCacheEntry *entry = Find(dm, pageNo, dm->rotation(), dm->zoomReal(), &tile);
    UINT renderDelay = 0;

    if (!entry) {
        if (renderedReplacement)
            *renderedReplacement = true;
        entry = Find(dm, pageNo, dm->rotation(), INVALID_ZOOM, &tile);
        renderDelay = GetRenderDelay(dm, pageNo, tile);
        if (renderMissing && RENDER_DELAY_UNDEFINED == renderDelay && !IsRenderQueueFull())
            Render(dm, pageNo, tile);
    }
    RenderedBitmap *renderedBmp = entry ? entry->bitmap : NULL;
    HBITMAP hbmp = renderedBmp ? renderedBmp->getBitmap() : NULL;

    if (!hbmp) {
        if (entry)
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
        int renderedBmpDx = renderedBmp->dx();
        int renderedBmpDy = renderedBmp->dy();
        int xSrc = -min(tileOnScreen->x, 0);
        int ySrc = -min(tileOnScreen->y, 0);
        float factor = min(1.0f * renderedBmpDx / tileOnScreen->dx, 1.0f * renderedBmpDy / tileOnScreen->dy);

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

UINT RenderCache::PaintTiles(HDC hdc, RECT *bounds, DisplayModel *dm, int pageNo,
                             RectI *pageOnScreen, USHORT tileRes, bool renderMissing,
                             bool *renderOutOfDateCue, bool *renderedReplacement)
{
    int rotation = dm->rotation();
    float zoom = dm->zoomReal();
    int tileCount = 1 << tileRes;

    TilePosition tile = { tileRes, 0, 0 };
    RectI screen = RectI_FromRECT(bounds);
    RectI isectPOS, isect;

    fz_matrix ctm = dm->pdfEngine->viewctm(pageNo, zoom, rotation);
    ctm = fz_concat(ctm, fz_translate((float)pageOnScreen->x, (float)pageOnScreen->y));

    UINT renderTimeMin = (UINT)-1;
    for (tile.row = 0; tile.row < tileCount; tile.row++) {
        for (tile.col = 0; tile.col < tileCount; tile.col++) {
            RectI tileOnScreen = GetTileOnScreen(dm->pdfEngine, pageNo, rotation, zoom, tile, ctm);
            if (RectI_Intersect(&tileOnScreen, pageOnScreen, &isectPOS) &&
                RectI_Intersect(&screen, &isectPOS, &isect)) {
                UINT renderTime = PaintTile(hdc, &isect, dm, pageNo, tile, &isectPOS, renderMissing, renderOutOfDateCue, renderedReplacement);
                renderTimeMin = min(renderTime, renderTimeMin);
            }
        }
    }

    return renderTimeMin;
}

UINT RenderCache::Paint(HDC hdc, RECT *bounds, DisplayModel *dm, int pageNo,
                        PdfPageInfo *pageInfo, bool *renderOutOfDateCue)
{
    assert(pageInfo->shown && pageInfo->visible);

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
