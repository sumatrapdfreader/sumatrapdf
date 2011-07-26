/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef RenderCache_h
#define RenderCache_h

#include "DisplayModel.h"

#define RENDER_DELAY_UNDEFINED ((UINT)-1)
#define RENDER_DELAY_FAILED    ((UINT)-2)
#define INVALID_TILE_RES       ((USHORT)-1)

class RenderingCallback {
public:
    virtual void Callback(RenderedBitmap *bmp=NULL) = 0;
};

/* A page is split into tiles of at most TILE_MAX_W x TILE_MAX_H pixels.
 * A given tile starts at (col / 2^res * page_width, row / 2^res * page_height). */
struct TilePosition {
    USHORT res, row, col;

    bool operator==(TilePosition other) {
        return res == other.res && row == other.row && col == other.col;
    }
};

/* We keep a cache of rendered bitmaps. BitmapCacheEntry keeps data
   that uniquely identifies rendered page (dm, pageNo, rotation, zoom)
   and corresponding rendered bitmap.
*/
struct BitmapCacheEntry {
    DisplayModel *   dm;
    int              pageNo;
    int              rotation;
    float            zoom;
    TilePosition     tile;

    RenderedBitmap * bitmap;
    int              refs;
};

/* Even though this looks a lot like a BitmapCacheEntry, we keep it
   separate for clarity in the code (PageRenderRequests are reused,
   while BitmapCacheEntries are ref-counted) */
struct PageRenderRequest {
    DisplayModel *      dm;
    int                 pageNo;
    int                 rotation;
    float               zoom;
    TilePosition        tile;

    RectD               pageRect; // calculated from TilePosition
    bool                abort;
    DWORD               timestamp;
    // owned by the PageRenderRequest (use it before reusing the request)
    // on rendering success, the callback gets handed the RenderedBitmap
    RenderingCallback * renderCb;
};

#define MAX_PAGE_REQUESTS 8

// keep this value reasonably low, else we'll run
// out of GDI memory when caching many larger bitmaps
#define MAX_BITMAPS_CACHED 64

class RenderCache
{
private:
    BitmapCacheEntry *  cache[MAX_BITMAPS_CACHED];
    int                 cacheCount;
    // make sure to never ask for _requestAccess in a _cacheAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION    cacheAccess;

    PageRenderRequest   requests[MAX_PAGE_REQUESTS];
    int                 requestCount;
    PageRenderRequest * curReq;
    CRITICAL_SECTION    requestAccess;
    HANDLE              renderThread;

    const SizeI         maxTileSize;

public:
    /* invert all colors for accessibility reasons (experimental!) */
    bool                invertColors;

    RenderCache();
    ~RenderCache();

    void    Render(DisplayModel *dm, int pageNo, RenderingCallback *callback=NULL);
    void    Render(DisplayModel *dm, int pageNo, int rotation, float zoom,
                   RectD pageRect, RenderingCallback& callback);
    void    CancelRendering(DisplayModel *dm);
    bool    Exists(DisplayModel *dm, int pageNo, int rotation,
                   float zoom=INVALID_ZOOM, TilePosition *tile=NULL);
    bool    FreeForDisplayModel(DisplayModel *dm);
    void    KeepForDisplayModel(DisplayModel *oldDm, DisplayModel *newDm);
    UINT    Paint(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                  PageInfo *pageInfo, bool *renderOutOfDateCue);

protected:
    /* Interface for page rendering thread */
    HANDLE  startRendering;

    bool    ClearCurrentRequest();
    bool    GetNextRequest(PageRenderRequest *req);
    void    Add(PageRenderRequest &req, RenderedBitmap *bitmap);
    bool    FreeNotVisible();

private:
    USHORT  GetTileRes(DisplayModel *dm, int pageNo);
    bool    ReduceTileSize();

    bool    IsRenderQueueFull() const {
                return requestCount == MAX_PAGE_REQUESTS;
            }
    UINT    GetRenderDelay(DisplayModel *dm, int pageNo, TilePosition tile);
    void    Render(DisplayModel *dm, int pageNo, TilePosition tile,
                   bool clearQueue=true, RenderingCallback *callback=NULL);
    bool    Render(DisplayModel *dm, int pageNo, int rotation, float zoom,
                   TilePosition *tile=NULL, RectD *pageRect=NULL,
                   RenderingCallback *callback=NULL);
    void    ClearQueueForDisplayModel(DisplayModel *dm, int pageNo=INVALID_PAGE_NO,
                                      TilePosition *tile=NULL);

    static DWORD WINAPI RenderCacheThread(LPVOID data);

    BitmapCacheEntry *  Find(DisplayModel *dm, int pageNo, int rotation,
                             float zoom=INVALID_ZOOM, TilePosition *tile=NULL);
    void    DropCacheEntry(BitmapCacheEntry *entry);
    bool    FreePage(DisplayModel *dm=NULL, int pageNo=-1, TilePosition *tile=NULL);

    UINT    PaintTile(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                      TilePosition tile, RectI *tileOnScreen, bool renderMissing,
                      bool *renderOutOfDateCue, bool *renderedReplacement);
    UINT    PaintTiles(HDC hdc, RectI *bounds, DisplayModel *dm, int pageNo,
                      RectI *pageOnScreen, USHORT tileRes, bool renderMissing,
                      bool *renderOutOfDateCue, bool *renderedReplacement);

};

#endif
