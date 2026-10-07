/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

void DocumentLayout_UnitTests() {
    TempStr path = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
    utassert(file::Exists(path));

    EngineBase* engine = CreateEngineMupdfFromFile(path, FileType::PDF, 96);
    utassert(engine != nullptr);
    utassert(engine->PageCount() == 2);

    // the layout is what a canvas would ask for; no window is involved
    DocumentLayoutParams params;
    params.displayMode = DisplayMode::Continuous;
    params.viewPortSize = {800, 600};
    params.zoomVirtual = 100;
    DocumentLayout layout;
    layout.Reset(engine->PageCount());
    for (int page = 1; page <= engine->PageCount(); page++) {
        layout.SetPageMediaBox(page, engine->PageMediabox(page));
    }
    layout.Relayout(params);
    utassert(len(layout.pages) == 2);
    utassert(layout.GetPage(1)->pos.dx > 0);

    Rect normalPage = layout.GetPage(1)->pos;
    params.freePan = true;
    DocumentLayout freePanLayout;
    freePanLayout.pages = layout.pages;
    freePanLayout.Relayout(params);
    Size slack = FreePanSlack(params.viewPortSize);
    utassert(freePanLayout.GetPage(1)->pos.x == normalPage.x + slack.dx);
    utassert(freePanLayout.GetPage(1)->pos.y == normalPage.y + slack.dy);
    utassert(freePanLayout.canvasSize.dx >= params.viewPortSize.dx * 2);
    utassert(freePanLayout.canvasSize.dy >= params.viewPortSize.dy * 2);

    engine->Release();
}
