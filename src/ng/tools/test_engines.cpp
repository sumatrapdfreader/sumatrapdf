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
#include <crtdbg.h>
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
#include "RtfDoc.h"
#include "DisplayModel.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "tools/TestEnginesCommon.h"

static void Usage() {
    printf("usage: test_engines <document-or-image-path>\n");
    printf("       test_engines <path> <out.png> [pageNo] render a page to a PNG\n");
    printf("       test_engines <path> -bench-mediabox   time PageMediabox() for every page\n");
    printf("       test_engines <path> -list-links       list link targets and rectangles\n");
    printf("       test_engines <path> -list-annots      list annotations\n");
    printf("       test_engines <path> -select-all-text  exercise text selection and extraction\n");
    printf("       test_engines <path> -find-text <term> search all pages for text\n");
    printf("       test_engines <path> -dump-text <out.txt> write the text of all pages\n");
    printf("       test_engines <path> -list-toc        list table-of-contents entries\n");
    printf("       test_engines <path> -list-properties list document properties\n");
    printf("       test_engines <path> -layout [zoom] [display-mode] lay out pages, print rects\n");
    printf("       test_engines <path> -model-toc       chm / markdown: pages and toc, no window\n");
}

EngineBase* CreateEngineForPath(Str path) {
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
    if (kind == FileType::Rtf) {
        return CreateEngineRtfFromFile(path, nullptr);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
    return nullptr;
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

// ng: what the annotation round-trip check reads back (step 13a)
static bool ListAnnots(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    Vec<Annotation*> annots;
    EngineGetAnnotations(engine, annots);
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

// the text of every page, to compare with another program's
static bool DumpText(Str path, Str outPath) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }
    str::Builder text;
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        PageText pt = engine->ExtractPageText(pageNo);
        text.Append(pt.text);
        text.AppendChar('\n');
        FreePageText(&pt);
    }
    bool ok = file::WriteFile(outPath, ToStr(text));
    engine->Release();
    return ok;
}

// Compares path::IsOnNetworkDrive() against PathIsNetworkPathW() (what it used
// to call) over a spread of path shapes. Windows-only: PathIsNetworkPathW and
// the drive-letter cases are Win32.
#if OS_WIN
#endif

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
#if OS_WIN && defined(_DEBUG)
    // a debug CRT assert must not stop an unattended run on a modal dialog
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
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
    if (argc == 4 && str::Eq(Str(argv[2]), StrL("-dump-text"))) {
        bool ok = DumpText(Str(argv[1]), Str(argv[3]));
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
