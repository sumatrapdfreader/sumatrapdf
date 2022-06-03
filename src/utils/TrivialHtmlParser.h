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
    char* name;
    char* val;
    HtmlAttr* next;
};

struct HtmlElement {
    HtmlTag tag;
    char* name; // name is nullptr whenever tag != Tag_NotFound
    HtmlAttr* firstAttr;
    HtmlElement *up, *down, *next;
    uint codepage;

    bool NameIs(const char* name) const;
    bool NameIsNS(const char* name, const char* ns) const;

    WCHAR* GetAttribute(const char* name) const;
    char* GetAttributeTemp(const char* name) const;
    HtmlElement* GetChildByTag(HtmlTag tag, int idx = 0) const;
};

class HtmlParser {
    PoolAllocator allocator;

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

    HtmlElement* AllocElement(HtmlTag tag, char* name, HtmlElement* parent);
    HtmlAttr* AllocAttr(char* name, HtmlAttr* next);

    void CloseTag(HtmlToken* tok);
    void StartTag(HtmlToken* tok);
    void AppendAttr(char* name, char* value);

    HtmlElement* FindParent(HtmlToken* tok);
    HtmlElement* ParseError(HtmlParseError err) {
        error = err;
        return nullptr;
    }

    void Reset();

  public:
    HtmlParseError error{ErrParsingNoError}; // parsing error, a static string
    const char* errorContext = nullptr;      // pointer within html showing which part we failed to parse

    HtmlParser();
    ~HtmlParser();

    HtmlElement* Parse(const ByteSlice& d, uint codepage = CP_ACP);
    HtmlElement* ParseInPlace(const ByteSlice& d, uint codepage = CP_ACP);

    size_t ElementsCount() const;
    size_t TotalAttrCount() const;

    HtmlElement* FindElementByName(const char* name, HtmlElement* from = nullptr);
    HtmlElement* FindElementByNameNS(const char* name, const char* ns, HtmlElement* from = nullptr);
};

WCHAR* DecodeHtmlEntitites(const char* string, uint codepage);
char* DecodeHtmlEntititesTemp(const char* string, uint codepage);

namespace strconv {

inline WCHAR* FromHtmlUtf8(const char* s, size_t len) {
    char* tmp = str::DupTemp(s, len);
    return DecodeHtmlEntitites(tmp, CP_UTF8);
}

inline char* FromHtmlUtf8Temp(const char* s, size_t len) {
    char* tmp = str::DupTemp(s, len);
    return DecodeHtmlEntititesTemp(tmp, CP_UTF8);
}
} // namespace strconv
