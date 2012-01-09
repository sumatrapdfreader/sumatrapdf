/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiHtmlParse_h
#define MobiHtmlParse_h

#include <stdint.h>

// format codes correspond to html formatting tags
// Note: if there is both start/end version of the
// formatting code, the end version should always
// be $startVersion - 1
// TODO: do I need to represent Tag_Guide, Tag_Reference ?
enum FormatCode : uint8_t {
    // <b>
    FC_BoldStart = 255,
    FC_BoldEnd = 254,
    // <blockquote>
    FC_BlockQuoteStart = 253,
    FC_BlockQuoteEnd = 252,
    // <i>
    FC_ItalicStart = 251,
    FC_ItalicEnd = 250,
    // <p>
    FC_ParagraphStart = 249,
    FC_ParagraphEnd = 248,
    // <mbp:pagebrake>
    FC_MobiPageBreak = 247,
    // <table>
    FC_TableStart = 246,
    FC_TableEnd = 245,
    // <td>
    FC_TdStart = 244,
    FC_TdEnd = 243,
    // <tr>
    FC_TrStart = 242,
    FC_TrEnd = 241,
    // <a>
    FC_A = 240,
    // <br>
    FC_Br = 239,
    // <div>
    FC_DivStart = 238,
    FC_DivEnd = 237,
    // <font>
    FC_FontStart = 236,
    FC_FontEnd = 235,
    // <h2>
    FC_H2Start = 234,
    FC_H2End = 233,
    // <img>
    FC_Img = 232,
    // <ol>
    FC_OlStart = 231,
    FC_OlEnd = 230,
    // <li>
    FC_LiStart = 229,
    FC_LiEnd = 228,
    // <span>
    FC_SpanStart = 227,
    FC_SpanEnd = 226,
    // <sup>
    FC_SupStart = 225,
    FC_SupEnd = 224,
    // <u>
    FC_UnderlineStart = 223,
    FC_UnderlineEnd = 222,
    // <ul>
    FC_UlStart = 221,
    FC_UlEnd = 220,

    FC_Last = 219,
    FC_Invalid = 0
};

enum AlignAttr : uint8_t {
    AlignLeft    = 0,
    AlignRight   = 1,
    AlignCenter  = 2,
    AlignJustify = 3
};

uint8_t *MobiHtmlToDisplay(uint8_t *s, size_t sLen, size_t& lenOut);

#endif
