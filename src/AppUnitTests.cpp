/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if defined(DEBUG)

#include "utils/BaseUtil.h"
#include "TipText.h"
#include "Commands.h"

// must be last to over-write assert()
#include "utils/UtAssert.h"

static void ParseTipExpectWordsLinks(Str input, int expWords, int expLinks) {
    ParsedTip tip;
    ParseTip(tip, input);
    utassert(tip.words.Size() == expWords);
    utassert(tip.links.Size() == expLinks);
}

static void ParseTipExpectPlainContains(Str input, Str needle) {
    ParsedTip tip;
    ParseTip(tip, input);
    TempStr plain = TipPlainTextTemp(tip);
    utassert(plain && str::Find(plain, needle));
}

static void ParseTipExpectLinkCmd(Str input, Str expCmd) {
    ParsedTip tip;
    ParseTip(tip, input);
    utassert(tip.links.Size() == 1);
    utassert(str::Eq(tip.links[0].cmd, expCmd));
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

    // nested brackets in link text
    ParseTipExpectWordsLinks("[foo [bar]](CmdFoo)", 2, 1);
    ParseTipExpectPlainContains("[foo [bar]](CmdFoo)", "foo");
    ParseTipExpectPlainContains("[foo [bar]](CmdFoo)", "[bar]");

    // (Key/...) only expands for real commands
    ParseTipExpectPlainContains("file (Key/foo).pdf", "(Key/foo).pdf");
    ParseTipExpectPlainContains("(Key/CmdCommandPalette)", "Ctrl");

    // whitespace: tab and newline break words
    ParseTipExpectWordsLinks("line1\nline2", 2, 0);
    ParseTipExpectWordsLinks("tab\there", 2, 0);

    // ordinary tips still work
    ParseTipExpectWordsLinks("before [valid](CmdFoo)", 2, 1);
    ParseTipExpectWordsLinks("[valid](CmdFoo) after", 2, 1);
}

int RunAppUnitTests() {
    ParseTip_UnitTests();
    return utassert_print_results();
}

#endif