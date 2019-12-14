#include <stdio.h>
#include "BaseUtil.h"
#include "ByteWriter.h"
#include "PalmDbReader.h"
#include "StrSlice.h"
#include "TxtParser.h"
#include "UtAssert.h"

constexpr const char ebookWinDesc[] = R"data(
Style [
    name: styleMainWnd
    bg_col: sepia
]

Style [
    name: stylePage
    padding: 32 16
    bg_col: transparent
]

Style [
    name: styleNextDefault
    parent: buttonDefault
    border_width: 0
    padding: 0 8
    stroke_width: 0
    fill: gray
    bg_col: transparent
    vert_align: center
]

Style [
    name: styleNextMouseOver
    parent: styleNextDefault
    fill: black
]

Style [
    name: styleStatus
    parent: buttonDefault
    bg_col: sepia
    col: black
    font_size: 8
    font_weight: regular
    padding: 3 0
    border_width: 0
    text_align: center
]

Style [
    name: styleProgress
    bg_col: light gray
    col: light blue
]

ButtonVector [
    name: nextButton
    clicked: next
    path: M0 0  L10 13 L0 ,26 Z
    style_default: styleNextDefault
    style_mouse_over: styleNextMouseOver
]

ButtonVector [
    name: prevButton
    clicked: prev
    path: M10 0 L0,  13 L10 26 z
    style_default: styleNextDefault
    style_mouse_over: styleNextMouseOver
]

Button [
    name: statusButton
    style: styleStatus
]

ScrollBar [
    name: progressScrollBar
    style: styleProgress
    cursor: hand
]

EbookPage [
    name: page1
    style: stylePage
]

EbookPage [
    name: page2
    style: stylePage
]

PagesLayout [
    name: pagesLayout
    page1: page1
    page2: page2
    spaceDx: 12
]

HorizontalLayout [
    name: top
    children [
        prevButton self 1 bottom
        pagesLayout 1 1 top
        nextButton self 1 center
    ]
]

VerticalLayout [
    name: mainLayout
    children [
        top 1 1 top
        progressScrollBar self 1 center
        statusButton self 1 center
    ]
]
)data";

void testTxtParser() {
    size_t ebookWinDescLen = static_strlen(ebookWinDesc);
    std::string_view ebookStr(ebookWinDesc, ebookWinDescLen);

    TxtParser parser;
    parser.SetToParse(ebookStr);
    bool ok = ParseTxt(parser);
    utassert(ok);

    {
        str::Str res = PrettyPrintTxt(parser);
        char* s = res.Get();
        str::NormalizeNewlinesInPlace(s);
        str::TrimWS(s, str::TrimOpt::Both);
        AutoFree orig(str::Dup(ebookWinDesc), str::Len(ebookWinDesc));

        char* s2 = orig.data;
        str::NormalizeNewlinesInPlace(s2);
        str::TrimWS(s2, str::TrimOpt::Both);

        ok = str::Eq(s, s2);
        utassert(ok);
    }
}

void testByteWriter() {
    char buf[32];
    ByteWriter w = MakeByteWriterLE(buf, dimof(buf));
    utassert(w.Left() == 32);
    w.Write16(3);
    utassert(w.Left() == 30);
    utassert(buf[0] == 3);
    w.Write8(3);
    utassert(w.Left() == 29);

    w = MakeByteWriterBE(buf, dimof(buf));
    utassert(w.Left() == 32);
    w.Write16(3);
    utassert(w.Left() == 30);
    utassert(buf[0] == 0);
}

extern void ByteOrderTests(); // ByteOrderDecoder_ut.cpp

int main(int, char**) {
    testByteWriter();
    testTxtParser();
    ByteOrderTests();
    utassert_print_results();
}
