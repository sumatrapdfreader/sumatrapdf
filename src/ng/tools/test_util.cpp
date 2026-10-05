/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

extern void AppendStoreTest();
extern void ArchiveTest();
extern void BaseUtilTest();
extern void ByteOrderTests();
extern void CryptoUtilTest();
extern void CssParser_UnitTests();
extern void DictTest();
extern void DirRemoveAllTest();
extern void FileUtilTest();
extern void GuessFileTypeTest();
extern void JsonTest();
extern void SettingsUtilTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void StrVecTest();
extern void VecTest();
extern bool AnnotSearch_UnitTests();
extern void ChapterTable_UnitTests();
extern void MobiDoc_UnitTests();
extern void LitDoc_UnitTests();
extern void PdfDarkModeImageClassifier_UnitTests();
extern void PdfDarkModeOklab_UnitTests();
extern void PdfSync_UnitTests();
extern void TextSelection_UnitTests();
extern void PagePosition_UnitTests();
extern void PageRenderPolicy_UnitTests();
extern void CommandPaletteModel_UnitTests();
extern void SumatraPDF_UnitTests();
extern void PageRenderService_UnitTests();
extern void CommandAvailability_UnitTests();
extern void MergePdf_UnitTests();
extern void Annotation_UnitTests();
extern void EngineDjvuDec_UnitTests();
extern void TipMarkup_UnitTests();
extern void ReadAloudHighlight_UnitTests();
extern void RefHoverTest();
extern void AppSettingsTest();
extern void SimpleLogTest();
extern bool ShortcutParse_UnitTestShiftedPunct();
extern bool ShortcutParse_UnitTestGpuiStroke();
extern bool ShortcutParse_UnitTestAccelTable();
extern bool MarkdownModel_UnitTestBrowserNavigationUrl();
extern bool MarkdownToc_UnitTestHtmlLinks();
extern bool MarkdownToc_UnitTestHtmlHeadings();
extern bool MarkdownToc_UnitTestMermaid();
#if IS_DEBUG
extern bool Accelerators_UnitTestFolderNavIsSafe();
extern bool NavFiles_UnitTestHidden();
#if !OS_WIN
extern bool MeasurementSystem_UnitTests();
extern bool DpiPosix_UnitTests();
#if !OS_WASM
extern bool AppToolsPosix_UnitTests();
#endif
#endif
extern bool Accelerators_UnitTestTreeTakesLetters();
extern bool Accelerators_UnitTestCreateAnnotEdit();
extern bool Accelerators_UnitTestCustomShortcutShown();
#endif
#if OS_WIN
extern void ClipboardImageTest();
extern void WinUtilTest();
#endif
#if OS_POSIX
extern void FileWatcher_UnitTests();
#if !OS_WASM
extern bool PlatformFontPosix_UnitTests();
#endif
#endif

// ng: test data lives in docs/test/. Works when run from the repo root and
// when run from out/<cfg>/, where the exe lives
TempStr TestDocPathTemp(Str relPath) {
    TempStr path = str::DupTemp(relPath);
    if (file::Exists(path)) {
        return path;
    }
    TempStr dir = path::JoinTemp(GetSelfExeDirTemp(), StrL("../.."));
    return path::JoinTemp(dir, relPath);
}

#if OS_WIN
static void PrintStdout(Str s) {
    if (str::IsNull(s)) {
        return;
    }
    printf("%.*s", s.len, s.s);
}

static LONG WINAPI ForAiCrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    printf("test_util crash\n");
    str::Builder s;
    dbghelp::GetExceptionInfo(s, exceptionInfo);
    PrintStdout(ToStr(s));
    fflush(stdout);
    ExitProcess(7);
    return EXCEPTION_EXECUTE_HANDLER;
}

// -for-ai: print assertion and crash callstacks to stdout instead of breaking
// into a debugger, so a script can report the failure
static void SetupForAi() {
    if (!dbghelp::Initialize(ToWStrTemp(GetSelfExeDirTemp()), true)) {
        printf("failed to initialize dbghelp symbols\n");
    }
    SetUnhandledExceptionFilter(ForAiCrashHandler);
}
#endif

int main(int argc, char** argv) {
    bool forAi = false;
    for (int i = 1; i < argc; i++) {
        if (str::Eq(Str(argv[i]), StrL("-for-ai"))) {
            forAi = true;
        }
    }
    if (forAi) {
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        utassert_set_for_ai(true);
    }
    printf("Running unit tests\n");

    InitDynCalls();
#if OS_WIN
    if (forAi) {
        SetupForAi();
    }
#endif

    AppendStoreTest();
    ArchiveTest();
    BaseUtilTest();
    ByteOrderTests();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    DirRemoveAllTest();
    FileUtilTest();
    GuessFileTypeTest();
    JsonTest();
    SettingsUtilTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    VecTest();
    utassert(AnnotSearch_UnitTests());
    ChapterTable_UnitTests();
    MobiDoc_UnitTests();
    LitDoc_UnitTests();
    PdfDarkModeImageClassifier_UnitTests();
    PdfDarkModeOklab_UnitTests();
    PdfSync_UnitTests();
    TextSelection_UnitTests();
    PagePosition_UnitTests();
    SumatraPDF_UnitTests();
    PageRenderService_UnitTests();
    CommandAvailability_UnitTests();
    MergePdf_UnitTests();
    Annotation_UnitTests();
    EngineDjvuDec_UnitTests();
    TipMarkup_UnitTests();
    ReadAloudHighlight_UnitTests();
    RefHoverTest();
    AppSettingsTest();
    SimpleLogTest();
    utassert(ShortcutParse_UnitTestShiftedPunct());
    utassert(ShortcutParse_UnitTestGpuiStroke());
    utassert(ShortcutParse_UnitTestAccelTable());
    utassert(MarkdownModel_UnitTestBrowserNavigationUrl());
    utassert(MarkdownToc_UnitTestHtmlLinks());
    utassert(MarkdownToc_UnitTestHtmlHeadings());
    utassert(MarkdownToc_UnitTestMermaid());
#if IS_DEBUG
    utassert(Accelerators_UnitTestFolderNavIsSafe());
    utassert(NavFiles_UnitTestHidden());
#if !OS_WIN
    utassert(MeasurementSystem_UnitTests());
    utassert(DpiPosix_UnitTests());
#if !OS_WASM
    utassert(AppToolsPosix_UnitTests());
#endif
#endif
    utassert(Accelerators_UnitTestTreeTakesLetters());
    utassert(Accelerators_UnitTestCreateAnnotEdit());
    utassert(Accelerators_UnitTestCustomShortcutShown());
#endif
#if OS_WIN
    ClipboardImageTest();
    WinUtilTest();
#endif
#if OS_POSIX
    FileWatcher_UnitTests();
#if !OS_WASM
    utassert(PlatformFontPosix_UnitTests());
#endif
#endif

    int res = utassert_print_results();
    DestroyTempArena();
    return res;
}
