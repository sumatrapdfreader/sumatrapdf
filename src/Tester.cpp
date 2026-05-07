/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments. */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/CryptoUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPrettyPrint.h"
#include "mui/Mui.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

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
#define kMobiSaveDir "..\\ebooks-converted"

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
    const char* zipFileName = "tester-tmp.zip";
    file::Delete(zipFileName);
    ZipCreator zc(zipFileName);
    auto ok = zc.AddFile("premake5.lua");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.AddFile("premake5.files.lua");
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

    CmdLineArgsIter argv(cmdLine);
    int nArgs = argv.nArgs;

    // InitAllCommonControls();
    // ScopedGdiPlus gdi;
    // mui::Initialize();

    char* dirOrFile = nullptr;

    int i = 2; // skip program name and "/tester"
    while (i < nArgs) {
        char* arg = argv.at(i);
        if (str::Eq(arg, "-layout")) {
            gLayout = true;
            ++i;
        } else if (str::Eq(arg, "-save-html")) {
            gSaveHtml = true;
            ++i;
        } else if (str::Eq(arg, "-save-images")) {
            gSaveImages = true;
            ++i;
        } else if (str::Eq(arg, "-zip-create")) {
            ZipCreateTest();
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

    mui::Destroy();
    system("pause");
    return 0;
}
