/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum HtmlParseError {
    ErrParsingNoError,
    ErrParsingElement, // syntax error parsing element
    ErrParsingExclOrPI,
    ErrParsingClosingElement, // syntax error in closing element
    ErrParsingElementName,    // syntax error after element name
    ErrParsingAttributes,     // syntax error in attributes
    ErrParsingAttributeName,  // syntax error after attribute name
    ErrParsingAttributeValue,
};

struct HtmlToken;

struct HtmlAttr {
    Str name;
    Str val;
    HtmlAttr* next;
};

struct HtmlElement {
    HtmlTag tag;
    Str name; // empty whenever tag != Tag_NotFound
    HtmlAttr* firstAttr;
    HtmlElement *up, *down, *next;
    uint codepage;

    bool NameIs(Str name) const;
    bool NameIsNS(Str name, Str ns) const;

    WStr GetAttribute(Str name) const;
    Str GetAttributeTemp(Str name) const;
    HtmlElement* GetChildByTag(HtmlTag tag, int idx = 0) const;
};

class HtmlParser {
    Arena* allocator = nullptr;

    // text to parse. It can be changed.
    char* html = nullptr;
    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeHtml = false;
    // the codepage used for converting text to Unicode
    uint codepage{CP_ACP};

    size_t elementsCount = 0;
    size_t attributesCount = 0;

    HtmlElement* rootElement = nullptr;
    HtmlElement* currElement = nullptr;

    HtmlElement* AllocElement(HtmlTag tag, Str name, HtmlElement* parent);
    HtmlAttr* AllocAttr(Str name, HtmlAttr* next);

    void CloseTag(HtmlToken* tok);
    void StartTag(HtmlToken* tok);
    void AppendAttr(Str name, Str value);

    HtmlElement* FindParent(HtmlToken* tok);
    HtmlElement* ParseError(HtmlParseError err) {
        error = err;
        return nullptr;
    }

    void Reset();

  public:
    HtmlParseError error{ErrParsingNoError}; // parsing error, a static string
    Str errorContext;                        // slice within html showing which part we failed to parse

    HtmlParser();
    ~HtmlParser();

    HtmlElement* Parse(const ByteSlice& d, uint codepage = CP_ACP);
    HtmlElement* ParseInPlace(const ByteSlice& d, uint codepage = CP_ACP);

    size_t ElementsCount() const;
    size_t TotalAttrCount() const;

    HtmlElement* FindElementByName(Str name, HtmlElement* from = nullptr);
    HtmlElement* FindElementByNameNS(Str name, Str ns, HtmlElement* from = nullptr);
};

WStr DecodeHtmlEntitites(Str string, uint codepage);
Str DecodeHtmlEntititesTemp(Str string, uint codepage);

namespace strconv {

inline WStr FromHtmlUtf8(Str s) {
    TempStr tmp = str::DupTemp(s);
    return DecodeHtmlEntitites(tmp, CP_UTF8);
}

inline Str FromHtmlUtf8Temp(Str s) {
    TempStr tmp = str::DupTemp(s);
    return DecodeHtmlEntititesTemp(tmp, CP_UTF8);
}
} // namespace strconv
