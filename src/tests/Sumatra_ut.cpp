/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#if IS_DEBUG

#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "AppTools.h"
#include "Settings.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "Flags.h"
#include "Commands.h"
#include "CrashHandler.h"
#include "AppUnitTests.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

// in src/base/tests/
void AppendStoreTest();
void BaseUtilTest();
void ByteOrderTests();
void ClipboardImageTest();
void CryptoUtilTest();
void CssParser_UnitTests();
void DictTest();
void DirRemoveAllTest();
void FileUtilTest();
void GuessFileTypeTest();
void JsonTest();
void RefHoverTest();
void SettingsUtilTest();
void SquareTreeTest();
void StrFormatTest();
void StrTest();
void StrVecTest();
void VecTest();
void WinUtilTest();

// in src/tests/*_ut.cpp
void ChapterTable_UnitTests();
void PagePosition_UnitTests();
void PageRenderPolicy_UnitTests();
void PdfDarkModeImageClassifier_UnitTests();
void PdfDarkModeOklab_UnitTests();
void SimpleLogTest();

void CommandPaletteModel_UnitTests();
void TextSelection_UnitTests();
void Layout_UnitTests();
void VirtCtrl_UnitTests();
bool TableOfContents_UnitTestSnapshotNamedDest();
bool MarkdownModel_UnitTestBrowserNavigationUrl();
bool MarkdownToc_UnitTestHtmlLinks();
bool MarkdownToc_UnitTestHtmlHeadings();
bool MarkdownToc_UnitTestMermaid();
bool EbookDoc_UnitTestNormalizeURL();
bool ExternalViewers_UnitTestPDFXChangePaths();
bool Canvas_UnitTestScrollLineAmount();
bool EngineMupdf_UnitTestEbookLineSpacingCss();
bool EngineMupdf_UnitTestEbookFontFamilyCss();
bool EngineMupdf_UnitTestEbookMarginCss();
bool EngineMupdf_UnitTestMergeEBookUI();
bool EngineMupdf_UnitTestPageLabels();
bool Accelerators_UnitTestFolderNavIsSafe();
bool Accelerators_UnitTestTreeTakesLetters();
bool ShortcutParse_UnitTestShiftedPunct();
bool AnnotSearch_UnitTests();
void ReadAloudHighlight_UnitTests();

#if OS_WIN
static void ParseCommandLineTest() {
    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench foo.pdf", i);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("foo.pdf"), i.pathsToBenchmark[0]));
        utassert(len(i.pathsToBenchmark[1]) == 0);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5", i);
        utassert(len(i.globalPrefArgs) == 2);
        Str s = i.globalPrefArgs[0];
        utassert(str::Eq(s, StrL("-fwdsearch-width")));
        s = i.globalPrefArgs[1];
        utassert(str::Eq(s, StrL("5")));
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("foo.pdf"), i.pathsToBenchmark[0]));
        utassert(len(i.pathsToBenchmark[1]) == 0);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf loadonly", i);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("loadonly"), i.pathsToBenchmark[1]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf 1 -set-color-range 0x123456 #abCDef", i);
        utassert(len(i.globalPrefArgs) == 3);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("1"), i.pathsToBenchmark[1]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34", i);
        utassert(4 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("1-5,3"), i.pathsToBenchmark[1]));
        utassert(str::Eq(StrL("some.pdf"), i.pathsToBenchmark[2]));
        utassert(str::Eq(StrL("1,3,8-34"), i.pathsToBenchmark[3]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf", i);
        utassert(true == i.enterPresentation);
        utassert(true == i.invertColors);
        utassert(2 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL("foo.pdf")));
        utassert(1 == i.fileNames.Find(StrL("bar.pdf")));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf", i);
        utassert(true == i.invertColors);
        utassert(1 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL("rosanna.pdf")));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), LR"(SumatraPDF.exe "foo \" bar \\.pdf" un\"quoted.pdf)", i);
        utassert(2 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL(R"(foo " bar \\.pdf)")));
        utassert(1 == i.fileNames.Find(StrL(R"(un"quoted.pdf)")));
    }

    {
        Flags i;
        ParseFlags(
            GetPermArena(),
            L"SumatraPDF.exe -page 37 -view continuousfacing -zoom fitcontent -scroll 45,1234         -reuse-instance",
            i);
        utassert(0 == len(i.fileNames));
        utassert(i.pageNumber == 37);
        utassert(i.startView == DisplayMode::ContinuousFacing);
        utassert(i.startZoom == kZoomFitContent);
        utassert(i.startScroll.x == 45 && i.startScroll.y == 1234);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), LR"(SumatraPDF.exe -view "single page" -zoom 237.45 -scroll -21,-1)", i);
        utassert(0 == len(i.fileNames));
        utassert(i.startView == DisplayMode::SinglePage);
        utassert(i.startZoom == 237.45f);
        utassert(i.startScroll.x == -21 && i.startScroll.y == -1);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -zoom 35%", i);
        utassert(0 == len(i.fileNames));
        utassert(i.startZoom == 35.f);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -zoom fit-content", i);
        utassert(i.startZoom == kZoomFitContent);
        utassert(0 == len(i.fileNames));
    }
}
#endif

static void BenchRangeTest() {
    utassert(IsBenchPagesInfo(StrL("1")));
    utassert(IsBenchPagesInfo(StrL("2-4")));
    utassert(IsBenchPagesInfo(StrL("5,7")));
    utassert(IsBenchPagesInfo(StrL("6,8,")));
    utassert(IsBenchPagesInfo(StrL("1-3,4,6-9,13")));
    utassert(IsBenchPagesInfo(StrL("2-")));
    utassert(IsBenchPagesInfo(StrL("loadonly")));

    utassert(!IsBenchPagesInfo(StrL("")));
    utassert(!IsBenchPagesInfo(StrL("-2")));
    utassert(!IsBenchPagesInfo(StrL("2--4")));
    utassert(!IsBenchPagesInfo(StrL("4-2")));
    utassert(!IsBenchPagesInfo(StrL("1-3,loadonly")));
    utassert(!IsBenchPagesInfo({}));
}

static void versioncheck_test() {
    utassert(IsValidProgramVersion(StrL("1")));
    utassert(IsValidProgramVersion(StrL("1.1")));
    utassert(IsValidProgramVersion(StrL("1.1.1\r\n")));
    utassert(IsValidProgramVersion(StrL("2662")));

    utassert(!IsValidProgramVersion(StrL("1.1b")));
    utassert(!IsValidProgramVersion(StrL("1..1")));
    utassert(!IsValidProgramVersion(StrL("1.1\r\n.1")));

    utassert(CompareProgramVersion(StrL("0.9.3.900"), StrL("0.9.3")) > 0);
    utassert(CompareProgramVersion(StrL("1.09.300"), StrL("1.09.3")) > 0);
    utassert(CompareProgramVersion(StrL("1.9.1"), StrL("1.09.3")) < 0);
    utassert(CompareProgramVersion(StrL("1.2.0"), StrL("1.2")) == 0);
    utassert(CompareProgramVersion(StrL("1.3.0"), StrL("2662")) < 0);
}

static void hexstrTest() {
    u8 buf[6] = {1, 2, 33, 255, 0, 18};
    u8 buf2[6]{};
    TempStr s = str::MemToHexTemp(Str((const char*)buf, dimofi(buf)));
    utassert(str::Eq(s, StrL("010221ff0012")));
    bool ok = str::HexToMem(s, Str((char*)buf2, dimofi(buf2)));
    utassert(ok);
    utassert(MemEq(buf, buf2, dimofi(buf)));

    FILETIME ft1{123, 456}, ft2;
    s = str::MemToHexTemp(Str((const char*)&ft1, sizeofi(ft1)));
    str::HexToMem(s, Str((char*)&ft2, sizeofi(ft2)));
    DWORD diff = FileTimeDiffInSecs(ft1, ft2);
    utassert(0 == diff);
    utassert(FileTimeEq(ft1, ft2));

    s = str::MemToHexTemp(Str());
    utassert(str::Eq(s, StrL("")));
    ok = str::HexToMem(s, Str());
    utassert(ok);
}

static void assertSerializedColor(Color c, Str s) {
    TempStr s2 = SerializeColorTemp(c);
    utassert(str::Eq(s2, s));
}

static void colorTest() {
    Color c = 0;
    bool ok = ParseColor(&c, StrL("0x01020304"));
    utassert(ok);
    assertSerializedColor(c, StrL("#01020304"));

    ok = ParseColor(&c, StrL("#01020304"));
    utassert(ok);
    assertSerializedColor(c, StrL("#01020304"));

    Color c2 = MkRgba(2, 3, 4, 1);
    assertSerializedColor(c2, StrL("#01020304"));
    utassert(c == c2);

    c2 = MkRgba(5, 7, 6, 8);
    assertSerializedColor(c2, StrL("#08050706"));
    ok = ParseColor(&c, StrL("#08050706"));
    utassert(ok);
    utassert(c == c2);
}

static void assertGoToNextPage3(int cmdId) {
    auto* cmd = FindCustomCommand(cmdId);
    utassert(cmd->origId == CmdGoToNextPage);
    auto* arg = GetCommandArg(cmd, kCmdArgN);
    utassert(arg->intVal == 3);
}

static void parseCommandsTest() {
    CommandArg* arg;

    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 openEdit copytoclipboard"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        arg = GetCommandArg(cmd, kCmdArgColor);
        utassert(arg != nullptr);
        arg = GetCommandArg(cmd, kCmdArgOpenEdit);
        utassert(arg != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, false) == true);
        utassert(GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false) == true);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 OpenEdit=yes"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        utassert(GetCommandArg(cmd, kCmdArgColor) != nullptr);
        utassert(GetCommandArg(cmd, kCmdArgOpenEdit) != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, false) == true);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 OpenEdit=no"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        utassert(GetCommandArg(cmd, kCmdArgColor) != nullptr);
        utassert(GetCommandArg(cmd, kCmdArgOpenEdit) != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, true) == false);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL("CmdCreateAnnotHighlight OpenEdit=bogus"));
        utassert(cmd == nullptr);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL("CmdCreateAnnotHighlight OpenEdit: bogus"));
        utassert(cmd == nullptr);
    }
    {
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n: 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n=3"));
            assertGoToNextPage3(cmd->id);
        }
    }
    {
        Str argStr = StrL(R"("C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1)");
        Str s = str::JoinTemp(StrL("CmdExec   "), argStr);
        auto* cmd = CreateCommandFromDefinition(s);
        utassert(cmd->origId == CmdExec);
        auto* cmd2 = FindCustomCommand(cmd->id);
        utassert(cmd == cmd2);
        arg = GetCommandArg(cmd, kCmdArgExe);
        utassert(str::Eq(arg->strVal, argStr));
    }
    {
        Str argStr = StrL(R"("C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1)");
        Str s = str::JoinTemp(StrL("CmdExec  filter: *.jpeg "), argStr);
        auto* cmd = CreateCommandFromDefinition(s);
        utassert(cmd->origId == CmdExec);
        auto* cmd2 = FindCustomCommand(cmd->id);
        utassert(cmd == cmd2);
        arg = GetCommandArg(cmd, kCmdArgExe);
        utassert(str::Eq(arg->strVal, argStr));
        arg = GetCommandArg(cmd, kCmdArgFilter);
        utassert(str::Eq(arg->strVal, StrL("*.jpeg")));
    }
}

static void DocPropertiesTest() {
    // gPropNames round-trips: first (Title=1), a middle one (FocalLength35mm=27)
    // and the last property (ImagePath=53), both directions.
    utassert(str::Eq(PropNameTemp(DocProp::Title), StrL("title")));
    utassert(str::Eq(PropNameTemp(DocProp::FocalLength35mm), StrL("focalLength35mm")));
    utassert(str::Eq(PropNameTemp(DocProp::ImagePath), StrL("imagePath")));
    utassert(PropFromName(StrL("title")) == DocProp::Title);
    utassert(PropFromName(StrL("focalLength35mm")) == DocProp::FocalLength35mm);
    utassert(PropFromName(StrL("imagePath")) == DocProp::ImagePath);
    // a couple more, plus unknown/None
    utassert(str::Eq(PropNameTemp(DocProp::CreationDate), StrL("creationDate")));
    utassert(PropFromName(StrL("modDate")) == DocProp::ModificationDate);
    utassert(PropFromName(StrL("bogusPropName")) == DocProp::None);
}

static void SumatraPDF_UnitTests() {
    PageRenderPolicy_UnitTests();
    CommandPaletteModel_UnitTests();
    DocPropertiesTest();
    parseCommandsTest();
    colorTest();
    BenchRangeTest();
#if OS_WIN
    ParseCommandLineTest();
#endif
    versioncheck_test();
    hexstrTest();
}

static void ParseTipExpectWordsLinks(Str input, int expWords, int expLinks) {
    VirtRichText* tip = ParseTip(input);
    utassert(TipWordCount(tip) == expWords);
    utassert(TipLinkCount(tip) == expLinks);
    delete tip;
}

static void ParseTipExpectPlainContains(Str input, Str needle) {
    VirtRichText* tip = ParseTip(input);
    TempStr plain = tip->PlainTextTemp();
    utassert(plain && str::Contains(plain, needle));
    delete tip;
}

static void ParseTipExpectLinkCmd(Str input, Str expCmd) {
    VirtRichText* tip = ParseTip(input);
    utassert(TipLinkCount(tip) == 1);
    utassert(str::Eq(tip->links.next->cmd, expCmd));
    delete tip;
}

static void ParseTip_UnitTests() {
    // issue #5752: brackets in filenames must not hang
    ParseTipExpectPlainContains(StrL("Loading Apocalypse Bringer Mynoghra_01 [CIW].pdf ..."), StrL("[CIW]"));

    // empty link text must not create a zero-word link (DrawTipWords crash)
    ParseTipExpectWordsLinks(StrL("[](CmdFoo)"), 1, 0);
    ParseTipExpectPlainContains(StrL("[](CmdFoo)"), StrL("[](CmdFoo)"));

    // URLs may contain balanced parentheses
    ParseTipExpectLinkCmd(StrL("[text](https://example.com/foo(bar))"), StrL("https://example.com/foo(bar)"));
    ParseTipExpectWordsLinks(StrL("[text](https://example.com/foo(bar))"), 1, 1);

    // Help/ link followed by trailing punctuation: the resolved URL must stop at
    // the link's ')' and not pull in the following ")." (the link cmd is a
    // non-NUL-terminated view into the tip line)
    ParseTipExpectLinkCmd(StrL("You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf)."),
                          StrL("https://www.sumatrapdfreader.org/docs/Tool-x-extract-text-from-pdf"));

    // nested brackets in link text
    ParseTipExpectWordsLinks(StrL("[foo [bar]](CmdFoo)"), 2, 1);
    ParseTipExpectPlainContains(StrL("[foo [bar]](CmdFoo)"), StrL("foo"));
    ParseTipExpectPlainContains(StrL("[foo [bar]](CmdFoo)"), StrL("[bar]"));

    // (Key/...) only expands for real commands
    ParseTipExpectPlainContains(StrL("file (Key/foo).pdf"), StrL("(Key/foo).pdf"));
    ParseTipExpectPlainContains(StrL("(Key/CmdCommandPalette)"), StrL("Ctrl"));
    ParseTipExpectPlainContains(StrL("(Key/CmdToggleKeyboardHelp)"), StrL("?"));

    // (Kbd/...) draws as a key-cap word; nests with (Key/...)
    {
        VirtRichText* tip = ParseTip(StrL("(Kbd/Cmd+Shift)"));
        utassert(TipWordCount(tip) == 1);
        utassert(tip->words.next->isKbd);
        utassert(str::Eq(tip->words.next->text, StrL("Cmd+Shift")));
        utassert(tip->HasRichContent());
        delete tip;
    }
    {
        VirtRichText* tip = ParseTip(StrL("(Kbd/(Key/CmdCommandPalette)): go"));
        utassert(TipWordCount(tip) >= 2);
        TipWord* w0 = tip->words.next;
        TipWord* w1 = w0->next;
        utassert(w0->isKbd);
        // expanded shortcut contains Ctrl (default binding)
        utassert(str::Contains(w0->text, StrL("Ctrl")));
        // ':' abuts the key-cap with no space
        utassert(w1->noSpaceBefore);
        utassert(str::Eq(w1->text, StrL(":")));
        delete tip;
    }

    // whitespace: tab and newline break words
    ParseTipExpectWordsLinks(StrL("line1\nline2"), 2, 0);
    ParseTipExpectWordsLinks(StrL("tab\there"), 2, 0);

    // ordinary tips still work
    ParseTipExpectWordsLinks(StrL("before [valid](CmdFoo)"), 2, 1);
    ParseTipExpectWordsLinks(StrL("[valid](CmdFoo) after"), 2, 1);

    // GHSA-2wv2-qm2f-vmxh: a file name can contain the markup, so text from
    // outside the app must never become a link. AddPlainText / AddPlainLink are
    // how such text gets in
    {
        Str evil = StrL("a[b](CmdExec calc.exe)c");
        VirtRichText* tip = new VirtRichText();
        tip->AddPlainText(evil);
        utassert(TipLinkCount(tip) == 0);
        utassert(str::Contains(tip->PlainTextTemp(), StrL("(CmdExec")));
        delete tip;

        // the same text as a link: exactly one link, and to our command
        tip = new VirtRichText();
        tip->AddPlainLink(evil, StrL("CmdOpenNextFileInFolder"));
        utassert(TipLinkCount(tip) == 1);
        utassert(str::Eq(tip->links.next->cmd, StrL("CmdOpenNextFileInFolder")));
        delete tip;

        // mixing our markup with outside text keeps them apart
        tip = new VirtRichText();
        ParseTipInto(tip, StrL("open"));
        tip->AddPlainLink(evil, StrL("CmdOpenNextFileInFolder"));
        ParseTipInto(tip, StrL("[browse](CmdNavigateFilesInFolder)"));
        utassert(TipLinkCount(tip) == 2);
        utassert(str::Eq(tip->links.next->cmd, StrL("CmdOpenNextFileInFolder")));
        utassert(str::Eq(tip->links.next->next->cmd, StrL("CmdNavigateFilesInFolder")));
        delete tip;
    }
}

static LONG WINAPI ForAiCrashHandler(EXCEPTION_POINTERS* ei) {
    printf("unit tests crash\n");
    str::Builder s;
    dbghelp::GetExceptionInfo(s, ei);
    Str info = ToStr(s);
    printf("%.*s", info.len, info.s);
    fflush(stdout);
    ExitProcess(7);
    return EXCEPTION_EXECUTE_HANDLER;
}

// -for-ai: print assertion and crash callstacks to stdout instead of breaking
// into a debugger, so a script can report the failure
static void SetupForAi() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    utassert_set_for_ai(true);
    InitializeDbgHelp(true);
    SetUnhandledExceptionFilter(ForAiCrashHandler);
}

int RunAppUnitTests(bool forAi) {
    if (forAi) {
        SetupForAi();
    }
    printf("Running unit tests\n");

    AppendStoreTest();
    BaseUtilTest();
    ByteOrderTests();
    ClipboardImageTest();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    DirRemoveAllTest();
    FileUtilTest();
    GuessFileTypeTest();
    JsonTest();
    RefHoverTest();
    SettingsUtilTest();
    SimpleLogTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    VecTest();
    WinUtilTest();

    ChapterTable_UnitTests();
    PagePosition_UnitTests();
    PdfDarkModeImageClassifier_UnitTests();
    PdfDarkModeOklab_UnitTests();
    SumatraPDF_UnitTests();

    ParseTip_UnitTests();
#if IS_DEBUG
    TextSelection_UnitTests();
    Layout_UnitTests();
#if OS_WIN
    LayoutWin_UnitTests();
#endif
    VirtCtrl_UnitTests();
    utassert(TableOfContents_UnitTestSnapshotNamedDest());
    utassert(MarkdownModel_UnitTestBrowserNavigationUrl());
    utassert(MarkdownToc_UnitTestHtmlLinks());
    utassert(MarkdownToc_UnitTestHtmlHeadings());
    utassert(MarkdownToc_UnitTestMermaid());
    utassert(EbookDoc_UnitTestNormalizeURL());
    utassert(ExternalViewers_UnitTestPDFXChangePaths());
    utassert(Canvas_UnitTestScrollLineAmount());
    utassert(EngineMupdf_UnitTestEbookLineSpacingCss());
    utassert(EngineMupdf_UnitTestEbookFontFamilyCss());
    utassert(EngineMupdf_UnitTestEbookMarginCss());
    utassert(EngineMupdf_UnitTestMergeEBookUI());
    utassert(EngineMupdf_UnitTestPageLabels());
    utassert(Accelerators_UnitTestFolderNavIsSafe());
    utassert(Accelerators_UnitTestTreeTakesLetters());
    utassert(ShortcutParse_UnitTestShiftedPunct());
    utassert(AnnotSearch_UnitTests());
    ReadAloudHighlight_UnitTests();
#endif
    return utassert_print_results();
}

#endif
