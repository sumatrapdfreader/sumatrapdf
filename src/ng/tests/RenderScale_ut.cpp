/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "base/Pixmap.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "RenderCache.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

namespace {

struct NoCb : DocControllerCallback {
    void PageNoChanged(DocController*, int) override {}
    void ZoomChanged(DocController*, float) override {}
    void GotoLink(IPageDestination*) override {}
    void Repaint() override {}
    void UpdateScrollbars(DisplayModel*, Size) override {}
    void RequestRendering(DisplayModel*, int) override {}
    void RequestPredictiveRendering(DisplayModel*, int, const int*, int) override {}
    void CleanUp(DisplayModel*) override {}
    void RenderThumbnail(DisplayModel*, Size, const OnBitmapRendered*) override {}
    void FocusFrame(bool) override {}
    void SaveDownload(Str, Str) override {}
    void FindResultReceived(int, int, int) override {}
    void FindAllResultReceived(Str) override {}
    void TocChanged(DocController*) override {}
    void PagesRenumbered(DisplayModel*) override {}
};

bool Near(float a, float b) {
    return fabsf(a - b) < 0.01f;
}

// A page that fits the canvas in layout pixels must still take the large-tile
// path when the bitmap is renderScale times bigger than that canvas.
bool TileResFollowsLayout(DisplayModel* dm, RenderCache* cache) {
    dm->SetInitialViewSettings(DisplayMode::Continuous, 1, {2000, 2000}, 96);
    dm->Relayout(100, 0);
    cache->maxTileSize = {200, 200};
    dm->renderScale = 4;
    u16 fits = cache->GetTileRes(dm, 1);

    dm->SetInitialViewSettings(DisplayMode::Continuous, 1, {400, 400}, 96);
    dm->Relayout(100, 0);
    u16 tight = cache->GetTileRes(dm, 1);
    return fits < tight;
}

} // namespace

void RenderScale_UnitTests() {
    Settings* prevSettings = gSettings;
    if (!gSettings) {
        gSettings = NewSettings({});
    }

    TempStr path = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
    utassert(file::Exists(path));
    EngineBase* engine = CreateEngineMupdfFromFile(path, FileType::PDF, 96);
    utassert(engine != nullptr);
    if (!engine) {
        if (!prevSettings) {
            DeleteSettings(gSettings);
            gSettings = nullptr;
        }
        return;
    }

    NoCb cb;
    DisplayModel dm(engine, &cb);
    dm.SetInitialViewSettings(DisplayMode::Continuous, 1, {800, 600}, 96);
    dm.Relayout(100, 0);

    float zoom = dm.GetZoomReal(1);
    Rect page = dm.GetPageInfo(1)->pageOnScreen;
    Point pt = dm.CvtToScreen(1, PointF{100, 100});
    utassert(zoom > 0);
    utassert(!page.IsEmpty());

    RenderCache cache;
    cache.maxRenderThreads = 0;

    int widthAt1 = 0;
    float scales[] = {1, 2, 1};
    for (float scale : scales) {
        cache.AbortRendering(&dm);
        cache.FreeForDisplayModel(&dm);
        dm.renderScale = scale;
        cache.RequestRendering(&dm, 1);
        utassert(cache.requestCount >= 1);
        if (cache.requestCount < 1) {
            break;
        }

        float renderZoom = zoom * scale;
        for (int i = 0; i < cache.requestCount; i++) {
            PageRenderRequest* req = &cache.requests[i];
            utassert(Near(req->zoom, renderZoom));
            RectF rect = req->pageRect;
            RenderPageArgs args(req->pageNo, req->zoom, req->rotation, &rect);
            Pixmap* bmp = engine->RenderPage(args);
            Rect expect = engine->Transform(rect, req->pageNo, req->zoom, req->rotation).Round();
            utassert(bmp != nullptr);
            if (!bmp) {
                continue;
            }
            utassert(bmp->width == expect.dx);
            utassert(bmp->height == expect.dy);
            if (scale == 1 && i == 0) {
                widthAt1 = bmp->width;
            }
            if (scale == 2 && i == 0 && widthAt1 > 0) {
                utassert(bmp->width >= widthAt1 * 2 - 2);
                utassert(bmp->width <= widthAt1 * 2 + 2);
            }
            cache.Add(*req, bmp);
        }
        utassert(Near(dm.GetZoomReal(1), zoom));
        utassert(dm.GetPageInfo(1)->pageOnScreen == page);
        utassert(dm.CvtToScreen(1, PointF{100, 100}) == pt);

        cache.AbortRendering(&dm);
        for (int i = 0; i < cache.cacheCount; i++) {
            BitmapCacheEntry* entry = cache.cache[i];
            if (!entry || entry->dm != &dm) {
                continue;
            }
            utassert(cache.Exists(&dm, entry->pageNo, entry->rotation, renderZoom, &entry->tile));
            if (scale == 2) {
                utassert(!cache.Exists(&dm, entry->pageNo, entry->rotation, zoom, &entry->tile));
            }
        }
        Str why;
        bool ready = cache.VisibleTargetTilesReady(&dm, &why);
        if (!ready) {
            printf("RenderScale: not ready at scale %.0f: %.*s\n", scale, why.len, why.s ? why.s : "");
        }
        utassert(ready);
    }

    cache.AbortRendering(&dm);
    cache.FreeForDisplayModel(&dm);
    dm.renderScale = 1;
    cache.RequestRendering(&dm, 1);
    int queued = cache.requestCount;
    dm.renderScale = 2;
    cache.RequestRendering(&dm, 1);
    utassert(cache.requestCount == queued);
    utassert(queued >= 1);
    if (queued >= 1) {
        utassert(Near(cache.requests[queued - 1].zoom, zoom * 2));
    }

    utassert(TileResFollowsLayout(&dm, &cache));

    cache.AbortRendering(&dm);
    cache.FreeForDisplayModel(&dm);
    if (!prevSettings) {
        DeleteSettings(gSettings);
        gSettings = nullptr;
    }
}
