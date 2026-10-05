/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "EngineBase.h"
#include "EngineAll.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

void MergePdf_UnitTests() {
    TempStr basePath = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
    TempStr addPath = TestDocPathTemp(StrL("docs/test/bookmarks.pdf"));
    TempStr destPath = GetTempFilePathTemp(StrL("merge-pdf-test"));
    EngineBase* base = CreateEngineFromFile(basePath, nullptr, true);
    EngineBase* add = CreateEngineFromFile(addPath, nullptr, true);
    utassert(base && add);
    Vec<PdfMergeSource> sources;
    VecAppend(sources, PdfMergeSource{basePath, {}});
    VecAppend(sources, PdfMergeSource{addPath, {}});
    Vec<PdfMergePage> pages;
    VecAppend(pages, PdfMergePage{1, 1});
    VecAppend(pages, PdfMergePage{0, 1});
    VecAppend(pages, PdfMergePage{1, 1});
    int expectedPages = len(pages);
    utassert(EngineMupdfMergePdfs(sources, pages, destPath));
    SafeEngineRelease(&base);
    SafeEngineRelease(&add);
    utassert(file::Exists(destPath));
    EngineBase* merged = CreateEngineMupdfFromFile(destPath, FileType::PDF, 96);
    utassert(merged && merged->PageCount() == expectedPages);
    SafeEngineRelease(&merged);
    file::Delete(destPath);
}
