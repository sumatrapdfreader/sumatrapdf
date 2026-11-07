/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "Tests.h"

void TestRenderPage(const Flags& i) {
    if (i.showConsole) {
        RedirectIOToConsole();
    }

    if (i.pageNumber == -1) {
        printf("pageNumber is -1\n");
        return;
    }
    auto files = i.fileNames;
    if (len(files) == 0) {
        printf("no file provided\n");
        return;
    }
    float zoom = kZoomActualSize;
    if (i.startZoom != kInvalidZoom) {
        zoom = i.startZoom;
    }
    for (auto fileName : files) {
        printf("rendering page %d for '%s', zoom: %.2f\n", i.pageNumber, fileName.s, zoom);
        auto* engine = CreateEngineFromFile(fileName, nullptr, true);
        if (engine == nullptr) {
            printf("failed to create engine\n");
            continue;
        }
        int pageNo = i.pageNumber;
        if (pageNo < 1 || pageNo > engine->PageCount()) {
            printf("invalid page number %d (document has %d pages)\n", pageNo, engine->PageCount());
            SafeEngineRelease(&engine);
            continue;
        }
        RenderPageArgs args(pageNo, zoom, 0);
        auto* bmp = engine->RenderPage(args);
        if (bmp == nullptr) {
            printf("failed to render page\n");
        }
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
    }
}

void TestExtractPage(const Flags& ci) {
    if (ci.showConsole) {
        RedirectIOToConsole();
    }

    int pageNo = ci.pageNumber;

    auto files = ci.fileNames;
    if (len(files) == 0) {
        printf("no file provided\n");
        return;
    }
    for (auto fileName : files) {
        auto* engine = CreateEngineFromFile(fileName, nullptr, true);
        if (engine == nullptr) {
            printf("failed to create engine for file '%s'\n", fileName.s);
            continue;
        }
        if (pageNo < 0) {
            int nPages = engine->PageCount();
            for (int i = 1; i <= nPages; i++) {
                ExtractPageTextToStdout(engine, i);
            }
        } else {
            ExtractPageTextToStdout(engine, pageNo);
        }

        SafeEngineRelease(&engine);
    }
}
