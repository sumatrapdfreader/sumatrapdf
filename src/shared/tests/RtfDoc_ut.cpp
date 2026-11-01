/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "RtfDoc.h"

#include "base/tests/UtAssert.h"

#if IS_DEBUG
bool RtfDoc_UnitTestKeywords();
#endif

// html of <rtf>, or {} (as an empty, non-null string) on failure
static TempStr RtfHtmlTemp(Str rtf) {
    Str html = RtfToHtml(rtf);
    TempStr res = str::DupTemp(html);
    str::Free(html);
    return res;
}

static void RtfHas(Str rtf, Str want) {
    TempStr html = RtfHtmlTemp(rtf);
    utassert(str::Contains(html, want));
}

static void RtfLacks(Str rtf, Str unwanted) {
    TempStr html = RtfHtmlTemp(rtf);
    utassert(len(html) > 0 && !str::Contains(html, unwanted));
}

// <first> comes before <second> in the html
static void RtfOrder(Str rtf, Str first, Str second) {
    TempStr html = RtfHtmlTemp(rtf);
    int i1 = str::IndexOf(html, first);
    int i2 = str::IndexOf(html, second);
    utassert(i1 >= 0 && i2 > i1);
}

void RtfDoc_UnitTests() {
#if IS_DEBUG
    utassert(RtfDoc_UnitTestKeywords());
#endif
    utassert(str::IsNull(RtfToHtml(StrL("plain text"))));

    // formatting, a font table and escaping
    Str doc = StrL(
        "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\froman Times;}{\\f1\\fmodern Courier New;}}"
        "Hello {\\b bold} {\\f1 code} a<b & \\{c\\}\\par}");
    RtfHas(doc, StrL("<span style=\"font-weight:bold;\">bold</span>"));
    RtfHas(doc, StrL("<span style=\"font-family:'Courier New',monospace;\">code</span>"));
    // a quoted font name must not end the style attribute, dropping the formatting after it
    Str fontThenBold = StrL("{\\rtf1\\deff0{\\fonttbl{\\f0 Times;}{\\f1\\fswiss Segoe UI;}}{\\f1\\fs48\\b\\i Title}}");
    RtfHas(
        fontThenBold,
        StrL("<span style=\"font-weight:bold;font-style:italic;font-size:2em;font-family:'Segoe UI',sans-serif;\">"));
    RtfHas(doc, StrL("a&lt;b &amp; {c}"));
    RtfHas(doc, StrL("<body style=\"font-family:'Times',serif\">"));

    // code pages: the document's and a font's
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1251 \\'cf\\'f0\\'e8}"), StrL("\xd0\x9f\xd1\x80\xd0\xb8"));
    RtfHas(StrL("{\\rtf1\\ansi{\\fonttbl{\\f0\\fcharset204 Arial;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    // no \fcharset: the code page comes from the font name's suffix, which isn't part of the real name
    Str fontCe = StrL("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\froman Times New Roman CE;}}si\\'ea}");
    RtfHas(fontCe, StrL("si\xc4\x99"));
    RtfHas(fontCe, StrL("<body style=\"font-family:'Times New Roman',serif\">"));
    RtfHas(StrL("{\\rtf1\\ansi{\\fonttbl{\\f0 Arial CYR;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    // an explicit \fcharset wins over the name
    RtfHas(StrL("{\\rtf1\\ansi{\\fonttbl{\\f0\\fcharset204 Arial CE;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    // \fcharset0 is Windows-1252 whatever the document's code page, unless the name says otherwise
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1251{\\fonttbl{\\f0\\fcharset0 Arial;}}\\f0 \\'cf}"), StrL("\xc3\x8f"));
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg932{\\fonttbl{\\f0\\fcharset0 Arial;}}\\f0 hab\\'eda}"), StrL("hab\xc3\xad"));
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1252{\\fonttbl{\\f0\\fcharset0 Arial Cyr;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    // no \fcharset, and \fcharset1 (default): the document's code page
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1251{\\fonttbl{\\f0 Arial;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1251{\\fonttbl{\\f0\\fcharset1 Arial;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));
    // not a suffix of a word
    RtfHas(StrL("{\\rtf1\\ansi{\\fonttbl{\\f0 Space;}}\\f0 caf\\'e9}"), StrL("caf\xc3\xa9"));

    // \uN with its fallback skipped, and a surrogate pair
    RtfHas(StrL("{\\rtf1\\uc1 \\u20320?\\u22909?}"), StrL("\xe4\xbd\xa0\xe5\xa5\xbd"));
    RtfLacks(StrL("{\\rtf1\\uc1 \\u20320?\\u22909?}"), StrL("?"));
    RtfHas(StrL("{\\rtf1\\uc1 \\u-10179?\\u-8704?}"), StrL("\xf0\x9f\x98\x80"));

    // what isn't document text
    Str skipped = StrL(
        "{\\rtf1{\\*\\unknown hidden}{\\header head}{\\info{\\title My Title}{\\author Me}}"
        "{\\v invisible}{\\*\\shppict{\\pict\\pngblip 89504e470d0a1a0a}}"
        "{\\nonshppict{\\pict\\wmetafile8 0102}}visible\\par}");
    RtfLacks(skipped, StrL("hidden"));
    RtfLacks(skipped, StrL("head<"));
    RtfLacks(skipped, StrL("invisible"));
    RtfLacks(skipped, StrL("Me"));
    RtfHas(skipped, StrL("<title>My Title</title>"));
    RtfHas(skipped, StrL("visible</p>"));
    RtfHas(skipped, StrL("<img src=\"data:image/png;base64,iVBORw0KGgo=\"/>"));

    // a WMF (WordPad) or EMF picture wrapping a 1x1 24-bit DIB shows as BMP
    Str wmf = StrL(
        "{\\rtf1{\\pict\\wmetafile8\\picw1\\pich1 010009000003300000000000240000000000"
        "24000000430f2000cc00000001000100000000000100010000000000280000000100000001000000010018000000"
        "000004000000000000000000000000000000000000000000ff00030000000000}}");
    RtfHas(wmf, StrL("<img src=\"data:image/png;base64,Qk"));
    Str emf = StrL(
        "{\\rtf1{\\pict\\emfblip 0100000008000000510000007c000000000000000000000000000000000000000000000000"
        "0000000000000000000000010000000100000050000000280000007800000004000000000000002000cc0001000000"
        "01000000280000000100000001000000010018000000000004000000000000000000000000000000000000000000ff00"
        "0e00000008000000}}");
    RtfHas(emf, StrL("<img src=\"data:image/png;base64,Qk"));

    // two bitmaps in one place are a mask and the picture drawn over it: show the last
    Str maskPair = StrL(
        "{\\rtf1{\\pict\\wmetafile8\\picw1\\pich1 010009000003300000000000240000000000"
        "24000000430f2000cc00000001000100000000000100010000000000280000000100000001000000010018000000"
        "000004000000000000000000000000000000000000000000ff00"
        "24000000430f2000cc00000001000100000000000100010000000000280000000100000001000000010018000000"
        "000004000000"
        "00000000000000000000000000000000"
        "00ff0000"
        "030000000000}}");
    RtfHas(maskPair, StrL("base64,Qk06AAAAAAAAADYAAAAoAAAAAQAAAAEAAAABABgAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAP8AAA=="));

    // bitmaps side by side are tiles of one picture
    Str tiles = StrL(
        "{\\rtf1{\\pict\\emfblip "
        "510000007c000000000000000000000000000000000000000000000000000000000000000000000001000000010000005000000028"
        "0000007800000004000000000000002000cc000100000001000000280000000100000001000000010018000000000004000000000000"
        "000000000000000000000000000000ff00"
        "510000007c000000000000000000000000000000000000000100000000000000000000000000000001000000010000005000000028"
        "0000007800000004000000000000002000cc000100000001000000280000000100000001000000010018000000000004000000000000"
        "0000000000000000000000000000ff0000"
        "0e00000008000000}}");
    RtfHas(tiles, StrL("base64,Qk0+AAAAAAAAADYAAAAoAAAAAgAAAAEAAAABABgAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAD/AP8AAAA="));

    // line spacing: a multiple of a line, exact, at least, and single, which is the default
    RtfHas(StrL("{\\rtf1\\pard\\sl360\\slmult1 a\\par}"), StrL("<p style=\"line-height:1.8;\">a</p>"));
    RtfHas(StrL("{\\rtf1\\pard\\sl-300 a\\par}"), StrL("<p style=\"line-height:15pt;\">a</p>"));
    RtfHas(StrL("{\\rtf1\\pard\\sl400\\slmult0 a\\par}"), StrL("<p style=\"line-height:20pt;\">a</p>"));
    RtfHas(StrL("{\\rtf1\\pard\\sl200\\slmult0 a\\par}"), StrL("<p>a</p>"));
    RtfHas(StrL("{\\rtf1\\pard\\sl240\\slmult1 a\\par}"), StrL("<p>a</p>"));
    RtfHas(StrL("{\\rtf1\\pard\\sl360\\slmult1 a\\par\\pard b\\par}"), StrL("<p>b</p>"));
    // a blank line is as tall as the font of its \par
    RtfHas(StrL("{\\rtf1 a\\par{\\fs48\\par}b\\par}"), StrL("<p style=\"font-size:2em;\">&#160;</p>"));

    // paragraphs, headings and page breaks
    RtfHas(StrL("{\\rtf1\\pard\\qc\\li720 centered\\par}"), StrL("<p style=\"text-align:center;margin-left:36pt;\">"));
    RtfHas(StrL("{\\rtf1\\pard\\outlinelevel1 Chapter\\par}"), StrL("<h2>Chapter</h2>"));
    RtfHas(StrL("{\\rtf1 a\\par\\par b}"), StrL("<p>&#160;</p>"));
    RtfOrder(StrL("{\\rtf1 one\\page two}"), StrL("page-break-after:always"), StrL("two"));
    RtfLacks(StrL("{\\rtf1 one\\page}"), StrL("page-break"));
    RtfOrder(StrL("{\\rtf1 one\\par\\pard\\pagebb two\\par}"), StrL("page-break-after:always"), StrL("two"));
    RtfOrder(StrL("{\\rtf1 one\\sect\\sectd two}"), StrL("page-break-after:always"), StrL("two"));
    RtfLacks(StrL("{\\rtf1 one\\sect\\sectd\\sbknone two}"), StrL("page-break"));
    RtfLacks(StrL("{\\rtf1\\pard\\pagebb first\\par}"), StrL("page-break"));

    // tables end at the first paragraph outside them
    Str table = StrL(
        "{\\rtf1\\trowd\\clbrdrt\\brdrs\\cellx1440\\cellx2880\\intbl a\\cell b\\cell\\row"
        "\\pard after\\par}");
    RtfHas(table, StrL("<td style=\"width:72pt\" class=\"b\"><p>a</p>"));
    RtfOrder(table, StrL("</table>"), StrL("after"));

    // a row too wide for the page has no fixed widths, so that it can shrink to fit
    Str wide = StrL("{\\rtf1\\trowd\\cellx3000\\cellx9000\\intbl a\\cell b\\cell\\row}");
    RtfHas(wide, StrL("<td><p>a</p>"));
    RtfLacks(wide, StrL("width:"));

    // a centered row: each row is placed on its own, so a change starts a new table
    Str centered = StrL(
        "{\\rtf1\\trowd\\trqc\\cellx1440\\intbl a\\cell\\row"
        "\\trowd\\trqc\\cellx1440\\intbl b\\cell\\row"
        "\\trowd\\cellx1440\\intbl c\\cell\\row}");
    RtfHas(centered,
           StrL("<table style=\"margin-left:auto;margin-right:auto\">\n<tr><td style=\"width:72pt\"><p>a</p>"));
    RtfHas(centered, StrL("</td></tr>\n<tr><td style=\"width:72pt\"><p>b</p>"));
    RtfHas(centered, StrL("</table>\n<table>\n<tr><td style=\"width:72pt\"><p>c</p>"));

    // a nested table: its row's properties come after its cells, and don't touch the outer row
    Str nested = StrL(
        "{\\rtf1\\trowd\\cellx4000\\pard\\intbl\\itap1 before\\par"
        "\\pard\\intbl\\itap2 n1\\nestcell n2\\nestcell"
        "{\\*\\nesttableprops\\trowd\\clbrdrt\\brdrs\\cellx1000\\cellx2000\\nestrow}{\\nonesttables\\par}"
        "\\pard\\intbl\\itap2 n3\\nestcell n4\\nestcell"
        "{\\*\\nesttableprops\\trowd\\cellx1000\\cellx2000\\nestrow}{\\nonesttables\\par}"
        "\\pard\\intbl\\itap1 outer\\cell\\row\\pard after\\par}");
    RtfHas(nested, StrL("<td style=\"width:200pt\"><p>before</p>\n<table>\n<tr>"));
    RtfHas(nested, StrL("<tr><td class=\"b\"><p>n1</p>\n</td><td class=\"b\"><p>n2</p>\n</td></tr>"));
    RtfHas(nested, StrL("<tr><td><p>n3</p>\n</td><td><p>n4</p>\n</td></tr>\n</table>\n<p>outer</p>"));
    RtfOrder(nested, StrL("outer"), StrL("after"));
    // deeper than we keep track of, and a cell that never ends: nothing is lost
    RtfHas(StrL("{\\rtf1\\pard\\intbl\\itap99999 deep\\nestcell\\nestrow\\cell\\row}"), StrL("deep"));
    RtfHas(StrL("{\\rtf1\\pard\\intbl\\itap2 open\\nestcell"), StrL("open"));

    // links and bookmarks
    RtfHas(StrL("{\\rtf1{\\field{\\*\\fldinst HYPERLINK \"https://x.org/?a=1&b=2\"}{\\fldrslt link}}}"),
           StrL("<a href=\"https://x.org/?a=1&amp;b=2\">link</a>"));
    RtfHas(StrL("{\\rtf1{\\field{\\*\\fldinst{HYPERLINK \\\\l \"sec\"}}{\\fldrslt go}}}"), StrL("<a href=\"#sec\">"));
    RtfHas(StrL("{\\rtf1{\\*\\bkmkstart sec}{\\*\\bkmkend sec}text}"), StrL("<a id=\"sec\"></a>"));

    // footnotes are numbered and moved to the end
    Str fn = StrL("{\\rtf1 Text{\\super\\chftn}{\\footnote\\pard{\\super\\chftn} Note.}\\par end\\par}");
    RtfOrder(fn, StrL("end"), StrL("<hr/>"));
    RtfOrder(fn, StrL("<hr/>"), StrL("Note."));

    // hostile input: deep nesting, unterminated groups, a huge \bin
    str::Builder deep;
    deep.Append(StrL("{\\rtf1 "));
    for (int i = 0; i < 5000; i++) {
        deep.AppendChar('{');
    }
    deep.Append(StrL("deep"));
    RtfHas(ToStr(deep), StrL("deep"));
    RtfHas(StrL("{\\rtf1 x{\\pict\\bin999999999 "), StrL("x"));
}
