#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include <shlwapi.h>

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"

void _uploadDebugReport(Str, Str, bool, bool) {}

void log(Str s) {
    if (!s) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}

void loga(Str s) {
    log(s);
}

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

static void Usage() {
    printf("usage: test_engines <document-or-image-path>\n");
    printf("       test_engines <path> -bench-mediabox   time PageMediabox() for every page\n");
}

static EngineBase* CreateEngineForPath(Str path) {
    if (IsEngineImageDirSupportedFile(path)) {
        return CreateEngineImageDirFromFile(path);
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
        digest = digest * 1000003 + (u64)(int)mb.dx * 65599 + (u64)(int)mb.dy;
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
// to call) over a spread of path shapes.
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

int main(int argc, char** argv) {
    if (argc == 2 && str::Eq(argv[1], StrL("-check-netdrive"))) {
        bool ok = CheckIsOnNetworkDrive();
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-bench-netdrive"))) {
        bool ok = BenchIsOnNetworkDrive(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-bench-mediabox"))) {
        bool ok = BenchMediabox(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-clone-after-delete"))) {
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
