#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/DirScan.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"
#include "base/UITask.h"

#if OS_WIN
#include <shlwapi.h>
#endif

extern "C" {
#include <mupdf/fitz.h>
}

#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "Annotation.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "LitDoc.h"
#include "DisplayModel.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "ChmModel.h"
#include "MarkdownModel.h"

void log(Str s) {
    if (len(s) == 0) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}

static void Usage() {
    printf("usage: test_engines <document-or-image-path>\n");
    printf("       test_engines <path> <out.png> [pageNo] render a page to a PNG\n");
    printf("       test_engines <path> -bench-mediabox   time PageMediabox() for every page\n");
    printf("       test_engines <path> -list-links       list link targets and rectangles\n");
    printf("       test_engines <path> -list-annots      list annotations\n");
    printf("       test_engines <path> -select-all-text  exercise text selection and extraction\n");
    printf("       test_engines <path> -find-text <term> search all pages for text\n");
    printf("       test_engines <path> -list-toc        list table-of-contents entries\n");
    printf("       test_engines <path> -list-properties list document properties\n");
    printf("       test_engines <path> -layout [zoom] [display-mode] lay out pages, print rects\n");
    printf("       test_engines <path> -model-toc       chm / markdown: pages and toc, no window\n");
}

static EngineBase* CreateEngineForPath(Str path) {
    if (IsEngineImageDirSupportedFile(path)) {
        return CreateEngineImageDirFromFile(path);
    }

    if (str::EndsWithI(path, StrL(".p7m"))) {
        Str wrapped = file::ReadFile(path);
        Str pdf = ExtractP7m(wrapped);
        str::Free(wrapped);
        if (GuessFileTypeFromData(pdf) != FileType::PDF) {
            str::Free(pdf);
            return nullptr;
        }
        EngineBase* engine = CreateEngineMupdfFromData(pdf, StrL("file.pdf"), nullptr);
        str::Free(pdf);
        if (engine) {
            engine->SetFilePath(path);
        }
        return engine;
    }

    FileType kind = GuessFileTypeFromName(path);
    if (IsEngineDjVuSupportedFileType(kind)) {
        return CreateEngineDjvuDecFromFile(path);
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return CreateEngineImageFromFile(path);
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return CreateEngineCbxFromFile(path, nullptr, kind);
    }
    if (kind == FileType::Lit) {
        return CreateEngineLitFromFile(path, nullptr);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
    return nullptr;
}

static bool RenderPath(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }

    int pageCount = engine->PageCount();
    printf("pages: %d\n", pageCount);
    bool ok = true;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        auto timeStart = TimeGet();
        RenderPageArgs args(pageNo, 1.0f, 0);
        Pixmap* pixmap = engine->RenderPage(args);
        double ms = TimeSinceInMs(timeStart);
        if (!pixmap) {
            printf("page %d: render failed %.2f ms\n", pageNo, ms);
            ok = false;
            continue;
        }
        printf("page %d: %d x %d %.2f ms\n", pageNo, pixmap->width, pixmap->height, ms);
        FreePixmap(pixmap);
    }

    engine->Release();
    return ok;
}

// ng: mupdf's PNG encoder, so the tool can write what it rendered. Pixmaps
// come out BGRA8 (or a Native DIB on Windows), fz wants RGB rows.
static bool SavePixmapAsPng(Pixmap* pixmap, Str path) {
    Pixmap* owned = nullptr;
    if (pixmap->format == PixmapFormat::Native) {
#if OS_WIN
        owned = PixmapCopyAs32bppDIB(pixmap);
#endif
        if (!owned) {
            printf("SavePixmapAsPng: can't read a native pixmap\n");
            return false;
        }
        pixmap = owned;
    }
    int w = pixmap->width;
    int h = pixmap->height;
    int bpp = PixmapBytesPerPixel(pixmap->format);
    bool isRgb = pixmap->format == PixmapFormat::RGBA8;

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        FreePixmap(owned);
        return false;
    }
    bool ok = false;
    fz_pixmap* fzPix = nullptr;
    fz_try(ctx) {
        fzPix = fz_new_pixmap(ctx, fz_device_rgb(ctx), w, h, nullptr, 0);
        for (int y = 0; y < h; y++) {
            const u8* src = pixmap->data + ((size_t)y * pixmap->stride);
            u8* dst = fzPix->samples + ((size_t)y * fzPix->stride);
            for (int x = 0; x < w; x++) {
                dst[0] = isRgb ? src[0] : src[2];
                dst[1] = src[1];
                dst[2] = isRgb ? src[2] : src[0];
                src += bpp;
                dst += 3;
            }
        }
        fz_save_pixmap_as_png(ctx, fzPix, CStrTemp(path));
        ok = true;
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, fzPix);
    }
    fz_catch(ctx) {
        printf("SavePixmapAsPng: %s\n", fz_caught_message(ctx));
    }
    fz_drop_context(ctx);
    FreePixmap(owned);
    return ok;
}

static void PrintProperties(EngineBase* engine) {
    Props props;
    engine->GetProperties(props);
    for (const PropValue& value : props) {
        TempStr name = PropNameTemp(value.prop);
        printf("%.*s: %.*s\n", name.len, name.s, value.val.len, value.val.s);
    }
}

// The "does this format work at all" check: load, report, render one page.
static bool RenderPageToPng(Str path, Str pngPath, int pageNo) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    printf("pages: %d\n", engine->PageCount());
    PrintProperties(engine);
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        printf("no page %d\n", pageNo);
        engine->Release();
        return false;
    }
    RenderPageArgs args(pageNo, 1.0f, 0);
    Pixmap* pixmap = engine->RenderPage(args);
    if (!pixmap) {
        printf("page %d: render failed\n", pageNo);
        engine->Release();
        return false;
    }
    bool ok = SavePixmapAsPng(pixmap, pngPath);
    printf("page %d: %d x %d -> %.*s %s\n", pageNo, pixmap->width, pixmap->height, pngPath.len, pngPath.s,
           ok ? "ok" : "FAILED");
    FreePixmap(pixmap);
    engine->Release();
    return ok;
}

static bool ListLinks(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }

    int linkCount = 0;
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        engine->BenchLoadPage(pageNo);
        Vec<IPageElement*> elements = engine->GetElements(pageNo);
        for (IPageElement* element : elements) {
            IPageDestination* dest = element->AsLink();
            if (!dest) {
                continue;
            }
            RectF rect = element->GetRect();
            Str value = dest->GetValue();
            printf("page %d: %.2f %.2f %.2f %.2f -> %.*s\n", pageNo, rect.x, rect.y, rect.dx, rect.dy, len(value),
                   value.s ? value.s : "");
            linkCount++;
        }
    }
    printf("links: %d\n", linkCount);
    engine->Release();
    return true;
}

// ng: what the annotation round-trip check reads back (step 13a)
static bool ListAnnots(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    Vec<Annotation*> annots;
    EngineMupdfGetAnnotations(engine, annots);
    for (Annotation* a : annots) {
        RectF r = GetBounds(a);
        Str name = AnnotationReadableNameTemp(a->type);
        str::Builder col;
        SerializePdfColor(GetColor(a), col);
        Str contents = Contents(a);
        printf("page %d: %.*s %.2f %.2f %.2f %.2f color=%.*s contents=%.*s\n", a->pageNo, len(name), name.s, r.x, r.y,
               r.dx, r.dy, len(ToStrTemp(col)), ToStrTemp(col).s, len(contents), contents.s ? contents.s : "");
    }
    printf("annotations: %d\n", len(annots));
    engine->Release();
    return true;
}

static bool SelectAllText(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }

    TextSelection selection(engine);
    selection.StartAt(1, 0);
    selection.SelectUpTo(engine->PageCount(), -1);
    Str text = selection.ExtractText(StrL("\n"));
    printf("selected bytes: %d\n", len(text));
    printf("selection rectangles: %d\n", len(selection.result));
    bool ok = len(text) > 0 && len(selection.result) > 0;
    str::Free(text);
    engine->Release();
    return ok;
}

static bool FindText(Str path, Str term) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }

    TextSearch search(engine);
    search.SetDirection(TextSearch::Direction::Forward);
    Vec<TextSel>* result = search.FindFirst(1, term);
    int matches = 0;
    while (result) {
        matches++;
        result = search.FindNext();
    }
    printf("matches: %d\n", matches);
    engine->Release();
    return matches > 0;
}

static int PrintTocItems(TocItem* item, int depth) {
    int count = 0;
    while (item) {
        printf("%*s%.*s -> page %d\n", depth * 2, "", item->title.len, item->title.s, item->pageNo);
        count += 1 + PrintTocItems(item->child, depth + 1);
        item = item->next;
    }
    return count;
}

static bool ListToc(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    TocTree* toc = engine->GetToc();
    int count = toc && toc->root ? PrintTocItems(toc->root->child, 0) : 0;
    printf("toc entries: %d\n", count);
    engine->Release();
    return count > 0;
}

static bool ListProperties(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    Props props;
    engine->GetProperties(props);
    for (const PropValue& value : props) {
        TempStr name = PropNameTemp(value.prop);
        printf("%.*s: %.*s\n", name.len, name.s, value.val.len, value.val.s);
    }
    printf("properties: %d\n", len(props));
    engine->Release();
    return len(props) > 0;
}

// Times PageMediabox() for every page: that's what the UI needs before it can
// lay out a document, and for image dirs / cbx it's the dominant open cost.
static bool BenchMediabox(Str path) {
    auto timeLoad = TimeGet();
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    double loadMs = TimeSinceInMs(timeLoad);
    int pageCount = engine->PageCount();

    auto timeStart = TimeGet();
    int nEmpty = 0;
    double maxMs = 0;
    // cheap order-sensitive digest of all page sizes, to compare runs
    u64 digest = 0;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        auto t = TimeGet();
        RectF mb = engine->PageMediabox(pageNo);
        double ms = TimeSinceInMs(t);
        if (ms > maxMs) {
            maxMs = ms;
        }
        digest = (digest * 1000003) + ((u64)(int)mb.dx * 65599) + (u64)(int)mb.dy;
        if (mb.IsEmpty()) {
            nEmpty++;
            printf("page %d: empty mediabox\n", pageNo);
        }
    }
    double totalMs = TimeSinceInMs(timeStart);
    printf("load: %.2f ms, pages: %d\n", loadMs, pageCount);
    printf("digest: %llu\n", (unsigned long long)digest);
    printf("mediabox: %.2f ms total, %.2f ms/page, max %.2f ms, empty %d\n", totalMs,
           pageCount ? totalMs / pageCount : 0, maxMs, nEmpty);

    engine->Release();
    return nEmpty == 0;
}

// Regression test for issue #5790: after the document's file is moved or
// deleted, Clone() must still succeed by re-using the bytes we hold in memory.
// Copies <srcPath> to a temp .pdf, loads it, deletes the temp file, then clones.
static bool CloneAfterDeleteTest(Str srcPath) {
    Str data = file::ReadFile(srcPath);
    if (len(data) == 0) {
        printf("CloneAfterDeleteTest: failed to read '%.*s'\n", srcPath.len, srcPath.s);
        return false;
    }
    TempStr tmp = str::JoinTemp(srcPath, StrL(".clonetest.pdf"));
    bool wrote = file::WriteFile(tmp, data);
    str::Free(data);
    if (!wrote) {
        printf("CloneAfterDeleteTest: failed to write temp copy '%s'\n", tmp.s);
        return false;
    }

    EngineBase* engine = CreateEngineForPath(tmp);
    if (!engine) {
        printf("CloneAfterDeleteTest: failed to load temp copy\n");
        file::Delete(tmp);
        return false;
    }
    int pages = engine->PageCount();

    bool deleted = file::Delete(tmp);
    printf("CloneAfterDeleteTest: deleted temp file: %d, file exists: %d\n", (int)deleted, (int)file::Exists(tmp));

    EngineBase* clone = engine->Clone();
    bool ok = clone != nullptr;
    printf("CloneAfterDeleteTest: Clone() after delete -> %s\n", ok ? "OK (non-null)" : "FAILED (null)");
    if (ok) {
        int clonePages = clone->PageCount();
        printf("CloneAfterDeleteTest: clone pages %d (orig %d)\n", clonePages, pages);
        ok = clonePages == pages;
        RenderPageArgs args(1, 1.0f, 0);
        Pixmap* pm = clone->RenderPage(args);
        printf("CloneAfterDeleteTest: clone render page 1 -> %s\n", pm ? "OK" : "FAILED");
        ok = ok && pm != nullptr;
        if (pm) {
            FreePixmap(pm);
        }
        clone->Release();
    }
    engine->Release();
    printf("CloneAfterDeleteTest: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Times path::IsOnNetworkDrive(): it sits in front of every cached file
// attribute query, and on drive-letter paths it can turn into a synchronous RPC
// to the LanmanWorkstation service.
static bool BenchIsOnNetworkDrive(Str path) {
    const int nIter = 2000;
    // first call in the process is a cache miss: that's the real recurring cost
    auto tFirst = TimeGet();
    bool first = path::IsOnNetworkDrive(path);
    double firstMs = TimeSinceInMs(tFirst);
    printf("first (uncached) call: %.4f ms -> %d\n", firstMs, (int)first);

    auto t = TimeGet();
    int nTrue = 0;
    for (int i = 0; i < nIter; i++) {
        if (path::IsOnNetworkDrive(path)) {
            nTrue++;
        }
    }
    double ms = TimeSinceInMs(t);
    printf("IsOnNetworkDrive('%.*s') -> %s\n", path.len, path.s, nTrue == nIter ? "true" : "false");
    printf("%d calls: %.2f ms total, %.4f ms/call\n", nIter, ms, ms / nIter);
    return true;
}

// Compares path::IsOnNetworkDrive() against PathIsNetworkPathW() (what it used
// to call) over a spread of path shapes. Windows-only: PathIsNetworkPathW and
// the drive-letter cases are Win32.
#if OS_WIN
static bool CheckIsOnNetworkDrive() {
    // X: is a mapped network drive here, Q: is unmapped
    struct TestCase {
        const char* path;
        bool want;
    };
    TestCase cases[] = {
        {R"(X:\tmp\file.pdf)", true},
        {R"(x:\tmp\file.pdf)", true},
        {R"(C:\Users\kjk\file.pdf)", false},
        {"c:/Users/kjk/file.pdf", false},
        {R"(\\server\share\file.pdf)", true},
        {R"(\\?\UNC\server\share\file.pdf)", true},
        {R"(\\?\C:\Users\file.pdf)", false},
        {R"(\\?\X:\tmp\file.pdf)", true},
        // a local device, not a network path: PathIsNetworkPathW() says true
        // here because it only looks for the leading "\\"
        {R"(\\.\PhysicalDrive0)", false},
        {"file.pdf", false},
        {R"(..\file.pdf)", false},
        {R"(Q:\unmapped\file.pdf)", false},
        {"", false},
        {"C", false},
    };
    bool ok = true;
    for (const TestCase& tc : cases) {
        Str path(tc.path);
        bool got = path::IsOnNetworkDrive(path);
        bool old = PathIsNetworkPathW(CWStrTemp(path)) != FALSE;
        const char* mark = (got == tc.want) ? "ok  " : "FAIL";
        printf("%s '%s' got=%d want=%d (PathIsNetworkPathW=%d)\n", mark, tc.path, (int)got, (int)tc.want, (int)old);
        ok = ok && (got == tc.want);
    }
    return ok;
}
#endif

// Times file::ReadN(): reads the first 64 KB of every file in a directory,
// which is what EngineImageDir::LoadMediabox() does per page.
// what file::ReadN() used to be on Windows: the portable CRT version, kept here
// so both can be timed in one process (cross-run timings are too noisy to tell
// them apart)
static int ReadNCrt(Str path, u8* buf, size_t toRead) {
    FILE* fp = file::OpenFILE(path);
    if (!fp) {
        return -1;
    }
    memset(buf, 0, toRead);
    size_t nRead = fread((void*)buf, 1, toRead, fp);
    int res = (int)nRead;
    if (nRead == 0 && ferror(fp)) {
        res = -1;
    }
    fclose(fp);
    return res;
}

static bool BenchReadN(Str dir) {
    StrVec files;
    DirIter di{dir};
    for (DirIterEntry* de : di) {
        files.Append(de->filePath);
    }
    int n = len(files);
    if (n == 0) {
        printf("no files in '%.*s'\n", dir.len, dir.s);
        return false;
    }
    const int bufSize = 64 * 1024;
    u8* buf = AllocArray<u8>(nullptr, bufSize);
    // alternate the two implementations pass by pass and keep each one's best,
    // so drift in the machine's state hits both equally
    const int nPasses = 8;
    double bestNew = 1e30;
    double bestCrt = 1e30;
    i64 bytesNew = 0;
    i64 bytesCrt = 0;
    for (int pass = 0; pass < nPasses; pass++) {
        auto t = TimeGet();
        i64 nBytes = 0;
        for (int i = 0; i < n; i++) {
            int nRead = file::ReadN(files.At(i), buf, (size_t)bufSize);
            if (nRead > 0) {
                nBytes += nRead;
            }
        }
        double ms = TimeSinceInMs(t);
        bytesNew = nBytes;
        if (ms < bestNew) {
            bestNew = ms;
        }

        t = TimeGet();
        nBytes = 0;
        for (int i = 0; i < n; i++) {
            int nRead = ReadNCrt(files.At(i), buf, (size_t)bufSize);
            if (nRead > 0) {
                nBytes += nRead;
            }
        }
        ms = TimeSinceInMs(t);
        bytesCrt = nBytes;
        if (ms < bestCrt) {
            bestCrt = ms;
        }
    }
    Free(nullptr, buf);
    printf("%d files, %lld bytes, best of %d alternating passes:\n", n, (long long)bytesNew, nPasses);
    printf("  file::ReadN (CreateFileW): %.2f ms, %.4f ms/file\n", bestNew, bestNew / n);
    printf("  CRT fopen/fread:           %.2f ms, %.4f ms/file\n", bestCrt, bestCrt / n);
    printf("  %.1f%% faster\n", (bestCrt - bestNew) * 100.0 / bestCrt);
    return bytesNew == bytesCrt;
}

// ng: proves the document model works with no window: build a DisplayModel for
// a document at a zoom and display mode and print the laid-out page rects
struct NoopDocControllerCallback : DocControllerCallback {
    void PageNoChanged(DocController*, int) override {}
    void ZoomChanged(DocController*, float) override {}
    void GotoLink(IPageDestination*) override {}
    void Repaint() override {}
    void UpdateScrollbars(DisplayModel*, Size) override {}
    void RequestRendering(DisplayModel*, int) override {}
    void RequestPredictiveRendering(DisplayModel*, int, const int*, int) override {}
    void CleanUp(DisplayModel*) override {}
    void RenderThumbnail(DisplayModel*, Size, const OnBitmapRendered*) override {}
    void FocusFrame(bool) override {}
    void SaveDownload(Str, Str) override {}
    void FindResultReceived(int, int, int) override {}
    void FindAllResultReceived(Str) override {}
    void TocChanged(DocController*) override {}
    void PagesRenumbered(DisplayModel*) override {}
};

static const Size kLayoutViewPort{800, 1000};

static bool LayoutDocument(Str path, float zoom, DisplayMode mode) {
    // DisplayModel reads WindowMargin / PageSpacing from the settings
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to open '%s'\n", path.s);
        return false;
    }

    NoopDocControllerCallback cb;
    auto* dm = new DisplayModel(engine, &cb);
    dm->SetInitialViewSettings(mode, 1, kLayoutViewPort, 96);
    dm->SetViewPortSize(kLayoutViewPort);
    dm->SetZoomVirtual(zoom, nullptr);

    Size canvas = dm->GetCanvasSize();
    printf("pages: %d, display mode: %s, zoom: %.2f\n", dm->PageCount(), DisplayModeToString(dm->GetDisplayMode()).s,
           dm->GetZoomVirtual());
    printf("viewport: %d x %d, canvas: %d x %d\n", kLayoutViewPort.dx, kLayoutViewPort.dy, canvas.dx, canvas.dy);
    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi) {
            continue;
        }
        printf("page %d: pos={%d,%d,%d,%d} zoomReal=%.2f shown=%d visible=%.2f\n", pageNo, pi->pos.x, pi->pos.y,
               pi->pos.dx, pi->pos.dy, pi->zoomReal, (int)pi->isShown, pi->visibleRatio);
    }

    bool ok = dm->PageCount() > 0 && canvas.dx > 0 && canvas.dy > 0;
    delete dm;
    return ok;
}

// ng: ChmModel and MarkdownModel are DocControllers with no EngineBase behind
// them (orig shows their pages in a browser control). Opens one and prints the
// page table and the table of contents the model built, with no window.
static bool ListModelToc(Str path) {
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    FileType kind = GuessFileType(path, true);
    NoopDocControllerCallback cb;
    DocController* ctrl = nullptr;
    if (ChmModel::IsSupportedFileType(kind)) {
        ctrl = ChmModel::Create(path, &cb);
    } else if (MarkdownModel::IsSupportedFileType(kind)) {
        ctrl = MarkdownModel::Create(path, &cb);
    } else {
        printf("not a chm or markdown document: %s\n", path.s);
        return false;
    }
    if (!ctrl) {
        printf("failed to open '%s'\n", path.s);
        return false;
    }

    // MarkdownModel fills the headings in on a worker and delivers the finished
    // tree through uitask; drain until it arrives (5 s is generous)
    uitask::Initialize(uitask::Dispatch::Queue);
    MarkdownModel* mm = ctrl->AsMarkdown();
    for (int i = 0; i < 500 && mm && mm->tocBuildTask; i++) {
        uitask::DrainQueue();
        if (mm->tocBuildTask) {
            SleepInMs(10);
        }
    }

    printf("model: %s, pages: %d, current page: %d\n", ctrl->GetDefaultFileExt().s, ctrl->PageCount(),
           ctrl->CurrentPageNo());
    TempStr title = ctrl->GetPropertyTemp(DocProp::Title);
    if (len(title) > 0) {
        printf("title: %s\n", title.s);
    }
    TocTree* toc = ctrl->GetToc();
    int count = toc && toc->root ? PrintTocItems(toc->root->child, 0) : 0;
    printf("toc entries: %d\n", count);

    // a chm without a toc file is still a document we opened
    bool ok = ctrl->PageCount() > 0;
    delete ctrl;
    uitask::Destroy();
    return ok;
}

int main(int argc, char** argv) {
    if ((argc == 3 || argc == 4) && argv[2][0] != '-') {
        int pageNo = (argc == 4) ? atoi(argv[3]) : 1;
        bool ok = RenderPageToPng(Str(argv[1]), Str(argv[2]), pageNo);
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 4 && str::Eq(Str(argv[2]), StrL("-find-text"))) {
        bool ok = FindText(Str(argv[1]), Str(argv[3]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc >= 3 && argc <= 5 && str::Eq(Str(argv[2]), StrL("-layout"))) {
        float zoom = (argc >= 4) ? ZoomFromString(Str(argv[3]), 100.f) : 100.f;
        DisplayMode mode =
            (argc == 5) ? DisplayModeFromString(Str(argv[4]), DisplayMode::Automatic) : DisplayMode::Automatic;
        bool ok = LayoutDocument(Str(argv[1]), zoom, mode);
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-model-toc"))) {
        bool ok = ListModelToc(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-list-toc"))) {
        bool ok = ListToc(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-list-properties"))) {
        bool ok = ListProperties(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-bench-readn"))) {
        bool ok = BenchReadN(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
#if OS_WIN
    if (argc == 2 && str::Eq(Str(argv[1]), StrL("-check-netdrive"))) {
        bool ok = CheckIsOnNetworkDrive();
        DestroyTempArena();
        return ok ? 0 : 1;
    }
#endif
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-bench-netdrive"))) {
        bool ok = BenchIsOnNetworkDrive(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-bench-mediabox"))) {
        bool ok = BenchMediabox(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-list-links"))) {
        bool ok = ListLinks(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-list-annots"))) {
        bool ok = ListAnnots(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-select-all-text"))) {
        bool ok = SelectAllText(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(Str(argv[2]), StrL("-clone-after-delete"))) {
        bool ok = CloneAfterDeleteTest(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc != 2) {
        Usage();
        return 1;
    }

    Str path(argv[1]);
    bool ok = RenderPath(path);
    DestroyTempArena();
    return ok ? 0 : 1;
}
