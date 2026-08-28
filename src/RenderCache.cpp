/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "gui/Dpi.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Timer.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "DarkMode_win.h"
#include "SumatraConfig.h"
#include "DocController.h"
#include "EngineBase.h"
#include "PdfDarkMode.h"
#include "DisplayModel.h"
#include "Canvas.h"
#include "RenderCache.h"

// CONSERVE_MEMORY sets the compile-time default for gConserveMemory. When defined,
// cached page bitmaps for non-visible pages are freed aggressively. Undefining it
// keeps more pages resident (higher GDI memory use, fewer re-renders).
#define CONSERVE_MEMORY

#ifdef CONSERVE_MEMORY
static bool gConserveMemory = true;
#else
bool gConserveMemory = false;
#endif

static DWORD WINAPI RenderCacheThread(LPVOID data);

static bool gShowTileLayout = false;
int gMaxRenderThreads = 8;

// Whether to run the bitmap recolor pass when no dark profile applies.
// MuPDF, DjVu, and native HTML-layout ebook engines (CHM, EPUB, MOBI, …)
// are recolored; image/comic collections keep original pixels.
static bool ShouldUpdateBitmapColorsLegacy(EngineBase* engine, RenderCache* cache) {
    (void)cache;
    if (EngineUsesReflowThemeCss(engine)) {
        return false;
    }
    return EngineUsesDocumentColorsFollowTheme(engine);
}

// Several preserved regions in one tile -> keep the largest artwork, drop layout
// ornaments. Always reduce to one region so patchy multi-image pages do not leave
// dark-recolored holes between photos (#5806).
static void FinalizeTileSkipRects(Vec<Rect>& skipRects, Size bmpSize) {
    if (len(skipRects) <= 1 || bmpSize.dx <= 0 || bmpSize.dy <= 0) {
        return;
    }
    int bestIdx = 0;
    i64 bestArea = 0;
    for (int i = 0; i < len(skipRects); i++) {
        i64 a = (i64)skipRects[i].dx * skipRects[i].dy;
        if (a > bestArea) {
            bestArea = a;
            bestIdx = i;
        }
    }
    Rect keep = skipRects[bestIdx];
    VecClear(skipRects);
    VecAppend(skipRects, keep);
}

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
    textColor = kColBlack;
    backgroundColor = kColWhite;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int numCores = (int)si.dwNumberOfProcessors;
    maxRenderThreads = std::max(gMaxRenderThreads, numCores);
    maxRenderThreads = std::min(maxRenderThreads, kMaxRenderThreads);

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
    // timeout fires -- after which destroying a still-in-use lock would
    // access-violate.
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
            SafeCloseThreadHandle(&renderThreads[i]);
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
}

/* Find a bitmap for a page defined by <dm> and <pageNo> and optionally also
   <rotation> and <zoom> in the cache - call DropCacheEntry when you
   no longer need a found entry. */
// out-of-line so RenderCache.h needn't include Pixmap.h (only forward-declare it)
BitmapCacheEntry::~BitmapCacheEntry() {
    FreePixmap(bitmap);
}

BitmapCacheEntry* RenderCache::Find(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition* tile) {
    ScopedRecursiveMutex scope(&cacheAccess);
    rotation = NormalizeRotation(rotation);
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* e = cache[i];
        if ((dm == e->dm) && (pageNo == e->pageNo) && (rotation == e->rotation) &&
            (kInvalidZoom == zoom || zoom == e->zoom) && (!tile || e->tile == *tile) &&
            (e->darkModeEpoch == darkModeEpoch)) {
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
    ScopedRecursiveMutex scope(&cacheAccess);
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

    RecordCacheChange(false, entry);

    delete entry;

    // fast removal by replacing freed item with the item at the end
    cache[idx] = nullptr;
    int lastIdx = cacheCount - 1;
    if ((lastIdx >= 0) && (idx != lastIdx)) {
        BitmapCacheEntry* moved = cache[lastIdx];
        ReportIf(!moved);
        if (moved) {
            moved->cacheIdx = idx;
            cache[idx] = moved;
        }
        cache[lastIdx] = nullptr;
    }
    cacheCount--;
    ReportIf(cacheCount < 0);

    // LogCacheSize();
    return true;
}

bool RenderCache::DropCacheEntryIfNotUsed(BitmapCacheEntry* entry) {
    ScopedRecursiveMutex scope(&cacheAccess);
    if (!entry || entry->refs > 1) {
        return false;
    }
    return DropCacheEntry(entry);
}

static bool FreeIfFull(RenderCache* rc, const PageRenderRequest& req) {
    int n = rc->cacheCount;
    if (n < kMaxBitmapsCached) {
        return true;
    }

    DisplayModel* dm = req.dm;
    // free an invisible page of the same DisplayModel ...
    for (int i = 0; i < n; i++) {
        auto* entry = rc->cache[i];
        if (entry->dm == dm && !dm->PageVisibleNearby(entry->pageNo)) {
            bool didDrop = rc->DropCacheEntryIfNotUsed(entry);
            if (didDrop) {
                return true;
            }
        }
    }

    // ... or just the oldest cached page
    for (int i = 0; i < n; i++) {
        auto* entry = rc->cache[i];
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
    ScopedRecursiveMutex scope(&cacheAccess);
    ReportIf(!req.dm);

    req.rotation = NormalizeRotation(req.rotation);
    ReportIf(cacheCount > kMaxBitmapsCached);

    /* It's possible there still is a cached bitmap with different zoom/rotation */
    FreePage(req.dm, req.pageNo, &req.tile);

    bool hasSpace = FreeIfFull(this, req);
    ReportIf(!hasSpace); // TODO: FreeIfFull() might actually fail to free
    ReportIf(cacheCount > kMaxBitmapsCached);
    if (!hasSpace || cacheCount >= kMaxBitmapsCached) {
        // Cannot grow past the fixed cache[]; drop this bitmap rather than overrun.
        FreePixmap(bmp);
        return;
    }

    // Copy the PageRenderRequest as it will be reused
    auto* entry = new BitmapCacheEntry(req.dm, req.pageNo, req.rotation, req.zoom, req.tile, bmp);
    entry->darkModeEpoch = darkModeEpoch;
    entry->cacheIdx = cacheCount;
    cache[cacheCount] = entry;
    cacheCount++;

    RecordCacheChange(true, entry);
}

static RectF GetTileRect(RectF pagerect, TilePosition tile) {
    ReportIf(tile.res > 30);
    RectF rect;
    rect.dx = pagerect.dx / (float)(1ULL << tile.res);
    rect.dy = pagerect.dy / (float)(1ULL << tile.res);
    rect.x = pagerect.x + ((float)tile.col * rect.dx);
    rect.y = pagerect.y + ((float)((1ULL << tile.res) - tile.row - 1) * rect.dy);
    return rect;
}

// get the coordinates of a specific tile
static Rect GetTileRectDevice(EngineBase* engine, int pageNo, int rotation, float zoom, TilePosition tile) {
    RectF mediabox = engine->PageMediabox(pageNo);
    if (tile.res > 0 && tile.res != kInvalidTileRes) {
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
    tileOnScreen.x -= (int)((float)tileOnScreen.dx * fuzz * 0.5);
    tileOnScreen.dx = (int)((float)tileOnScreen.dx * (fuzz + 1));
    tileOnScreen.y -= (int)((float)tileOnScreen.dy * fuzz * 0.5);
    tileOnScreen.dy = (int)((float)tileOnScreen.dy * (fuzz + 1));
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
    ScopedRecursiveMutex scope(&cacheAccess);

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
    ScopedRecursiveMutex scope(&cacheAccess);
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
    ScopedRecursiveMutex scope(&cacheAccess);
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
    ScopedRecursiveMutex scope(&cacheAccess);
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
    ScopedRecursiveMutex scopeReq(&requestAccess);

    ClearQueueForDisplayModel(dm, pageNo);
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i] && curReqs[i]->dm == dm && curReqs[i]->pageNo == pageNo) {
            AbortCurrentRequest(i);
        }
    }

    ScopedRecursiveMutex scopeCache(&cacheAccess);

    RectF mediabox = dm->GetEngine()->PageMediabox(pageNo);
    for (int i = 0; i < cacheCount; i++) {
        auto* e = cache[i];
        if (e->dm == dm && e->pageNo == pageNo && !GetTileRect(mediabox, e->tile).Intersect(rect).IsEmpty()) {
            e->zoom = kInvalidZoom;
            e->outOfDate = true;
        }
    }
}

// determine the count of tiles required for a page at a given zoom level
USHORT RenderCache::GetTileRes(DisplayModel* dm, int pageNo) const {
    auto* engine = dm->GetEngine();
    RectF mediabox = engine->PageMediabox(pageNo);
    float zoom = dm->GetZoomReal(pageNo);
    float zoomVirt = dm->GetZoomVirtual();
    Rect viewPort = dm->GetViewPort();
    int rotation = dm->GetRotation();
    RectF pixelbox = engine->Transform(mediabox, pageNo, zoom, rotation);

    float factorW = pixelbox.dx / (float)(maxTileSize.dx + 1);
    float factorH = pixelbox.dy / (float)(maxTileSize.dy + 1);
    // using the geometric mean instead of the maximum factor
    // so that the tile area doesn't get too small in comparison
    // to maxTileSize (but remains smaller)
    float factorAvg = sqrtf(factorW * factorH);

    // use larger tiles when fitting page or width or when a page is smaller
    // than the visible canvas width/height or when rendering pages
    // without clipping optimizations
    if (zoomVirt == kZoomFitPage || zoomVirt == kZoomFitWidth || pixelbox.dx <= (float)viewPort.dx ||
        pixelbox.dy < (float)viewPort.dy || !engine->HasClipOptimizations(pageNo)) {
        factorAvg /= 2.0;
    }

    USHORT res = 0;
    if (factorAvg > 1.5) {
        res = (USHORT)ceilf(logf(factorAvg) / logf(2.0f));
    }
    // limit res to 30, so that (1 << res) doesn't overflow for 32-bit signed int
    return std::min(res, (USHORT)30);
}

// get the maximum resolution available for the given page
USHORT RenderCache::GetMaxTileRes(DisplayModel* dm, int pageNo, int rotation) {
    ScopedRecursiveMutex scope(&cacheAccess);
    USHORT maxRes = 0;
    for (int i = 0; i < cacheCount; i++) {
        auto* e = cache[i];
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

    ScopedRecursiveMutex scope1(&requestAccess);
    ScopedRecursiveMutex scope2(&cacheAccess);

    if (maxTileSize.dx > maxTileSize.dy) {
        maxTileSize.dx /= 2;
    } else {
        maxTileSize.dy /= 2;
    }
    nTileSizeReductions++;

    // invalidate all rendered bitmaps and all requests (force-clear: PaintTile may
    // hold refs from Find(), so DropCacheEntryIfNotUsed would never make progress)
    for (int i = cacheCount - 1; i >= 0; i--) {
        delete cache[i];
        cache[i] = nullptr;
    }
    cacheCount = 0;
    requestCount = 0;
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
    ScopedRecursiveMutex scope(&requestAccess);
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
        // an aborted request will be discarded when the render thread notices, so
        // it doesn't count as rendering this page - we must queue a new request
        bool isRenderingTile = cr && !cr->abort && (cr->pageNo == pageNo) && (cr->dm == dm) && (cr->tile == tile);
        if (isRenderingTile) {
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
// start (or continue) a chained predictive render anchored to originPageNo
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

    ScopedRecursiveMutex scope(&requestAccess);
    PageRenderRequest* newRequest;

    /* add request to the queue */
    if (requestCount == kMaxPageRequests) {
        /* queue is full -> remove the oldest items on the queue */
        if (requests[0].renderFinishedCb.IsValid()) {
            requests[0].abort = true;
            requests[0].bmp = nullptr;
            requests[0].errorCode = 0;
            requests[0].renderFinishedCb.Call(&requests[0]);
        }
        // PageRenderRequest holds a Func1, so it isn't trivially copyable and
        // memmove() over it is undefined; shift with assignment instead
        for (int i = 0; i < kMaxPageRequests - 1; i++) {
            requests[i] = requests[i + 1];
        }
        newRequest = &(requests[kMaxPageRequests - 1]);
    } else {
        newRequest = &(requests[requestCount]);
        requestCount++;
    }
    ReportIf(requestCount > kMaxPageRequests);

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
    newRequest->timestamp = GetTickCount64();
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
    ScopedRecursiveMutex scope(&requestAccess);

    for (int i = 0; i < nRenderThreads; i++) {
        auto* cr = curReqs[i];
        // an aborted request produces no bitmap, so don't report it as a pending
        // render - the caller would wait for a result that never arrives
        bool isRenderingTile = cr && !cr->abort && (cr->pageNo == pageNo) && (cr->dm == dm) && (cr->tile == tile);
        if (isRenderingTile) {
            return (int)(GetTickCount64() - cr->timestamp);
        }
    }

    for (int i = 0; i < requestCount; i++) {
        if (requests[i].pageNo == pageNo && requests[i].dm == dm && requests[i].tile == tile) {
            return (int)(GetTickCount64() - requests[i].timestamp);
        }
    }

    return kRenderDelayUndefined;
}

bool RenderCache::GetNextRequest(PageRenderRequest* req, int threadIdx) {
    ScopedRecursiveMutex scope(&requestAccess);

    if (requestCount <= 0 || requestCount > kMaxPageRequests) {
        return false;
    }

    int idx = requestCount - 1;
    requestCount = idx;
    *req = requests[idx];
    req->darkModeEpoch = darkModeEpoch;
    curReqs[threadIdx] = req;
    ReportIf(req->abort);

    UpdateRenderInfo();
    return true;
}

bool RenderCache::ClearCurrentRequest(int threadIdx) {
    ScopedRecursiveMutex scope(&requestAccess);
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
void RenderCache::CancelRenderingBlocking(DisplayModel* dm) {
    ClearQueueForDisplayModel(dm);

    for (;;) {
        requestAccess.Lock();
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
            requestAccess.Unlock();
            return;
        }
        requestAccess.Unlock();

        /* TODO: busy loop is not good, but I don't have a better idea */
        Sleep(50);
    }
}

// Like CancelRenderingBlocking() but returns immediately instead of waiting for an
// in-flight render to notice the abort. mupdf only checks the abort cookie
// between display-list ops, so a single big image decode makes that wait run
// into the hundreds of ms -- on the UI thread it's a visible freeze.
//
// Only for callers that are NOT destroying dm: the render thread keeps using
// dm and its engine until the current page is done. Anything that frees the
// model must still use CancelRenderingBlocking() (DisplayModel's destructor does).
// true if a render thread is currently working on a page of dm. Only a snapshot:
// no new requests can appear for a dm that's being torn down (its tab is gone
// and pauseRendering is set), so a false answer stays false.
bool RenderCache::IsRenderingFor(DisplayModel* dm) {
    ScopedRecursiveMutex scope(&requestAccess);
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i] && curReqs[i]->dm == dm) {
            return true;
        }
    }
    return false;
}

// What the render threads and the cache are doing right now, in one line. A
// test (or a CI run) that times out waiting for a render otherwise only knows
// "still busy", which doesn't say whether one tile is taking forever, tiles
// keep being thrown away and rendered again, or the tiles are simply huge.
TempStr RenderCache::BusyInfoTemp(DisplayModel* dm) {
    ScopedRecursiveMutex scope(&requestAccess);
    u64 now = GetTickCount64();
    TempStr res =
        fmt("tile=%dx%d cache=%d reduced=%d", maxTileSize.dx, maxTileSize.dy, cacheCount, nTileSizeReductions);
    for (int i = 0; i < nRenderThreads; i++) {
        auto* r = curReqs[i];
        if (!r) {
            continue;
        }
        u64 age = r->timestamp <= now ? now - r->timestamp : 0;
        Str aborted = r->abort ? StrL(",abort") : Str();
        Str otherDm = (dm && r->dm != dm) ? StrL(",other-dm") : Str();
        TempStr one = fmt(" t%d=p%d,res%d,r%dc%d,%dms%s%s", i, r->pageNo, (int)r->tile.res, (int)r->tile.row,
                          (int)r->tile.col, (int)age, aborted, otherDm);
        res = str::JoinTemp(res, one);
    }
    return res;
}

// true if a worker is rendering a visible page of dm or one is queued.
// Off-screen predictive work does not count: the picture the user (or a
// test capture) sees does not wait on those.
bool RenderCache::IsBusyFor(DisplayModel* dm) {
    if (!dm) {
        return false;
    }
    ScopedRecursiveMutex scope(&requestAccess);
    auto isVisibleReq = [&](PageRenderRequest* r) -> bool {
        return r && r->dm == dm && !r->abort && dm->PageVisible(r->pageNo);
    };
    for (int i = 0; i < nRenderThreads; i++) {
        if (isVisibleReq(curReqs[i])) {
            return true;
        }
    }
    for (int i = 0; i < requestCount; i++) {
        if (isVisibleReq(&requests[i])) {
            return true;
        }
    }
    return false;
}

// true when every on-screen tile of dm is cached at the resolution Paint()
// would ask for. A low-res preview (res 0, or a tile from another zoom) does
// not count: that is the picture waitForWindowIdle mistakes for "done".
bool RenderCache::VisibleTargetTilesReady(DisplayModel* dm, Str* whyNot) {
    auto no = [whyNot](TempStr reason) -> bool {
        if (whyNot) {
            *whyNot = str::DupTemp(reason);
        }
        return false;
    };
    if (!dm || !dm->GetEngine()) {
        return no(StrL("no-dm"));
    }
    int pageCount = dm->PageCount();
    bool anyVisible = false;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || pi->visibleRatio == 0) {
            continue;
        }
        if (pi->pageOnScreen.IsEmpty()) {
            return no(fmt("p%d no-rect", pageNo));
        }
        anyVisible = true;
        if (!dm->ShouldCacheRendering(pageNo)) {
            continue;
        }
        int rotation = dm->GetRotation();
        float zoom = dm->GetZoomReal(pageNo);
        if (zoom <= 0) {
            return no(fmt("p%d zoom=%.2f", pageNo, zoom));
        }
        USHORT targetRes = GetTileRes(dm, pageNo);
        Rect screen(Point(), dm->GetViewPort().Size());
        // same subdivision Paint() uses, so we only look at tiles that
        // actually show — at 1000000% a full 2^res grid would be millions
        Vec<TilePosition> queue;
        VecAppend(queue, TilePosition(0, 0, 0));
        bool sawTarget = false;
        while (len(queue) > 0) {
            TilePosition tile = VecPopAt(queue, 0);
            Rect tileOnScreen = GetTileOnScreen(dm->GetEngine(), pageNo, rotation, zoom, tile, pi->pageOnScreen);
            if (tileOnScreen.IsEmpty()) {
                continue;
            }
            tileOnScreen = pi->pageOnScreen.Intersect(tileOnScreen);
            if (tileOnScreen.IsEmpty() || tileOnScreen.Intersect(screen).IsEmpty()) {
                continue;
            }
            if (tile.res == targetRes) {
                sawTarget = true;
                if (!Exists(dm, pageNo, rotation, zoom, &tile)) {
                    return no(fmt("p%d miss res=%d r%d,c%d", pageNo, (int)tile.res, (int)tile.row, (int)tile.col));
                }
                continue;
            }
            if (tile.res >= targetRes) {
                continue;
            }
            VecAppend(queue, TilePosition((USHORT)(tile.res + 1), (USHORT)(tile.row * 2), (USHORT)(tile.col * 2)));
            VecAppend(queue,
                      TilePosition((USHORT)(tile.res + 1), (USHORT)(tile.row * 2), (USHORT)((tile.col * 2) + 1)));
            VecAppend(queue,
                      TilePosition((USHORT)(tile.res + 1), (USHORT)((tile.row * 2) + 1), (USHORT)(tile.col * 2)));
            VecAppend(queue,
                      TilePosition((USHORT)(tile.res + 1), (USHORT)((tile.row * 2) + 1), (USHORT)((tile.col * 2) + 1)));
        }
        if (!sawTarget) {
            return no(fmt("p%d no-tile-at-res=%d", pageNo, (int)targetRes));
        }
    }
    if (!anyVisible) {
        // No page overlaps the viewport. That is a real, settled state, not
        // work in progress: a document with one page far wider than the rest
        // centers the narrow ones in a canvas as wide as the widest, so
        // scrolled to the left edge the viewport can show no page at all
        // (issue #1438's document). There is nothing left to render, so this
        // is idle - unless the pages have not been laid out yet, which is what
        // an empty canvas means.
        if (dm->GetCanvasSize().IsEmpty()) {
            return no(StrL("no-layout"));
        }
    }
    return true;
}

void RenderCache::AbortRendering(DisplayModel* dm) {
    ScopedRecursiveMutex scope(&requestAccess);
    ClearQueueForDisplayModel(dm);
    for (int i = 0; i < nRenderThreads; i++) {
        if (curReqs[i] && curReqs[i]->dm == dm) {
            AbortCurrentRequest(i);
        }
    }
}

void RenderCache::ClearQueueForDisplayModel(DisplayModel* dm, int pageNo, TilePosition* tile) {
    ScopedRecursiveMutex scope(&requestAccess);
    int reqCount = requestCount;
    int curPos = 0;
    for (int i = 0; i < reqCount; i++) {
        PageRenderRequest* req = &(requests[i]);
        bool shouldRemove = req->dm == dm && (pageNo == kRenderCacheAllPages || req->pageNo == pageNo) &&
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
    ScopedRecursiveMutex scope(&requestAccess);
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
                ScopedRecursiveMutex scope(&cache->requestAccess);
                cache->idleThreads++;
            }
            DWORD waitResult = WaitForSingleObject(cache->startRendering, INFINITE);
            {
                ScopedRecursiveMutex scope(&cache->requestAccess);
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
        // the canvas paints the document background before drawing the page,
        // so a page with transparency composites over it (#5844)
        args.keepAlpha = true;
        args.transparentBackdrop = ShowTransparencyGrid();
        DarkModeProfile darkProfile;
        BuildViewDarkModeProfile(engine, &darkProfile);
        if (darkProfile.mode != PageColorMode::Normal) {
            args.darkProfile = &darkProfile;
        }
        // a previous render might have run a 3rd-party WIC codec that unmasked
        // fp exceptions on this thread, which would crash mupdf float math
        MaskFpExceptions();
        auto timeStart = TimeGet();
        bmp = engine->RenderPage(args);
        if (req.abort || req.darkModeEpoch != cache->darkModeEpoch) {
            // aborted or colors changed mid-render - discard result
            FreePixmap(bmp);
            continue;
        }
        auto durMs = TimeSinceInMs(timeStart);
        if (durMs > 300) {
            auto path = engine->FilePath();
            logf("Slow rendering: %.2f ms, page: %d in '%s'\n", (float)durMs, req.pageNo, path);
        }

        req.bmp = bmp;
        req.errorCode = bmp ? 0 : 1;

        if (bmp) {
            const DarkModeProfile* profile = args.darkProfile;
            bool recolor;
            if (profile) {
                // object-level smart dark renders themed output directly
                recolor = DarkModeProfileUsesLegacyPostProcess(profile);
            } else {
                recolor = ShouldUpdateBitmapColorsLegacy(engine, cache);
            }
            if (recolor && !bmp->hasAlpha) {
                bool preserve = profile && profile->mode == PageColorMode::PreserveImages && profile->preservePdfImages;
                Vec<Rect> skipRects;
                Vec<Rect>* skipRectsPtr = nullptr;
                if (preserve) {
                    Size bmpSize(bmp->width, bmp->height);
                    engine->GetBitmapRecolorSkipRects(req.pageNo, req.zoom, req.rotation, req.pageRect, bmpSize,
                                                      skipRects);
                    FinalizeTileSkipRects(skipRects, bmpSize);
                    if (len(skipRects) > 0) {
                        skipRectsPtr = &skipRects;
                    }
                }
                Color textCol = profile ? profile->foreground : cache->textColor;
                Color bgCol = profile ? profile->pageBackground : cache->backgroundColor;
                Color linkCol = profile ? profile->linkColor : cache->linkColor;
                RecolorPixmap(bmp, textCol, bgCol, linkCol, skipRectsPtr);
            }
            if (req.abort || req.darkModeEpoch != cache->darkModeEpoch) {
                // colors changed while recoloring - discard result
                FreePixmap(bmp);
                req.bmp = nullptr;
                continue;
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
        if (renderMissing && kRenderDelayUndefined == renderDelay && !IsRenderQueueFull()) {
            RequestRendering(dm, pageNo, tile);
            renderDelay = 1;
        }
    }
    Pixmap* renderedBmp = entry ? entry->bitmap : nullptr;

    if (!renderedBmp || !renderedBmp->data) {
        bool didReduce = entry && ReduceTileSize();
        if (entry && !didReduce) {
            renderDelay = kRenderDelayFailed;
        } else if (0 == renderDelay) {
            renderDelay = 1;
        }

        // ReduceTileSize() deletes all cache entries; don't touch a stale pointer
        if (entry && !didReduce) {
            DropCacheEntry(entry);
        }
        return renderDelay;
    }

    Size bmpSize = Size(renderedBmp->width, renderedBmp->height);
    int xSrc = -std::min(tileOnScreen.x, 0);
    int ySrc = -std::min(tileOnScreen.y, 0);
    float factor =
        std::min(1.0f * (float)bmpSize.dx / (float)tileOnScreen.dx, 1.0f * (float)bmpSize.dy / (float)tileOnScreen.dy);

    Rect target = bounds;
    Rect source(xSrc, ySrc, bounds.dx, bounds.dy);
    if (factor != 1.0f) {
        source.x = (int)((float)xSrc * factor);
        source.y = (int)((float)ySrc * factor);
        source.dx = (int)((float)bounds.dx * factor);
        source.dy = (int)((float)bounds.dy * factor);
    }
    BlitPixmapRegion(renderedBmp, hdc, target, source);

    if (gShowTileLayout) {
        HPEN pen = CreatePen(PS_SOLID, 1, kColYellow);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HdcDrawRect(hdc, bounds);
        DeletePen(SelectObject(hdc, oldPen));
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

static int cmpTilePosition(const TilePosition* a, const TilePosition* b) {
    if (a->res != b->res) {
        return a->res - b->res;
    }
    if (a->row != b->row) {
        return a->row - b->row;
    }
    return a->col - b->col;
}

// returns how much time in ms has past since the most recent rendering
// request for the visible part of the page if nothing at all could be
// painted, 0 if something has been painted and kRenderDelayFailed on failure
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
        args.keepAlpha = true; // see the other RenderPageArgs above (#5844)
        args.transparentBackdrop = ShowTransparencyGrid();
        Pixmap* bmp = dm->GetEngine()->RenderPage(args);
        bool success = bmp && BlitPixmap(bmp, hdc, bounds);
        FreePixmap(bmp);

        return success ? 0 : kRenderDelayFailed;
    }

    int rotation = dm->GetRotation();
    float zoom = dm->GetZoomReal(pageNo);
    USHORT targetRes = GetTileRes(dm, pageNo);
    USHORT maxRes = GetMaxTileRes(dm, pageNo, rotation);
    maxRes = std::max(maxRes, targetRes);

    Vec<TilePosition> queue;
    VecAppend(queue, TilePosition(0, 0, 0));
    int renderDelayMin = kRenderDelayUndefined;
    bool neededScaling = false;

    while (len(queue) > 0) {
        TilePosition tile = VecPopAt(queue, 0);
        Rect tileOnScreen = GetTileOnScreen(dm->GetEngine(), pageNo, rotation, zoom, tile, pi->pageOnScreen);
        if (tileOnScreen.IsEmpty()) {
            // display an error message when only empty tiles should be drawn (i.e. on page loading errors)
            renderDelayMin = std::min(kRenderDelayFailed, renderDelayMin);
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
            VecAppend(queue, TilePosition(tile.res + 1, tile.row * 2, tile.col * 2));
            VecAppend(queue, TilePosition(tile.res + 1, tile.row * 2, (tile.col * 2) + 1));
            VecAppend(queue, TilePosition(tile.res + 1, (tile.row * 2) + 1, tile.col * 2));
            VecAppend(queue, TilePosition(tile.res + 1, (tile.row * 2) + 1, (tile.col * 2) + 1));
        }
        if (isTargetRes && renderDelay != 0) {
            neededScaling = true;
        }
        if (renderDelay == kRenderDelayFailed || renderDelayMin == kRenderDelayFailed) {
            renderDelayMin = kRenderDelayFailed;
        } else {
            renderDelayMin = std::min(renderDelay, renderDelayMin);
        }
        // paint tiles from left to right from top to bottom
        if (tile.res > 0 && len(queue) > 0 && tile.res < queue[0].res) {
            VecSort(queue, cmpTilePosition);
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
    ScopedRecursiveMutex scope(&cacheAccess);
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

struct DebugTextWnd : WindowBase {
    Edit* edit = nullptr;
    PlatformFont* monoFont = nullptr;

    bool Create(Str title, int fontSize);
    void UpdateTheme() override;
    void ApplyDarkMode() override;
    void SetTextContent(Str text);
};

void DebugTextWnd::ApplyDarkMode() {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

void DebugTextWnd::UpdateTheme() {
    WindowBase::UpdateTheme();
    // Re-apply monospaced font after darkmode child theming (may reset font).
    if (edit && monoFont) {
        edit->SetFont(monoFont);
    }
}

void DebugTextWnd::SetTextContent(Str text) {
    if (edit) {
        edit->SetText(text);
    }
}

bool DebugTextWnd::Create(Str title, int fontSize) {
    {
        CreateCustomArgs args;
        args.title = title;
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    Edit::CreateArgs args;
    args.parent = hwnd;
    args.isMultiLine = true;
    args.withBorder = true;
    edit = new Edit();
    edit->Create(args);
    SendMessageW(edit->hwnd, EM_SETREADONLY, TRUE, 0);
    SendMessageW(edit->hwnd, EM_SETLIMITTEXT, 0, 0);

    HDC hdc = GetDC(hwnd);
    monoFont = HdcCreateSimpleFont(hdc, StrL("Consolas"), fontSize);
    ReleaseDC(hwnd, hdc);
    if (monoFont) {
        edit->SetFont(monoFont);
    }
    layout = edit;

    int winW = DpiScale(700);
    int winH = DpiScale(500);
    SetWindowPos(hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);
    DoLayout();
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

static DebugTextWnd* gRenderInfoWnd = nullptr;
static DebugTextWnd* gCacheInfoWnd = nullptr;

static void TeardownDebugTextWnd(DebugTextWnd** slot, DebugTextWnd* w) {
    if (!slot || !*slot || *slot != w) {
        return;
    }
    *slot = nullptr;
    w->ScheduleDelete();
}

static void CloseDebugTextWnd(DebugTextWnd** slot) {
    if (!slot || !*slot) {
        return;
    }
    DebugTextWnd* w = *slot;
    if (w->hwnd && IsWindow(w->hwnd)) {
        w->Close();
    } else {
        TeardownDebugTextWnd(slot, w);
    }
}

static void OnRenderInfoClose(WindowBase::CloseEvent* ev) {
    TeardownDebugTextWnd(&gRenderInfoWnd, (DebugTextWnd*)ev->e->self);
}

static void OnRenderInfoDestroy(WindowBase::DestroyEvent* ev) {
    TeardownDebugTextWnd(&gRenderInfoWnd, (DebugTextWnd*)ev->e->self);
}

static void OnCacheInfoClose(WindowBase::CloseEvent* ev) {
    TeardownDebugTextWnd(&gCacheInfoWnd, (DebugTextWnd*)ev->e->self);
}

static void OnCacheInfoDestroy(WindowBase::DestroyEvent* ev) {
    TeardownDebugTextWnd(&gCacheInfoWnd, (DebugTextWnd*)ev->e->self);
}

bool IsRenderInfoWindowVisible() {
    return gRenderInfoWnd && gRenderInfoWnd->hwnd && IsWindow(gRenderInfoWnd->hwnd);
}

static void SerializePredictive(str::Builder& s, int originPageNo, int nPred, const int* pred) {
    if (nPred <= 0) {
        return;
    }
    s.Append(fmt("  pred[origin=%d:", originPageNo));
    for (int j = 0; j < nPred; j++) {
        s.Append(fmt(" %d", pred[j]));
    }
    s.Append(StrL("]"));
}

static void SerializeRequest(str::Builder& s, Str label, PageRenderRequest* r, u64 now) {
    int ageMs = (int)(now - r->timestamp);
    s.Append(fmt("%-9s page %3d  zoom %6.2f  rot %3d  tile[res=%d row=%d col=%d]  age %5dms", label, r->pageNo, r->zoom,
                 r->rotation, r->tile.res, r->tile.row, r->tile.col, ageMs));
    if (r->abort) {
        s.Append(StrL("  ABORT"));
    }
    SerializePredictive(s, r->predictiveOriginPageNo, r->nPredictiveRequests, r->predictiveRequests);
    if (r->dm && r->dm->GetEngine()) {
        TempStr name = path::GetBaseNameTemp(r->dm->GetEngine()->FilePath());
        s.Append(fmt("  %s", name));
    }
    s.Append(StrL("\r\n"));
}

static void SerializeFinished(str::Builder& s, FinishedRequestInfo* r, u64 now) {
    int durMs = (int)(r->finishedAt - r->timestamp);
    int agoMs = (int)(now - r->finishedAt);
    Str label = r->aborted ? StrL("ABORTED") : StrL("DONE");
    s.Append(fmt("%-9s page %3d  zoom %6.2f  rot %3d  tile[res=%d row=%d col=%d]  took %5dms  %6dms ago", label,
                 r->pageNo, r->zoom, r->rotation, r->tile.res, r->tile.row, r->tile.col, durMs, agoMs));
    SerializePredictive(s, r->predictiveOriginPageNo, r->nPredictiveRequests, r->predictiveRequests);
    if (r->fileName[0]) {
        s.Append(fmt("  %s", Str(r->fileName)));
    }
    s.Append(StrL("\r\n"));
}

// record a just-finished request in finishedHistory (call holding requestAccess)
void RenderCache::RecordFinishedRequest(PageRenderRequest* r) {
    FinishedRequestInfo& fi = finishedHistory[finishedHistoryNext];
    fi.pageNo = r->pageNo;
    fi.zoom = r->zoom;
    fi.rotation = r->rotation;
    fi.tile = r->tile;
    fi.timestamp = r->timestamp;
    fi.finishedAt = GetTickCount64();
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

// serialize the queue (in-progress + queued requests) as plain text, one
// line per request, for the render-info debug window
void RenderCache::SerializeQueueState(str::Builder& s) {
    ScopedRecursiveMutex scope(&requestAccess);
    u64 now = GetTickCount64();
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
    if (gRenderInfoWnd) {
        gRenderInfoWnd->SetTextContent(*s);
    }
    str::Free(*s);
    delete s;
}

// if the render-info debug window is shown, refresh it with the current
// queue state. Cheap no-op when the window is hidden. Safe to call from
// any thread (and while holding requestAccess).
void RenderCache::UpdateRenderInfo() {
    if (!IsRenderInfoWindowVisible()) {
        return;
    }
    str::Builder s;
    SerializeQueueState(s);
    // marshal to the UI thread: updating the window from a render thread while
    // holding requestAccess could deadlock if the UI thread is blocked on it
    auto* dup = new Str(str::Dup(ToStr(s)));
    auto fn = MkFunc0<Str>(SetRenderInfoTextOnUI, dup);
    uitask::Post(fn, "RenderInfo");
}

static void CreateRenderInfoWindow() {
    auto* wnd = new DebugTextWnd();
    wnd->closeOnEsc = gSettings->escToExit;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnRenderInfoClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnRenderInfoDestroy);
    if (!wnd->Create(StrL("Render Queue Info"), 12)) {
        delete wnd;
        return;
    }
    gRenderInfoWnd = wnd;
}

// render queue debug window (CmdDebugToggleRenderInfo)
void ToggleRenderInfoWindow() {
    if (gRenderInfoWnd) {
        CloseDebugTextWnd(&gRenderInfoWnd);
        return;
    }
    CreateRenderInfoWindow();
    if (gRenderCache) {
        gRenderCache->UpdateRenderInfo();
    }
}

// --------- bitmap cache debug window (CmdDebugToggleCacheInfo) ---------

bool IsCacheInfoWindowVisible() {
    return gCacheInfoWnd && gCacheInfoWnd->hwnd && IsWindow(gCacheInfoWnd->hwnd);
}

static TempStr FormatCacheBytesTemp(i64 bytes) {
    if (bytes < 1024) {
        return fmt("%d B", (int)bytes);
    }
    if (bytes < 1024LL * 1024) {
        return fmt("%.1f KB", bytes / 1024.0);
    }
    return fmt("%.2f MB", bytes / (1024.0 * 1024.0));
}

static void SetDmFileName(DisplayModel* dm, char* buf, int bufLen) {
    buf[0] = 0;
    if (dm && dm->GetEngine()) {
        TempStr name = path::GetBaseNameTemp(dm->GetEngine()->FilePath());
        str::BufSet(Str(buf, bufLen), name);
    }
}

// record a cache add/remove in cacheHistory (call holding cacheAccess)
void RenderCache::RecordCacheChange(bool isAdd, BitmapCacheEntry* entry) {
    ReportIf(!entry);
    if (!entry) {
        return;
    }
    CacheChangeInfo& ci = cacheHistory[cacheHistoryNext];
    ci.isAdd = isAdd;
    ci.pageNo = entry->pageNo;
    ci.zoom = entry->zoom;
    ci.rotation = entry->rotation;
    ci.tile = entry->tile;
    ci.bytes = entry->bitmap ? PixmapByteSize(entry->bitmap) : 0;
    ci.timestamp = GetTickCount64();
    SetDmFileName(entry->dm, ci.fileName, dimof(ci.fileName));
    cacheHistoryNext = (cacheHistoryNext + 1) % kCacheHistorySize;
    if (cacheHistoryCount < kCacheHistorySize) {
        cacheHistoryCount++;
    }
    UpdateCacheInfo();
}

static void SerializeCacheChange(str::Builder& s, CacheChangeInfo* c, u64 now) {
    Str label = c->isAdd ? StrL("ADD") : StrL("REMOVE");
    int agoMs = (int)(now - c->timestamp);
    s.Append(fmt("%-7s page %3d  zoom %6.2f  rot %3d  tile[res=%d row=%d col=%d]  %8s  %6dms ago", label, c->pageNo,
                 c->zoom, c->rotation, c->tile.res, c->tile.row, c->tile.col, FormatCacheBytesTemp(c->bytes), agoMs));
    if (c->fileName[0]) {
        s.Append(fmt("  %s", Str(c->fileName)));
    }
    s.Append(StrL("\r\n"));
}

// serialize cache stats and recent changes as plain text for the cache-info
// debug window
void RenderCache::SerializeCacheState(str::Builder& s) {
    ScopedRecursiveMutex scope(&cacheAccess);
    u64 now = GetTickCount64();
    i64 totalBytes = 0;
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* e = cache[i];
        if (e->bitmap) {
            totalBytes += PixmapByteSize(e->bitmap);
        }
    }
    s.Append(fmt("Cache: %d / %d entries, %s total\r\n\r\n", cacheCount, kMaxBitmapsCached,
                 FormatCacheBytesTemp(totalBytes)));

    if (cacheHistoryCount > 0) {
        s.Append(fmt("Recent %d changes:\r\n", cacheHistoryCount));
        int idx = cacheHistoryNext - 1;
        for (int n = 0; n < cacheHistoryCount; n++) {
            if (idx < 0) {
                idx += kCacheHistorySize;
            }
            SerializeCacheChange(s, &cacheHistory[idx], now);
            idx--;
        }
    }
}

static void SetCacheInfoTextOnUI(Str* s) {
    if (gCacheInfoWnd) {
        gCacheInfoWnd->SetTextContent(*s);
    }
    str::Free(*s);
    delete s;
}

// if the cache-info debug window is shown, refresh it. Cheap no-op when
// hidden. Safe to call from any thread (and while holding cacheAccess).
void RenderCache::UpdateCacheInfo() {
    if (!IsCacheInfoWindowVisible()) {
        return;
    }
    str::Builder s;
    SerializeCacheState(s);
    auto* dup = new Str(str::Dup(ToStr(s)));
    auto fn = MkFunc0<Str>(SetCacheInfoTextOnUI, dup);
    uitask::Post(fn, "CacheInfo");
}

static void CreateCacheInfoWindow() {
    auto* wnd = new DebugTextWnd();
    wnd->closeOnEsc = gSettings->escToExit;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnCacheInfoClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnCacheInfoDestroy);
    if (!wnd->Create(StrL("Cache Info"), 12)) {
        delete wnd;
        return;
    }
    gCacheInfoWnd = wnd;
}

// bitmap cache debug window (CmdDebugToggleCacheInfo)
void ToggleCacheInfoWindow() {
    if (gCacheInfoWnd) {
        CloseDebugTextWnd(&gCacheInfoWnd);
        return;
    }
    CreateCacheInfoWindow();
    if (gRenderCache) {
        gRenderCache->UpdateCacheInfo();
    }
}
