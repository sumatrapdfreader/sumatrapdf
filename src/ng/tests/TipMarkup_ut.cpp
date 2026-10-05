/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the tip markup parser of src/TipMarkup.cpp, which the home page's tip
// band and the notifications share. orig's equivalent is ParseTip_UnitTests
// over VirtRichText, which needs win32 drawing.

#include "base/Base.h"

#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "TipMarkup.h"

#include "base/tests/UtAssert.h"

static void CheckSpan(const Vec<TipSpan>& spans, int idx, TipSpanKind kind, Str text, Str target) {
    utassert(idx < len(spans));
    const TipSpan& sp = spans[idx];
    utassert(sp.kind == kind);
    utassert(str::Eq(sp.text, text));
    if (len(target) > 0) {
        utassert(str::Eq(sp.target, target));
    }
}

static void PlainText() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("just text"));
    utassert(len(spans) == 1);
    CheckSpan(spans, 0, TipSpanKind::Text, StrL("just text"), {});
    utassert(!TipSpansHaveRichContent(spans));
    TipSpansFree(spans);
}

static void Link() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("open [browse](CmdNavigateFilesInFolder) now"));
    utassert(len(spans) == 3);
    CheckSpan(spans, 0, TipSpanKind::Text, StrL("open "), {});
    CheckSpan(spans, 1, TipSpanKind::Link, StrL("browse"), StrL("CmdNavigateFilesInFolder"));
    CheckSpan(spans, 2, TipSpanKind::Text, StrL(" now"), {});
    utassert(TipSpansHaveRichContent(spans));
    TipSpansFree(spans);
}

static void HttpAndHelpLinks() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("[docs](Help/Commands) [site](https://x.org)"));
    utassert(len(spans) == 3);
    CheckSpan(spans, 0, TipSpanKind::Link, StrL("docs"), StrL("https://www.sumatrapdfreader.org/docs/Commands"));
    CheckSpan(spans, 2, TipSpanKind::Link, StrL("site"), StrL("https://x.org"));
    TipSpansFree(spans);
}

static void BoldAndBullet() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("**Esc**: exit * **V**: mode"));
    // Esc | ": exit " | · | " " | V | ": mode"
    CheckSpan(spans, 0, TipSpanKind::Bold, StrL("Esc"), {});
    CheckSpan(spans, 1, TipSpanKind::Text, StrL(": exit "), {});
    CheckSpan(spans, 2, TipSpanKind::Text, StrL("\xc2\xb7"), {});
    CheckSpan(spans, 4, TipSpanKind::Bold, StrL("V"), {});
    TipSpansFree(spans);
}

static void Code() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("type `#` to search"));
    utassert(len(spans) == 3);
    CheckSpan(spans, 1, TipSpanKind::Code, StrL("#"), {});
    TipSpansFree(spans);
}

// (Key/CmdXxx) is the shortcut text; (Kbd/..) makes a key cap of what's inside
static void KeyAndKbd() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("(Kbd/(Key/CmdOpenFile)) open"));
    utassert(len(spans) == 2);
    utassert(spans[0].kind == TipSpanKind::Kbd);
    utassert(str::Eq(spans[0].text, StrL("Ctrl + O")));
    CheckSpan(spans, 1, TipSpanKind::Text, StrL(" open"), {});
    TipSpansFree(spans);

    TipSpansParse(spans, StrL("press (Key/CmdSaveAs)"));
    utassert(len(spans) == 2);
    utassert(spans[1].kind == TipSpanKind::Kbd);
    utassert(str::Eq(spans[1].text, StrL("Ctrl + S")));
    TipSpansFree(spans);

    // not a command: the markup stays literal
    TipSpansParse(spans, StrL("(Key/NotACommand)"));
    utassert(len(spans) == 1);
    CheckSpan(spans, 0, TipSpanKind::Text, StrL("(Key/NotACommand)"), {});
    TipSpansFree(spans);
}

// a file name is added with TipSpansAddLink, so a "](CmdExec ...)" in it
// cannot break out of the link (GHSA-2wv2-qm2f-vmxh)
static void PlainLinkIsNotParsed() {
    Vec<TipSpan> spans;
    Str evil = StrL("a](CmdExec calc.exe).pdf");
    TipSpansAddLink(spans, evil, StrL("CmdOpenNextFileInFolder"));
    utassert(len(spans) == 1);
    CheckSpan(spans, 0, TipSpanKind::Link, evil, StrL("CmdOpenNextFileInFolder"));
    TipSpansFree(spans);
}

static void PlainTextOfSpans() {
    Vec<TipSpan> spans;
    TipSpansParse(spans, StrL("**a** b `c`"));
    utassert(str::Eq(TipSpansPlainTextTemp(spans), StrL("a b c")));
    TipSpansFree(spans);
}

void TipMarkup_UnitTests() {
    // (Key/CmdXxx) resolves through the accelerator table
    CreateSumatraAcceleratorTable();
    PlainText();
    Link();
    HttpAndHelpLinks();
    BoldAndBullet();
    Code();
    KeyAndKbd();
    PlainLinkIsNotParsed();
    PlainTextOfSpans();
    FreeAcceleratorTables();
}
