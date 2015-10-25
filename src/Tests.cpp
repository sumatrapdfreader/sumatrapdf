/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "WinUtil.h"

#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "ParseCommandLine.h"
#include "BaseEngine.h"
#include "EngineManager.h"

extern "C" void fz_redirect_dll_io_to_console();

void TestRenderPage(const CommandLineInfo& i) {
    if (i.showConsole) {
        RedirectIOToConsole();
        fz_redirect_dll_io_to_console();
    }

    if (i.pageNumber == -1) {
        printf("pageNumber is -1\n");
        return;
    }
    auto files = i.fileNames;
    if (files.Count() == 0) {
        printf("no file provided\n");
        return;
    }
    float zoom = ZOOM_ACTUAL_SIZE;
    if (i.startZoom != INVALID_ZOOM) {
        zoom = i.startZoom;
    }
    for (auto fileName : files) {
        ScopedMem<char> fileNameUtf(str::conv::ToUtf8(fileName));
        printf("rendering page %d for '%s', zoom: %.2f\n", i.pageNumber, fileNameUtf.Get(), zoom);
        auto engine = EngineManager::CreateEngine(fileName);
        if (engine == nullptr) {
            printf("failed to create engine\n");
            continue;
        }
        auto bmp = engine->RenderBitmap(i.pageNumber, zoom, 0);
        if (bmp == nullptr) {
            printf("failed to render page\n");
        }
        delete bmp;
        delete engine;
    }
}

void TestExtractPage(const CommandLineInfo& i) {
    if (i.showConsole) {
        RedirectIOToConsole();
        fz_redirect_dll_io_to_console();
    }

    if (i.pageNumber == -1) {
        printf("pageNumber is -1\n");
        return;
    }
    auto files = i.fileNames;
    if (files.Count() == 0) {
        printf("no file provided\n");
        return;
    }
    for (auto fileName : files) {
        ScopedMem<char> fileNameUtf(str::conv::ToUtf8(fileName));
        auto engine = EngineManager::CreateEngine(fileName);
        if (engine == nullptr) {
            printf("failed to create engine for file '%s'\n", fileNameUtf.Get());
            continue;
        }
        RectI* coordsOut; // not using the result, only to trigger the code path
        WCHAR* uni = engine->ExtractPageText(i.pageNumber, L"_", &coordsOut);
        char* utf = str::conv::ToUtf8(uni);
        printf("text on page %d: '", i.pageNumber);
        // print characters as hex because I don't know what kind of locale-specific mangling
        // printf() might do
        int idx = 0;
        while (utf[idx] != 0) {
            char c = utf[idx++];
            printf("%02x ", (unsigned char)c);
        }
        printf("'\n");
        free(uni);
        free(utf);
        free(coordsOut);
        delete engine;
    }
}
