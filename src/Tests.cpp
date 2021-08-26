/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"

void TestRenderPage(const Flags& i) {
    if (i.showConsole) {
        RedirectIOToConsole();
    }

    if (i.pageNumber == -1) {
        printf("pageNumber is -1\n");
        return;
    }
    auto files = i.fileNames;
    if (files.size() == 0) {
        printf("no file provided\n");
        return;
    }
    float zoom = ZOOM_ACTUAL_SIZE;
    if (i.startZoom != INVALID_ZOOM) {
        zoom = i.startZoom;
    }
    for (auto fileName : files) {
        auto fileNameA(ToUtf8Temp(fileName));
        printf("rendering page %d for '%s', zoom: %.2f\n", i.pageNumber, fileNameA.Get(), zoom);
        auto engine = CreateEngine(fileName, nullptr, true);
        if (engine == nullptr) {
            printf("failed to create engine\n");
            continue;
        }
        RenderPageArgs args(i.pageNumber, zoom, 0);
        auto bmp = engine->RenderPage(args);
        if (bmp == nullptr) {
            printf("failed to render page\n");
        }
        delete bmp;
        delete engine;
    }
}

static void extractPageText(EngineBase* engine, int pageNo) {
    PageText pageText = engine->ExtractPageText(pageNo);
    if (!pageText.text) {
        return;
    }
    AutoFreeWstr uni = str::Replace(pageText.text, L"\n", L"_");
    auto uniA = ToUtf8Temp(uni);
    printf("text on page %d: '", pageNo);
    // print characters as hex because I don't know what kind of locale-specific mangling
    // printf() might do
    int idx = 0;
    while (uniA.Get()[idx] != 0) {
        char c = uniA.Get()[idx++];
        printf("%02x ", (u8)c);
    }
    printf("'\n");
    FreePageText(&pageText);
}

void TestExtractPage(const Flags& ci) {
    if (ci.showConsole) {
        RedirectIOToConsole();
    }

    int pageNo = ci.pageNumber;

    auto files = ci.fileNames;
    if (files.size() == 0) {
        printf("no file provided\n");
        return;
    }
    for (auto fileName : files) {
        auto fileNameA(ToUtf8Temp(fileName));
        auto engine = CreateEngine(fileName, nullptr, true);
        if (engine == nullptr) {
            printf("failed to create engine for file '%s'\n", fileNameA.Get());
            continue;
        }
        if (pageNo < 0) {
            int nPages = engine->PageCount();
            for (int i = 1; i <= nPages; i++) {
                extractPageText(engine, i);
            }
        } else {
            extractPageText(engine, pageNo);
        }

        delete engine;
    }
}
