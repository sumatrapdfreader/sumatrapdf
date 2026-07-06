/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Timer.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"

#pragma warning(disable : 28159) // silence /analyze: Consider using 'GetTickCount64' instead of 'GetTickCount'

// CONSERVE_MEMORY sets the compile-time default for gConserveMemory. When defined,
// cached page bitmaps for non-visible pages are freed aggressively. Undefining it
// keeps more pages resident (higher GDI memory use, fewer re-renders).
#define CONSERVE_MEMORY

#if defined(CONSERVE_MEMORY)
bool gConserveMemory = true;
#else
bool gConserveMemory = false;
#endif

static DWORD WINAPI RenderCacheThread(LPVOID data);

bool gShowTileLayout = false;
int gMaxRenderThreads = 8;

// RenderCache's verbose per-operation logging (FreePage / Paint / DropCacheEntry
// / ...) is noisy, so it's disabled by default. Set gLogRenderCache = true to
// re-enable it when debugging the cache.
static bool gLogRenderCache = false;
#define rcLogf(...)                \
    do {                           \
        if (gLogRenderCache) {     \
            log(fmt(__VA_ARGS__)); \
        }                          \
    } while (0)

struct RenderThreadData {
    RenderCache* cache;
    int threadIdx;
};

RenderCache::RenderCache() : maxTileSize({GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}) {
    // enable when debugging RenderCache logic
    // gEnableDbgLog = true;

    isRemoteSession = GetSystemMetrics(SM_REMOTESESSION);
    textColor = WIN_COL_BLACK;
    backgroundColor = WIN_COL_WHITE;

    InitializeCriticalSection(&cacheAccess);
    InitializeCriticalSection(&requestAccess);

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int numCores = (int)si.dwNumberOfProcessors;
    maxRenderThreads = std::max(gMaxRenderThreads, numCores);
    if (maxRenderThreads > kMaxRenderThreads) {
        maxRenderThreads = kMaxRenderThreads;
    }

    // use a semaphore so each queued request wakes one thread.
    // threads themselves are spawned lazily in Render() when work appears
    // and no idle thread is available -- many sessions only ever need a
    // couple of render threads, so creating 8+ upfront is wasteful.
    startRendering = CreateSemaphoreW(nullptr, 0, INT_MAX, nullptr);
}

RenderCache::~RenderCache() {
    // Signal threads to exit FIRST, then wait for them WITHOUT holding the
    // critical sections. Workers take requestAccess for their idle bookkeeping,
    // so holding it here would deadlock until the WaitForMultipleObjects
    // timeout fires -- after which DeleteCriticalSection on a still-in-use
    // CS would access-violate.
    AtomicBoolSet(&shouldExit, true);

    if (nRenderThreads > 0) {
        // wake all threads waiting on the semaphore
        ReleaseSemaphore(startRendering, nRenderThreads, nullptr);

        // wait for all threads to finish
        DWORD res = WaitForMultipleObjects((DWORD)nRenderThreads, renderThreads, TRUE, 5000);
        if (res == WAIT_TIMEOUT) {
            logf("RenderCache::~RenderCache: threads didn't exit in 5 seconds\n");
        }

        for (int i = 0; i < nRenderThreads; i++) {
            CloseHandle(renderThreads[i]);
        }
    }
    CloseHandle(startRendering);

    // Threads are gone; remaining state inspection is single-threaded.
    bool hasCurReq = false;
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i]) {
            hasCurReq = true;
        }
    }
    if (hasCurReq || 0 != requestCount || cacheCount != 0) {
        rcLogf("RenderCache::~RenderCache: hasCurReq: %d, requestCount: %d, cacheCount: %d\n", (int)hasCurReq,
               requestCount, cacheCount);
        ReportIf(true);
    }

    DeleteCriticalSection(&cacheAccess);
    DeleteCriticalSection(&requestAccess);
}

/* Find a bitmap for a page defined by <dm> and <pageNo> and optionally also
   <rotation> and <zoom> in the cache - call DropCacheEntry when you
   no longer need a found entry. */
// out-of-line so RenderCache.h needn't include Pixmap.h (only forward-declare it)
BitmapCacheEntry::~BitmapCacheEntry() {
    FreePixmap(bitmap);
}

BitmapCacheEntry* RenderCache::Find(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition* tile) {
    ScopedMutex scope(&cacheAccess);
    rotation = NormalizeRotation(rotation);
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* e = cache[i];
        if ((dm == e->dm) && (pageNo == e->pageNo) && (rotation == e->rotation) &&
            (kInvalidZoom == zoom || zoom == e->zoom) && (!tile || e->tile == *tile)) {
            e->refs++;
            ReportIf(i != e->cacheIdx);
            return e;
        }
    }
    return nullptr;
}

bool RenderCache::Exists(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition* tile) {
    BitmapCacheEntry* entry = Find(dm, pageNo, rotation, zoom, tile);
    if (entry) {
        DropCacheEntry(entry);
    }
    return entry != nullptr;
}

bool RenderCache::DropCacheEntry(BitmapCacheEntry* entry) {
    ScopedMutex scope(&cacheAccess);
    ReportIf(!entry);
    if (!entry) {
        return false;
    }
    int idx = entry->cacheIdx;
    ReportIf(idx < 0);
    ReportIf(idx >= cacheCount);
    if ((idx < 0) || (idx >= cacheCount)) {
        return false;
    }
    ReportIf(entry->refs <= 0);
    --entry->refs;
    if (entry->refs > 0) {
        return false;
    }
    ReportIf(entry->refs != 0);
    ReportIf(cache[idx] != entry);
    rcLogf("RenderCache::DropCacheEntry: dm: 0x%p, pageNo: %d, rotation: %d, zoom: %.2f\n", entry->dm, entry->pageNo,
           entry->rotation, entry->zoom);

    delete entry;

    // fast removal by replacing freed item with the item at the end
    cache[idx] = nullptr;
    int lastIdx = cacheCount - 1;
    if ((lastIdx >= 0) && (idx != lastIdx)) {
        cache[idx] = cache[lastIdx];
        cache[idx]->cacheIdx = idx;
        cache[lastIdx] = nullptr;
    }
    cacheCount--;
    ReportIf(cacheCount < 0);

    // LogCacheSize();
    return true;
}

bool RenderCache::DropCacheEntryIfNotUsed(BitmapCacheEntry* entry) {
    ScopedMutex scope(&cacheAccess);
    if (!entry || entry->refs > 1) {
        return false;
    }
    return DropCacheEntry(entry);
}

static bool FreeIfFull(RenderCache* rc, const PageRenderRequest& req) {
    int n = rc->cacheCount;
    if (n < MAX_BITMAPS_CACHED) {
        return true;
    }

    DisplayModel* dm = req.dm;
    // free an invisible page of the same DisplayModel ...
    for (int i = 0; i < n; i++) {
        auto entry = rc->cache[i];
        if (entry->dm == dm && !dm->PageVisibleNearby(entry->pageNo)) {
            bool didDrop = rc->DropCacheEntryIfNotUsed(entry);
            if (didDrop) {
                return true;
            }
        }
    }

    // ... or just the oldest cached page
    for (int i = 0; i < n; i++) {
        auto entry = rc->cache[i];
        if (entry->dm == dm) {
            // don't free pages from the document we're currently displaying
            // as it leads to flicker
            // TODO: it can still flicker if the dm is from a visible tab
            // in a different window, but it's harder to detect
            continue;
        }
        bool didDrop = rc->DropCacheEntryIfNotUsed(entry);
        if (didDrop) {
            return true;
        }
    }
    return false;
}

void RenderCache::Add(PageRenderRequest& req, Pixmap* bmp) {
    ScopedMutex scope(&cacheAccess);
    ReportIf(!req.dm);

    req.rotation = NormalizeRotation(req.rotation);
    ReportIf(cacheCount > MAX_BITMAPS_CACHED);

    /* It's possible there still is a cached bitmap with different zoom/rotation */
    FreePage(req.dm, req.pageNo, &req.tile);

    bool hasSpace = FreeIfFull(this, req);
    ReportIf(!hasSpace); // TODO: FreeIfFull() might actually fail to free
    ReportIf(cacheCount > MAX_BITMAPS_CACHED);

    // Copy the PageRenderRequest as it will be reused
    auto entry = new BitmapCacheEntry(req.dm, req.pageNo, req.rotation, req.zoom, req.tile, bmp);
    entry->cacheIdx = cacheCount;
    cache[cacheCount] = entry;
    cacheCount++;

    // LogCacheSize();
}

static RectF GetTileRect(RectF pagerect, TilePosition tile) {
    ReportIf(tile.res > 30);
    RectF rect;
    rect.dx = pagerect.dx / (1ULL << tile.res);
    rect.dy = pagerect.dy / (1ULL << tile.res);
    rect.x = pagerect.x + tile.col * rect.dx;
    rect.y = pagerect.y + ((1ULL << tile.res) - tile.row - 1) * rect.dy;
    return rect;
}

// get the coordinates of a specific tile
static Rect GetTileRectDevice(EngineBase* engine, int pageNo, int rotation, float zoom, TilePosition tile) {
    RectF mediabox = engine->PageMediabox(pageNo);
    if (tile.res > 0 && tile.res != INVALID_TILE_RES) {
        mediabox = GetTileRect(mediabox, tile);
    }
    RectF pixelbox = engine->Transform(mediabox, pageNo, zoom, rotation);
    return pixelbox.Round();
}

static RectF GetTileRectUser(EngineBase* engine, int pageNo, int rotation, float zoom, TilePosition tile) {
    Rect pixelbox = GetTileRectDevice(engine, pageNo, rotation, zoom, tile);
    return engine->Transform(ToRectF(pixelbox), pageNo, zoom, rotation, true);
}

static Rect GetTileOnScreen(EngineBase* engine, int pageNo, int rotation, float zoom, TilePosition tile,
                            Rect pageOnScreen) {
    Rect bbox = GetTileRectDevice(engine, pageNo, rotation, zoom, tile);
    bbox.Offset(pageOnScreen.x, pageOnScreen.y);
    return bbox;
}

static bool IsTileVisible(DisplayModel* dm, int pageNo, TilePosition tile, float fuzz = 0) {
    if (!dm) {
        return false;
    }
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    EngineBase* engine = dm->GetEngine();
    if (!engine || !pageInfo) {
        return false;
    }
    int rotation = dm->GetRotation();
    float zoom = dm->GetZoomReal(pageNo);
    Rect r = pageInfo->pageOnScreen;
    Rect tileOnScreen = GetTileOnScreen(engine, pageNo, rotation, zoom, tile, r);
    // consider nearby tiles visible depending on the fuzz factor
    tileOnScreen.x -= (int)(tileOnScreen.dx * fuzz * 0.5);
    tileOnScreen.dx = (int)(tileOnScreen.dx * (fuzz + 1));
    tileOnScreen.y -= (int)(tileOnScreen.dy * fuzz * 0.5);
    tileOnScreen.dy = (int)(tileOnScreen.dy * (fuzz + 1));
    Rect screen(Point(), dm->GetViewPort().Size());
    return !tileOnScreen.Intersect(screen).IsEmpty();
}

/* Free all bitmaps in the cache that are of a specific page (or all pages
   of the given DisplayModel, or even all invisible pages). */
void RenderCache::FreePage(DisplayModel* dm, int pageNo, TilePosition* tile) {
    rcLogf("RenderCache::FreePage: dm: 0x%p, pageNo: %d\n", dm, pageNo);
    ReportIf(!dm || (pageNo == kInvalidPageNo));
    if (!dm || (pageNo == kInvalidPageNo)) {
        return;
    }
    ScopedMutex scope(&cacheAccess);

    // must go from end because freeing changes the cache
    for (int i = cacheCount - 1; i >= 0; i--) {
        BitmapCacheEntry* entry = cache[i];
        bool shouldFree = (entry->dm == dm) && (entry->pageNo == pageNo);
        if (shouldFree && tile) {
            // a given tile of the page or all tiles not rendered at a given resolution
            // (and at resolution 0 for quick zoom previews)
            shouldFree = (entry->tile == *tile ||
                          tile->row == (USHORT)-1 && entry->tile.res > 0 && entry->tile.res != tile->res ||
                          tile->row == (USHORT)-1 && entry->tile.res == 0 && entry->outOfDate);
        }
        if (shouldFree) {
            DropCacheEntryIfNotUsed(entry);
        }
    }
}

void RenderCache::FreeForDisplayModel(DisplayModel* dm) {
    rcLogf("RenderCache::FreeForDisplayModel: dm: 0x%p\n", dm);
    ScopedMutex scope(&cacheAccess);
    // must go from end because freeing changes the cache
    for (int i = cacheCount - 1; i >= 0; i--) {
        BitmapCacheEntry* entry = cache[i];
        if (entry->dm == dm) {
            DropCacheEntryIfNotUsed(entry);
        }
    }
}

void RenderCache::FreeNotVisible() {
    // rcLogf("RenderCache::FreeNotVisible\n");
    ScopedMutex scope(&cacheAccess);
    // must go from end because freeing changes the cache
    for (int i = cacheCount - 1; i >= 0; i--) {
        BitmapCacheEntry* entry = cache[i];
        // all invisible pages resp. page tiles
        bool shouldFree = !entry->dm->PageVisibleNearby(entry->pageNo);
        if (!shouldFree && entry->tile.res > 1) {
            shouldFree = !IsTileVisible(entry->dm, entry->pageNo, entry->tile, 2.0);
        }
        if (shouldFree) {
            DropCacheEntryIfNotUsed(entry);
        }
    }
}

// keep the cached bitmaps for visible pages to avoid flickering during a reload.
// mark invisible pages as out-of-date to prevent inconsistencies
void RenderCache::KeepForDisplayModel(DisplayModel* oldDm, DisplayModel* newDm) {
    ScopedMutex scope(&cacheAccess);
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* entry = cache[i];
        if (entry->dm != oldDm) {
            continue;
        }
        if (oldDm->PageVisible(entry->pageNo)) {
            entry->dm = newDm;
        }
        // make sure that the page is rerendered eventually
        entry->zoom = kInvalidZoom;
        entry->outOfDate = true;
    }
}

// marks all tiles containing rect of pageNo as out of date
void RenderCache::Invalidate(DisplayModel* dm, int pageNo, RectF rect) {
    ScopedMutex scopeReq(&requestAccess);

    ClearQueueForDisplayModel(dm, pageNo);
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i] && curReqs[i]->dm == dm && curReqs[i]->pageNo == pageNo) {
            AbortCurrentRequest(i);
        }
    }

    ScopedMutex scopeCache(&cacheAccess);

    RectF mediabox = dm->GetEngine()->PageMediabox(pageNo);
    for (int i = 0; i < cacheCount; i++) {
        auto e = cache[i];
        if (e->dm == dm && e->pageNo == pageNo && !GetTileRect(mediabox, e->tile).Intersect(rect).IsEmpty()) {
            e->zoom = kInvalidZoom;
            e->outOfDate = true;
        }
    }
}

// determine the count of tiles required for a page at a given zoom level
USHORT RenderCache::GetTileRes(DisplayModel* dm, int pageNo) const {
    auto engine = dm->GetEngine();
    RectF mediabox = engine->PageMediabox(pageNo);
    float zoom = dm->GetZoomReal(pageNo);
    float zoomVirt = dm->GetZoomVirtual();
    Rect viewPort = dm->GetViewPort();
    int rotation = dm->GetRotation();
    RectF pixelbox = engine->Transform(mediabox, pageNo, zoom, rotation);

    float factorW = (float)pixelbox.dx / (maxTileSize.dx + 1);
    float factorH = (float)pixelbox.dy / (maxTileSize.dy + 1);
    // using the geometric mean instead of the maximum factor
    // so that the tile area doesn't get too small in comparison
    // to maxTileSize (but remains smaller)
    float factorAvg = sqrtf(factorW * factorH);

    // use larger tiles when fitting page or width or when a page is smaller
    // than the visible canvas width/height or when rendering pages
    // without clipping optimizations
    if (zoomVirt == kZoomFitPage || zoomVirt == kZoomFitWidth || pixelbox.dx <= viewPort.dx ||
        pixelbox.dy < viewPort.dy || !engine->HasClipOptimizations(pageNo)) {
        factorAvg /= 2.0;
    }

    USHORT res = 0;
    if (factorAvg > 1.5) {
        res = (USHORT)ceilf((float)(log(factorAvg) / log(2.0f)));
    }
    // limit res to 30, so that (1 << res) doesn't overflow for 32-bit signed int
    return std::min(res, (USHORT)30);
}

// get the maximum resolution available for the given page
USHORT RenderCache::GetMaxTileRes(DisplayModel* dm, int pageNo, int rotation) {
    ScopedMutex scope(&cacheAccess);
    USHORT maxRes = 0;
    for (int i = 0; i < cacheCount; i++) {
        auto e = cache[i];
        if (e->dm == dm && e->pageNo == pageNo && e->rotation == rotation) {
            maxRes = std::max(e->tile.res, maxRes);
        }
    }
    return maxRes;
}

// reduce the size of tiles in order to hopefully use less memory overall
bool RenderCache::ReduceTileSize() {
    rcLogf("RenderCache::ReduceTileSize: reducing tile size (current: %d x %d)\n", maxTileSize.dx, maxTileSize.dy);
    if (maxTileSize.dx < 200 || maxTileSize.dy < 200) {
        return false;
    }

    ScopedMutex scope1(&requestAccess);
    ScopedMutex scope2(&cacheAccess);

    if (maxTileSize.dx > maxTileSize.dy) {
        maxTileSize.dx /= 2;
    } else {
        maxTileSize.dy /= 2;
    }

    // invalidate all rendered bitmaps and all requests
    while (cacheCount > 0) {
        FreeForDisplayModel(cache[0]->dm);
    }
    while (requestCount > 0) {
        ClearQueueForDisplayModel(requests[0].dm);
    }
    for (int i = 0; i < nRenderThreads; i++) {
        AbortCurrentRequest(i);
    }

    return true;
}

void RenderCache::RequestRendering(DisplayModel* dm, int pageNo) {
    // a page that laid out with an invalid zoom (e.g. a zero-sized page) can't be rendered
    if (dm->GetZoomReal(pageNo) <= 0) {
        return;
    }
    TilePosition tile(GetTileRes(dm, pageNo), 0, 0);
    // only honor the request if there's a good chance that the
    // rendered tile will actually be used
    if (tile.res > 1) {
        return;
    }

    RequestRendering(dm, pageNo, tile);
    // render both tiles of the first row when splitting a page in four
    // (which always happens on larger displays for Fit Width)
    if (tile.res == 1 && !IsRenderQueueFull()) {
        tile.col = 1;
        RequestRendering(dm, pageNo, tile, false);
    }
}

/* Render a bitmap for page <pageNo> in <dm>. */
void RenderCache::RequestRendering(DisplayModel* dm, int pageNo, TilePosition tile, bool clearQueueForPage,
                                   const PredictiveChain* chain) {
    // rcLogf("RenderCache::RequestRendering: pageNo %d\n", pageNo);
    ScopedMutex scope(&requestAccess);
    ReportIf(!dm);
    if (!dm || dm->pauseRendering) {
        return;
    }

    for (int i = 0; i < nRenderThreads; i++) {
        auto* cr = curReqs[i];
        if (cr && cr->dm == dm && !dm->PageVisibleNearby(cr->pageNo)) {
            AbortCurrentRequest(i);
        }
    }

    int rotation = NormalizeRotation(dm->GetRotation());
    float zoom = dm->GetZoomReal(pageNo);

    for (int i = 0; i < nRenderThreads; i++) {
        auto* cr = curReqs[i];
        if (cr && (cr->pageNo == pageNo) && (cr->dm == dm) && (cr->tile == tile)) {
            if ((cr->zoom == zoom) && (cr->rotation == rotation)) {
                /* we're already rendering exactly the same page */
                return;
            }
            /* Currently rendered page is for the same page but with different zoom
            or rotation, so abort it */
            AbortCurrentRequest(i);
        }
    }

    // clear requests for tiles of different resolution and invisible tiles
    if (clearQueueForPage) {
        ClearQueueForDisplayModel(dm, pageNo, &tile);
    }

    for (int i = 0; i < requestCount; i++) {
        PageRenderRequest* req = &(requests[i]);
        if ((req->pageNo == pageNo) && (req->dm == dm) && (req->tile == tile)) {
            if ((req->zoom == zoom) && (req->rotation == rotation)) {
                /* Request with exactly the same parameters already queued for
                   rendering. Move it to the top of the queue so that it'll
                   be rendered faster. */
                PageRenderRequest tmp;
                tmp = requests[requestCount - 1];
                requests[requestCount - 1] = *req;
                *req = tmp;
            } else {
                /* There was a request queued for the same page but with different
                   zoom or rotation, so only replace this request */
                req->zoom = zoom;
                req->rotation = rotation;
            }
            return;
        }
    }

    if (Exists(dm, pageNo, rotation, zoom, &tile)) {
        /* This page has already been rendered in the correct dimensions
           and isn't about to be rerendered in different dimensions */
        return;
    }

    auto cb = MkMethod1<DisplayModel, PageRenderRequest*, &DisplayModel::RenderFinishedAsync>(dm);
    Render(dm, pageNo, rotation, zoom, &tile, nullptr, cb, chain);
}

// Start (or continue) a chain of predictive renders. Renders the first page in
// `pages` that still needs rendering, carrying the rest forward so that when it
// finishes the next one is requested, and so on - rendering predicted pages one
// at a time instead of flooding the queue. The chain stops once `originPageNo`
// (the visible page that started it) is no longer visible.
void RenderCache::RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages, int nPages) {
    ReportIf(!dm);
    if (!dm || dm->pauseRendering) {
        return;
    }
    // the view has moved on - don't keep rendering pages predicted for it
    if (!dm->PageVisible(originPageNo)) {
        return;
    }

    int rotation = NormalizeRotation(dm->GetRotation());
    // find the first page that actually needs rendering (skip cached/invalid)
    int i = 0;
    for (; i < nPages; i++) {
        int pageNo = pages[i];
        if (!dm->ValidPageNo(pageNo) || !dm->ShouldCacheRendering(pageNo)) {
            continue;
        }
        float zoom = dm->GetZoomReal(pageNo);
        if (zoom <= 0) {
            continue;
        }
        TilePosition tile(GetTileRes(dm, pageNo), 0, 0);
        if (tile.res > 1) {
            continue;
        }
        if (!Exists(dm, pageNo, rotation, zoom, &tile)) {
            break;
        }
    }
    if (i >= nPages) {
        // nothing left to predict
        return;
    }

    // carry the remaining pages forward so they're chained after this one
    PredictiveChain chain;
    chain.originPageNo = originPageNo;
    for (int j = i + 1; j < nPages; j++) {
        chain.pages[chain.nPages++] = pages[j];
    }
    TilePosition tile(GetTileRes(dm, pages[i]), 0, 0);
    RequestRendering(dm, pages[i], tile, true, &chain);
}

void RenderCache::Render(DisplayModel* dm, int pageNo, int rotation, float zoom, RectF pageRect,
                         const Func1<PageRenderRequest*>& callback) {
    bool ok = Render(dm, pageNo, rotation, zoom, nullptr, &pageRect, callback);
    if (!ok) {
        // create a dummy request to notify callback of failure
        PageRenderRequest req;
        req.dm = dm;
        req.pageNo = pageNo;
        req.bmp = nullptr;
        req.errorCode = 1;
        callback.Call(&req);
    }
}

bool RenderCache::Render(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition* tile, RectF* pageRect,
                         const Func1<PageRenderRequest*>& renderFinishedCb, const PredictiveChain* chain) {
    rcLogf("RenderCache::Render: pageNo %d\n", pageNo);
    ReportIf(!dm);
    if (!dm || dm->pauseRendering) {
        return false;
    }
    ReportIf(!renderFinishedCb.IsValid());

    ReportIf(!(tile || pageRect));
    if (!tile && !pageRect) {
        return false;
    }

    ScopedMutex scope(&requestAccess);
    PageRenderRequest* newRequest;

    /* add request to the queue */
    if (requestCount == MAX_PAGE_REQUESTS) {
        /* queue is full -> remove the oldest items on the queue */
        if (requests[0].renderFinishedCb.IsValid()) {
            requests[0].abort = true;
            requests[0].bmp = nullptr;
            requests[0].errorCode = 0;
            requests[0].renderFinishedCb.Call(&requests[0]);
        }
        memmove(&(requests[0]), &(requests[1]), sizeof(PageRenderRequest) * (MAX_PAGE_REQUESTS - 1));
        newRequest = &(requests[MAX_PAGE_REQUESTS - 1]);
    } else {
        newRequest = &(requests[requestCount]);
        requestCount++;
    }
    ReportIf(requestCount > MAX_PAGE_REQUESTS);

    newRequest->dm = dm;
    newRequest->pageNo = pageNo;
    newRequest->rotation = rotation;
    newRequest->zoom = zoom;
    if (tile) {
        newRequest->pageRect = GetTileRectUser(dm->GetEngine(), pageNo, rotation, zoom, *tile);
        newRequest->tile = *tile;
    } else if (pageRect) {
        newRequest->pageRect = *pageRect;
    } else {
        CrashMe();
    }
    newRequest->abort = false;
    newRequest->abortCookie = nullptr;
    newRequest->timestamp = GetTickCount();
    newRequest->bmp = nullptr;
    newRequest->errorCode = 0;
    newRequest->predictiveOriginPageNo = 0;
    newRequest->nPredictiveRequests = 0;
    if (chain) {
        newRequest->predictiveOriginPageNo = chain->originPageNo;
        newRequest->nPredictiveRequests = chain->nPages;
        for (int i = 0; i < chain->nPages; i++) {
            newRequest->predictiveRequests[i] = chain->pages[i];
        }
    }
    newRequest->renderFinishedCb = renderFinishedCb;

    ReleaseSemaphore(startRendering, 1, nullptr);

    // Lazy thread spawn: if no thread is currently waiting and we're below
    // the cap, start a new one. Existing busy threads will pick up the work
    // when they finish their current task.
    if (idleThreads == 0 && nRenderThreads < maxRenderThreads) {
        int idx = nRenderThreads;
        auto* td = new RenderThreadData{this, idx};
        renderThreads[idx] = CreateThread(nullptr, 0, RenderCacheThread, td, 0, nullptr);
        if (renderThreads[idx]) {
            nRenderThreads++;
        } else {
            delete td;
        }
    }

    UpdateRenderInfo();
    return true;
}

int RenderCache::GetRenderDelay(DisplayModel* dm, int pageNo, TilePosition tile) {
    ScopedMutex scope(&requestAccess);

    for (int i = 0; i < nRenderThreads; i++) {
        auto* cr = curReqs[i];
        if (cr && cr->pageNo == pageNo && cr->dm == dm && cr->tile == tile) {
            return GetTickCount() - cr->timestamp;
        }
    }

    for (int i = 0; i < requestCount; i++) {
        if (requests[i].pageNo == pageNo && requests[i].dm == dm && requests[i].tile == tile) {
            return GetTickCount() - requests[i].timestamp;
        }
    }

    return RENDER_DELAY_UNDEFINED;
}

bool RenderCache::GetNextRequest(PageRenderRequest* req, int threadIdx) {
    ScopedMutex scope(&requestAccess);

    if (requestCount == 0) {
        return false;
    }

    ReportIf(requestCount < 0);
    ReportIf(requestCount > MAX_PAGE_REQUESTS);
    requestCount--;
    *req = requests[requestCount];
    curReqs[threadIdx] = req;
    ReportIf(requestCount < 0);
    ReportIf(req->abort);

    UpdateRenderInfo();
    return true;
}

bool RenderCache::ClearCurrentRequest(int threadIdx) {
    ScopedMutex scope(&requestAccess);
    if (curReqs[threadIdx]) {
        RecordFinishedRequest(curReqs[threadIdx]);
        delete curReqs[threadIdx]->abortCookie;
    }
    curReqs[threadIdx] = nullptr;

    UpdateRenderInfo();
    bool isQueueEmpty = requestCount == 0;
    return isQueueEmpty;
}

/* Wait until rendering of a page beloging to <dm> has finished. */
/* TODO: this might take some time, would be good to show a dialog to let the
   user know he has to wait until we finish */
void RenderCache::CancelRendering(DisplayModel* dm) {
    ClearQueueForDisplayModel(dm);

    for (;;) {
        EnterCriticalSection(&requestAccess);
        bool found = false;
        for (int i = 0; i < nRenderThreads; i++) {
            if (curReqs[i] && curReqs[i]->dm == dm) {
                AbortCurrentRequest(i);
                found = true;
            }
        }
        if (!found) {
            // to be on the safe side
            ClearQueueForDisplayModel(dm);
            LeaveCriticalSection(&requestAccess);
            return;
        }
        LeaveCriticalSection(&requestAccess);

        /* TODO: busy loop is not good, but I don't have a better idea */
        Sleep(50);
    }
}

void RenderCache::ClearQueueForDisplayModel(DisplayModel* dm, int pageNo, TilePosition* tile) {
    ScopedMutex scope(&requestAccess);
    int reqCount = requestCount;
    int curPos = 0;
    for (int i = 0; i < reqCount; i++) {
        PageRenderRequest* req = &(requests[i]);
        bool shouldRemove = req->dm == dm && (pageNo == kInvalidPageNo || req->pageNo == pageNo) &&
                            (!tile || req->tile.res != tile->res || !IsTileVisible(dm, req->pageNo, *tile, 0.5));
        if (i != curPos) {
            requests[curPos] = requests[i];
        }
        if (shouldRemove) {
            // don't call renderFinishedCb for cleared requests - treat as aborted
            requestCount--;
        } else {
            curPos++;
        }
    }
    UpdateRenderInfo();
}

void RenderCache::AbortCurrentRequest(int threadIdx) {
    ScopedMutex scope(&requestAccess);
    auto* cr = curReqs[threadIdx];
    if (!cr) {
        return;
    }
    if (cr->abortCookie) {
        cr->abortCookie->Abort();
    }
    cr->abort = true;
    UpdateRenderInfo();
}

static DWORD WINAPI RenderCacheThread(LPVOID data) {
    auto* td = (RenderThreadData*)data;
    RenderCache* cache = td->cache;
    int threadIdx = td->threadIdx;
    delete td;

    PageRenderRequest req;
    Pixmap* bmp;

    for (;;) {
        if (AtomicBoolGet(&cache->shouldExit)) {
            break;
        }
        if (cache->ClearCurrentRequest(threadIdx)) {
            // Mark ourselves idle so Render() knows whether to spawn a new
            // thread when work appears. Increment before waiting, decrement
            // after waking (whether due to new work or shutdown).
            {
                ScopedMutex scope(&cache->requestAccess);
                cache->idleThreads++;
            }
            DWORD waitResult = WaitForSingleObject(cache->startRendering, INFINITE);
            {
                ScopedMutex scope(&cache->requestAccess);
                cache->idleThreads--;
            }
            if (AtomicBoolGet(&cache->shouldExit)) {
                break;
            }
            // Is it not a page render request?
            if (WAIT_OBJECT_0 != waitResult) {
                continue;
            }
        }

        if (!cache->GetNextRequest(&req, threadIdx)) {
            continue;
        }

        if (!req.dm->PageVisibleNearby(req.pageNo) && !req.renderFinishedCb.IsValid()) {
            continue;
        }

        if (req.dm->pauseRendering) {
            // aborted due to pause - do nothing
            continue;
        }

        ReportIf(req.abortCookie != nullptr);
        EngineBase* engine = req.dm->GetEngine();

        RenderPageArgs args(req.pageNo, req.zoom, req.rotation, &req.pageRect, RenderTarget::View, &req.abortCookie);
        // a previous render might have run a 3rd-party WIC codec that unmasked
        // fp exceptions on this thread, which would crash mupdf float math
        MaskFpExceptions();
        auto timeStart = TimeGet();
        bmp = engine->RenderPage(args);
        if (req.abort) {
            // aborted - do nothing, discard result
            FreePixmap(bmp);
            continue;
        }
        auto durMs = TimeSinceInMs(timeStart);
        if (durMs > 100) {
            auto path = engine->FilePath();
            logfa("Slow rendering: %.2f ms, page: %d in '%s'\n", (float)durMs, req.pageNo, path);
        }

        req.bmp = bmp;
        req.errorCode = bmp ? 0 : 1;

        if (bmp) {
            if (!engine->IsImageCollection()) {
                UpdateBitmapColors(bmp->hbmp, cache->textColor, cache->backgroundColor);
            }
            cache->Add(req, bmp);
            req.bmp = nullptr; // ownership transferred to cache
        }

        ReportIf(!req.renderFinishedCb.IsValid());
        req.renderFinishedCb.Call(&req);
        ResetTempArena();
    }
    logf("RenderCacheThread: exiting\n");
    DestroyTempArena();
    return 0;
}

// TODO: conceptually, RenderCache is not the right place for code that paints
//       (this is the only place that knows about Tiles, though)
int RenderCache::PaintTile(HDC hdc, Rect bounds, DisplayModel* dm, int pageNo, TilePosition tile, Rect tileOnScreen,
                           bool renderMissing, bool* renderOutOfDateCue, bool* renderedReplacement) {
    float zoom = dm->GetZoomReal(pageNo);
    BitmapCacheEntry* entry = Find(dm, pageNo, dm->GetRotation(), zoom, &tile);
    int renderDelay = 0;

    if (!entry) {
        if (!isRemoteSession) {
            if (renderedReplacement) {
                *renderedReplacement = true;
            }
            entry = Find(dm, pageNo, dm->GetRotation(), kInvalidZoom, &tile);
        }
        renderDelay = GetRenderDelay(dm, pageNo, tile);
        if (renderMissing && RENDER_DELAY_UNDEFINED == renderDelay && !IsRenderQueueFull()) {
            RequestRendering(dm, pageNo, tile);
            renderDelay = 1;
        }
    }
    Pixmap* renderedBmp = entry ? entry->bitmap : nullptr;
    HBITMAP hbmp = renderedBmp ? renderedBmp->hbmp : nullptr;

    if (!hbmp) {
        if (entry && !(renderedBmp && ReduceTileSize())) {
            renderDelay = RENDER_DELAY_FAILED;
        } else if (0 == renderDelay) {
            renderDelay = 1;
        }

        if (entry) {
            DropCacheEntry(entry);
        }
        return renderDelay;
    }

    HDC bmpDC = CreateCompatibleDC(hdc);
    if (bmpDC) {
        Size bmpSize = Size(renderedBmp->width, renderedBmp->height);
        int xSrc = -std::min(tileOnScreen.x, 0);
        int ySrc = -std::min(tileOnScreen.y, 0);
        float factor = std::min(1.0f * bmpSize.dx / tileOnScreen.dx, 1.0f * bmpSize.dy / tileOnScreen.dy);

        HGDIOBJ prevBmp = SelectObject(bmpDC, hbmp);
        int xDst = bounds.x;
        int yDst = bounds.y;
        int dxDst = bounds.dx;
        int dyDst = bounds.dy;
        if (factor != 1.0f) {
            xSrc = (int)(xSrc * factor);
            ySrc = (int)(ySrc * factor);
            int dxSrc = (int)(bounds.dx * factor);
            int dySrc = (int)(bounds.dy * factor);
            StretchBlt(hdc, xDst, yDst, dxDst, dyDst, bmpDC, xSrc, ySrc, dxSrc, dySrc, SRCCOPY);
        } else {
            BitBlt(hdc, xDst, yDst, dxDst, dyDst, bmpDC, xSrc, ySrc, SRCCOPY);
        }

        SelectObject(bmpDC, prevBmp);
        DeleteDC(bmpDC);

        if (gShowTileLayout) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0xff, 0x00));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            DrawRect(hdc, bounds);
            DeletePen(SelectObject(hdc, oldPen));
        }
    }

    if (entry->outOfDate) {
        if (renderOutOfDateCue) {
            *renderOutOfDateCue = true;
        }
        ReportIf(renderedReplacement && !*renderedReplacement);
    }

    DropCacheEntry(entry);
    return 0;
}

static int cmpTilePosition(const void* a, const void* b) {
    const TilePosition *ta = (const TilePosition*)a, *tb = (const TilePosition*)b;
    return ta->res != tb->res ? ta->res - tb->res : ta->row != tb->row ? ta->row - tb->row : ta->col - tb->col;
}

int RenderCache::Paint(HDC hdc, Rect bounds, DisplayModel* dm, int pageNo, PageInfo* pi, bool* renderOutOfDateCue) {
    ReportIf(!pi->isShown || 0.0 == pi->visibleRatio);

#if 0
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        rcLogf("RenderCache::Paint() pageNo: %d, bounds={%d,%d,%d,%d} in %.2f ms\n", pageNo, bounds.x, bounds.y, bounds.dx,
             bounds.dy, dur);
    };
#endif

    if (!dm->ShouldCacheRendering(pageNo)) {
        int rotation = dm->GetRotation();
        float zoom = dm->GetZoomReal(pageNo);
        bounds = pi->pageOnScreen.Intersect(bounds);

        RectF area = ToRectF(bounds);
        area.Offset((float)-pi->pageOnScreen.x, (float)-pi->pageOnScreen.y);
        area = dm->GetEngine()->Transform(area, pageNo, zoom, rotation, true);

        RenderPageArgs args(pageNo, zoom, rotation, &area);
        Pixmap* bmp = dm->GetEngine()->RenderPage(args);
        bool success = bmp && bmp->hbmp && BlitPixmap(bmp, hdc, bounds);
        FreePixmap(bmp);

        return success ? 0 : RENDER_DELAY_FAILED;
    }

    int rotation = dm->GetRotation();
    float zoom = dm->GetZoomReal(pageNo);
    USHORT targetRes = GetTileRes(dm, pageNo);
    USHORT maxRes = GetMaxTileRes(dm, pageNo, rotation);
    if (maxRes < targetRes) {
        maxRes = targetRes;
    }

    Vec<TilePosition> queue;
    queue.Append(TilePosition(0, 0, 0));
    int renderDelayMin = RENDER_DELAY_UNDEFINED;
    bool neededScaling = false;

    while (len(queue) > 0) {
        TilePosition tile = queue.PopAt(0);
        Rect tileOnScreen = GetTileOnScreen(dm->GetEngine(), pageNo, rotation, zoom, tile, pi->pageOnScreen);
        if (tileOnScreen.IsEmpty()) {
            // display an error message when only empty tiles should be drawn (i.e. on page loading errors)
            renderDelayMin = std::min(RENDER_DELAY_FAILED, renderDelayMin);
            continue;
        }
        tileOnScreen = pi->pageOnScreen.Intersect(tileOnScreen);
        Rect isect = bounds.Intersect(tileOnScreen);
        if (isect.IsEmpty()) {
            continue;
        }

        bool isTargetRes = tile.res == targetRes;
        int renderDelay = PaintTile(hdc, isect, dm, pageNo, tile, tileOnScreen, isTargetRes, renderOutOfDateCue,
                                    isTargetRes ? &neededScaling : nullptr);
        if (!(isTargetRes && 0 == renderDelay) && tile.res < maxRes) {
            queue.Append(TilePosition(tile.res + 1, tile.row * 2, tile.col * 2));
            queue.Append(TilePosition(tile.res + 1, tile.row * 2, tile.col * 2 + 1));
            queue.Append(TilePosition(tile.res + 1, tile.row * 2 + 1, tile.col * 2));
            queue.Append(TilePosition(tile.res + 1, tile.row * 2 + 1, tile.col * 2 + 1));
        }
        if (isTargetRes && renderDelay != 0) {
            neededScaling = true;
        }
        if (renderDelay == RENDER_DELAY_FAILED || renderDelayMin == RENDER_DELAY_FAILED) {
            renderDelayMin = RENDER_DELAY_FAILED;
        } else {
            renderDelayMin = std::min(renderDelay, renderDelayMin);
        }
        // paint tiles from left to right from top to bottom
        if (tile.res > 0 && len(queue) > 0 && tile.res < queue[0].res) {
            queue.Sort(cmpTilePosition);
        }
    }

    if (gConserveMemory) {
        if (!neededScaling) {
            if (renderOutOfDateCue) {
                *renderOutOfDateCue = false;
            }
            // free tiles with different resolution
            TilePosition tile(targetRes, (USHORT)-1, 0);
            rcLogf("RenderCache::Paint: calling FreePage() pageNo: %d\n", pageNo);
            FreePage(dm, pageNo, &tile);
        }
        FreeNotVisible();
    }

    return renderDelayMin;
}

void RenderCache::LogCacheSize() {
    ScopedMutex scope(&cacheAccess);
    i64 size = 0;
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* e = cache[i];
        if (e->bitmap) {
            i64 bs = PixmapByteSize(e->bitmap);
            size += bs;
        }
    }
}

// --------- render queue debug window (CmdDebugToggleRenderInfo) ---------

extern RenderCache* gRenderCache;

constexpr const WCHAR* kRenderInfoWinClass = L"SUMATRA_RENDER_INFO";

static HWND gRenderInfoHwnd = nullptr;
static HWND gRenderInfoEdit = nullptr;

bool IsRenderInfoWindowVisible() {
    return gRenderInfoHwnd != nullptr;
}

static void SerializePredictive(str::Builder& s, int originPageNo, int nPred, const int* pred) {
    if (nPred <= 0) {
        return;
    }
    s.Append(fmt("  pred[origin=%d:", originPageNo));
    for (int j = 0; j < nPred; j++) {
        s.Append(fmt(" %d", pred[j]));
    }
    s.Append("]");
}

static void SerializeRequest(str::Builder& s, Str label, PageRenderRequest* r, DWORD now) {
    int ageMs = (int)(now - r->timestamp);
    s.Append(fmt("%-9s page %3d  zoom %6.2f  rot %3d  tile[res=%d row=%d col=%d]  age %5dms", label, r->pageNo, r->zoom,
                 r->rotation, r->tile.res, r->tile.row, r->tile.col, ageMs));
    if (r->abort) {
        s.Append("  ABORT");
    }
    SerializePredictive(s, r->predictiveOriginPageNo, r->nPredictiveRequests, r->predictiveRequests);
    if (r->dm && r->dm->GetEngine()) {
        TempStr name = path::GetBaseNameTemp(r->dm->GetEngine()->FilePath());
        s.Append(fmt("  %s", name));
    }
    s.Append("\r\n");
}

static void SerializeFinished(str::Builder& s, FinishedRequestInfo* r, DWORD now) {
    int durMs = (int)(r->finishedAt - r->timestamp);
    int agoMs = (int)(now - r->finishedAt);
    Str label = r->aborted ? StrL("ABORTED") : StrL("DONE");
    s.Append(fmt("%-9s page %3d  zoom %6.2f  rot %3d  tile[res=%d row=%d col=%d]  took %5dms  %6dms ago", label,
                 r->pageNo, r->zoom, r->rotation, r->tile.res, r->tile.row, r->tile.col, durMs, agoMs));
    SerializePredictive(s, r->predictiveOriginPageNo, r->nPredictiveRequests, r->predictiveRequests);
    if (r->fileName[0]) {
        s.Append(fmt("  %s", Str(r->fileName)));
    }
    s.Append("\r\n");
}

void RenderCache::RecordFinishedRequest(PageRenderRequest* r) {
    FinishedRequestInfo& fi = finishedHistory[finishedHistoryNext];
    fi.pageNo = r->pageNo;
    fi.zoom = r->zoom;
    fi.rotation = r->rotation;
    fi.tile = r->tile;
    fi.timestamp = r->timestamp;
    fi.finishedAt = GetTickCount();
    fi.aborted = r->abort;
    fi.predictiveOriginPageNo = r->predictiveOriginPageNo;
    fi.nPredictiveRequests = r->nPredictiveRequests;
    for (int i = 0; i < kMaxPredictiveRequests; i++) {
        fi.predictiveRequests[i] = r->predictiveRequests[i];
    }
    fi.fileName[0] = 0;
    if (r->dm && r->dm->GetEngine()) {
        TempStr name = path::GetBaseNameTemp(r->dm->GetEngine()->FilePath());
        str::BufSet(Str(fi.fileName, dimof(fi.fileName)), name);
    }
    finishedHistoryNext = (finishedHistoryNext + 1) % kFinishedHistorySize;
    if (finishedHistoryCount < kFinishedHistorySize) {
        finishedHistoryCount++;
    }
}

void RenderCache::SerializeQueueState(str::Builder& s) {
    ScopedMutex scope(&requestAccess);
    DWORD now = GetTickCount();
    int nInProgress = 0;
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i]) {
            nInProgress++;
        }
    }
    s.Append(
        fmt("Render queue: %d rendering, %d queued (%d threads)\r\n\r\n", nInProgress, requestCount, nRenderThreads));

    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i]) {
            SerializeRequest(s, StrL("RENDERING"), curReqs[i], now);
        }
    }
    // queued requests are rendered LIFO, so list from top (next to render) down
    for (int i = requestCount - 1; i >= 0; i--) {
        SerializeRequest(s, StrL("QUEUED"), &requests[i], now);
    }

    // recently finished requests, most recently finished first
    if (finishedHistoryCount > 0) {
        s.Append(fmt("\r\nLast %d finished:\r\n", finishedHistoryCount));
        int idx = finishedHistoryNext - 1;
        for (int n = 0; n < finishedHistoryCount; n++) {
            if (idx < 0) {
                idx += kFinishedHistorySize;
            }
            SerializeFinished(s, &finishedHistory[idx], now);
            idx--;
        }
    }
}

static void SetRenderInfoTextOnUI(Str* s) {
    if (gRenderInfoEdit) {
        HwndSetText(gRenderInfoEdit, *s);
    }
    str::Free(*s);
    delete s;
}

void RenderCache::UpdateRenderInfo() {
    if (!IsRenderInfoWindowVisible()) {
        return;
    }
    str::Builder s;
    SerializeQueueState(s);
    // marshal to the UI thread: updating the window from a render thread while
    // holding requestAccess could deadlock if the UI thread is blocked on it
    auto dup = new Str(str::Dup(ToStr(s)));
    auto fn = MkFunc0<Str>(SetRenderInfoTextOnUI, dup);
    uitask::Post(fn, "RenderInfo");
}

static LRESULT CALLBACK WndProcRenderInfo(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (gRenderInfoEdit) {
                MoveWindow(gRenderInfoEdit, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            gRenderInfoHwnd = nullptr;
            gRenderInfoEdit = nullptr;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void CreateRenderInfoWindow() {
    HMODULE h = GetModuleHandleW(nullptr);
    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, kRenderInfoWinClass, WndProcRenderInfo);
    wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassEx(&wcex);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    HWND hwnd = CreateWindowExW(0, kRenderInfoWinClass, L"Render Queue Info", dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
                                700, 500, nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        return;
    }
    gRenderInfoHwnd = hwnd;

    Rect cRc = ClientRect(hwnd);
    DWORD editStyle =
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL;
    HWND hwndEdit = CreateWindowExW(0, WC_EDITW, L"", editStyle, 0, 0, cRc.dx, cRc.dy, hwnd, nullptr, h, nullptr);
    gRenderInfoEdit = hwndEdit;

    HDC hdc = GetDC(hwnd);
    HFONT font = CreateSimpleFont(hdc, "Consolas", 12);
    ReleaseDC(hwnd, hdc);
    if (font) {
        SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
    }
    ShowWindow(hwnd, SW_SHOW);
}

void ToggleRenderInfoWindow() {
    if (gRenderInfoHwnd) {
        DestroyWindow(gRenderInfoHwnd);
        return;
    }
    CreateRenderInfoWindow();
    if (gRenderCache) {
        gRenderCache->UpdateRenderInfo();
    }
}
