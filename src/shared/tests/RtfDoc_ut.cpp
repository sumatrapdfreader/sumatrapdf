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
    RtfHas(doc, StrL("font-family:\"Courier New\",monospace;"));
    RtfHas(doc, StrL("a&lt;b &amp; {c}"));
    RtfHas(doc, StrL("<body style=\"font-family:'Times',serif\">"));

    // code pages: the document's and a font's
    RtfHas(StrL("{\\rtf1\\ansi\\ansicpg1251 \\'cf\\'f0\\'e8}"), StrL("\xd0\x9f\xd1\x80\xd0\xb8"));
    RtfHas(StrL("{\\rtf1\\ansi{\\fonttbl{\\f0\\fcharset204 Arial;}}\\f0 \\'cf}"), StrL("\xd0\x9f"));

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
