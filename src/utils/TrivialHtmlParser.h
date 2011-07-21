/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef TrivialHtmlParser_h
#define TrivialHtmlParser_h

#include "Vec.h"

typedef size_t HtmlElementId;
typedef size_t HtmlAttrId;

enum HtmlParseError {
    ErrParsingNoError,
    ErrParsingElement, // syntax error parsing element
    ErrParsingExclOrPI,
    ErrParsingClosingElement, // syntax error in closing element
    ErrParsingElementName, // syntax error after element name
    ErrParsingAttributes, // syntax error in attributes
    ErrParsingAttributeName, // syntax error after attribute name
    ErrParsingAttributeValue,
};

struct HtmlElement {
    char *name;
    char *val;
    HtmlAttrId firstAttrId;
    HtmlElementId up, down, next;
};

struct HtmlAttr {
    char *name;
    char *val;
    HtmlAttrId nextAttrId;
};

class HtmlParser {
    Vec<HtmlElement> elAllocator;
    Vec<HtmlAttr> attrAllocator;

    // text to parse. It can be changed.
    char *html;
    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeHtml;

    HtmlElementId currElementId;

    HtmlElementId AllocElement(HtmlElementId parentId, char *name, HtmlElement **elOut);
    HtmlAttrId AllocAttr(HtmlAttr **attrOut);

    void CloseTag(char *tagName);
    void StartTag(char *tagName);
    void StartAttr(char *attrName);
    void SetAttrVal(char *attrVal);
    bool ParseError(HtmlParseError err) {
        error = err;
        return false;
    }

public:
    static const HtmlElementId RootElementId = 0;

    HtmlParseError error;  // parsing error, a static string
    char *errorContext; // pointer within html showing which part we failed to parse

    HtmlParser();
    ~HtmlParser();

    bool Parse(const char *s);
    bool ParseInPlace(char *s);

    HtmlElement *GetRootElement() const {
        return GetElement(RootElementId);
    }
    HtmlElement *GetElement(HtmlElementId id) const;
    size_t ElementsCount() const {
        return elAllocator.Count();
    }
    HtmlAttr *GetAttr(HtmlAttrId id) const;

    size_t TotalAttrCount() const {
        return attrAllocator.Count();
    }
    char *GetElementName(HtmlElementId id) const;
    char *GetAttrName(HtmlAttrId id) const;
    HtmlElementId GetParent(HtmlElementId id) const;
    size_t GetSiblingCount(HtmlElementId id) const;
    HtmlElementId GetSibling(HtmlElementId id, size_t siblingNo) const;
    HtmlAttr *GetAttrByName(HtmlElement *el, const char *name) const;
};

#endif
