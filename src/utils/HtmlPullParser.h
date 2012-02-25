/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef HtmlPullParser_h
#define HtmlPullParser_h

#include "Vec.h"

class Allocator;

// enums must match HTML_TAGS_STRINGS order
enum HtmlTag {
    Tag_NotFound = -1,
    Tag_A = 0,
    Tag_Abbr = 1,
    Tag_Acronym = 2,
    Tag_Area = 3,
    Tag_Audio = 4,
    Tag_B = 5,
    Tag_Base = 6,
    Tag_Basefont = 7,
    Tag_Blockquote = 8,
    Tag_Body = 9,
    Tag_Br = 10,
    Tag_Center = 11,
    Tag_Code = 12,
    Tag_Col = 13,
    Tag_Dd = 14,
    Tag_Div = 15,
    Tag_Dl = 16,
    Tag_Dt = 17,
    Tag_Em = 18,
    Tag_Font = 19,
    Tag_Frame = 20,
    Tag_Guide = 21,
    Tag_H1 = 22,
    Tag_H2 = 23,
    Tag_H3 = 24,
    Tag_H4 = 25,
    Tag_H5 = 26,
    Tag_Head = 27,
    Tag_Hr = 28,
    Tag_Html = 29,
    Tag_I = 30,
    Tag_Img = 31,
    Tag_Input = 32,
    Tag_Li = 33,
    Tag_Link = 34,
    Tag_Mbp_Pagebreak = 35,
    Tag_Meta = 36,
    Tag_Object = 37,
    Tag_Ol = 38,
    Tag_P = 39,
    Tag_Pagebreak = 40,
    Tag_Param = 41,
    Tag_Pre = 42,
    Tag_Reference = 43,
    Tag_S = 44,
    Tag_Small = 45,
    Tag_Span = 46,
    Tag_Strike = 47,
    Tag_Strong = 48,
    Tag_Style = 49,
    Tag_Sub = 50,
    Tag_Sup = 51,
    Tag_Table = 52,
    Tag_Td = 53,
    Tag_Th = 54,
    Tag_Title = 55,
    Tag_Tr = 56,
    Tag_Tt = 57,
    Tag_U = 58,
    Tag_Ul = 59,
    Tag_Video = 60,
    Tag_Last = 61
};

#define HTML_TAGS_STRINGS "a\0abbr\0acronym\0area\0audio\0b\0base\0basefont\0blockquote\0body\0br\0center\0code\0col\0dd\0div\0dl\0dt\0em\0font\0frame\0guide\0h1\0h2\0h3\0h4\0h5\0head\0hr\0html\0i\0img\0input\0li\0link\0mbp:pagebreak\0meta\0object\0ol\0p\0pagebreak\0param\0pre\0reference\0s\0small\0span\0strike\0strong\0style\0sub\0sup\0table\0td\0th\0title\0tr\0tt\0u\0ul\0video\0"

// enums must match ALIGN_ATTRS_STRINGS order
enum AlignAttr {
    Align_NotFound = -1,
    Align_Center = 0,
    Align_Justify = 1,
    Align_Left = 2,
    Align_Right = 3,
    Align_Last = 4
};
#define ALIGN_ATTRS_STRINGS "center\0justify\0left\0right\0"

struct AttrInfo {
    const char *      name;
    size_t            nameLen;
    const char *      val;
    size_t            valLen;

    bool NameIs(const char *s) const;
    bool ValIs(const char *s) const;
};

struct HtmlToken;
size_t GetTagLen(const HtmlToken *tok);

// TrivialHtmlParser needs to enumerate all attributes of an HtmlToken
class HtmlParser;

struct HtmlToken {
    friend HtmlParser;

    enum TokenType {
        StartTag,           // <foo>
        EndTag,             // </foo>
        EmptyElementTag,    // <foo/>
        Text,               // <foo>text</foo> => "text"
        Error
    };

    enum ParsingError {
        ExpectedElement,
        NonTagAtEnd,
        UnclosedTag,
        InvalidTag
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsEmptyElementEndTag() const { return type == EmptyElementTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsEmptyElementEndTag(); }
    bool IsText() const { return type == Text; }
    bool IsError() const { return type == Error; }

    const char *GetReparsePoint() const;
    void SetValue(TokenType new_type, const char *new_s, const char *end);
    void SetError(ParsingError err, const char *errContext);

    TokenType        type;
    ParsingError     error;
    const char *     s;
    size_t           sLen;

    bool             NameIs(const char *name) const;
    AttrInfo *       GetAttrByName(const char *name);

protected:
    AttrInfo *       NextAttr();
    const char *     nextAttr;
    AttrInfo         attrInfo;
};

/* A very simple pull html parser. Call Next() to get the next HtmlToken,
which can be one one of 3 tag types or error. If a tag has attributes,
the caller has to parse them out (using HtmlToken::NextAttr()) */
class HtmlPullParser {
    const char *   currPos;
    const char *   end;

    HtmlToken      currToken;

public:
    Vec<HtmlTag>   tagNesting;

    HtmlPullParser(const char *s, size_t len) : currPos(s), end(s + len) { }
    HtmlPullParser(const char *s, const char *end) : currPos(s), end(end) { }

    HtmlToken *Next();
};

void        SkipWs(const char*& s, const char *end);
void        SkipNonWs(const char*& s, const char *end);
bool        IsSpaceOnly(const char *s, const char *end);
bool        IsInArray(uint8 val, uint8 *arr, size_t arrLen);
bool        IsTagSelfClosing(const char *s, size_t len = -1);
bool        IsTagSelfClosing(HtmlTag tag);
bool        IsInlineTag(HtmlTag tag);

int         HtmlEntityNameToRune(const char *name, size_t nameLen);
int         HtmlEntityNameToRune(const WCHAR *name, size_t nameLen);

HtmlTag     FindTag(const char *s, size_t len = -1);
HtmlTag     FindTag(HtmlToken *tok);

AlignAttr   GetAlignAttrByName(const char *attr, size_t len);

char *      PrettyPrintHtml(const char *s, size_t len, size_t& lenOut);
const char *ResolveHtmlEntities(const char *s, const char *end, Allocator *alloc);

#endif
