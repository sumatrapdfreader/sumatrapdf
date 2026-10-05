/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig has no test for PageRenderService (it only ever ran inside the GTK
// port). The port needs one: rendering has to happen on a worker thread and
// the "page is ready" notification has to arrive on the main thread through
// uitask, with no window involved.

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/UITask.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "PageRenderPolicy.h"
#include "PageRenderService.h"
#include "ReaderModel.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

struct RenderReadyFlag {
    bool ready = false;
    ThreadId threadId = 0;
};

static void OnPageReady(RenderReadyFlag* flag) {
    flag->ready = true;
    flag->threadId = GetCurrentThreadId();
}

void PageRenderService_UnitTests() {
    TempStr path = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
    utassert(file::Exists(path));

    ReaderModel* model = ReaderModel::Create(path);
    utassert(model != nullptr);
    utassert(model->PageCount() == 2);

    // the layout is what a canvas would ask for; no window is involved
    DocumentLayoutParams params;
    params.displayMode = DisplayMode::Continuous;
    params.viewPortSize = {800, 600};
    params.zoomVirtual = 100;
    DocumentLayout layout;
    utassert(model->Layout(params, &layout));
    utassert(len(layout.pages) == 2);
    utassert(layout.GetPage(1)->pos.dx > 0);

    Rect normalPage = layout.GetPage(1)->pos;
    params.freePan = true;
    DocumentLayout freePanLayout;
    utassert(model->Layout(params, &freePanLayout));
    Size slack = FreePanSlack(params.viewPortSize);
    utassert(freePanLayout.GetPage(1)->pos.x == normalPage.x + slack.dx);
    utassert(freePanLayout.GetPage(1)->pos.y == normalPage.y + slack.dy);
    utassert(freePanLayout.canvasSize.dx >= params.viewPortSize.dx * 2);
    utassert(freePanLayout.canvasSize.dy >= params.viewPortSize.dy * 2);

    uitask::Initialize();
    ThreadId mainThread = GetCurrentThreadId();

    RenderReadyFlag flag;
    auto* service = PageRenderService::Create(model->GetEngine(), MkFunc0(OnPageReady, &flag));
    utassert(service != nullptr);

    PageRenderKey key;
    key.pageNo = 1;
    key.zoom = 1.0f;
    service->Request(key, PageRenderPriority::Visible);

    // the worker renders while we drain; 30 s is generous for a 2-page PDF
    for (int i = 0; i < 3000 && !flag.ready; i++) {
        uitask::DrainQueue();
        if (!flag.ready) {
            SleepInMs(10);
        }
    }
    utassert(flag.ready);
    // the notification ran on the thread that drained, not on the worker
    utassert(flag.threadId == mainThread);

    Pixmap* px = service->CopyPage(key);
    utassert(px != nullptr);
    utassert(px->width > 0 && px->height > 0);
    utassert(service->CacheBytes() > 0);
    FreePixmap(px);

    delete service;
    uitask::Destroy();
    delete model;
}
