/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiHtmlParse_h
#define MobiHtmlParse_h

#include <stdint.h>

// enums must match gTags order
enum HtmlTag {
    Tag_NotFound = -1,
    Tag_A = 0,
    Tag_B = 1,
    Tag_Blockquote = 2,
    Tag_Body = 3,
    Tag_Br = 4,
    Tag_Div = 5,
    Tag_Font = 6,
    Tag_Guide = 7,
    Tag_H2 = 8,
    Tag_Head = 9,
    Tag_Hr = 10,
    Tag_Html = 11,
    Tag_I = 12,
    Tag_Img = 13,
    Tag_Li = 14,
    Tag_Mbp_Pagebreak = 15,
    Tag_Ol = 16,
    Tag_P = 17,
    Tag_Reference = 18,
    Tag_Span = 19,
    Tag_Sup = 20,
    Tag_Table = 21,
    Tag_Td = 22,
    Tag_Tr = 23,
    Tag_U = 24,
    Tag_Ul = 25,
    Tag_Last = 26
};

// enums must match gAttrs order
enum HtmlAttr {
    Attr_NotFound = -1,
    Attr_Align = 0,
    Attr_Height = 1,
    Attr_Width = 2,
    Attr_Last = 3
};

// enums must match gAlignAttrs order
enum AlignAttr {
    Align_NotFound = -1,
    Align_Center = 0,
    Align_Justify = 1,
    Align_Left = 2,
    Align_Right = 3,
    Align_Last = 4
};

#define Tag_First (255 - Tag_Last)

#define IS_END_TAG_MASK  0x01
#define HAS_ATTR_MASK    0x02

bool      AttrHasEnumVal(HtmlAttr attr);
uint8_t * MobiHtmlToDisplay(uint8_t *s, size_t sLen, size_t& lenOut);

#endif
