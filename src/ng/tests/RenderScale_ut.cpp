/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/SettingsUtil.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "RenderCache.h"

#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

struct RenderScaleCallback : DocControllerCallback {
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

void RenderScale_UnitTests() {
    Settings* saved = gSettings;
    gSettings = NewSettings({});
    {
        TempStr path = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
        EngineBase* engine = CreateEngineMupdfFromFile(path, FileType::PDF, 96);
        utassert(engine);
        RenderScaleCallback cb;
        DisplayModel dm(engine, &cb);
        dm.SetInitialViewSettings(DisplayMode::Continuous, 1, {800, 600}, 96);
        dm.Relayout(100, 0);
        float zoom = dm.GetZoomReal(1);
        utassert(zoom > 0);
        Rect page = dm.GetPageInfo(1)->pageOnScreen;
        Point point = dm.CvtToScreen(1, PointF(100, 100));

        RenderCache cache;
        cache.maxRenderThreads = 0;
        TilePosition tile(0, 0, 0);
        const float scales[] = {1, 2, 1};
        for (float scale : scales) {
            dm.renderScale = scale;
            utassert(!cache.VisibleTargetTilesReady(&dm));
            cache.RequestRendering(&dm, 1, tile);
            utassert(cache.requestCount == 1);
            PageRenderRequest req = cache.requests[0];
            utassert(req.zoom == zoom * scale);
            utassert(dm.GetZoomReal(1) == zoom);
            utassert(dm.GetPageInfo(1)->pageOnScreen == page);
            utassert(dm.CvtToScreen(1, PointF(100, 100)) == point);
            cache.requestCount = 0;

            RenderPageArgs args(1, req.zoom, req.rotation, &req.pageRect);
            Pixmap* bmp = engine->RenderPage(args);
            utassert(bmp);
            Rect expected = engine->Transform(req.pageRect, 1, zoom * scale, req.rotation).Round();
            utassert(bmp->width == expected.dx && bmp->height == expected.dy);
            cache.Add(req, bmp);
            utassert(cache.Exists(&dm, 1, 0, zoom * scale, &tile));
            utassert(cache.VisibleTargetTilesReady(&dm));
            if (scale == 2) {
                utassert(!cache.Exists(&dm, 1, 0, zoom, &tile));
            }
        }

        cache.FreePage(&dm, 1);
        dm.renderScale = 1;
        cache.RequestRendering(&dm, 1, tile);
        dm.renderScale = 2;
        cache.RequestRendering(&dm, 1, tile);
        utassert(cache.requestCount == 1);
        utassert(cache.requests[0].zoom == zoom * 2);
    }
    DeleteSettings(gSettings);
    gSettings = saved;
}
