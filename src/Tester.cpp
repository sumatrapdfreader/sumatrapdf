/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments. */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"
#include "base/Win.h"
#include "base/Zip.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "PalmDbReader.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to kMobiSaveDir. The name will be
// ${file}.html and ${file}_pp.html
static bool gSaveHtml = false;
// if true, we'll also save images in mobi files. The name
// will be ${file}_img_${imgNo}.[jpg|png]
// gMobiSaveHtml must be true as well
static bool gSaveImages = false;
// if true, we'll do a layout of mobi files
static bool gLayout = false;
// directory to which we'll save mobi html and images
constexpr const char* kMobiSaveDir = "..\\ebooks-converted";

static int Usage() {
    printf("Tester.exe\n");
    printf("  -layout - will also layout mobi files\n");
    printf("  -save-html] - will save html content of mobi file\n");
    printf("  -save-images - will save images extracted from mobi files\n");
    printf("  -zip-create - creates a sample zip file that needs to be manually checked that it worked\n");
    printf("  -bench-md5 - compare Window's md5 vs. our code\n");
    system("pause");
    return 1;
}

// we assume this is called from main sumatradirectory, e.g. as:
// ./obj-dbg/tester.exe, so we use the known files
void ZipCreateTest() {
    Str zipFileName = StrL("tester-tmp.zip");
    file::Delete(zipFileName);
    ZipCreator zc(zipFileName);
    auto ok = zc.AddFile(StrL("premake5.lua"));
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.AddFile(StrL("premake5.files.lua"));
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.Finish();
    if (!ok) {
        printf("ZipCreateTest(): Finish() failed");
    }
}

int TesterMain() {
    RedirectIOToConsole();

    WCHAR* cmdLine = GetCommandLine();

    StrNode* argv = ParseCmdLine(cmdLine);
    defer {
        FreeStrNode(nullptr, argv);
    };

    // InitAllCommonControls();
    // ScopedGdiPlus gdi;

    StrNode* argNode = argv;
    for (int i = 0; argNode && i < 2; i++) {
        argNode = argNode->next;
    }
    int nArgs = 2;
    for (StrNode* n = argNode; n; n = n->next) {
        nArgs++;
    }
    int i = 2; // skip program name and "/tester"
    while (argNode) {
        Str arg = argNode->s;
        if (str::Eq(arg, StrL("-layout"))) {
            gLayout = true;
            argNode = argNode->next;
            ++i;
        } else if (str::Eq(arg, StrL("-save-html"))) {
            gSaveHtml = true;
            argNode = argNode->next;
            ++i;
        } else if (str::Eq(arg, StrL("-save-images"))) {
            gSaveImages = true;
            argNode = argNode->next;
            ++i;
        } else if (str::Eq(arg, StrL("-zip-create"))) {
            ZipCreateTest();
            argNode = argNode->next;
            ++i;
        } else {
            // unknown argument
            return Usage();
        }
    }
    if (2 == i) {
        // no arguments
        return Usage();
    }

    system("pause");
    return 0;
}
