/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct AttrInfo {
    Str name;
    Str val;

    bool NameIs(Str s) const;
    bool NameIsNS(Str nameToCheck, Str ns) const;
    bool ValIs(Str s) const;
};

struct HtmlToken {
    enum TokenType {
        StartTag,        // <foo>
        EndTag,          // </foo>
        EmptyElementTag, // <foo/>
        Text,            // <foo>text</foo> => "text"
        Error
    };

    enum ParsingError {
        NoError,
        ExpectedElement,
        UnclosedTag,
        InvalidTag
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsEmptyElementEndTag() const { return type == EmptyElementTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsEmptyElementEndTag(); }
    bool IsText() const { return type == Text; }
    bool IsError() const { return type == Error; }

    Str GetReparsePoint() const;
    void SetTag(TokenType new_type, Str slice);
    void SetError(ParsingError err, Str errContext);
    void SetText(Str slice);

    TokenType type = Error;
    ParsingError error = NoError;
    Str s;

    // only for tags: type and name length
    HtmlTag tag = Tag_NotFound;
    size_t nLen = 0;

    bool NameIs(Str name) const;
    bool NameIsNS(Str name, Str ns) const;
    AttrInfo* GetAttrByName(Str name);
    AttrInfo* GetAttrByNameNS(Str name, Str attrNS);

  protected:
    AttrInfo* NextAttr();
    int nextAttrOff = -1;
    AttrInfo attrInfo;
};

/* A very simple pull html parser. Call Next() to get the next HtmlToken,
which can be one one of 3 tag types or error. If a tag has attributes,
use HtmlToken::GetAttrByName() / GetAttrByNameNS() to access them. */
class HtmlPullParser {
    Str html;
    int currPos = 0;

    HtmlToken currToken{};

  public:
    explicit HtmlPullParser(Str s) : html(s) {}

    void SetCurrPosOff(ptrdiff_t off) { currPos = (int)off; }
    size_t Len() const { return (size_t)html.len; }
    Str Html() const { return html; }
    int PosOf(Str p) const { return (int)(p.s - html.s); }

    HtmlToken* Next();
};

bool SkipWs(Str s, int& off);
bool SkipNonWs(Str s, int& off);
bool SkipUntil(Str s, int& off, char c);
bool SkipUntil(Str s, int& off, Str term);
bool IsSpaceOnly(Str s);

int HtmlEntityNameToRune(Str name);
int HtmlEntityNameToRune(WStr name);

Str ResolveHtmlEntity(Str str, int& rune);
Str ResolveHtmlEntities(Str s, Arena* alloc);
Str ResolveHtmlEntities(Str s);
Str ResolveHtmlEntitiesTemp(Str s);

WStr DecodeHtmlEntities(Str string, uint codepage);
Str DecodeHtmlEntitiesTemp(Str string, uint codepage);

namespace strconv {
inline TempStr HtmlUtf8ToStrTemp(Str s) {
    TempStr tmp = str::DupTemp(s);
    return DecodeHtmlEntitiesTemp(tmp, CP_UTF8);
}

inline TempWStr HtmlUtf8ToWStrTemp(Str s) {
    TempStr tmp = str::DupTemp(s);
    WStr ws = DecodeHtmlEntities(tmp, CP_UTF8);
    TempWStr res = str::DupTemp(ws);
    wstr::Free(ws);
    return res;
}
} // namespace strconv
