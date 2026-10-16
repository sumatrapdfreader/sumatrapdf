/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
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

struct RequestLockProbe {
    RecursiveMutex* mutex = nullptr;
    bool available = false;
};

static void ProbeRequestLock(RequestLockProbe* probe) {
    probe->available = probe->mutex->TryLock();
    if (probe->available) {
        probe->mutex->Unlock();
    }
}

class LockProbeAbortCookie : public AbortCookie {
  public:
    RecursiveMutex* mutex = nullptr;
    bool* available = nullptr;

    LockProbeAbortCookie(RecursiveMutex* mutex, bool* available) : mutex(mutex), available(available) {}
    ~LockProbeAbortCookie() override {
        RequestLockProbe probe{mutex};
        ThreadHandle thread = StartThread(MkFunc0(ProbeRequestLock, &probe), StrL("request-lock-probe"));
        if (!thread) {
            return;
        }
        JoinThread(&thread, -1);
        *available = probe.available;
    }

    void Abort() override {}
    void* GetData() override { return nullptr; }
};

bool RenderCache_UnitTestCookieUnlocked() {
    RenderCache cache;
    PageRenderRequest req;
    bool available = false;
    req.abortCookie = new LockProbeAbortCookie(&cache.requestAccess, &available);
    cache.curReqs[0] = &req;

    cache.ClearCurrentRequest(0);
    return available;
}

namespace {

struct NoopDocCb : DocControllerCallback {
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

// mupdf repairs the missing xref
const char* kOnePagePdf =
    "%PDF-1.4\n"
    "1 0 obj <</Type/Catalog/Pages 2 0 R>> endobj\n"
    "2 0 obj <</Type/Pages/Kids[3 0 R]/Count 1>> endobj\n"
    "3 0 obj <</Type/Page/Parent 2 0 R/MediaBox[0 0 600 600]>> endobj\n"
    "trailer <</Root 1 0 R>>\n"
    "%%EOF\n";

int TileDistance(TilePosition a, TilePosition b) {
    int dr = abs((int)a.row - (int)b.row);
    int dc = abs((int)a.col - (int)b.col);
    return std::max(dr, dc);
}

} // namespace

// A deeply zoomed single page fills the cache with its own tiles. Off-screen
// tiles of that visible page must be evicted to make room for visible ones.
bool RenderCache_UnitTestFullOfOneVisiblePage() {
    Settings* prevSettings = gSettings;
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    EngineBase* engine = CreateEngineMupdfFromData(Str(kOnePagePdf), StrL("one-page.pdf"), nullptr);
    if (!engine) {
        return false;
    }

    NoopDocCb cb;
    auto* dm = new DisplayModel(engine, &cb);
    dm->SetInitialViewSettings(DisplayMode::Continuous, 1, {400, 300}, 96);
    dm->Relayout(1600, 0);

    bool ok = false;
    {
        RenderCache cache;
        cache.maxRenderThreads = 0;
        cache.maxTileSize = {200, 200};
        PageRenderRequest visible;
        visible.dm = dm;
        visible.pageNo = 1;
        visible.zoom = dm->GetRenderZoom(1);
        visible.darkModeEpoch = cache.darkModeEpoch;

        // the tile under the viewport's top-left corner; rows count from the page bottom
        u16 res = cache.GetTileRes(dm, 1);
        Rect page = dm->GetPageInfo(1)->pageOnScreen;
        int nTiles = 1 << res;
        int col = std::max(0, -page.x) * nTiles / page.dx;
        int rowFromTop = std::max(0, -page.y) * nTiles / page.dy;
        visible.tile = TilePosition(res, (u16)(nTiles - 1 - rowFromTop), (u16)col);

        if (res >= 5) {
            for (int r = 0; r < nTiles && cache.cacheCount < kMaxBitmapsCached; r++) {
                for (int c = 0; c < nTiles && cache.cacheCount < kMaxBitmapsCached; c++) {
                    PageRenderRequest req = visible;
                    req.tile = TilePosition(res, (u16)r, (u16)c);
                    if (TileDistance(req.tile, visible.tile) < 4) {
                        continue;
                    }
                    cache.Add(req, AllocPixmap(4, 4));
                }
            }
            bool full = cache.cacheCount == kMaxBitmapsCached;
            cache.Add(visible, AllocPixmap(4, 4));
            ok = full && cache.Exists(dm, 1, visible.rotation, visible.zoom, &visible.tile);
        }
        cache.AbortRendering(dm);
        cache.FreeForDisplayModel(dm);
    }

    delete dm;
    if (!prevSettings) {
        DeleteSettings(gSettings);
        gSettings = nullptr;
    }
    return ok;
}
