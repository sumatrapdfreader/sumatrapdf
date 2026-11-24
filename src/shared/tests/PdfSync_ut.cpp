/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "PdfSync.h"

#include "base/tests/UtAssert.h"

#if !OS_WASM

struct TestSync : Synchronizer {
    TestSync(Str syncPath, Str pdfPath) : Synchronizer(syncPath, pdfPath) {}
    int DocToSource(int, Point, Str&, int*, int*) override { return PDFSYNCERR_SUCCESS; }
    int SourceToDoc(Str, int, int, int*, Vec<Rect>&) override { return PDFSYNCERR_SUCCESS; }
};

// Moves the file's mtime forward by a second, as a LaTeX re-compile would.
static void BumpModTime(Str path) {
    FILETIME ft = file::GetModificationTime(path);
    ft = FileTimeFromU64(FileTimeToU64(ft) + kFileTimeTicksPerSec);
    file::SetModificationTime(path, ft);
}

// letter-sized pages; counts mediabox requests for pages that don't exist
struct SyncTestEngine : EngineBase {
    int badPageRequests = 0;

    explicit SyncTestEngine(int nPages) { pageCount = nPages; }
    EngineBase* Clone() override { return nullptr; }
    RectF PageMediabox(int pageNo) override {
        if (pageNo < 1 || pageNo > pageCount) {
            badPageRequests++;
        }
        return RectF(0, 0, 612, 792);
    }
    Pixmap* RenderPage(RenderPageArgs&) override { return nullptr; }
    Str GetFileData() override { return {}; }
    bool SaveFileAs(Str) override { return false; }
    TempStr GetPropertyTemp(DocProp) override { return {}; }
    Vec<IPageElement*> GetElements(int) override { return Vec<IPageElement*>(); }
    IPageElement* GetElementAtPos(int, PointF) override { return nullptr; }
};

static Synchronizer* CreatePdfsync(Str dir, Str syncData, EngineBase* engine) {
    TempStr pdfPath = path::JoinTemp(dir, StrL("doc.pdf"));
    TempStr syncPath = path::JoinTemp(dir, StrL("doc.pdfsync"));
    utassert(file::WriteFile(syncPath, syncData));
    Synchronizer* sync = nullptr;
    utassert(Synchronizer::Create(pdfPath, engine, &sync) == PDFSYNCERR_SUCCESS);
    return sync;
}

// a bogus sheet number in a .pdfsync must not reach the engine as page -1,
// and sheets are found by number, not by order of appearance
static void PdfsyncSheetsTest() {
    TempStr dir = GetTempFilePathTemp(StrL("pdfsync-ut"));
    file::Delete(dir);
    utassert(dir::CreateAll(dir));
    TempStr texPath = path::JoinTemp(dir, StrL("doc.tex"));

    SyncTestEngine* engine = new SyncTestEngine(2);
    Str data = StrL("doc\nversion 1\nl 1 5\nl 2 7\ns 4294967295\np 1 100 100\ns 2\np 1 200 200\ns 1\np 2 300 300\n");
    Synchronizer* sync = CreatePdfsync(dir, data, engine);

    int page = 0;
    Vec<Rect> rects;
    utassert(sync->SourceToDoc(texPath, 5, 0, &page, rects) == PDFSYNCERR_SUCCESS);
    utassert(page == 2);
    utassert(len(rects) == 1);
    utassert(engine->badPageRequests == 0);

    Str srcFile;
    int line = 0;
    int col = 0;
    Point pt(0, 792); // every sync point is within a pixel of the top-left corner
    utassert(sync->DocToSource(1, pt, srcFile, &line, &col) == PDFSYNCERR_SUCCESS);
    utassert(line == 7);
    str::Free(srcFile);

    delete sync;
    SafeEngineRelease(&engine);
    dir::RemoveAll(dir);
}

void PdfSync_UnitTests() {
    PdfsyncSheetsTest();

    TempStr tmp = GetTempFilePathTemp(StrL("synctex-ut"));
    utassert(len(tmp) > 0);
    TempStr syncPath = str::JoinTemp(tmp, StrL(".synctex"));
    utassert(file::WriteFile(syncPath, StrL("SyncTeX Version:1\n")));

    TestSync sync(syncPath, StrL("doc.pdf"));
    utassert(sync.NeedsToRebuildIndex());
    sync.MarkIndexWasRebuilt();
    utassert(!sync.NeedsToRebuildIndex());

    // the sync file changed on disk
    BumpModTime(syncPath);
    utassert(sync.NeedsToRebuildIndex());

    // the rebuild failed, so it never called MarkIndexWasRebuilt(): the next
    // query must rebuild too, not run against an index that was never built
    utassert(sync.NeedsToRebuildIndex());

    file::Delete(syncPath);
    file::Delete(tmp);
}

#else

void PdfSync_UnitTests() {}

#endif
