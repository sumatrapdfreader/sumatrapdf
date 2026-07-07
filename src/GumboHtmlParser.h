/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "GumboHelpers.h"

struct AttrInfo {
    Str name;
    Str val;

    bool NameIs(Str s) const;
    bool NameIsNS(Str nameToCheck, Str ns) const;
    bool ValIs(Str s) const;
};

struct HtmlToken {
    enum TokenType {
        StartTag,
        EndTag,
        EmptyElementTag,
        Text,
        Error
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsEmptyElementEndTag() const { return type == EmptyElementTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsEmptyElementEndTag(); }
    bool IsText() const { return type == Text; }
    bool IsError() const { return type == Error; }

    Str GetReparsePoint() const;
    void SetTag(TokenType newType, Str name);
    void SetText(Str slice);

    TokenType type = Error;
    Str s;
    Str name;
    Str reparsePoint;
    HtmlTag tag = Tag_NotFound;
    const GumboNode* node = nullptr;

    bool NameIs(Str nameToFind) const;
    bool NameIsNS(Str nameToCheck, Str ns) const;
    AttrInfo* GetAttrByName(Str name);
    AttrInfo* GetAttrByNameNS(Str name, Str attrNS);

  private:
    AttrInfo attrInfo;
};

class GumboHtmlParser {
    struct Event {
        HtmlToken::TokenType type = HtmlToken::Error;
        const GumboNode* node = nullptr;
        Str s;
        Str name;
        Str reparsePoint;
        ptrdiff_t off = 0;
    };

    Str html;
    GumboOptions opts{};
    GumboOutput* output = nullptr;
    Vec<Event> events;
    size_t eventIdx = 0;
    ptrdiff_t textStartOff = -1;

    HtmlToken currToken{};

    void BuildEvents();
    HtmlToken* TokenFromEvent(Event& ev);

  public:
    explicit GumboHtmlParser(Str s);
    ~GumboHtmlParser();

    void SetCurrPosOff(ptrdiff_t off);
    size_t Len() const { return (size_t)html.len; }
    Str Html() const { return html; }
    int PosOf(Str p) const;

    HtmlToken* Next();
};

bool SkipWs(Str s, int& off);
bool SkipNonWs(Str s, int& off);
bool SkipUntil(Str s, int& off, char c);
bool SkipUntil(Str s, int& off, Str term);
bool IsSpaceOnly(Str s);

int HtmlEntityNameToRune(Str name);

Str ResolveHtmlEntity(Str str, int& rune);
Str ResolveHtmlEntities(Str s, Arena* a);
Str ResolveHtmlEntities(Str s);
Str ResolveHtmlEntitiesTemp(Str s);

namespace strconv {
inline TempStr HtmlUtf8ToStrTemp(Str s) {
    return ResolveHtmlEntitiesTemp(s);
}
} // namespace strconv
