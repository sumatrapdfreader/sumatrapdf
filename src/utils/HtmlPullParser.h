/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef HtmlPullParser_h
#define HtmlPullParser_h

#include "Vec.h"

// enums must match HTML_TAGS_STRINGS order
enum HtmlTag {
    Tag_NotFound = -1,
    Tag_A = 0,
    Tag_Abbr = 1,
    Tag_Acronym = 2,
    Tag_Audio = 3,
    Tag_B = 4,
    Tag_Blockquote = 5,
    Tag_Body = 6,
    Tag_Br = 7,
    Tag_Center = 8,
    Tag_Code = 9,
    Tag_Dd = 10,
    Tag_Div = 11,
    Tag_Dl = 12,
    Tag_Dt = 13,
    Tag_Em = 14,
    Tag_Font = 15,
    Tag_Guide = 16,
    Tag_H1 = 17,
    Tag_H2 = 18,
    Tag_H3 = 19,
    Tag_H4 = 20,
    Tag_H5 = 21,
    Tag_Head = 22,
    Tag_Hr = 23,
    Tag_Html = 24,
    Tag_I = 25,
    Tag_Img = 26,
    Tag_Li = 27,
    Tag_Link = 28,
    Tag_Mbp_Pagebreak = 29,
    Tag_Meta = 30,
    Tag_Object = 31,
    Tag_Ol = 32,
    Tag_P = 33,
    Tag_Pagebreak = 34,
    Tag_Pre = 35,
    Tag_Reference = 36,
    Tag_S = 37,
    Tag_Small = 38,
    Tag_Span = 39,
    Tag_Strike = 40,
    Tag_Strong = 41,
    Tag_Style = 42,
    Tag_Sub = 43,
    Tag_Sup = 44,
    Tag_Table = 45,
    Tag_Td = 46,
    Tag_Th = 47,
    Tag_Title = 48,
    Tag_Tr = 49,
    Tag_Tt = 50,
    Tag_U = 51,
    Tag_Ul = 52,
    Tag_Video = 53,
    Tag_Last = 54
};
#define HTML_TAGS_STRINGS "a\0abbr\0acronym\0audio\0b\0blockquote\0body\0br\0center\0code\0dd\0div\0dl\0dt\0em\0font\0guide\0h1\0h2\0h3\0h4\0h5\0head\0hr\0html\0i\0img\0li\0link\0mbp:pagebreak\0meta\0object\0ol\0p\0pagebreak\0pre\0reference\0s\0small\0span\0strike\0strong\0style\0sub\0sup\0table\0td\0th\0title\0tr\0tt\0u\0ul\0video\0"

// enums must match HTML_ATTRS_STRINGS order
enum HtmlAttr {
    Attr_NotFound = -1,
    Attr_Align = 0,
    Attr_Bgcolor = 1,
    Attr_Border = 2,
    Attr_Class = 3,
    Attr_Clear = 4,
    Attr_Color = 5,
    Attr_Colspan = 6,
    Attr_Controls = 7,
    Attr_Face = 8,
    Attr_Filepos = 9,
    Attr_Height = 10,
    Attr_Href = 11,
    Attr_Id = 12,
    Attr_Lang = 13,
    Attr_Link = 14,
    Attr_Mediarecindex = 15,
    Attr_Recindex = 16,
    Attr_Rowspan = 17,
    Attr_Size = 18,
    Attr_Style = 19,
    Attr_Title = 20,
    Attr_Valign = 21,
    Attr_Value = 22,
    Attr_Vlink = 23,
    Attr_Width = 24,
    Attr_Xmlns = 25,
    Attr_Xmlns_Dc = 26,
    Attr_Last = 27
};
#define HTML_ATTRS_STRINGS "align\0bgcolor\0border\0class\0clear\0color\0colspan\0controls\0face\0filepos\0height\0href\0id\0lang\0link\0mediarecindex\0recindex\0rowspan\0size\0style\0title\0valign\0value\0vlink\0width\0xmlns\0xmlns:dc\0"

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

struct HtmlToken {
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

/* A very simple pull html parser. Simply call Next() to get the next part of
html, which can be one one of 3 tag types or error. If a tag has attributes,
the caller has to parse them out (using HtmlToken::NextAttr()) */
class HtmlPullParser {
    const char *   currPos;
    const char *   end;

    HtmlToken      currToken;

public:
    struct State {
        const char *s;
        const char *end;
    };

    HtmlPullParser(const char *s, size_t len) : currPos(s), end(s + len) { }
    HtmlPullParser(const char *s, const char *end) : currPos(s), end(end) { }
    HtmlPullParser(const State& state) : currPos(state.s), end(state.end) { }

    // Get the current state of html parsing. Can be used to restart
    // parsing from that point later on
    State GetParsingState() {
        State state = { currPos, end };
        return state;
    }

    HtmlToken *Next();
};

void        SkipWs(const char*& s, const char *end);
void        SkipNonWs(const char*& s, const char *end);

HtmlTag     FindTag(HtmlToken *tok);
HtmlAttr    FindAttr(AttrInfo *attrInfo);
AlignAttr   FindAlignAttr(const char *attr, size_t len);

void RecordEndTag(Vec<HtmlTag> *tagNesting, HtmlTag tag);
void RecordStartTag(Vec<HtmlTag>* tagNesting, HtmlTag tag);
char *PrettyPrintHtml(const char *s, size_t len, size_t& lenOut);

#endif
