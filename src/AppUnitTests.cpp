/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#if defined(DEBUG)

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "Commands.h"

#if defined(DEBUG)
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
#endif

// must be last to over-write assert()
#include "base/UtAssert.h"

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
    ParseTipExpectPlainContains("Loading Apocalypse Bringer Mynoghra_01 [CIW].pdf ...", "[CIW]");

    // empty link text must not create a zero-word link (DrawTipWords crash)
    ParseTipExpectWordsLinks("[](CmdFoo)", 1, 0);
    ParseTipExpectPlainContains("[](CmdFoo)", "[](CmdFoo)");

    // URLs may contain balanced parentheses
    ParseTipExpectLinkCmd("[text](https://example.com/foo(bar))", "https://example.com/foo(bar)");
    ParseTipExpectWordsLinks("[text](https://example.com/foo(bar))", 1, 1);

    // Help/ link followed by trailing punctuation: the resolved URL must stop at
    // the link's ')' and not pull in the following ")." (the link cmd is a
    // non-NUL-terminated view into the tip line)
    ParseTipExpectLinkCmd("You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).",
                          "https://www.sumatrapdfreader.org/docs/Tool-x-extract-text-from-pdf");

    // nested brackets in link text
    ParseTipExpectWordsLinks("[foo [bar]](CmdFoo)", 2, 1);
    ParseTipExpectPlainContains("[foo [bar]](CmdFoo)", "foo");
    ParseTipExpectPlainContains("[foo [bar]](CmdFoo)", "[bar]");

    // (Key/...) only expands for real commands
    ParseTipExpectPlainContains("file (Key/foo).pdf", "(Key/foo).pdf");
    ParseTipExpectPlainContains("(Key/CmdCommandPalette)", "Ctrl");

    // (Kbd/...) draws as a key-cap word; nests with (Key/...)
    {
        VirtRichText* tip = ParseTip("(Kbd/Cmd+Shift)");
        utassert(TipWordCount(tip) == 1);
        utassert(tip->words.next->isKbd);
        utassert(str::Eq(tip->words.next->text, StrL("Cmd+Shift")));
        utassert(tip->HasRichContent());
        delete tip;
    }
    {
        VirtRichText* tip = ParseTip("(Kbd/(Key/CmdCommandPalette)): go");
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
    ParseTipExpectWordsLinks("line1\nline2", 2, 0);
    ParseTipExpectWordsLinks("tab\there", 2, 0);

    // ordinary tips still work
    ParseTipExpectWordsLinks("before [valid](CmdFoo)", 2, 1);
    ParseTipExpectWordsLinks("[valid](CmdFoo) after", 2, 1);

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

int RunAppUnitTests() {
    ParseTip_UnitTests();
#if defined(DEBUG)
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
#endif
    return utassert_print_results();
}

#endif
