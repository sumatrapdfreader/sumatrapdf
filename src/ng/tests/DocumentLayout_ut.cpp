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
        layout.GetPage(page)->mediaBox = engine->PageMediabox(page);
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

    // 200x400 pages. Rotation 90/270 swaps that to 400x200, so a vertical stack would overlap.
    auto layoutPages = [](DocumentLayout& layout, int rot, bool follow, DisplayMode mode) {
        DocumentLayoutParams params;
        params.displayMode = mode;
        params.viewPortSize = {800, 600};
        params.zoomVirtual = 100;
        params.dpiFactor = 1;
        params.rotation = rot;
        params.scrollFollowsRotation = follow;
        layout.Reset(2);
        for (int pageNo = 1; pageNo <= 2; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            page->mediaBox = RectF(0, 0, 200, 400);
            page->rotation = rot;
        }
        layout.Relayout(params);
    };

    DocumentLayout right;
    layoutPages(right, 90, true, DisplayMode::Continuous);
    utassert(right.GetPage(1)->pos.dx == 400);
    utassert(right.GetPage(1)->pos.dy == 200);
    utassert(right.GetPage(2)->pos.x + right.GetPage(2)->pos.dx <= right.GetPage(1)->pos.x);
    utassert(right.GetPage(1)->pos.y == right.GetPage(2)->pos.y);

    DocumentLayout left;
    layoutPages(left, 270, true, DisplayMode::Continuous);
    utassert(left.GetPage(1)->pos.x + left.GetPage(1)->pos.dx <= left.GetPage(2)->pos.x);
    utassert(left.GetPage(1)->pos.y == left.GetPage(2)->pos.y);

    DocumentLayout half;
    layoutPages(half, 180, true, DisplayMode::Continuous);
    utassert(half.GetPage(1)->pos.dx == 200);
    utassert(half.GetPage(1)->pos.dy == 400);
    utassert(half.GetPage(2)->pos.y == 0);
    utassert(half.GetPage(1)->pos.y == 400);

    DocumentLayout down;
    layoutPages(down, 90, false, DisplayMode::Continuous);
    utassert(down.GetPage(2)->pos.y >= down.GetPage(1)->pos.y + down.GetPage(1)->pos.dy);
    utassert(down.GetPage(1)->pos.dx == 400);

    DocumentLayout upright;
    layoutPages(upright, 0, true, DisplayMode::Continuous);
    utassert(upright.GetPage(2)->pos.y >= upright.GetPage(1)->pos.y + upright.GetPage(1)->pos.dy);

    DocumentLayout facing;
    layoutPages(facing, 90, true, DisplayMode::ContinuousFacing);
    utassert(facing.GetPage(1)->pos.x == facing.GetPage(2)->pos.x);
    utassert(facing.GetPage(2)->pos.y > facing.GetPage(1)->pos.y);
}
