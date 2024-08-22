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
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

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
    printf("  -mobi dirOrFile : run mobi tests in a given directory or for a given file\n");
    printf("  -layout - will also layout mobi files\n");
    printf("  -save-html] - will save html content of mobi file\n");
    printf("  -save-images - will save images extracted from mobi files\n");
    printf("  -zip-create - creates a sample zip file that needs to be manually checked that it worked\n");
    printf("  -bench-md5 - compare Window's md5 vs. our code\n");
    system("pause");
    return 1;
}

static void MobiSaveHtml(const char* filePathBase, MobiDoc* mb) {
    ReportIf(!gSaveHtml);

    char* outFile = str::JoinTemp(filePathBase, "_pp.html");

    ByteSlice htmlData = mb->GetHtmlData();

    ByteSlice ppHtml = PrettyPrintHtml(htmlData);
    file::WriteFile(outFile, ppHtml);

    outFile = str::JoinTemp(filePathBase, ".html");
    file::WriteFile(outFile, htmlData);
}

static void MobiSaveImage(const char* filePathBase, size_t imgNo, ByteSlice img) {
    // it's valid to not have image data at a given index
    if (img.empty()) {
        return;
    }
    const char* ext = GfxFileExtFromData(img);
    ReportIf(!ext);
    TempStr path = str::FormatTemp("%s_img_%d%s", filePathBase, (int)imgNo, ext);
    file::WriteFile(path, img);
}

static void MobiSaveImages(const char* filePathBase, MobiDoc* mb) {
    for (size_t i = 0; i < mb->imagesCount; i++) {
        ByteSlice* img = mb->GetImage(i + 1);
        if (!img) {
            continue;
        }
        MobiSaveImage(filePathBase, i, *img);
    }
}

// This loads and layouts a given mobi file. Used for profiling layout process.
static void MobiLayout(MobiDoc* mobiDoc) {
    PoolAllocator textAllocator;

    HtmlFormatterArgs args;
    args.pageDx = 640;
    args.pageDy = 480;
    args.SetFontName(L"Tahoma");
    args.fontSize = 12;
    args.htmlStr = mobiDoc->GetHtmlData();
    args.textAllocator = &textAllocator;

    MobiFormatter mf(&args, mobiDoc);
    Vec<HtmlPage*>* pages = mf.FormatAllPages();
    DeleteVecMembers<HtmlPage*>(*pages);
    delete pages;
}

static void MobiTestFile(const char* filePath) {
    printf("Testing file '%s'\n", filePath);
    MobiDoc* mobiDoc = MobiDoc::CreateFromFile(filePath);
    if (!mobiDoc) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (gLayout) {
        auto t = TimeGet();
        MobiLayout(mobiDoc);
        printf("Spent %.2f ms laying out %s\n", TimeSinceInMs(t), filePath);
    }

    if (gSaveHtml || gSaveImages) {
        // Given the name of the name of source mobi file "${srcdir}/${file}.mobi"
        // construct a base name for extracted html/image files in the form
        // "${kMobiSaveDir}/${file}" i.e. change dir to kMobiSaveDir and
        // remove the file extension
        const char* dir = kMobiSaveDir;
        dir::CreateAll(dir);
        TempStr fileName = path::GetBaseNameTemp(filePath);
        TempStr filePathBase = path::JoinTemp(dir, fileName);
        char* ext = str::FindCharLast(filePathBase, '.');
        *ext = 0;

        if (gSaveHtml) {
            MobiSaveHtml(filePathBase, mobiDoc);
        }
        if (gSaveImages) {
            MobiSaveImages(filePathBase, mobiDoc);
        }
    }

    delete mobiDoc;
}

static void MobiTestDir(char* dir) {
    printf("Testing mobi files in '%s'\n", dir);
    DirIter di{dir};
    di.recurse = true;
    for (DirIterEntry* de : di) {
        const char* path = de->filePath;
        Kind kind = GuessFileTypeFromName(path);
        if (kind == kindFileMobi) {
            MobiTestFile(path);
        }
    }
}

static void MobiTest(char* dirOrFile) {
    Kind kind = GuessFileTypeFromName(dirOrFile);
    if (file::Exists(dirOrFile) && kind == kindFileMobi) {
        MobiTestFile(dirOrFile);
    } else if (path::IsDirectory(dirOrFile)) {
        MobiTestDir(dirOrFile);
    }
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

    bool mobiTest = false;
    int i = 2; // skip program name and "/tester"
    while (i < nArgs) {
        char* arg = argv.at(i);
        if (str::Eq(arg, "-mobi")) {
            ++i;
            if (i == nArgs) {
                return Usage();
            }
            mobiTest = true;
            dirOrFile = argv.at(i);
            ++i;
        } else if (str::Eq(arg, "-layout")) {
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

    if (mobiTest) {
        MobiTest(dirOrFile);
    }

    mui::Destroy();
    system("pause");
    return 0;
}
