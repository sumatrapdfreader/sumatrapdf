/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments. */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineParser.h"
#include "utils/CryptoUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPrettyPrint.h"
#include "mui/Mui.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to MOBI_SAVE_DIR. The name will be
// ${file}.html and ${file}_pp.html
static bool gSaveHtml = false;
// if true, we'll also save images in mobi files. The name
// will be ${file}_img_${imgNo}.[jpg|png]
// gMobiSaveHtml must be true as well
static bool gSaveImages = false;
// if true, we'll do a layout of mobi files
static bool gLayout = false;
// directory to which we'll save mobi html and images
#define MOBI_SAVE_DIR L"..\\ebooks-converted"

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

/* This benchmarks md5 checksum using fitz code (CalcMD5Digest()) and
Windows' CryptoAPI (CalcMD5DigestWin(). The results are usually in CryptoApi
favor (the first run is on cold cache, the second on warm cache):
10MB
CalcMD5Digest   : 76.913000 ms
CalcMD5DigestWin: 92.389000 ms
diff: -15.476000
5MB
CalcMD5Digest   : 17.556000 ms
CalcMD5DigestWin: 13.125000 ms
diff: 4.431000
1MB
CalcMD5Digest   : 3.329000 ms
CalcMD5DigestWin: 2.834000 ms
diff: 0.495000
10MB
CalcMD5Digest   : 33.682000 ms
CalcMD5DigestWin: 25.918000 ms
diff: 7.764000
5MB
CalcMD5Digest   : 16.174000 ms
CalcMD5DigestWin: 12.853000 ms
diff: 3.321000
1MB
CalcMD5Digest   : 3.534000 ms
CalcMD5DigestWin: 2.605000 ms
diff: 0.929000
*/

static void BenchMD5Size(void* data, size_t dataSize, char* desc) {
    unsigned char d1[16], d2[16];
    auto t1 = TimeGet();
    CalcMD5Digest((unsigned char*)data, dataSize, d1);
    double dur1 = TimeSinceInMs(t1);

    auto t2 = TimeGet();
    CalcMD5DigestWin(data, dataSize, d2);
    bool same = memeq(d1, d2, 16);
    CrashAlwaysIf(!same);
    double dur2 = TimeSinceInMs(t2);
    double diff = dur1 - dur2;
    printf("%s\nCalcMD5Digest   : %f ms\nCalcMD5DigestWin: %f ms\ndiff: %f\n", desc, dur1, dur2, diff);
}

static void BenchMD5() {
    size_t dataSize = 10 * 1024 * 1024;
    void* data = malloc(dataSize);
    BenchMD5Size(data, dataSize, "10MB");
    BenchMD5Size(data, dataSize / 2, "5MB");
    BenchMD5Size(data, dataSize / 10, "1MB");
    // repeat to see if timings change drastically
    BenchMD5Size(data, dataSize, "10MB");
    BenchMD5Size(data, dataSize / 2, "5MB");
    BenchMD5Size(data, dataSize / 10, "1MB");
    free(data);
}

static void MobiSaveHtml(const WCHAR* filePathBase, MobiDoc* mb) {
    CrashAlwaysIf(!gSaveHtml);

    AutoFreeWstr outFile(str::Join(filePathBase, L"_pp.html"));

    const std::string_view htmlData = mb->GetHtmlData();
    size_t htmlLen = htmlData.size();
    const char* html = htmlData.data();
    size_t ppHtmlLen;
    char* ppHtml = PrettyPrintHtml(html, htmlLen, ppHtmlLen);
    file::WriteFile(outFile.Get(), {ppHtml, ppHtmlLen});

    outFile.Set(str::Join(filePathBase, L".html"));
    file::WriteFile(outFile.Get(), {html, htmlLen});
}

static void MobiSaveImage(const WCHAR* filePathBase, size_t imgNo, ImageData* img) {
    // it's valid to not have image data at a given index
    if (!img || !img->data)
        return;
    const WCHAR* ext = GfxFileExtFromData(img->data, img->len);
    CrashAlwaysIf(!ext);
    AutoFreeWstr fileName(str::Format(L"%s_img_%d%s", filePathBase, imgNo, ext));
    file::WriteFile(fileName.Get(), {img->data, img->len});
}

static void MobiSaveImages(const WCHAR* filePathBase, MobiDoc* mb) {
    for (size_t i = 0; i < mb->imagesCount; i++) {
        MobiSaveImage(filePathBase, i, mb->GetImage(i + 1));
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

static void MobiTestFile(const WCHAR* filePath) {
    wprintf(L"Testing file '%s'\n", filePath);
    MobiDoc* mobiDoc = MobiDoc::CreateFromFile(filePath);
    if (!mobiDoc) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (gLayout) {
        auto t = TimeGet();
        MobiLayout(mobiDoc);
        wprintf(L"Spent %.2f ms laying out %s\n", TimeSinceInMs(t), filePath);
    }

    if (gSaveHtml || gSaveImages) {
        // Given the name of the name of source mobi file "${srcdir}/${file}.mobi"
        // construct a base name for extracted html/image files in the form
        // "${MOBI_SAVE_DIR}/${file}" i.e. change dir to MOBI_SAVE_DIR and
        // remove the file extension
        WCHAR* dir = MOBI_SAVE_DIR;
        dir::CreateAll(dir);
        AutoFreeWstr fileName(str::Dup(path::GetBaseNameNoFree(filePath)));
        AutoFreeWstr filePathBase(path::Join(dir, fileName));
        WCHAR* ext = (WCHAR*)str::FindCharLast(filePathBase.Get(), '.');
        *ext = 0;

        if (gSaveHtml)
            MobiSaveHtml(filePathBase, mobiDoc);
        if (gSaveImages)
            MobiSaveImages(filePathBase, mobiDoc);
    }

    delete mobiDoc;
}

static bool IsMobiFile(const WCHAR* f) {
    return str::EndsWithI(f, L".mobi") || str::EndsWithI(f, L".azw") || str::EndsWithI(f, L".azw1") ||
           str::EndsWithI(f, L".prc");
}

static void MobiTestDir(WCHAR* dir) {
    wprintf(L"Testing mobi files in '%s'\n", dir);
    DirIter di(dir, true);
    for (const WCHAR* p = di.First(); p; p = di.Next()) {
        if (IsMobiFile(p))
            MobiTestFile(p);
    }
}

static void MobiTest(WCHAR* dirOrFile) {
    if (file::Exists(dirOrFile) && IsMobiFile(dirOrFile))
        MobiTestFile(dirOrFile);
    else
        MobiTestDir(dirOrFile);
}

// we assume this is called from main sumatradirectory, e.g. as:
// ./obj-dbg/tester.exe, so we use the known files
void ZipCreateTest() {
    WCHAR* zipFileName = L"tester-tmp.zip";
    file::Delete(zipFileName);
    ZipCreator zc(zipFileName);
    auto ok = zc.AddFile(L"premake5.lua");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.AddFile(L"premake5.files.lua");
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

    WStrVec argv;
    ParseCmdLine(cmdLine, argv);

    // InitAllCommonControls();
    // ScopedGdiPlus gdi;
    // mui::Initialize();

    WCHAR* dirOrFile = nullptr;

    bool mobiTest = false;
    size_t i = 2; // skip program name and "/tester"
    while (i < argv.size()) {
        if (str::Eq(argv[i], L"-mobi")) {
            ++i;
            if (i == argv.size())
                return Usage();
            mobiTest = true;
            dirOrFile = argv[i];
            ++i;
        } else if (str::Eq(argv[i], L"-layout")) {
            gLayout = true;
            ++i;
        } else if (str::Eq(argv[i], L"-save-html")) {
            gSaveHtml = true;
            ++i;
        } else if (str::Eq(argv[i], L"-save-images")) {
            gSaveImages = true;
            ++i;
        } else if (str::Eq(argv[i], L"-zip-create")) {
            ZipCreateTest();
            ++i;
        } else if (str::Eq(argv[i], L"-bench-md5")) {
            BenchMD5();
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
