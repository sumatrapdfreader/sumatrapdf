/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Note: they must be in this numeric order for ::Paint() logic to detect
// page that couldn't be rendered
constexpr int RENDER_DELAY_UNDEFINED = INT_MAX - 1;
constexpr int RENDER_DELAY_FAILED = INT_MAX - 2;

#define INVALID_TILE_RES ((USHORT) - 1)

#define MAX_PAGE_REQUESTS 8
// keep this value reasonably low, else we'll run out of
// GDI resources/memory when caching many larger bitmaps
// TODO: this should be based on amount of memory taken by rendered pages
// i.e. one big page can use as much memory as lots of small pages
#define MAX_BITMAPS_CACHED 128

// predictive rendering renders up to this many pages ahead, one at a time
// (chained), so they don't flood the render queue
constexpr int kMaxPredictiveRequests = 4;

struct PageInfo;
struct Pixmap;

// describes the chain of pages to render predictively after the current page.
// originPageNo is the visible page that anchors the chain; the chain stops
// once it's no longer visible.
struct PredictiveChain {
    int originPageNo = 0;
    int nPages = 0;
    int pages[kMaxPredictiveRequests]{};
};

/* A page is split into tiles of at most TILE_MAX_W x TILE_MAX_H pixels.
   A given tile starts at (col / 2^res * page_width, row / 2^res * page_height). */
struct TilePosition {
    USHORT res = INVALID_TILE_RES;
    USHORT row = (USHORT)-1;
    USHORT col = (USHORT)-1;

    TilePosition() = default;
    explicit TilePosition(USHORT res, USHORT row, USHORT col) {
        this->res = res;
        this->row = row;
        this->col = col;
    }
    bool operator==(const TilePosition& other) const {
        return res == other.res && row == other.row && col == other.col;
    }
};

/* We keep a cache of rendered bitmaps. BitmapCacheEntry keeps data
   that uniquely identifies rendered page (dm, pageNo, rotation, zoom)
   and the corresponding rendered bitmap. */
struct BitmapCacheEntry {
    DisplayModel* dm = nullptr;
    int pageNo = 0;
    int rotation = 0;
    float zoom = 0.f;
    TilePosition tile;
    int cacheIdx = -1; // index within RenderCache.cache

    // owned by the BitmapCacheEntry
    Pixmap* bitmap = nullptr;
    bool outOfDate = false;
    int refs = 1;

    BitmapCacheEntry(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition tile, Pixmap* bitmap) {
        this->dm = dm;
        this->pageNo = pageNo;
        this->rotation = rotation;
        this->zoom = zoom;
        this->tile = tile;
        this->bitmap = bitmap;
    }
    ~BitmapCacheEntry();
};

/* Even though this looks a lot like a BitmapCacheEntry, we keep it
   separate for clarity in the code (PageRenderRequests are reused,
   while BitmapCacheEntries are ref-counted) */
struct PageRenderRequest {
    DisplayModel* dm = nullptr;
    int pageNo = 0;
    int rotation = 0;
    float zoom = 0.f;
    TilePosition tile;

    RectF pageRect; // calculated from TilePosition
    bool abort = false;
    AbortCookie* abortCookie = nullptr;
    DWORD timestamp = 0;

    // set by render thread before calling renderFinishedCb
    Pixmap* bmp = nullptr;
    int errorCode = 0; // 0 = success

    // Predictive rendering: once this page finishes rendering, the pages in
    // predictiveRequests are rendered one at a time (chained), each new request
    // carrying the remaining pages forward. predictiveOriginPageNo is the visible
    // page that anchors the chain - if it's no longer visible the chain stops.
    int predictiveOriginPageNo = 0;
    int nPredictiveRequests = 0;
    int predictiveRequests[kMaxPredictiveRequests]{};

    // called when rendering finishes (success or failure)
    // if null, render cache handles caching directly (legacy path)
    Func1<PageRenderRequest*> renderFinishedCb;
};

constexpr int kMaxRenderThreads = 32;

extern int gMaxRenderThreads;

// keep a small history of recently finished render requests for the
// render-info debug window
constexpr int kFinishedHistorySize = 32;
constexpr int kCacheHistorySize = 32;

// snapshot of a finished render request (kept after the request is gone, so it
// copies the file name instead of holding on to a DisplayModel pointer)
// snapshot of a cache add/remove for the cache-info debug window
struct CacheChangeInfo {
    bool isAdd = false;
    int pageNo = 0;
    float zoom = 0;
    int rotation = 0;
    TilePosition tile;
    i64 bytes = 0;
    DWORD timestamp = 0;
    char fileName[128]{};
};

struct FinishedRequestInfo {
    int pageNo = 0;
    float zoom = 0;
    int rotation = 0;
    TilePosition tile;
    DWORD timestamp = 0;  // when it was requested
    DWORD finishedAt = 0; // when it finished
    bool aborted = false;
    int predictiveOriginPageNo = 0;
    int nPredictiveRequests = 0;
    int predictiveRequests[kMaxPredictiveRequests]{};
    char fileName[128]{};
};

struct RenderCache {
    BitmapCacheEntry* cache[MAX_BITMAPS_CACHED]{};
    int cacheCount = 0;
    // make sure to never ask for requestAccess in a cacheAccess
    // protected critical section in order to avoid deadlocks
    RecursiveMutex cacheAccess;

    PageRenderRequest requests[MAX_PAGE_REQUESTS]{};
    int requestCount = 0;

    // ring buffer of recently finished requests (for the render-info window),
    // protected by requestAccess
    FinishedRequestInfo finishedHistory[kFinishedHistorySize]{};
    int finishedHistoryCount = 0; // number of valid entries (capped at size)
    int finishedHistoryNext = 0;  // next slot to write

    // ring buffer of recent cache adds/removals (for the cache-info window),
    // protected by cacheAccess
    CacheChangeInfo cacheHistory[kCacheHistorySize]{};
    int cacheHistoryCount = 0;
    int cacheHistoryNext = 0;
    // per-thread current request tracking (index matches thread index)
    PageRenderRequest* curReqs[kMaxRenderThreads]{};
    RecursiveMutex requestAccess;
    ThreadHandle renderThreads[kMaxRenderThreads]{};
    // Render threads are spawned lazily: nRenderThreads is the count actually
    // running so far, maxRenderThreads is the cap. Threads track idleThreads
    // (incremented when they're about to wait on startRendering); Render()
    // only spawns a fresh thread when no idle one is available.
    int nRenderThreads = 0;
    int maxRenderThreads = 0;
    int idleThreads = 0;

    Size maxTileSize{};
    bool isRemoteSession = false;

    COLORREF textColor = 0;
    COLORREF backgroundColor = 0;

    /* Interface for page rendering thread */
    HANDLE startRendering = nullptr; // semaphore, signaled once per queued request
    AtomicBool shouldExit = 0;

    RenderCache();
    RenderCache(RenderCache const&) = delete;
    RenderCache& operator=(RenderCache const&) = delete;
    ~RenderCache();

    void RequestRendering(DisplayModel* dm, int pageNo);
    void Render(DisplayModel* dm, int pageNo, int rotation, float zoom, RectF pageRect,
                const Func1<PageRenderRequest*>& callback);
    void CancelRendering(DisplayModel* dm);
    bool Exists(DisplayModel* dm, int pageNo, int rotation, float zoom = kInvalidZoom, TilePosition* tile = nullptr);
    void FreeForDisplayModel(DisplayModel* dm);
    void KeepForDisplayModel(DisplayModel* oldDm, DisplayModel* newDm);
    void Invalidate(DisplayModel* dm, int pageNo, RectF rect);
    // returns how much time in ms has past since the most recent rendering
    // request for the visible part of the page if nothing at all could be
    // painted, 0 if something has been painted and RENDER_DELAY_FAILED on failure
    int Paint(HDC hdc, Rect bounds, DisplayModel* dm, int pageNo, PageInfo* pi, bool* renderOutOfDateCue);

    bool ClearCurrentRequest(int threadIdx);
    bool GetNextRequest(PageRenderRequest* req, int threadIdx);
    void Add(PageRenderRequest& req, Pixmap* bmp);

    USHORT GetTileRes(DisplayModel* dm, int pageNo) const;
    USHORT GetMaxTileRes(DisplayModel* dm, int pageNo, int rotation);
    bool ReduceTileSize();

    bool IsRenderQueueFull() const { return requestCount == MAX_PAGE_REQUESTS; }
    int GetRenderDelay(DisplayModel* dm, int pageNo, TilePosition tile);
    void RequestRendering(DisplayModel* dm, int pageNo, TilePosition tile, bool clearQueueForPage = true,
                          const PredictiveChain* chain = nullptr);
    // start (or continue) a chained predictive render anchored to originPageNo
    void RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages, int nPages);
    bool Render(DisplayModel* dm, int pageNo, int rotation, float zoom, TilePosition* tile, RectF* pageRect,
                const Func1<PageRenderRequest*>& renderFinishedCb, const PredictiveChain* chain = nullptr);
    void ClearQueueForDisplayModel(DisplayModel* dm, int pageNo = kInvalidPageNo, TilePosition* tile = nullptr);
    void AbortCurrentRequest(int threadIdx);

    BitmapCacheEntry* Find(DisplayModel* dm, int pageNo, int rotation, float zoom = kInvalidZoom,
                           TilePosition* tile = nullptr);
    bool DropCacheEntry(BitmapCacheEntry* entry);
    bool DropCacheEntryIfNotUsed(BitmapCacheEntry* entry);
    void FreePage(DisplayModel* dm, int pageNo, TilePosition* tile = nullptr);
    void FreeNotVisible();

    int PaintTile(HDC hdc, Rect bounds, DisplayModel* dm, int pageNo, TilePosition tile, Rect tileOnScreen,
                  bool renderMissing, bool* renderOutOfDateCue, bool* renderedReplacement);
    void LogCacheSize();

    // record a just-finished request in finishedHistory (call holding requestAccess)
    void RecordFinishedRequest(PageRenderRequest* req);
    // serialize the queue (in-progress + queued requests) as plain text, one
    // line per request, for the render-info debug window
    void SerializeQueueState(str::Builder& s);
    // if the render-info debug window is shown, refresh it with the current
    // queue state. Cheap no-op when the window is hidden. Safe to call from
    // any thread (and while holding requestAccess).
    void UpdateRenderInfo();

    // record a cache add/remove in cacheHistory (call holding cacheAccess)
    void RecordCacheChange(bool isAdd, BitmapCacheEntry* entry);
    // serialize cache stats and recent changes as plain text for the cache-info
    // debug window
    void SerializeCacheState(str::Builder& s);
    // if the cache-info debug window is shown, refresh it. Cheap no-op when
    // hidden. Safe to call from any thread (and while holding cacheAccess).
    void UpdateCacheInfo();
};

// render queue debug window (CmdDebugToggleRenderInfo)
void ToggleRenderInfoWindow();
bool IsRenderInfoWindowVisible();

// bitmap cache debug window (CmdDebugToggleCacheInfo)
void ToggleCacheInfoWindow();
bool IsCacheInfoWindowVisible();
