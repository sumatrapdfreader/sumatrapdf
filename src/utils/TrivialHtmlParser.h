/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef TrivialHtmlParser_h
#define TrivialHtmlParser_h

#include "Vec.h"

static inline size_t RoundUpTo8(size_t n)
{
    return ((n+8-1)/8)*8;
}

class BlockAllocator {
    struct MemBlockNode {
        struct MemBlockNode *next;
    };

    size_t remainsInBlock;
    char *currMem;
    MemBlockNode *currBlock;

    void Init() {
        currBlock = NULL;
        currMem = NULL;
        remainsInBlock = 0;
    }
public:
    size_t  blockSize;

    BlockAllocator()  {
        blockSize = 4096;
        Init();
    }

    void FreeAll() {
        MemBlockNode *b = currBlock;
        while (b) {
            MemBlockNode *next = b->next;
            free(b);
            b = next;
        }
        Init();
    }

    ~BlockAllocator() {
        FreeAll();
    }

    void AllocBlock(size_t minSize) {
        minSize = RoundUpTo8(minSize);
        size_t size = blockSize;
        if (minSize > size)
            size = minSize;
        MemBlockNode *node = (MemBlockNode*)calloc(1, sizeof(MemBlockNode) + size);
        currMem = (char*)node + sizeof(MemBlockNode);
        remainsInBlock = size;
        node->next = currBlock;
        currBlock = node;
    }

    void *Alloc(size_t size) {
        size = RoundUpTo8(size);
        if (remainsInBlock < size) {
            AllocBlock(size);
        }
        void *mem = (void*)currMem;
        currMem += size;
        remainsInBlock -= size;
        return mem;
    }

    // only valid for structs, could alloc objects with
    // placement new()
    template <typename T>
    T *AllocStruct() {
        return (T *)Alloc(sizeof(T));
    }
};

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

struct HtmlAttr;

struct HtmlElement {
    char *name;
    HtmlAttr *firstAttr;
    HtmlElement *up, *down, *next;

    TCHAR *GetAttribute(const char *name) const;
    HtmlElement *GetChildByName(const char *name, int idx=0) const;
};

class HtmlParser {
    BlockAllocator allocator;

    // text to parse. It can be changed.
    char *html;
    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeHtml;

    size_t elementsCount;
    size_t attributesCount;

    HtmlElement *rootElement;
    HtmlElement *currElement;

    HtmlElement *AllocElement(char *name, HtmlElement *parent);
    HtmlAttr *AllocAttr(char *name, HtmlAttr *next);

    void CloseTag(char *tagName);
    void StartTag(char *tagName);
    void StartAttr(char *name);
    void SetAttrVal(char *val);
    HtmlElement *FindParent(char *tagName);
    HtmlElement *ParseError(HtmlParseError err) {
        error = err;
        return NULL;
    }

public:
    HtmlParseError error;  // parsing error, a static string
    char *errorContext; // pointer within html showing which part we failed to parse

    HtmlParser();
    ~HtmlParser();

    HtmlElement *Parse(const char *s);
    HtmlElement *ParseInPlace(char *s);

    size_t ElementsCount() const {
        return elementsCount;
    }

    size_t TotalAttrCount() const {
        return attributesCount;
    }

    HtmlElement *FindElementByName(const char *name, HtmlElement *from=NULL);
};

#endif
