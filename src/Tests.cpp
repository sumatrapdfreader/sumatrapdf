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

static int DecodeUtf8Rune(const u8* p, int n) {
    if (n == 2) {
        return ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    }
    if (n == 3) {
        return ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    }
    if (n == 4) {
        return ((p[0] & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
    }
    return p[0];
}

static void extractPageText(EngineBase* engine, int pageNo) {
    PageText pageText = engine->ExtractPageText(pageNo);
    if (len(pageText.text) == 0) {
        return;
    }
    TempStr s = str::ReplaceTemp(Str(pageText.text.s), StrL("\n"), StrL("_"));
    printf("text on page %d: '", pageNo);
    // print characters as hex because I don't know what kind of locale-specific mangling
    // printf() might do
    int idx = 0;
    while (s.s[idx] != 0) {
        char c = s.s[idx++];
        printf("%02x ", (u8)c);
    }
    printf("'\n");
    // codepoint:height, same order as the text. tests/issue-6348.ts
    printf("boxh on page %d:", pageNo);
    const u8* p = (const u8*)pageText.text.s;
    const u8* end = p + pageText.text.len;
    for (int i = 0; i < pageText.nCodepoints && p < end; i++) {
        int n = utf8RuneLen(p);
        if (n < 1 || p + n > end) {
            break;
        }
        int cp = DecodeUtf8Rune(p, n);
        int dy = pageText.coords ? pageText.coords[i].dy : 0;
        printf(" %d:%d", cp, dy);
        p += n;
    }
    printf("\n");

    // codepoint:x,y,dx, same order as the text. tests/issue-6500.ts
    // one write: log lines printed meanwhile would otherwise split it
    str::Builder boxes;
    boxes.Append(fmt("boxes on page %d:", pageNo));
    p = (const u8*)pageText.text.s;
    for (int i = 0; i < pageText.nCodepoints && p < end; i++) {
        int n = utf8RuneLen(p);
        if (n < 1 || p + n > end) {
            break;
        }
        int cp = DecodeUtf8Rune(p, n);
        Rect r = pageText.coords ? pageText.coords[i] : Rect{};
        boxes.Append(fmt(" %d:%d,%d,%d", cp, r.x, r.y, r.dx));
        p += n;
    }
    boxes.AppendChar('\n');
    fflush(stdout);
    Str boxesStr = ToStr(boxes);
    fwrite(boxesStr.s, 1, len(boxesStr), stdout);
    fflush(stdout);
    FreePageText(&pageText);
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
                extractPageText(engine, i);
            }
        } else {
            extractPageText(engine, pageNo);
        }

        SafeEngineRelease(&engine);
    }
}
