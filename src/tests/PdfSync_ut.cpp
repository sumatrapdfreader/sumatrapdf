/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/tests/UtAssert.h"

#include "PdfSync.h"

struct TestSync : Synchronizer {
    TestSync(Str syncPath, Str pdfPath) : Synchronizer(syncPath, pdfPath) {}
    int DocToSource(int, Point, Str&, int*, int*) override { return PDFSYNCERR_SUCCESS; }
    int SourceToDoc(Str, int, int, int*, Vec<Rect>&) override { return PDFSYNCERR_SUCCESS; }
};

// Moves the file's mtime forward by a second, as a LaTeX re-compile would.
static void BumpModTime(Str path) {
    constexpr i64 kOneSecIn100ns = 10LL * 1000 * 1000;

    FILETIME ft = file::GetModificationTime(path);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart += kOneSecIn100ns;
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    file::SetModificationTime(path, ft);
}

void PdfSync_UnitTests() {
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
